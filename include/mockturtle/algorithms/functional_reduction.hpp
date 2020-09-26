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
  \file functional_reduction.hpp
  \brief Functional reduction for any network type

  \author Siang-Yun Lee
*/

#pragma once

#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"

#include <bill/sat/interface/abc_bsat2.hpp>
#include <kitty/partial_truth_table.hpp>

#include <mockturtle/algorithms/circuit_validator.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/io/write_patterns.hpp>

namespace mockturtle
{

struct functional_reduction_params
{
  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};

  /*! \brief Whether to use pre-generated patterns stored in a file.
   * If not, by default, 1024 random pattern + 1x stuck-at patterns will be generated.
   */
  std::optional<std::string> pattern_filename{};

  /*! \brief Whether to save the appended patterns (with CEXs) into file. */
  std::optional<std::string> save_patterns{};

  /*! \brief Conflict limit for the SAT solver. */
  uint32_t conflict_limit{1000};

  /*! \brief Maximum number of clauses of the SAT solver. (incremental CNF construction) */
  uint32_t max_clauses{1000};
};

struct functional_reduction_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{0};

  /*! \brief Time for simulation. */
  stopwatch<>::duration time_sim{0};

  /*! \brief Time for SAT solving. */
  stopwatch<>::duration time_sat{0};

  /*! \brief Number of accepted constant nodes. */
  uint32_t num_const_accepts{0};

  /*! \brief Number of accepted functionally equivalent nodes. */
  uint32_t num_equ_accepts{0};

  /*! \brief Number of patterns used. */
  uint32_t num_pats{0};

  /*! \brief Number of counter-examples (SAT calls). */
  uint32_t num_cex{0};

  /*! \brief Number of successful node reductions (UNSAT calls). */
  uint32_t num_reduction{0};

  /*! \brief Number of SAT solver timeout. */
  uint32_t num_timeout{0};

  void report() const
  {
  }
};

namespace detail
{
template<typename Ntk, typename validator_t = circuit_validator<Ntk, bill::solvers::bsat2>>
class functional_reduction_impl
{
public:
  using stats = functional_reduction_stats;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = unordered_node_map<kitty::partial_truth_table, Ntk>;

  explicit functional_reduction_impl( Ntk& ntk, functional_reduction_params const& ps, stats& st )
      : ntk( ntk ), ps( ps ), st( st ), tts( ntk ), validator( ntk, vps )
  {
    static_assert( !validator_t::use_odc_, "`circuit_validator::use_odc` flag should be turned off." );

    vps.conflict_limit = ps.conflict_limit;
    vps.max_clauses = ps.max_clauses;
  }

  ~functional_reduction_impl()
  {
    if ( ps.save_patterns )
    {
      write_patterns( sim, *ps.save_patterns );
    }
  }

  void run()
  {
    stopwatch t( st.time_total );

    /* prepare simulation patterns. */
    if ( ps.pattern_filename )
    {
      sim = partial_simulator( *ps.pattern_filename );
    }
    else
    {
      sim = partial_simulator( ntk.num_pis(), 256 );
    }
    st.num_pats = sim.num_bits();

    /* first simulation: the whole circuit; from 0 bits. */
    call_with_stopwatch( st.time_sim, [&]() {
      simulate_nodes<Ntk>( ntk, tts, sim, true );
    } );

    /* remove constant nodes. */
    substitute_constants();

    /* substitute functional equivalent nodes. */
    substitute_equivalent_nodes();
  }

private:
  void substitute_constants()
  {
    progress_bar pbar{ntk.size(), "FR-const |{0}| node = {1:>4}   cand = {2:>4}", ps.progress};

    auto zero = sim.compute_constant( false );
    auto one = sim.compute_constant( true );
    ntk.foreach_gate( [&]( auto const& n, auto i ) {
      pbar( i, i, candidates );

      if ( ntk.is_dead( n ) )
      {
        return true; /* next */
      }

      check_tts( n );
      bool const_value;
      if ( tts[n] == zero )
      {
        const_value = false;
      }
      else if ( tts[n] == one )
      {
        const_value = true;
      }
      else /* not constant */
      {
        return true; /* next */
      }

      /* update progress bar */
      candidates++;

      const auto res = call_with_stopwatch( st.time_sat, [&]() {
        return validator.validate( n, const_value );
      } );
      if ( !res ) /* timeout */
      {
        ++st.num_timeout;
        return true;
      }
      else if ( !( *res ) ) /* SAT, cex found */
      {
        found_cex();
        zero = sim.compute_constant( false );
        one = sim.compute_constant( true );
      }
      else /* UNSAT, constant verified */
      {
        ++st.num_reduction;
        ++st.num_const_accepts;
        /* update network */
        ntk.substitute_node( n, ntk.get_constant( const_value ) );
      }
      return true;
    } );
  }

