/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file window_resub.hpp
  \brief Windowing for small-window-based, enumeration-based (classical) resubstitution

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../../traits.hpp"
#include "../../views/depth_view.hpp"
#include "../../views/fanout_view.hpp"
#include "../../utils/index_list.hpp"
#include "../../networks/xag.hpp"
#include "../../networks/aig.hpp"
#include "../detail/resub_utils.hpp"
#include "../resyn_engines/xag_resyn.hpp"
#include "../resyn_engines/aig_enumerative.hpp"
#include "../resyn_engines/mig_resyn.hpp"
#include "../resyn_engines/mig_enumerative.hpp"
#include "../simulation.hpp"
#include "../dont_cares.hpp"
#include "../reconv_cut.hpp"
#include <kitty/kitty.hpp>

#include <optional>
#include <functional>
#include <vector>

namespace mockturtle::experimental
{

struct complete_tt_windowing_params
{
  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{8};

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{150};

  /*! \brief Maximum number of nodes added by resubstitution. */
  uint32_t max_inserts{2};

  /*! \brief Maximum fanout of a node to be considered as root. */
  uint32_t skip_fanout_limit_for_roots{1000};

  /*! \brief Maximum fanout of a node to be considered as divisor. */
  uint32_t skip_fanout_limit_for_divisors{100};

  /*! \brief Use don't cares for optimization. */
  bool use_dont_cares{false};

  /*! \brief Window size for don't cares calculation. */
  uint32_t window_size{12u};

  /*! \brief Whether to update node levels lazily. */
  bool update_levels_lazily{false};

  /*! \brief Whether to prevent from increasing depth. */
  bool preserve_depth{false};

  /*! \brief Whether to normalize the truth tables.
   * 
   * For some enumerative resynthesis engines, if the truth tables
   * are normalized, some cases can be eliminated and thus improves
   * efficiency. When this option is turned off, be sure to use an
   * implementation of resynthesis that does not make this assumption;
   * otherwise, quality degradation may be observed.
   * 
   * Normalization is typically only useful for enumerative methods
   * and for smaller solutions (i.e. when `max_inserts` < 2). Turning
   * on normalization may result in larger runtime overhead when there
   * are many divisors or when the truth tables are long.
   */
  bool normalize{false};
};

struct complete_tt_windowing_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{0};

  /*! \brief Accumulated runtime for cut computation. */
  stopwatch<>::duration time_cuts{0};

  /*! \brief Accumulated runtime for mffc computation. */
  stopwatch<>::duration time_mffc{0};

  /*! \brief Accumulated runtime for divisor collection. */
  stopwatch<>::duration time_divs{0};

  /*! \brief Accumulated runtime for simulation. */
  stopwatch<>::duration time_sim{0};

  /*! \brief Accumulated runtime for don't care computation. */
  stopwatch<>::duration time_dont_care{0};

  /*! \brief Total number of leaves. */
  uint64_t num_leaves{0u};

  /*! \brief Total number of divisors. */
  uint64_t num_divisors{0u};

  /*! \brief Number of constructed windows. */
  uint32_t num_windows{0u};

  /*! \brief Total number of MFFC nodes. */
  uint64_t sum_mffc_size{0u};

  void report() const
  {
    // clang-format off
    fmt::print( "[i] complete_tt_windowing report\n" );
    fmt::print( "    tot. #leaves = {:5d}, tot. #divs = {:5d}, sum  |MFFC| = {:5d}\n", num_leaves, num_divisors, sum_mffc_size );
    fmt::print( "    avg. #leaves = {:>5.2f}, avg. #divs = {:>5.2f}, avg. |MFFC| = {:>5.2f}\n", float( num_leaves ) / float( num_windows ), float( num_divisors ) / float( num_windows ), float( sum_mffc_size ) / float( num_windows ) );
    fmt::print( "    ===== Runtime Breakdown =====\n" );
    fmt::print( "    Total       : {:>5.2f} secs\n", to_seconds( time_total ) );
    fmt::print( "      Cut       : {:>5.2f} secs\n", to_seconds( time_cuts ) );
    fmt::print( "      MFFC      : {:>5.2f} secs\n", to_seconds( time_mffc ) );
    fmt::print( "      Divs      : {:>5.2f} secs\n", to_seconds( time_divs ) );
    fmt::print( "      Simulation: {:>5.2f} secs\n", to_seconds( time_sim ) );
    fmt::print( "      Dont cares: {:>5.2f} secs\n", to_seconds( time_dont_care ) );
    // clang-format on
  }
};

