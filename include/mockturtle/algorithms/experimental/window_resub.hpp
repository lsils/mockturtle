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
};

struct complete_tt_windowing_stats
{
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
    // clang-format on
  }
};

namespace detail
{

template<class Ntk, class TT>
struct small_window
{
  using node = typename Ntk::node;
  node root;
  std::vector<node> divs;
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
    : ntk( ntk ), ps( ps ), st( st ), cps( {ps.max_pis} ), mffc_mgr( ntk )
  {
    static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method (please wrap with fanout_view)" );
    // TODO: static asserts, e.g. for depth interface, when needed
  }

  ~complete_tt_windowing()
  {
    if ( lazy_update_event )
    {
      mockturtle::detail::release_lazy_level_update_events( ntk, lazy_update_event );
    }
  }

  void init()
  {
    if ( ps.update_levels_lazily )
    {
      lazy_update_event = mockturtle::detail::register_lazy_level_update_events( ntk );
    }
  }

  std::optional<std::reference_wrapper<problem_t>> operator()( node const& n )
  {
    if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
    {
      return std::nullopt; /* skip nodes with too many fanouts */
    }

    win.root = n;
    if ( ps.preserve_depth )
    {
      win.max_level = ntk.level( n ) - 1;
    }

    /* compute a cut and collect supported nodes */
    std::vector<node> leaves = reconvergence_driven_cut( ntk, {n}, cps ).first;
    std::vector<node> supported;
    if ( !collect_supported_nodes( leaves, supported ) )
    {
      return std::nullopt; /* skip too large window */
    }

    /* simulate */
    if constexpr ( std::is_same_v<TT, kitty::dynamic_truth_table> )
    {
      default_simulator<kitty::dynamic_truth_table> sim( ps.max_pis );
      simulate_window<TT>( ntk, leaves, supported, win.tts, sim );
    }
    else
    {
      simulate_window<TT>( ntk, leaves, supported, win.tts );
    }

    /* mark MFFC nodes and collect divisors */
    ++mffc_marker;
    win.mffc_size = mffc_mgr.call_on_mffc_and_count( n, leaves, [&]( node const& n ){
      ntk.set_value( n, mffc_marker );
    });
    collect_divisors( leaves, supported );

    /* compute don't cares */
    if ( ps.use_dont_cares )
    {
      win.care = ~satisfiability_dont_cares( ntk, leaves, ps.window_size );
    }
    else
    {
      win.care = ~kitty::create<TT>( ps.max_pis );
    }

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
    // assert that res_t is some index_list type
    return prob.mffc_size - res.num_gates();
  }

  template<typename res_t>
  bool update_ntk( problem_t const& prob, res_t const& res )
  {
    // assert that res_t is some index_list type
    assert( res.num_pos() == 1 );
    insert<false>( ntk, std::begin( prob.divs ), std::end( prob.divs ), res, [&]( signal const& g ){
      ntk.substitute_node( prob.root, g );
    } );
    return true; /* continue optimization */
  }

  template<typename res_t>
  bool report( problem_t const& prob, res_t const& res )
  {
    // assert that res_t is some index_list type
    assert( res.num_pos() == 1 );
    //insert<false>( ntk, std::begin( prob.divs ), std::end( prob.divs ), res, [&]( signal const& g ){} );
    fmt::print( "[i] found solution {} for root node {}\n", to_index_list_string( res ), prob.root );
    return true;
  }

private:
  void collect_tfi_rec( node const& n, std::vector<node>& supported )
  {
    /* collect until leaves and skip visited nodes */
    if ( ntk.visited( n ) == ntk.trav_id() )
    {
      return;
    }
    ntk.set_visited( n, ntk.trav_id() );

    /* collect in topological order -- lower nodes first */
    ntk.foreach_fanin( n, [&]( const auto& f ) {
      collect_tfi_rec( ntk.get_node( f ), supported );
    } );

    if ( !ntk.is_constant( n ) )
    {
      supported.emplace_back( n );
    }
  }

  void collect_wings( node const& n, std::vector<node>& supported )
  {
    if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_divisors )
    {
      return;
    }

    /* if the fanout has all fanins in the set, add it */
    ntk.foreach_fanout( n, [&]( node const& p ) {
      if ( ntk.visited( p ) == ntk.trav_id() || ntk.level( p ) > win.max_level )
      {
        return true; /* next fanout */
      }

      bool all_fanins_visited = true;
      ntk.foreach_fanin( p, [&]( const auto& g ) {
        if ( ntk.visited( ntk.get_node( g ) ) != ntk.trav_id() )
        {
          all_fanins_visited = false;
          return false; /* terminate fanin-loop */
        }
        return true; /* next fanin */
      } );

      if ( !all_fanins_visited )
      {
        return true; /* next fanout */
      }

      bool has_root_as_child = false;
      ntk.foreach_fanin( p, [&]( const auto& g ) {
        if ( ntk.get_node( g ) == win.root )
        {
          has_root_as_child = true;
          return false; /* terminate fanin-loop */
        }
        return true; /* next fanin */
      } );

      if ( has_root_as_child )
      {
        return true; /* next fanout */
      }

      supported.emplace_back( p );
      ntk.set_visited( p, ntk.trav_id() );

      /* quit collecting if there are too many of them */
      if ( supported.size() > ps.max_divisors )
      {
        return false; /* terminate fanout-loop */
      }

      return true; /* next fanout */
    } );
  }
  
