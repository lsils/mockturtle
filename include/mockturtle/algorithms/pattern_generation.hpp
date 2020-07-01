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
  \file pattern_generation.hpp
  \brief Expressive Simulation Pattern Generation

  \author Siang-Yun Lee
*/

#pragma once

#include <random>
#include <mockturtle/networks/aig.hpp>
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/algorithms/dont_cares.hpp>
#include <mockturtle/algorithms/circuit_validator.hpp>
#include <kitty/partial_truth_table.hpp>
#include <bill/sat/interface/z3.hpp>
#include <bill/sat/interface/abc_bsat2.hpp>

namespace mockturtle
{

struct patgen_params
{
  /*! \brief Whether to substitute constant nodes. */
  bool substitute_const{true};

  /*! \brief Number of patterns each node should have for both values. */
  uint32_t num_stuck_at{1};

  /*! \brief Fanout levels to consider for observability. -1 = no limit. */
  int odc_levels{-1};

  /*! \brief Whether to check and re-generate type 1 observable patterns. */
  bool observability_type1{false};

  /*! \brief Whether to check and re-generate type 2 observable patterns. */
  bool observability_type2{false};

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. Note that it will take more time to do extra ODC computation if this is turned on. */
  bool verbose{false};

  /*! \brief Random seed. */
  std::default_random_engine::result_type random_seed{0};

  /*! \brief Conflict limit of the SAT solver. */
  uint32_t conflict_limit{1000};
};

struct patgen_stats
{
  stopwatch<>::duration time_total{0};

  /*! \brief Time for simulations. */
  stopwatch<>::duration time_sim{0};

  /*! \brief Time for SAT solving. */
  stopwatch<>::duration time_sat{0};

  /*! \brief Time for ODC computation */
  stopwatch<>::duration time_odc{0};

  /*! \brief Number of constant nodes found. */
  uint32_t num_constant{0};

  /*! \brief Number of generated patterns. */
  uint32_t num_generated_patterns{0};

  /*! \brief Number of type1 unobservable nodes. */
  uint32_t unobservable_type1{0};

  /*! \brief Number of resolved type1 unobservable nodes. */
  uint32_t unobservable_type1_resolved{0};

  /*! \brief Number of type2 unobservable nodes. */
  uint32_t unobservable_type2{0};

  /*! \brief Number of resolved type2 unobservable nodes. */
  uint32_t unobservable_type2_resolved{0};
};

namespace detail
{

template<class Ntk, bool use_odc = false>
class patgen_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = unordered_node_map<kitty::partial_truth_table, Ntk>;

  explicit patgen_impl( Ntk& ntk, partial_simulator& sim, patgen_params const& ps, validator_params& vps, patgen_stats& st )
    : ntk( ntk ), ps( ps ), st( st ), vps( vps ), validator( ntk, vps ),
      tts( ntk ), sim( sim )
  { }

