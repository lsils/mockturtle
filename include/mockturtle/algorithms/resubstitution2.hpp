/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2020  EPFL
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
  \file resubstitution2.hpp
  \brief Generalized resubstitution framework

  TODOs:
  - Integrate ordinary sim_resub, move related code to sim_resub.hpp
  - (ODC consideration for sim_resub)
  - Provide some examples of composed algorithms (partly done)
  - Documentation
  - Adjust the interface of window-based resub functors
  - Replace resubstitution.hpp, and adjust other algorithms depending
    on it with the new interface
  - Integrate node_resynthesis engines
  - Provide the stats object

  More TODOs:
  - Integrate frontier-based exploration
  - Provide possibility of different cost functions:
      If it's different way of counting circuit size, simply replace
      `node_mffc_inside`.
      If it's circuit depth, we will need another `divisor_collector`
      implementation, which does not exclude MFFC nodes, and computes
      the desired `potential_gain`.
      The `resub_engine`s also need to be adjusted.

  \author Heinz Riener
  \author Siang-Yun Lee
*/

#pragma once

#include <vector>
#include <algorithm>

#include "../traits.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/depth_view.hpp"
#include "../views/fanout_view.hpp"
#include "detail/resub_utils.hpp"
#include "dont_cares.hpp"
#include "reconv_cut2.hpp"

#include "../utils/abc_resub.hpp"
#include <mockturtle/algorithms/circuit_validator.hpp>
#include <mockturtle/algorithms/simulation.hpp>

//#include <mockturtle/networks/aig.hpp>
//#include <mockturtle/algorithms/aig_resub.hpp>

namespace mockturtle
{

/*! \brief Parameters for resubstitution.
 *
 * The data structure `resubstitution_params` holds configurable parameters with
 * default arguments for `resubstitution`.
 */
struct resubstitution_params
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

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};

  /*! \brief Use don't cares for optimization. */
  bool use_dont_cares{false};

  /* \brief Window size for don't cares calculation */
  uint32_t window_size{12u};
};

/*! \brief Statistics for resubstitution.
 *
 * The data structure `resubstitution_stats` provides data collected by running
 * `resubstitution`.
 */
struct resubstitution_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{0};

  /*! \brief Accumulated runtime for divisor computation. */
  stopwatch<>::duration time_divs{0};

  /*! \brief Accumulated runtime for cut evaluation/computing a resubsitution. */
  stopwatch<>::duration time_eval{0};

  /*! \brief Accumulated runtime for updating the network. */
  stopwatch<>::duration time_callback{0};

  /*! \brief Total number of divisors  */
  uint64_t num_total_divisors{0};

  /*! \brief Total number of gain  */
  uint64_t estimated_gain{0};

  /*! \brief Initial network size (before resubstitution) */
  uint64_t initial_size{0};

  void report() const
  {
    std::cout << fmt::format( "[i] total time                                                  ({:>5.2f} secs)\n", to_seconds( time_total ) );
    std::cout << fmt::format( "[i]   divs time                                                 ({:>5.2f} secs)\n", to_seconds( time_divs ) );
    std::cout << fmt::format( "[i]   evaluation time                                           ({:>5.2f} secs)\n", to_seconds( time_eval ) );
    std::cout << fmt::format( "[i]   callback time                                             ({:>5.2f} secs)\n", to_seconds( time_callback ) );
    std::cout << fmt::format( "[i] total divisors            = {:8d}\n", ( num_total_divisors ) );
    std::cout << fmt::format( "[i] estimated gain            = {:8d} ({:>5.2f}%)\n",
                              estimated_gain, ( ( 100.0 * estimated_gain ) / initial_size ) );
  }
};

namespace detail
{

template<typename Ntk>
bool substitute_fn( Ntk& ntk, typename Ntk::node const& n, typename Ntk::signal const& g )
{
  ntk.substitute_node( n, g );
  return true;
};

template<typename Ntk>
bool report_fn( Ntk& ntk, typename Ntk::node const& n, typename Ntk::signal const& g )
{
  (void)ntk;
  std::cout << "substitute node " << unsigned( n ) << " with node " << unsigned( ntk.get_node( g ) ) << std::endl;
  return false;
};

struct divisor_collector_params
{
  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{8};

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{150};