namespace detail
{

template<class Ntk, class TT>
struct small_window
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  signal root;
  std::vector<signal> divs;
  std::vector<uint32_t> div_ids; /* positions of divisor truth tables in `tts` */
  std::vector<TT> tts;
  TT care;
  uint32_t mffc_size;
  uint32_t max_size{std::numeric_limits<uint32_t>::max()};
  uint32_t max_level{std::numeric_limits<uint32_t>::max()};
};

template<class Ntk, class TT = kitty::dynamic_truth_table>
class complete_tt_windowing
{
public:
  using problem_t = small_window<Ntk, TT>;
  using params_t = complete_tt_windowing_params;
  using stats_t = complete_tt_windowing_stats;

  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit complete_tt_windowing( Ntk& ntk, params_t const& ps, stats_t& st )
    : ntk( ntk ), ps( ps ), st( st ), cps( {ps.max_pis} ), mffc_mgr( ntk ), 
      divs_mgr( ntk, divisor_collector_params( {ps.max_divisors, ps.max_divisors, ps.skip_fanout_limit_for_divisors} ) ),
      sim( ntk, win.tts, ps.max_pis )
  {
    static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
    static_assert( has_value_v<Ntk>, "Ntk does not implement the value method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
    static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the substitute_node method" );
    if constexpr ( !has_level_v<Ntk> )
    {
      assert( !ps.preserve_depth && "Ntk does not have depth interface" );
      assert( !ps.update_levels_lazily && "Ntk does not have depth interface" );
    }
  }

  ~complete_tt_windowing()
  {
    if constexpr ( has_level_v<Ntk> )
    {
      if ( lazy_update_event )
      {
        mockturtle::detail::release_lazy_level_update_events( ntk, lazy_update_event );
      }
    }
  }

  void init()
  {
    if constexpr ( has_level_v<Ntk> )
    {
      if ( ps.update_levels_lazily )
      {
        lazy_update_event = mockturtle::detail::register_lazy_level_update_events( ntk );
      }
    }
  }

  std::optional<std::reference_wrapper<problem_t>> operator()( node const& n )
  {
    stopwatch t( st.time_total );
    if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
    {
      return std::nullopt; /* skip nodes with too many fanouts */
    }

    if constexpr ( has_level_v<Ntk> )
    {
      if ( ps.preserve_depth )
      {
        win.max_level = ntk.level( n ) - 1;
        divs_mgr.set_max_level( win.max_level );
      }
    }

    /* compute a cut and collect supported nodes */
    std::vector<node> leaves = call_with_stopwatch( st.time_cuts, [&]() {
      return reconvergence_driven_cut<Ntk, false, has_level_v<Ntk>>( ntk, {n}, cps ).first;
    });
    std::vector<node> supported;
    call_with_stopwatch( st.time_divs, [&]() {
      divs_mgr.collect_supported_nodes( n, leaves, supported );
    });

    /* simulate */
    call_with_stopwatch( st.time_sim, [&]() {
      sim.simulate( leaves, supported );
    });

    /* mark MFFC nodes and collect divisors */
    ++mffc_marker;
    win.mffc_size = call_with_stopwatch( st.time_mffc, [&]() {
      return mffc_mgr.call_on_mffc_and_count( n, leaves, [&]( node const& n ){
        ntk.set_value( n, mffc_marker );
      });
    });
    call_with_stopwatch( st.time_divs, [&]() {
      collect_divisors( leaves, supported );
    });

    /* normalize */
    call_with_stopwatch( st.time_sim, [&]() {
      if ( ps.normalize )
      {
        win.root = normalize_truth_tables() ? !ntk.make_signal( n ) : ntk.make_signal( n );
      }
      else
      {
        win.root = ntk.make_signal( n );
      }
    });

    /* compute don't cares */
    call_with_stopwatch( st.time_dont_care, [&]() {
      if ( ps.use_dont_cares )
      {
        win.care = ~satisfiability_dont_cares( ntk, leaves, ps.window_size );
      }
      else
      {
        win.care = ~kitty::create<TT>( ps.max_pis );
      }
    });

    win.max_size = std::min( win.mffc_size - 1, ps.max_inserts );

    st.num_windows++;
    st.num_leaves += leaves.size();
    st.num_divisors += win.divs.size();
    st.sum_mffc_size += win.mffc_size;

    return win;
  }

  template<typename res_t>
  uint32_t gain( problem_t const& prob, res_t const& res ) const
  {
    static_assert( is_index_list_v<res_t>, "res_t is not an index_list (windowing engine and resynthesis engine do not match)" );
    return prob.mffc_size - res.num_gates();
  }

  template<typename res_t>
  bool update_ntk( problem_t const& prob, res_t const& res )
  {
    static_assert( is_index_list_v<res_t>, "res_t is not an index_list (windowing engine and resynthesis engine do not match)" );
    assert( res.num_pos() == 1 );
    insert( ntk, std::begin( prob.divs ), std::end( prob.divs ), res, [&]( signal const& g ){
      ntk.substitute_node( ntk.get_node( prob.root ), ntk.is_complemented( prob.root ) ? !g : g );
    } );
    return true; /* continue optimization */
  }

  template<typename res_t>
  bool report( problem_t const& prob, res_t const& res )
  {
    static_assert( is_index_list_v<res_t>, "res_t is not an index_list (windowing engine and resynthesis engine do not match)" );
    assert( res.num_pos() == 1 );
    fmt::print( "[i] found solution {} for root signal {}{}\n", to_index_list_string( res ), ntk.is_complemented( prob.root ) ? "!" : "", ntk.get_node( prob.root ) );
    return true;
  }

private:
  void collect_divisors( std::vector<node> const& leaves, std::vector<node> const& supported )
  {
    win.divs.clear();
    win.div_ids.clear();

    uint32_t i{1};
    for ( auto const& l : leaves )
    {
      win.div_ids.emplace_back( i++ );
      win.divs.emplace_back( ntk.make_signal( l ) );
    }

    i = ps.max_pis + 1;
    for ( auto const& n : supported )
    {
      if ( ntk.value( n ) != mffc_marker ) /* not in MFFC, not root */
      {
        win.div_ids.emplace_back( i );
        win.divs.emplace_back( ntk.make_signal( n ) );
      }
      ++i;
    }
    assert( i == win.tts.size() );
  }

  bool normalize_truth_tables()
  {
    assert( win.divs.size() == win.div_ids.size() );
    for ( auto i = 0u; i < win.divs.size(); ++i )
    {
      if ( kitty::get_bit( win.tts.at( win.div_ids.at( i ) ), 0 ) )
      {
        win.tts.at( win.div_ids.at( i ) ) = ~win.tts.at( win.div_ids.at( i ) );
        win.divs.at( i ) = !win.divs.at( i );
      }
    }

    if ( kitty::get_bit( win.tts.back(), 0 ) )
    {
      win.tts.back() = ~win.tts.back();
      return true;
    }
    else
    {
      return false;
    }
  }

private:
  Ntk& ntk;
  problem_t win;
  params_t const& ps;
  stats_t& st;
  reconvergence_driven_cut_parameters const cps;
  typename mockturtle::detail::node_mffc_inside<Ntk> mffc_mgr; // TODO: namespaces can be removed when we move out of experimental::
  divisor_collector<Ntk> divs_mgr;
  window_simulator<Ntk, TT> sim;
  uint32_t mffc_marker{0u};
  std::shared_ptr<typename network_events<Ntk>::modified_event_type> lazy_update_event;
}; /* complete_tt_windowing */

template<class Ntk, class TT, class ResynEngine, bool preserve_depth = false>
class complete_tt_resynthesis
{
public:
  using problem_t = small_window<Ntk, TT>;
  using res_t = typename ResynEngine::index_list_t;
  using params_t = null_params;
  using stats_t = null_stats;

