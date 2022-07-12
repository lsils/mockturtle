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
  \file costfn_window.hpp
  \brief generic widnowing algorithm with customized cost function

  \author Siang-Yun (Sonia) Lee
  \author Hanyu Wang
*/

#pragma once

#include "../../networks/aig.hpp"
#include "../../networks/xag.hpp"
#include "../../traits.hpp"
#include "../../utils/index_list.hpp"
#include "../../utils/stopwatch.hpp"
#include "../../views/cost_view.hpp"
#include "../../views/depth_view.hpp"
#include "../../views/fanout_view.hpp"
#include "../../views/topo_view.hpp"
#include "../detail/resub_utils.hpp"
#include "../dont_cares.hpp"
#include "../reconv_cut.hpp"
#include "../simulation.hpp"
#include "boolean_optimization.hpp"
#include "search_core.hpp"
#include <kitty/kitty.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace mockturtle::experimental
{

/*! \brief Parameters for cost.
 */
struct cost_generic_windowing_params
{
  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{ 8 };

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{ 150 };

  /*! \brief Maximum number of nodes added by resubstitution. */
  uint32_t max_inserts{ 2 };

  /*! \brief Maximum fanout of a node to be considered as root. */
  uint32_t skip_fanout_limit_for_roots{ 1000 };

  /*! \brief Maximum fanout of a node to be considered as divisor. */
  uint32_t skip_fanout_limit_for_divisors{ 100 };

  /*! \brief Use don't cares for optimization. */
  bool use_dont_cares{ false };

  /*! \brief Window size for don't cares calculation. */
  uint32_t window_size{ 12u };

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
  bool normalize{ false };
};

struct cost_generic_windowing_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{ 0 };

  /*! \brief Accumulated runtime for cut computation. */
  stopwatch<>::duration time_cuts{ 0 };

  /*! \brief Accumulated runtime for mffc computation. */
  stopwatch<>::duration time_mffc{ 0 };

  /*! \brief Accumulated runtime for divisor collection. */
  stopwatch<>::duration time_divs{ 0 };

  /*! \brief Accumulated runtime for simulation. */
  stopwatch<>::duration time_sim{ 0 };

  /*! \brief Accumulated runtime for don't care computation. */
  stopwatch<>::duration time_dont_care{ 0 };

  void report() const
  {
    // clang-format off
    fmt::print( "[i] cost_generic_windowing report\n" );
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


struct cost_generic_resynthesis_params
{};
struct cost_generic_resynthesis_stats
{
  void report() const {}
};


namespace detail
{
/**
 * @brief The problem we agree on for cost function aware algorithm 
 */
template<class Ntk, class TT>
struct cost_generic_problem
{
  using signal = typename Ntk::signal;
  using node = typename Ntk::node;
  signal po;
  TT care;
  std::vector<signal> pis;
  std::vector<signal> divs;
  Ntk window;
  signal target;
  uint32_t mffc;
};

template<class Ntk, class TT = kitty::dynamic_truth_table>
class cost_generic_windowing
{
public:
  using problem_t = cost_generic_problem<Ntk, TT>;
  using params_t = cost_generic_windowing_params;
  using stats_t = cost_generic_windowing_stats;

  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit cost_generic_windowing( Ntk& ntk, params_t const& ps, stats_t& st )
      : ntk( ntk ), ps( ps ), st( st ), cps( { ps.max_pis } ), mffc_mgr( ntk ),
        divs_mgr( ntk, divisor_collector_params( { ps.max_divisors, ps.max_divisors, ps.skip_fanout_limit_for_divisors } ) )
  {
    static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
    static_assert( has_value_v<Ntk>, "Ntk does not implement the value method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
    static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the substitute_node method" );
  }

  void init()
  {
  }

  /**
   * @brief Create the cost-aware problem
   * 
   * @param n 
   * @return std::optional<std::reference_wrapper<problem_t>> 
   */
  std::optional<std::reference_wrapper<problem_t>> operator()( node const& n )
  {
    stopwatch t( st.time_total );
    if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
    {
      return std::nullopt; /* skip nodes with too many fanouts */
    }

    /* compute a cut and collect supported nodes */
    std::vector<node> leaves = call_with_stopwatch( st.time_cuts, [&]() {
      return reconvergence_driven_cut<Ntk, false, has_level_v<Ntk>>( ntk, { n }, cps ).first;
    } );
    std::vector<node> supported;
    call_with_stopwatch( st.time_divs, [&]() {
      divs_mgr.collect_supported_nodes( n, leaves, supported ); /* root will be the last */
    } );

    /* mark MFFC nodes and collect divisors */
    ++mffc_marker;
    prob.mffc = call_with_stopwatch( st.time_mffc, [&]() {
      return mffc_mgr.call_on_mffc_and_count( n, leaves, [&]( node const& n ) {
        ntk.set_value( n, mffc_marker );
      } );
    } );
    call_with_stopwatch( st.time_divs, [&]() {
      collect_divisors( leaves, supported );
    } );

    prob.po = ntk.make_signal( n );

    /* compute don't cares */
    call_with_stopwatch( st.time_dont_care, [&]() {
      if ( ps.use_dont_cares )
      {
        prob.care = ~satisfiability_dont_cares( ntk, leaves, ps.window_size );
      }
      else
      {
        prob.care = ~kitty::create<TT>( ps.max_pis );
      }
    } );
    return prob;
  }

  template<typename res_t>
  uint32_t gain( problem_t const& problem, res_t const& res ) const
  {
    static_assert( is_index_list_v<res_t>, "res_t is not an index_list (windowing engine and resynthesis engine do not match)" );
    return 1;
  }

  template<typename res_t>
  bool update_ntk( problem_t const& problem, res_t const& res )
  {
    static_assert( is_index_list_v<res_t>, "res_t is not an index_list (windowing engine and resynthesis engine do not match)" );
    assert( res.num_pos() == 1 );
    insert( ntk, std::begin( problem.pis ), std::end( problem.pis ), res, [&]( signal const& g ) {
      ntk.substitute_node( ntk.get_node( prob.po ), ntk.is_complemented( prob.po ) ? !g : g );
    } );
    return true; /* continue optimization */
  }

  template<typename res_t>
  bool report( problem_t const& problem, res_t const& res )
  {
    static_assert( is_index_list_v<res_t>, "res_t is not an index_list (windowing engine and resynthesis engine do not match)" );
    assert( res.num_pos() == 1 );
    fmt::print( "[i] found solution {} for root signal {}{}\n", to_index_list_string( res ), ntk.is_complemented( problem.po ) ? "!" : "", ntk.get_node( problem.po ) );
    return true;
  }

private:
  void collect_divisors( std::vector<node> const& leaves, std::vector<node> const& supported )
  {
    Ntk window;
    prob.pis.clear();
    prob.divs.clear();
    node_map<signal, Ntk> old_to_new( ntk );
    old_to_new[ntk.get_constant( false )] = window.get_constant( false );
    if ( ntk.get_node( ntk.get_constant( true ) ) != ntk.get_node( ntk.get_constant( false ) ) )
    {
      old_to_new[ntk.get_constant( true )] = window.get_constant( true );
    }
    window.incr_trav_id();
    for ( auto const& l : leaves )
    {
      prob.pis.emplace_back( ntk.make_signal( l ) );
      signal s = window.create_pi();
      old_to_new[ntk.make_signal( l )] = s;
      window.set_context( window.get_node( s ), ntk.get_context( l ) );
    }
    for ( auto const& n : supported ) /* supported nodes are in topo order */
    {
      /* collect children */
      std::vector<signal> children;
      ntk.foreach_fanin( n, [&]( auto child, auto ) {
        const auto f = old_to_new[child];
        if ( ntk.is_complemented( child ) )
        {
          children.push_back( window.create_not( f ) );
        }
        else
        {
          children.push_back( f );
        }
      } );
      signal s = window.clone_node( ntk, n, children ); /* this will update cost automatically */
      old_to_new[n] = s;
      if ( ntk.value( n ) != mffc_marker ) /* not in MFFC, not root */
      {
        prob.divs.emplace_back( s );
      }
    }

    prob.target = old_to_new[supported.back()];
    prob.window = window;
  }

private:
  Ntk& ntk;
  problem_t prob;
  params_t const& ps;
  stats_t& st;
  reconvergence_driven_cut_parameters const cps;
  typename mockturtle::detail::node_mffc_inside<Ntk> mffc_mgr; // TODO: namespaces can be removed when we move out of experimental::
  divisor_collector<Ntk> divs_mgr;
  uint32_t mffc_marker{ 0u };
  std::shared_ptr<typename network_events<Ntk>::modified_event_type> lazy_update_event;
}; /* cost_generic_windowing */

template<class Ntk, class TT>
class cost_generic_resynthesis
{
public:
  using problem_t = cost_generic_problem<Ntk, TT>;
  using res_t = large_xag_index_list;
  using index_list_t = large_xag_index_list;
  using params_t = cost_generic_resynthesis_params;
  using stats_t = cost_generic_resynthesis_stats;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using cost_t = typename Ntk::costfn_t::cost_t;
  using p = std::pair<uint32_t, node>;

  explicit cost_generic_resynthesis( Ntk const& ntk, params_t const& ps, stats_t& st )
      : ntk( ntk )
  {
    static_assert( has_cost_v<Ntk>, "Ntk does not implement the get_cost method" );
  }

  void init()
  {
  }

  /**
   * @brief Solve the cost aware resynthesis problem with the 
   * window and divisors with context
   * 
   * The input of this solver is the initial network
   * 
   * @param prob 
   * @return std::optional<res_t> 
   */
  std::optional<res_t> operator()( problem_t& prob )
  {
    /* we will modify the network in the problem but not the original network */
    default_simulator<kitty::dynamic_truth_table> sim( prob.window.num_pis() );
    unordered_node_map<TT, Ntk> tts( prob.window );
    simulate_nodes<TT>( prob.window, tts, sim );
    std::priority_queue<p, std::vector<p>, std::greater<p>> q;
    /* find target tt */
    TT target = tts[prob.target]; /* assume target is not complement */
    uint32_t max_cost = prob.window.get_cost( prob.window.get_node( prob.target ), prob.divs );
    if (max_cost != prob.mffc) fmt::print("{} != {}\n",max_cost, prob.mffc);
    /* insert all the nodes */
    prob.window.foreach_node( [&]( node n ){
      q.push( std::pair( prob.window.get_cost( n, prob.divs ), n ) );
    } );
    while( !q.empty() )
    {
      uint32_t cost = q.top().first;
      if ( cost >= max_cost )
      {
        break;
      }
      node n = q.top().second;
      if ( tts[n] == target )
      {
        return get_result( prob.window, prob.window.make_signal( n ) );
      }
      if ( ~tts[n] == target )
      {
        return get_result( prob.window, !prob.window.make_signal( n ) );
      }
      /* pop the node and make better guess */
      q.pop();

      TT const& tt = tts[n];
    }
    return std::nullopt;
  }

private:
  index_list_t get_result( auto& window, signal po ) 
  {
    window.create_po( po );
    auto _ntk = cleanup_dangling<typename Ntk::base_type>( window ); /* only logic */
    index_list_t res;
    encode( res, _ntk );
    return res;
  }
private:
  Ntk const& ntk;
  
}; /* cost_generic_resynthesis */

} /* namespace detail */

using cost_generic_params = boolean_optimization_params<cost_generic_windowing_params, cost_generic_resynthesis_params>;
using cost_generic_stats = boolean_optimization_stats<cost_generic_windowing_stats, cost_generic_resynthesis_stats>;

/*! \brief Generic resubstitution algorithm.
 *
 * This algorithm creates a reconvergence-driven window for each node in the 
 * network, collects divisors, and builds the resynthesis problem. A search core
 * then collects all the resubstitution candidates with the same functionality as 
 * the target. The candidate with the lowest cost will then replace the MFFC
 * of the window. 
 * 
 * Basic cost functions include:
 * - `and_cost`: number of AND nodes
 * - `gate_cost`: number of AND / XOR nodes
 * - `supp_cost`: sum of supports of each node
 * - `level_cost`: depth
 * - `adp_cost`: sum of level of each node
 *
 * \param ntk Network
 * \param cost_fn Customized cost function
 * \param ps Optimization params
 * \param pst Optimization statistics
 */
template<class Ntk, class CostFn>
void cost_generic_optimization( Ntk& ntk, CostFn cost_fn, cost_generic_params const& ps, cost_generic_stats* pst = nullptr )
{
  fanout_view fntk( ntk );
  cost_view viewed( fntk, cost_fn );
  using Viewed = decltype( viewed );
  using TT = typename kitty::dynamic_truth_table;
  using windowing_t = typename detail::cost_generic_windowing<Viewed, TT>;
  using engine_t = search_core<Viewed, TT>;
  using resyn_t = typename detail::cost_generic_resynthesis<Viewed, TT>;
  using opt_t = typename detail::boolean_optimization_impl<Viewed, windowing_t, resyn_t>;

  cost_generic_stats st;
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

} // namespace mockturtle::experimental