  /*! \brief Maximum fanout of a node to be considered as root. */
  uint32_t skip_fanout_limit_for_roots{1000};

  /*! \brief Maximum fanout of a node to be considered as divisor. */
  uint32_t skip_fanout_limit_for_divisors{100};
};

template<class Ntk, class CutMgr = cut_manager<Ntk>, class MffcMgr = node_mffc_inside<Ntk>>
class default_divisor_collector
{
public:
  using node = typename Ntk::node;

public:
  explicit default_divisor_collector( Ntk const& ntk, divisor_collector_params const& ps = {} )
      : ntk( ntk ), ps( ps ), cut_mgr( ps.max_pis )
  {
  }

  /* Prepare the three public data members `leaves`, `divs` and `MFFC`
   * to be ready for usage.
   *
   * `leaves`: sufficient support for all divisors
   * `divs`: divisor nodes that can be used for resubstitution
   * `MFFC`: MFFC nodes which are needed to do simulation from
   * `leaves`, through `divs` and `MFFC` until the root node,
   * but should be excluded from resubstitution.
   * The last element of `MFFC` is always the root node.
   *
   * `divs` and `MFFC` are in topological order. 
   */
  bool run( node const& n, uint32_t& potential_gain )
  {
    /* skip nodes with many fanouts */
    if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
    {
      return false;
    }

    /* compute a reconvergence-driven cut */
    leaves = reconv_driven_cut( cut_mgr, ntk, n );

    /* collect the MFFC */
    MffcMgr mffc_mgr( ntk );
    auto num_mffc = mffc_mgr.run( n, leaves, MFFC );
    assert( num_mffc == int( MFFC.size() ) ); // will not be true for MC cost function
    potential_gain = num_mffc;

    /* collect the divisor nodes in the cut */
    bool div_comp_success = collect_divisors( n );

    if ( !div_comp_success )
    {
      return false;
    }

    return true;
  }

private:
  void collect_divisors_rec( node const& n )
  {
    /* skip visited nodes */
    if ( ntk.visited( n ) == ntk.trav_id() )
    {
      return;
    }
    ntk.set_visited( n, ntk.trav_id() );

    ntk.foreach_fanin( n, [&]( const auto& f ) {
      collect_divisors_rec( ntk.get_node( f ) );
    } );

    /* collect the internal nodes */
    if ( ntk.value( n ) == 0 && n != 0 ) /* ntk.fanout_size( n ) */
    {
      divs.emplace_back( n );
    }
  }