  explicit complete_tt_resynthesis( Ntk const& ntk, params_t const& ps, stats_t& st )
    : ntk( ntk ), engine( rst )
  { }

  void init()
  { }

  std::optional<res_t> operator()( problem_t& prob )
  {
    if constexpr ( preserve_depth ) // TODO: maybe separate via different problem type
    {
      return engine( prob.tts.back(), prob.care, std::begin( prob.div_ids ), std::end( prob.div_ids ), prob.tts, prob.max_size, prob.max_level );
    }
    else
    {
      return engine( prob.tts.back(), prob.care, std::begin( prob.div_ids ), std::end( prob.div_ids ), prob.tts, prob.max_size );
    }
  }

private:
  Ntk const& ntk;
  typename ResynEngine::stats rst;
  ResynEngine engine;
}; /* complete_tt_resynthesis */

} /* namespace detail */

using window_resub_params = boolean_optimization_params<complete_tt_windowing_params, null_params>;
using window_resub_stats = boolean_optimization_stats<complete_tt_windowing_stats, null_stats>;

template<class Ntk>
void window_xag_heuristic_resub( Ntk& ntk, window_resub_params const& ps = {}, window_resub_stats* pst = nullptr )
{
  static_assert( std::is_same_v<typename Ntk::base_type, xag_network>, "Ntk::base_type is not xag_network" );

  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  fanout_view<Ntk> fntk( ntk );
  ViewedNtk viewed( fntk );

  using TT = typename kitty::dynamic_truth_table;  
  using windowing_t = typename detail::complete_tt_windowing<ViewedNtk, TT>;
  using engine_t = xag_resyn_decompose<TT, xag_resyn_static_params_default<TT>>;
  using resyn_t = typename detail::complete_tt_resynthesis<ViewedNtk, TT, engine_t>; 
  using opt_t = typename detail::boolean_optimization_impl<ViewedNtk, windowing_t, resyn_t>;

  window_resub_stats st;
  opt_t p( viewed, ps, st );
  p.run();

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
}