  bool collect_supported_nodes( std::vector<node> const& leaves, std::vector<node>& supported )
  {
    /* collect TFI nodes until (excluding) leaves */
    ntk.incr_trav_id();
    for ( const auto& l : leaves )
    {
      ntk.set_visited( l, ntk.trav_id() );
    }
    collect_tfi_rec( win.root, supported );
    supported.pop_back(); /* remove `root` */

    if ( supported.size() > ps.max_divisors )
    {
      return false;
    }

    /* collect "wings" */
    for ( auto const& l : leaves )
    {
      collect_wings( l, supported );
      if ( supported.size() > ps.max_divisors )
      {
        break;
      }
    }

    /* note: we cannot use range-based loop here because we push to the vector in the loop */
    for ( auto i = 0u; i < supported.size(); ++i )
    {
      collect_wings( supported.at( i ), supported );
      if ( supported.size() > ps.max_divisors )
      {
        break;
      }
    }

    supported.emplace_back( win.root );
    return true;
  }

  void collect_divisors( std::vector<node> const& leaves, std::vector<node> const& supported )
  {
    win.divs.clear();
    win.div_ids.clear();

    uint32_t i{1};
    if ( ntk.get_node( ntk.get_constant( false ) ) != ntk.get_node( ntk.get_constant( true ) ) )
    {
      ++i;
    }

    for ( auto const& l : leaves )
    {
      win.div_ids.emplace_back( i++ );
      win.divs.emplace_back( l );
    }

    for ( auto const& n : supported )
    {
      if ( ntk.value( n ) != mffc_marker ) /* not in MFFC, not root */
      {
        win.div_ids.emplace_back( i );
        win.divs.emplace_back( n );
      }
      ++i;
    }
    assert( i == win.tts.size() );
  }

private:
  Ntk& ntk;
  problem_t win;
  params_t const& ps;
  stats_t& st;
  reconvergence_driven_cut_parameters const cps;
  typename mockturtle::detail::node_mffc_inside<Ntk> mffc_mgr; // TODO: namespaces can be removed when we move out of experimental::
  uint32_t mffc_marker{0u};
  std::shared_ptr<typename network_events<Ntk>::modified_event_type> lazy_update_event;
};

template<class Ntk, class TT, class ResynEngine, bool preserve_depth = false>
class complete_tt_resynthesis
{
public:
  using problem_t = small_window<Ntk, TT>;
  using res_t = typename ResynEngine::index_list_t;
  using params_t = null_params;
  using stats_t = null_stats;

  explicit complete_tt_resynthesis( Ntk const& ntk, params_t const& ps, stats_t& st )
    : ntk( ntk )
  { }

  void init()
  { }

  std::optional<res_t> operator()( problem_t& prob )
  {
    ResynEngine engine( rst, rps );
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
  typename ResynEngine::params rps;
  typename ResynEngine::stats rst;
};

} /* namespace detail */

using window_resub_params = boolean_optimization_params<complete_tt_windowing_params, null_params>;
using window_resub_stats = boolean_optimization_stats<complete_tt_windowing_stats, null_stats>;

template<class Ntk>
void window_xag_heuristic_resub( Ntk& ntk, window_resub_params const& ps = {}, window_resub_stats* pst = nullptr )
{
  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  fanout_view<Ntk> fntk( ntk );
  ViewedNtk viewed( fntk );

  constexpr bool use_xor = std::is_same_v<typename Ntk::base_type, xag_network>;
  using TT = typename kitty::dynamic_truth_table;
  using windowing_t = typename detail::complete_tt_windowing<ViewedNtk, TT>;
  using engine_t = xag_resyn_decompose<TT, std::vector<TT>, use_xor, /*copy_tts*/false, /*node_type*/uint32_t>;
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
  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  fanout_view<Ntk> fntk( ntk );
  ViewedNtk viewed( fntk );

  using TT = typename kitty::dynamic_truth_table;
  using windowing_t = typename detail::complete_tt_windowing<ViewedNtk, TT>;
  using engine_t = aig_enumerative_resyn<TT>;
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
void window_mig_heuristic_resub( Ntk& ntk, window_resub_params const& ps = {}, window_resub_stats* pst = nullptr )
{
  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  fanout_view<Ntk> fntk( ntk );
  ViewedNtk viewed( fntk );

  using TT = typename kitty::dynamic_truth_table;
  using windowing_t = typename detail::complete_tt_windowing<ViewedNtk, TT>;
  using engine_t = mig_resyn_bottomup<TT>;
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