  bool collect_divisors( node const& root )
  {
    /* add the leaves of the cuts to the divisors */
    divs.clear();

    ntk.incr_trav_id();
    for ( const auto& l : leaves )
    {
      divs.emplace_back( l );
      ntk.set_visited( l, ntk.trav_id() );
    }

    /* mark nodes in the MFFC */
    for ( const auto& t : MFFC )
    {
      ntk.set_value( t, 1 );
    }

    /* collect the cone (without MFFC) */
    collect_divisors_rec( root );

    /* unmark the current MFFC */
    for ( const auto& t : MFFC )
    {
      ntk.set_value( t, 0 );
    }

    /* check if the number of divisors is not exceeded */
    if ( divs.size() - leaves.size() + MFFC.size() >= ps.max_divisors - ps.max_pis )
    {
      return false;
    }

    /* get the number of divisors to collect */
    int32_t limit = ps.max_divisors - ps.max_pis - ( uint32_t( divs.size() ) + 1 - uint32_t( leaves.size() ) + uint32_t( MFFC.size() ) );

    /* explore the fanouts, which are not in the MFFC */
    int32_t counter = 0;
    bool quit = false;

    /* NOTE: this is tricky and cannot be converted to a range-based loop */
    auto size = divs.size();
    for ( auto i = 0u; i < size; ++i )
    {
      auto const d = divs.at( i );

      if ( ntk.fanout_size( d ) > ps.skip_fanout_limit_for_divisors )
      {
        continue;
      }

      /* if the fanout has all fanins in the set, add it */
      ntk.foreach_fanout( d, [&]( node const& p ) {
        if ( ntk.visited( p ) == ntk.trav_id() )
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
          return true; /* next fanout */

        bool has_root_as_child = false;
        ntk.foreach_fanin( p, [&]( const auto& g ) {
          if ( ntk.get_node( g ) == root )
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

        divs.emplace_back( p );
        ++size;
        ntk.set_visited( p, ntk.trav_id() );

        /* quit computing divisors if there are too many of them */
        if ( ++counter == limit )
        {
          quit = true;
          return false; /* terminate fanout-loop */
        }

        return true; /* next fanout */
      } );

      if ( quit )
      {
        break;
      }
    }

    /* Note: different from the previous version, now we do not add MFFC nodes into divs */
    assert( root == MFFC.at( MFFC.size() - 1u ) );
    assert( divs.size() + MFFC.size() - leaves.size() <= ps.max_divisors - ps.max_pis );

    return true;
  }

private:
  Ntk const& ntk;
  divisor_collector_params const& ps;

  CutMgr cut_mgr;

public:
  std::vector<node> leaves;
  std::vector<node> divs;
  std::vector<node> MFFC;
};

struct resub_engine_params
{
  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{8};

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{150};

  /*! \brief Maximum number of nodes added by resubstitution. */
  uint32_t max_inserts{2};

  /****** window-based specific ******/

  /*! \brief Use don't cares for optimization. */
  bool use_dont_cares{false};

  /* \brief Window size for don't cares calculation */
  uint32_t window_size{12u};

  /****** simulation-based specific ******/

  /*! \brief Maximum number of divisors to consider in k-resub engine. */
  uint32_t max_divisors_k{50};

  /*! \brief Maximum number of trials to call the k-resub engine. */
  uint32_t num_trials_k{100};

  /*! \brief Conflict limit for the SAT solver. */
  uint32_t conflict_limit{1000};

  /*! \brief Random seed for the SAT solver (influences the randomness of counter-examples). */
  uint32_t random_seed{0};
};

template<class Ntk, class TT, class ResubFn = default_resub_functor<Ntk, window_simulator<Ntk, TT>, TT>>
class window_based_resub_engine
{
public:
  static constexpr bool require_leaves_and_MFFC = true;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit window_based_resub_engine( Ntk& ntk, resub_engine_params const& ps = {} )
      : ntk( ntk ), ps( ps ), sim( ntk, ps.max_divisors, ps.max_pis )
  {
  }

  std::optional<signal> run( node const& n, std::vector<node> const& leaves, std::vector<node> const& divs, std::vector<node> const& MFFC, uint32_t potential_gain, uint32_t& last_gain )
  {
    /* simulate the collected divisors */
    simulate( leaves, divs, MFFC );

    auto care = kitty::create<TT>( static_cast<unsigned int>( leaves.size() ) );
    if ( ps.use_dont_cares )
    {
      care = ~satisfiability_dont_cares( ntk, leaves, ps.window_size );
    }
    else
    {
      care = ~care;
    }

    typename ResubFn::stats resub_st; /* TODO: should be passed from higher level classes */
    ResubFn resub_fn( ntk, sim, divs, divs.size(), resub_st );
    return resub_fn( n, care, std::numeric_limits<uint32_t>::max(), ps.max_inserts, potential_gain, last_gain );
  }

private:
  void simulate( std::vector<node> const& leaves, std::vector<node> const& divs, std::vector<node> const& MFFC )
  {
    sim.resize();
    for ( auto i = 0u; i < divs.size() + MFFC.size(); ++i )
    {
      const auto d = i < divs.size() ? divs.at( i ) : MFFC.at( i - divs.size() );

      /* skip constant 0 */
      if ( d == 0 )
        continue;

      /* assign leaves to variables */
      if ( i < leaves.size() )
      {
        sim.assign( d, i + 1 );
        continue;
      }

      /* compute truth tables of inner nodes */
      sim.assign( d, i - uint32_t( leaves.size() ) + ps.max_pis + 1 );
      std::vector<TT> tts;
      ntk.foreach_fanin( d, [&]( const auto& s ) {
        tts.emplace_back( sim.get_tt( ntk.make_signal( ntk.get_node( s ) ) ) ); /* ignore sign */
      } );

      auto const tt = ntk.compute( d, tts.begin(), tts.end() );
      sim.set_tt( i - uint32_t( leaves.size() ) + ps.max_pis + 1, tt );
    }

    /* normalize truth tables */
    sim.normalize( divs );
    sim.normalize( MFFC );
  }

private:
  Ntk& ntk;
  resub_engine_params const& ps;

  window_simulator<Ntk, TT> sim;
}; /* window_based_resub_engine */

struct simresub_stats
{
  /*! \brief Total time. */
  stopwatch<>::duration time_total{0};

  /*! \brief Time for simulation. */
  stopwatch<>::duration time_sim{0};

  /*! \brief Time for SAT solving. */
  stopwatch<>::duration time_sat{0};

  /*! \brief Time for computing reconvergence-driven cut. */
  stopwatch<>::duration time_cut{0};

  /*! \brief Time for MFFC computation. */
  stopwatch<>::duration time_mffc{0};

  /*! \brief Time for divisor collection. */
  stopwatch<>::duration time_divs{0};

  /*! \brief Time for executing the user-specified callback when candidates found. */
  stopwatch<>::duration time_callback{0};

  /*! \brief Time for finding dependency function. */
  stopwatch<>::duration time_compute_function{0};

  /*! \brief Number of counter-examples. */
  uint32_t num_cex{0};

  /*! \brief Number of successful resubstitutions. */
  uint32_t num_resub{0};

  /*! \brief Total number of gain. */
  uint64_t estimated_gain{0};

  /*! \brief Total number of collected divisors. */
  uint64_t num_total_divisors{0};
};

template<class Ntk>
class simulation_based_resub_engine
{
public:
  static constexpr bool require_leaves_and_MFFC = false;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = unordered_node_map<kitty::partial_truth_table, Ntk>;
  using validator_t = circuit_validator<Ntk, bill::solvers::bsat2, false, true, false>;
  using vgate = typename validator_t::gate;
  using fanin = typename vgate::fanin;
  using gtype = typename validator_t::gate_type;

  explicit simulation_based_resub_engine( Ntk& ntk, resub_engine_params const& ps = {} )
      : ntk( ntk ), ps( ps ), tts( ntk ), validator( ntk, vps )
  {
    sim = partial_simulator( ntk.num_pis(), 256 ); // TODO: integrate pattern_generation
    vps.conflict_limit = ps.conflict_limit;
    vps.random_seed = ps.random_seed;

    ntk._events->on_add.emplace_back( [&]( const auto& n ) {
      simulate_node<Ntk>( ntk, n, tts, sim );
    } );

    /* first simulation: the whole circuit; from 0 bits. */
    simulate_nodes<Ntk>( ntk, tts, sim );

    abcresub::Abc_ResubPrepareManager( sim.compute_constant( false ).num_blocks() );
  }

  ~simulation_based_resub_engine()
  {
    abcresub::Abc_ResubPrepareManager( 0 );
  }

  std::optional<signal> run( node const& n, std::vector<node> const& divs, uint32_t potential_gain, uint32_t& last_gain )
  {
    uint32_t size = 0;
    const auto g = resub_divk( n, divs, std::min( int( potential_gain ) - 1, int( ps.max_inserts ) ), size );
    if ( g )
    {
      last_gain = potential_gain - size;
    }
    return g;
  }

private:
  void found_cex()
  {
    ++st.num_cex;
    sim.add_pattern( validator.cex );

    /* re-simulate the whole circuit (for the last block) when a block is full */
    if ( sim.num_bits() % 64 == 0 )
    {
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_nodes<Ntk>( ntk, tts, sim, false );
      } );

      call_with_stopwatch( st.time_compute_function, [&]() {
        abcresub::Abc_ResubPrepareManager( sim.compute_constant( false ).num_blocks() );
      } );
    }
  }