template<class Ntk>
void window_aig_heuristic_resub( Ntk& ntk, window_resub_params const& ps = {}, window_resub_stats* pst = nullptr )
{
  static_assert( std::is_same_v<typename Ntk::base_type, aig_network>, "Ntk::base_type is not aig_network" );

  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  fanout_view<Ntk> fntk( ntk );
  ViewedNtk viewed( fntk );

  using TT = typename kitty::dynamic_truth_table;  
  using windowing_t = typename detail::complete_tt_windowing<ViewedNtk, TT>;
  using engine_t = xag_resyn_decompose<TT, aig_resyn_static_params_default<TT>>;
  using resyn_t = typename detail::complete_tt_resynthesis<ViewedNtk, TT, engine_t>; 
  using opt_t = typename detail::boolean_optimization_impl<ViewedNtk, windowing_t, resyn_t>;

  window_resub_stats st;
  opt_t p( viewed, ps, st );
  p.run();

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
}

template<class Ntk>
void window_aig_enumerative_resub( Ntk& ntk, window_resub_params const& ps = {}, window_resub_stats* pst = nullptr )
{
  //using ViewedNtk = fanout_view<Ntk>;
  //ViewedNtk viewed( ntk );
  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  fanout_view<Ntk> fntk( ntk );
  ViewedNtk viewed( fntk );

  window_resub_stats st;

  using TT = typename kitty::static_truth_table<8>;
  using windowing_t = typename detail::complete_tt_windowing<ViewedNtk, TT>;
  if ( ps.wps.normalize )
  {
    using engine_t = aig_enumerative_resyn<TT, true>;
    using resyn_t = typename detail::complete_tt_resynthesis<ViewedNtk, TT, engine_t>; 
    using opt_t = typename detail::boolean_optimization_impl<ViewedNtk, windowing_t, resyn_t>;
    
    opt_t p( viewed, ps, st );
    p.run();
  }
  else
  {
    using engine_t = aig_enumerative_resyn<TT>;
    using resyn_t = typename detail::complete_tt_resynthesis<ViewedNtk, TT, engine_t>; 
    using opt_t = typename detail::boolean_optimization_impl<ViewedNtk, windowing_t, resyn_t>;
    
    opt_t p( viewed, ps, st );
    p.run();
  }

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
}

template<class Ntk>
void window_mig_heuristic_resub( Ntk& ntk, window_resub_params const& ps = {}, window_resub_stats* pst = nullptr )
{
  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  fanout_view<Ntk> fntk( ntk );
  ViewedNtk viewed( fntk );

  using TT = typename kitty::dynamic_truth_table;
  using windowing_t = typename detail::complete_tt_windowing<ViewedNtk, TT>;
  using engine_t = mig_resyn_topdown<TT>;
  using resyn_t = typename detail::complete_tt_resynthesis<ViewedNtk, TT, engine_t>; 
  using opt_t = typename detail::boolean_optimization_impl<ViewedNtk, windowing_t, resyn_t>;

  window_resub_stats st;
  opt_t p( viewed, ps, st );
  p.run();

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
}

template<class Ntk>
void window_mig_enumerative_resub( Ntk& ntk, window_resub_params const& ps = {}, window_resub_stats* pst = nullptr )
{
  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  fanout_view<Ntk> fntk( ntk );
  ViewedNtk viewed( fntk );

  using TT = typename kitty::dynamic_truth_table;
  using windowing_t = typename detail::complete_tt_windowing<ViewedNtk, TT>;
  using engine_t = mig_enumerative_resyn<TT>;
  using resyn_t = typename detail::complete_tt_resynthesis<ViewedNtk, TT, engine_t>; 
  using opt_t = typename detail::boolean_optimization_impl<ViewedNtk, windowing_t, resyn_t>;

  window_resub_stats st;
  opt_t p( viewed, ps, st );
  p.run();

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle::experimental */