  void substitute_equivalent_nodes()
  {
    progress_bar pbar{ntk.size(), "FR-equ |{0}| node = {1:>4}   cand = {2:>4}", ps.progress};
    ntk.foreach_gate( [&]( auto const& root, auto i ) {
      pbar( i, i, candidates );

      if ( ntk.is_dead( root ) )
      {
        return true; /* next */
      }

      check_tts( root );
      auto tt = tts[root];
      auto ntt = ~tts[root];
      foreach_transitive_fanin( root, [&]( auto const& n ) {
        signal g;
        if ( tt == tts[n] )
        {
          g = ntk.make_signal( n );
        }
        else if ( ntt == tts[n] )
        {
          g = !ntk.make_signal( n );
        }
        else /* not equivalent */
        {
          return true; /* try next transitive fanin node */
        }

        /* update progress bar */
        candidates++;

        const auto res = call_with_stopwatch( st.time_sat, [&]() {
          return validator.validate( n, g );
        } );
        if ( !res ) /* timeout */
        {
          ++st.num_timeout;
          return true; /* try next transitive fanin node */
        }
        else if ( !( *res ) ) /* SAT, cex found */
        {
          found_cex();
          check_tts( root );
          tt = tts[root];
          ntt = ~tts[root];
          return true; /* try next transitive fanin node */
        }
        else /* UNSAT, equivalent node verified */
        {
          ++st.num_reduction;
          ++st.num_equ_accepts;
          /* update network */
          ntk.substitute_node( n, g );
          return false; /* break `foreach_transitive_fanin` */
        }
      } );

      return true; /* next */
    } );
  }

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

  template<typename Fn>
  void foreach_transitive_fanin( node const& n, Fn&& fn )
  {
    ntk.incr_trav_id();
    auto trav_id = ntk.trav_id();

    foreach_transitive_fanin_rec( n, trav_id, fn );
  }

  template<typename Fn>
  bool foreach_transitive_fanin_rec( node const& n, uint32_t const& trav_id, Fn&& fn )
  {
    ntk.set_visited( n, trav_id );
    if ( !fn( n ) )
    {
      return false;
    }
    bool continue_loop = true;
    ntk.foreach_fanin( n, [&]( auto const& f ) {
      if ( ntk.visited( ntk.get_node( f ) ) == trav_id )
      {
        return true;
      } /* skip visited node, continue looping. */

      continue_loop = foreach_transitive_fanin_rec( ntk.get_node( f ), trav_id, fn );
      return continue_loop; /* break `foreach_fanin` loop immediately when receiving `false`. */
    } );
    return continue_loop; /* return `false` only if `false` has ever been received from recursive calls. */
  }

private:
  Ntk& ntk;
  functional_reduction_params const& ps;
  stats& st;

  TT tts;
  partial_simulator sim;

  validator_params vps;
  validator_t validator;

  uint32_t candidates{0};
}; /* functional_reduction_impl */

} /* namespace detail */

template<class Ntk>
void functional_reduction( Ntk& ntk, functional_reduction_params const& ps = {}, functional_reduction_stats* pst = nullptr )
{
  functional_reduction_stats st;
  detail::functional_reduction_impl p( ntk, ps, st );
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