  void check_tts( node const& n )
  {
    if ( tts[n].num_bits() != sim.num_bits() )
    {
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_node<Ntk>( ntk, n, tts, sim );
      } );
    }
  }

  /* TODO: should provide different functors here */
  std::optional<signal> resub_divk( node const& root, std::vector<node> const& divs, uint32_t num_inserts, uint32_t& size )
  {
    for ( auto j = 0u; j < ps.num_trials_k; ++j )
    {
      check_tts( root );
      for ( auto i = 0u; i < divs.size(); ++i )
      {
        check_tts( divs[i] );
      }

      auto const res = call_with_stopwatch( st.time_compute_function, [&]() {
        abcresub::Abc_ResubPrepareManager( sim.compute_constant( false ).num_blocks() );

        abc_resub rs( 2ul + divs.size(), tts[root].num_blocks(), ps.max_divisors_k );
        rs.add_root( root, tts );
        rs.add_divisors( std::begin( divs ), std::end( divs ), tts );

        if constexpr ( std::is_same<typename Ntk::base_type, xag_network>::value )
        {
          return rs.compute_function( num_inserts, true );
        }
        else
        {
          return rs.compute_function( num_inserts, false );
        }
      } );

      if ( res )
      {
        auto const& index_list = *res;
        if ( index_list.size() == 1u ) /* div0 or constant */
        {
          const auto valid = call_with_stopwatch( st.time_sat, [&]() {
            if ( index_list[0] < 2 )
            {
              return validator.validate( root, ntk.get_constant( bool( index_list[0] ) ) );
            }
            assert( index_list[0] >= 4 );
            return validator.validate( root, bool( index_list[0] % 2 ) ? !ntk.make_signal( divs[( index_list[0] >> 1u ) - 2u] ) : ntk.make_signal( divs[( index_list[0] >> 1u ) - 2u] ) );
          } );

          if ( !valid ) /* timeout */
          {
            break;
          }
          else
          {
            if ( *valid )
            {
              size = 0u;
              if ( index_list[0] < 2 )
              {
                return ntk.get_constant( bool( index_list[0] ) );
              }
              else
              {
                return bool( index_list[0] % 2 ) ? !ntk.make_signal( divs[( index_list[0] >> 1u ) - 2u] ) : ntk.make_signal( divs[( index_list[0] >> 1u ) - 2u] );
              }
            }
            else
            {
              found_cex();
              continue;
            }
          }
        }

        uint64_t const num_gates = ( index_list.size() - 1u ) / 2u;
        std::vector<vgate> gates;
        gates.reserve( num_gates );
        size = 0u;
        for ( auto i = 0u; i < num_gates; ++i )
        {
          fanin f0{uint32_t( ( index_list[2 * i] >> 1u ) - 2u ), bool( index_list[2 * i] % 2 )};
          fanin f1{uint32_t( ( index_list[2 * i + 1u] >> 1u ) - 2u ), bool( index_list[2 * i + 1u] % 2 )};
          gates.emplace_back( vgate{{f0, f1}, f0.index < f1.index ? gtype::AND : gtype::XOR} );

          if constexpr ( std::is_same<typename Ntk::base_type, xag_network>::value )
          {
            ++size;
          }
          else
          {
            size += ( gates[i].type == gtype::AND ) ? 1u : 3u;
          }
        }
        bool const out_neg = bool( index_list.back() % 2 );
        assert( size <= num_inserts );

        const auto valid = call_with_stopwatch( st.time_sat, [&]() {
          return validator.validate( root, std::begin( divs ), std::end( divs ), gates, out_neg );
        } );

        if ( !valid ) /* timeout */
        {
          break;
        }
        else
        {
          if ( *valid )
          {
            std::vector<signal> ckt;
            for ( auto i = 0u; i < divs.size(); ++i )
            {
              ckt.emplace_back( ntk.make_signal( divs[i] ) );
            }

            for ( auto g : gates )
            {
              auto const f0 = g.fanins[0].inverted ? !ckt[g.fanins[0].index] : ckt[g.fanins[0].index];
              auto const f1 = g.fanins[1].inverted ? !ckt[g.fanins[1].index] : ckt[g.fanins[1].index];
              if ( g.type == gtype::AND )
              {
                ckt.emplace_back( ntk.create_and( f0, f1 ) );
              }
              else if ( g.type == gtype::XOR )
              {
                ckt.emplace_back( ntk.create_xor( f0, f1 ) );
              }
            }

            return out_neg ? !ckt.back() : ckt.back();
          }
          else
          {
            found_cex();
          }
        }
      }
      else /* loop until no result can be found by the engine */
      {
        return std::nullopt;
      }
    }

    return std::nullopt;
  }

private:
  Ntk& ntk;
  resub_engine_params const& ps;
  simresub_stats st;
  validator_params vps;

  TT tts;
  partial_simulator sim;

  validator_t validator;
}; /* simulation_based_resub_engine */

/*! \brief The top-level resubstitution flow.
 *
 * **Template arguments:**
 * - ResubEngine: The engine that computes the resubtitution for a given root
 * node and divisors. One can choose from `window_based_resub_engine` which
 * does complete simulation within small windows, or `simulation_based_resub_engine`
 * which does partial simulation on the whole circuit.
 *
 * - DivCollector: Collects divisors near a given root node, and compute
 * the potential gain (MFFC size or its variants).
 * Currently only `default_divisor_collector` is implemented, but
 * a frontier-based approach may be integrated in the future.
 * When using `window_based_resub_engine`, the `DivCollector` should prepare
 * three public data members: `leaves`, `divs`, and `MFFC` (see documentation
 * of `default_divisor_collector` for details). When using `simulation_based_resub_engine`,
 * only `divs` is needed.
 */
template<class Ntk, class ResubEngine = window_based_resub_engine<Ntk, kitty::dynamic_truth_table>, class DivCollector = default_divisor_collector<Ntk>>
class resubstitution_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using resub_callback_t = std::function<bool( Ntk&, node const&, signal const& )>;

  explicit resubstitution_impl( Ntk& ntk, resubstitution_params const& ps, resubstitution_stats& st, resub_callback_t const& callback = substitute_fn<Ntk> )
      : ntk( ntk ), ps( ps ), st( st ), callback( callback )
  {
    st.initial_size = ntk.num_gates();

    auto const update_level_of_new_node = [&]( const auto& n ) {
      ntk.resize_levels();
      update_node_level( n );
    };

    auto const update_level_of_existing_node = [&]( node const& n, const auto& old_children ) {
      (void)old_children;
      ntk.resize_levels();
      update_node_level( n );
    };

    auto const update_level_of_deleted_node = [&]( const auto& n ) {
      ntk.set_level( n, -1 );
    };

    ntk._events->on_add.emplace_back( update_level_of_new_node );

    ntk._events->on_modified.emplace_back( update_level_of_existing_node );

    ntk._events->on_delete.emplace_back( update_level_of_deleted_node );
  }

  void run()
  {
    stopwatch t( st.time_total );

    /* start the managers */
    divisor_collector_params collector_ps( {ps.max_pis, ps.max_divisors, ps.skip_fanout_limit_for_roots, ps.skip_fanout_limit_for_divisors} );
    DivCollector collector( ntk, collector_ps );

    resub_engine_params engine_ps( {ps.max_pis, ps.max_divisors, ps.max_inserts, ps.use_dont_cares, ps.window_size} );
    ResubEngine resub_engine( ntk, engine_ps );

    progress_bar pbar{ntk.size(), "resub |{0}| node = {1:>4}   cand = {2:>4}   est. gain = {3:>5}", ps.progress};

    auto const size = ntk.num_gates();
    ntk.foreach_gate( [&]( auto const& n, auto i ) {
      if ( i >= size )
      {
        return false; /* terminate */
      }

      pbar( i, i, candidates, st.estimated_gain );

      if ( ntk.is_dead( n ) )
      {
        return true; /* next */
      }

      /* compute cut, collect divisors, compute MFFC */
      uint32_t potential_gain;
      const auto collector_success = call_with_stopwatch( st.time_divs, [&]() {
        return collector.run( n, potential_gain );
      });
      if ( !collector_success )
      {
        return true; /* next */
      }

      /* update statistics */
      last_gain = 0;
      st.num_total_divisors += collector.divs.size();

      /* try to find a resubstitution with the divisors */
      auto g = call_with_stopwatch( st.time_eval, [&]() {
        if constexpr ( resub_engine.require_leaves_and_MFFC ) /* window-based */
        {
          return resub_engine.run( n, collector.leaves, collector.divs, collector.MFFC, potential_gain, last_gain );
        }
        else /* simulation-based */
        {
          return resub_engine.run( n, collector.divs, potential_gain, last_gain );
        }
      });
      if ( !g )
      {
        return true; /* next */
      }

      /* update progress bar */
      candidates++;
      st.estimated_gain += last_gain;

      /* update network */
      call_with_stopwatch( st.time_callback, [&]() {
        return callback( ntk, n, *g );
      } );

      return true; /* next */
    } );
  }