  void run()
  {
    stopwatch t( st.time_total );

    call_with_stopwatch( st.time_sim, [&]() {
      simulate_nodes<Ntk>( ntk, tts, sim );
    });

    if ( ps.num_stuck_at > 0 )
    {
      stuck_at_check();
      if ( ps.substitute_const )
      {
        for ( auto n : const_nodes )
        {
          if ( !ntk.is_dead( ntk.get_node( n ) ) )
          {
            ntk.substitute_node( ntk.get_node( n ), ntk.get_constant( ntk.is_complemented( n ) ) );
          }
        }
      }
    }

    if constexpr ( use_odc )
    {
      if ( ps.observability_type2 )
      {
        observability_check();
      }
    }
  }

private:
  void stuck_at_check()
  {
    progress_bar pbar{ntk.size(), "patgen-sa |{0}| node = {1:>4} #pat = {2:>4}", ps.progress};

    kitty::partial_truth_table zero = sim.compute_constant( false );

    ntk.foreach_gate( [&]( auto const& n, auto i ) 
    {
      pbar( i, i, sim.num_bits() );

      if ( tts[n].num_bits() != sim.num_bits() )
      {
        call_with_stopwatch( st.time_sim, [&]() {
          simulate_node<Ntk>( ntk, n, tts, sim );
        });
      }

      if ( (tts[n] == zero) || (tts[n] == ~zero) )
      {
        bool value = (tts[n] == zero); /* wanted value of n */
      
        const auto res = call_with_stopwatch( st.time_sat, [&]() {
          vps.odc_levels = 0;
          return validator.validate( n, !value );
        });
        if ( !res )
        {
          return true; /* timeout, next node */
        }
        else if ( !(*res) ) /* SAT, pattern found */
        {
          if constexpr ( use_odc )
          {
            if ( ps.observability_type1 )
            {
              /* check if the found pattern is observable */ 
              bool observable = call_with_stopwatch( st.time_odc, [&]() { 
                  return pattern_is_observable( ntk, n, validator.cex, ps.odc_levels );
                });
              if ( !observable )
              {
                if ( ps.verbose )
                {
                  std::cout << "\t[i] generated pattern is not observable (type 1). node: " << n << ", with value " << value << "\n" ;
                }
                ++st.unobservable_type1;
                const auto res2 = call_with_stopwatch( st.time_sat, [&]() {
                  vps.odc_levels = ps.odc_levels;
                  return validator.validate( n, !value );
                });
                if ( res2 )
                {
                  if ( !(*res2) )
                  {
                    ++st.unobservable_type1_resolved;
                    if ( ps.verbose )
                    {
                      assert( pattern_is_observable( ntk, n, validator.cex, ps.odc_levels ) );
                      std::cout << "\t[i] unobservable pattern resolved.\n";
                    }
                  }
                  else
                  {
                    if ( ps.verbose )
                    {
                      std::cout << "\t[i] unobservable node " << n << "\n";
                    }
                  }
                }
              }
            }
          }

          new_pattern( validator.cex );

          if ( ps.num_stuck_at > 1 )
          {
            auto generated = validator.generate_pattern( n, value, {validator.cex}, ps.num_stuck_at - 1 );
            for ( auto& pattern : generated )
            {
              new_pattern( pattern );
            }
          }

          zero = sim.compute_constant( false );
        }
        else /* UNSAT, constant node */
        {
          ++st.num_constant;
          const_nodes.emplace_back( value ? ntk.make_signal( n ) : !ntk.make_signal( n ) );
          return true; /* next gate */
        }
      }
      else if ( ps.num_stuck_at > 1 )
      {
        auto const& tt = tts[n];
        if ( kitty::count_ones( tt ) < ps.num_stuck_at )
        {
          generate_more_patterns( n, tt, true, zero );
        }
        else if ( kitty::count_zeros( tt ) < ps.num_stuck_at )
        {
          generate_more_patterns( n, tt, false, zero );
        }
      }
      return true; /* next gate */
    } );
  }