private:
  /* maybe should move to depth_view */
  void update_node_level( node const& n, bool top_most = true )
  {
    uint32_t curr_level = ntk.level( n );

    uint32_t max_level = 0;
    ntk.foreach_fanin( n, [&]( const auto& f ) {
      auto const p = ntk.get_node( f );
      auto const fanin_level = ntk.level( p );
      if ( fanin_level > max_level )
      {
        max_level = fanin_level;
      }
    } );
    ++max_level;

    if ( curr_level != max_level )
    {
      ntk.set_level( n, max_level );

      /* update only one more level */
      if ( top_most )
      {
        ntk.foreach_fanout( n, [&]( const auto& p ) {
          update_node_level( p, false );
        } );
      }
    }
  }

private:
  Ntk& ntk;

  resubstitution_params const& ps;
  resubstitution_stats& st;

  resub_callback_t const& callback;

  /* temporary statistics for progress bar */
  uint32_t candidates{0};
  uint32_t last_gain{0};
};

} /* namespace detail */

/*! \brief Window-based Boolean resubstitution with default resub functor (only div0). */
template<class Ntk>
void default_resub( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
  static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the substitute_node method" );
  static_assert( has_value_v<Ntk>, "Ntk does not implement the value method" );
  static_assert( has_visited_v<Ntk>, "Ntk does not implement the visited method" );

  using resub_view_t = fanout_view<depth_view<Ntk>>;
  depth_view<Ntk> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  resubstitution_stats st;

  detail::resubstitution_impl<resub_view_t> p( resub_view, ps, st );
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

#if 0 // To compile this function, you need to include aig_resub.hpp, but NOT include resubstitution.hpp in it.
/*! \brief Window-based Boolean resubstitution on AIG. */
void aig_resub( aig_network& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  using resub_view_t = fanout_view<depth_view<aig_network>>;
  depth_view<aig_network> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  resubstitution_stats st;

  using truthtable_t = kitty::dynamic_truth_table;
  using simulator_t = detail::window_simulator<resub_view_t, truthtable_t>;
  using resubstitution_functor_t = aig_resub_functor<resub_view_t, simulator_t, truthtable_t>;
  using resub_engine_t = detail::window_based_resub_engine<resub_view_t, truthtable_t, resubstitution_functor_t>;
  //typename resubstitution_functor_t::stats resub_st;
  detail::resubstitution_impl<resub_view_t, resub_engine_t> p( resub_view, ps, st );
  p.run();

  if ( ps.verbose )
  {
    st.report();
    //resub_st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
}
#endif

/*! \brief Simulation-based Boolean resubstitution. */
template<class Ntk>
void sim_resub( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  using resub_view_t = fanout_view<depth_view<Ntk>>;
  depth_view<Ntk> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  resubstitution_stats st;

  using resub_engine_t = detail::simulation_based_resub_engine<resub_view_t>;
  detail::resubstitution_impl<resub_view_t, resub_engine_t> p( resub_view, ps, st );
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
} /* namespace mockturtle */