  void observability_check()
  {
    progress_bar pbar{ntk.size(), "patgen-obs2 |{0}| node = {1:>4} #pat = {2:>4}", ps.progress};

    ntk.foreach_gate( [&]( auto const& n, auto i ) 
    {
      pbar( i, i, sim.num_bits() );
      
      /* compute ODC */
      auto odc = call_with_stopwatch( st.time_odc, [&]() { 
          return observability_dont_cares<Ntk>( ntk, n, sim, tts, ps.odc_levels );
        });

      /* check if under non-ODCs n is always the same value */ 
      if ( ( tts[n] & ~odc ) == sim.compute_constant( false ) )
      {
        if ( ps.verbose )
        {
          std::cout << "\t[i] under all observable patterns, node " << n << " is always 0 (type 2).\n";
        }
        ++st.unobservable_type2;

        const auto res = call_with_stopwatch( st.time_sat, [&]() {
          vps.odc_levels = ps.odc_levels;
          return validator.validate( n, false );
        });
        if ( res )
        {
          if ( !(*res) )
          {
            new_pattern( validator.cex );
            ++st.unobservable_type2_resolved;

            if ( ps.verbose ) 
            {
              auto odc2 = call_with_stopwatch( st.time_odc, [&]() { return observability_dont_cares<Ntk>( ntk, n, sim, tts, ps.odc_levels ); });
              assert( ( tts[n] & ~odc2 ) != sim.compute_constant( false ) );
              std::cout << "\t[i] added generated pattern to resolve unobservability.\n";
            }
          }
          else
          {
            if ( ps.verbose )
            {
              std::cout << "\t[i] unobservable node " << n << "\n";
            }
          }
        }
      }
      else if ( ( tts[n] | odc ) == sim.compute_constant( true ) )
      {
        if ( ps.verbose )
        {
          std::cout << "\t[i] under all observable patterns, node " << n << " is always 1 (type 2).\n";
        }
        ++st.unobservable_type2;

        const auto res = call_with_stopwatch( st.time_sat, [&]() {
          vps.odc_levels = ps.odc_levels;
          return validator.validate( n, true );
        });
        if ( res )
        {
          if ( !(*res) )
          {
            new_pattern( validator.cex );
            ++st.unobservable_type2_resolved;

            if ( ps.verbose ) 
            {
              auto odc2 = call_with_stopwatch( st.time_odc, [&]() { return observability_dont_cares<Ntk>( ntk, n, sim, tts, ps.odc_levels ); });
              assert( ( tts[n] | odc2 ) != sim.compute_constant( true ) );
              std::cout << "\t[i] added generated pattern to resolve unobservability.\n";
            }
          }
          else
          {
            if ( ps.verbose )
            {
              std::cout << "\t[i] unobservable node " << n << "\n";
            }
          }
        }
      }

      return true; /* next gate */
    } );
  }

private:
  void new_pattern( std::vector<bool> const& pattern )
  {
    sim.add_pattern( pattern );
    ++st.num_generated_patterns;

    /* re-simulate */
    if ( sim.num_bits() % 64 == 0 )
    {
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_nodes<Ntk>( ntk, tts, sim, false );
      });
    }
  }

  void generate_more_patterns( node const& n, kitty::partial_truth_table const& tt, bool value, kitty::partial_truth_table& zero )
  {
    /* collect the `value` patterns */
    std::vector<std::vector<bool>> patterns;
    for ( auto i = 0u; i < tt.num_bits(); ++i )
    {
      if ( kitty::get_bit( tt, i ) == value )
      {
        patterns.emplace_back();
        ntk.foreach_pi( [&]( auto const& pi ){ 
          patterns.back().emplace_back( kitty::get_bit( tts[pi], i ) );
        });
      }
    }

    auto generated = validator.generate_pattern( n, value, patterns, ps.num_stuck_at - patterns.size() );
    for ( auto& pattern : generated )
    {
      new_pattern( pattern );
    }
    zero = sim.compute_constant( false );
  }

private:
  Ntk& ntk;

  patgen_params const& ps;
  patgen_stats& st;

  validator_params& vps;
  circuit_validator<Ntk, bill::solvers::bsat2, true, true, use_odc> validator;
  
  TT tts;
  std::vector<signal> const_nodes;

  partial_simulator& sim;
};

} /* namespace detail */

template<class Ntk>
void pattern_generation( Ntk& ntk, partial_simulator& sim, patgen_params const& ps = {}, patgen_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the has substitute_node method" );

  patgen_stats st;
  validator_params vps;
  vps.conflict_limit = ps.conflict_limit;
  vps.random_seed = ps.random_seed;

  if ( ps.observability_type1 || ps.observability_type2 )
  {
    using fanout_view_t = fanout_view<Ntk>;
    fanout_view<Ntk> fanout_view{ntk};

    detail::patgen_impl<fanout_view_t, true> p( fanout_view, sim, ps, vps, st );
    p.run();
  }
  else
  {
    detail::patgen_impl p( ntk, sim, ps, vps, st );
    p.run();
  }

  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */
