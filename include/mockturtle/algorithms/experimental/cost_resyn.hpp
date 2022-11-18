/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
  \file cost_resyn.hpp
  \brief Solver of cost-aware resynthesis problem.
         Given a resynthesis problem and the cost function, returns
         the solution with (1) correct functionality (2) lower cost.

         This solver is cost-generic.

  \author Hanyu Wang
*/

#pragma once

#include "../../algorithms/cleanup.hpp"
#include "../../algorithms/exorcism.hpp"
#include "../../utils/index_list.hpp"
#include "../detail/resub_utils.hpp"
#include "../simulation.hpp"
#include "resub_functors.hpp"

#include <algorithm>
#include <optional>
#include <queue>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace mockturtle::experimental
{

struct cost_resyn_params
{
  /* maximum number of feasible solutions to collect, 0: no limit */
  uint32_t max_solutions{ 0u };

  uint32_t max_pis{ 8u };

  /* use esop */
  bool use_esop{ false };

};

struct cost_resyn_stats
{
  /* time for cost view the solution network */
  stopwatch<>::duration time_eval{ 0 };

  /* time for searching the equivalent network */
  stopwatch<>::duration time_search{ 0 };

  /* number of solutions */
  uint32_t num_solutions{ 0 };

  /* number of problems */
  uint32_t num_problems{ 0 };

  /* number of solution with 0,1,2,3 insertions */
  uint32_t num_resub[4]{ 0 };

  /* size of the forest of feasible solutions */
  uint32_t size_forest{ 0 };

  /* number of root (feasible solutions) */
  uint32_t num_roots{ 0 };

  /* number of total gains */
  uint32_t num_gain{ 0 };

  /* data */
  void report() const
  {
    fmt::print( "[i]         <cost_resyn>\n" );
    fmt::print( "[i]             Evalutation      : {:>5.2f} secs\n", to_seconds( time_eval ) );
    fmt::print( "[i]             Searching        : {:>5.2f} secs\n", to_seconds( time_search ) );
    fmt::print( "[i]             # Problem        : {}\n", num_problems );
    fmt::print( "[i]             Avg. forest size : {:>5.2f}\n", (float)size_forest / num_problems );
    fmt::print( "[i]             Avg. num solution: {:>5.2f}\n", (float)num_roots / num_problems );
    fmt::print( "[i]             Opt. ratio       : {:>5.2f}%\n", (float)num_solutions / num_problems * 100 );
    fmt::print( "[i]             0 - resub        : {:>5.2f}\n", (float)num_resub[0] / num_problems );
    fmt::print( "[i]             1 - resub        : {:>5.2f}\n", (float)num_resub[1] / num_problems );
    fmt::print( "[i]             2 - resub        : {:>5.2f}\n", (float)num_resub[2] / num_problems );
    fmt::print( "[i]             3 - resub        : {:>5.2f}\n", (float)num_resub[3] / num_problems );
    fmt::print( "[i]             Gain             : {:>5.2f}\n", (float)num_gain / num_problems );
  }
};

template<class Ntk, class TT>
class cost_resyn
{
public:
  using params = cost_resyn_params;
  using stats = cost_resyn_stats;
  using signal = typename Ntk::signal;
  using node = typename Ntk::node;
  using context_t = typename Ntk::context_t;
  using index_list_t = large_xag_index_list;
;

public:
  explicit cost_resyn( Ntk const& ntk, params const& ps, stats& st ) noexcept
      : ntk( ntk ), ps( ps ), st( st )
  {
  }

  std::optional<signal> operator()( std::vector<signal> const& leaves, std::vector<signal> const& divs, std::vector<signal> const& mffcs, signal& root )
  {
    uint32_t n_solutions = 0u;
    std::optional<signal> best_candidate;

    /* convert the divisors to a tree */
    Ntk forest;
    unordered_node_map<signal, Ntk> old_to_new( ntk );
    std::for_each( std::begin(leaves), std::end(leaves), [&]( signal const& s ) {
      const auto f = forest.create_pi();
      forest.set_context( forest.get_node( f ), ntk.get_context( ntk.get_node( s ) ) );
      old_to_new[ ntk.get_node( s ) ] = ntk.is_complemented( s )? forest.create_not( f ) : f;
    } );

    std::for_each( std::begin(divs), std::end(divs), [&]( signal const& s ) {
      auto const n = ntk.get_node( s );
      if ( old_to_new.has( n ) ) return;
      std::vector<signal> children;
      ntk.foreach_fanin( n, [&]( auto child, auto ) {
        const auto f = old_to_new[child];
        if ( ntk.is_complemented( child ) )
        {
          children.push_back( forest.create_not( f ) );
        }
        else
        {
          children.push_back( f );
        }
      } );
      auto const _s = forest.clone_node( ntk, n, children );
      old_to_new[n] = ntk.is_complemented( s )? forest.create_not( _s ) : _s;
    } );

    std::for_each( std::begin(mffcs), std::end(mffcs), [&]( signal const& s ) {
      auto const n = ntk.get_node( s );
      if ( old_to_new.has( n ) ) return;
      std::vector<signal> children;
      ntk.foreach_fanin( n, [&]( auto child, auto ) {
        const auto f = old_to_new[child];
        if ( ntk.is_complemented( child ) )
        {
          children.push_back( forest.create_not( f ) );
        }
        else
        {
          children.push_back( f );
        }
      } );
      auto const _s = forest.clone_node( ntk, n, children );
      old_to_new[n] = ntk.is_complemented( s )? forest.create_not( _s ) : _s;
    } );

    std::vector<signal> pis;
    std::for_each( std::begin(divs), std::end(divs), [&]( signal const& s ) {
      pis.emplace_back( old_to_new[ ntk.get_node( s ) ]);
    } );
    auto const po = old_to_new[ ntk.get_node( root ) ];

    uint32_t best_cost = forest.get_cost( forest.get_node( po ), pis );
    fmt::print( "cost = {}\n", best_cost );

    // /* esop solutions (try building ntk without divisors ) */
    // if ( ps.use_esop )
    // {
    //   const auto s = create_function( forest, target );
    //   auto rw_cost = forest.get_cost( forest.get_node( s ), leaves );
    //   if ( rw_cost < best_cost )
    //   {
    //     best_cost = rw_cost;
    //     best_candidate = s;
    //   }
    // }

    // /* grow the forest */
    // resub_functor<Ntk, TT, decltype(begin), decltype(tts)> engine( forest, target, care, begin, end, tts );
    // engine.run( [&]( signal g ) {
    //   forest.incr_trav_id();
    //   uint32_t curr_cost = forest.get_cost( forest.get_node( g ), leaves );
    //   if ( curr_cost < best_cost )
    //   {
    //     n_solutions++;
    //     best_cost = curr_cost;
    //     best_candidate = g;
    //     return (ps.max_solutions > 0) && (n_solutions >= ps.max_solutions); /* stop searching */
    //   }
    //   return false; /* keep searching */
    // } );

    // if ( best_candidate )
    // {
    //   index_list_t index_list;
    //   forest.create_po( *best_candidate );
    //   typename Ntk::base_type solution = cleanup_dangling( (typename Ntk::base_type)forest );
    //   encode( index_list, solution );
    //   return index_list;
    // }
    return std::nullopt;
  }


  std::vector<kitty::cube> create_sop_form( TT const& func ) const
  {
    if ( auto it = sop_hash_.find( func ); it != sop_hash_.end() )
    {
      return it->second;
    }
    else
    {
      return sop_hash_[func] = mockturtle::exorcism( func ); // TODO generalize
    }
  }

  cotext_signal_pair<Ntk> create_and_tree( Ntk& dest, cotext_signal_queue<Ntk>& queue ) const
  {
    if ( queue.empty() )
    {
      context_t context{};
      return { context, dest.get_constant( true ) };
    }

    while ( queue.size() > 1u )
    {
      auto [l1, s1] = queue.top();
      queue.pop();
      auto [l2, s2] = queue.top();
      queue.pop();
      const auto s = dest.create_and( s1, s2 ); /* context propagate automatically */
      queue.push( { dest.get_context( dest.get_node( s ) ), s } );
    }
    return queue.top();
  }

  /* esop rebalancing */
  signal create_function( Ntk& dest, TT const& tt ) const
  {
    const auto esop = create_sop_form( tt );

    std::vector<signal> and_terms;
    uint32_t max_level{};
    uint32_t num_and_gates{};

    for ( auto const& cube : esop )
    {
      cotext_signal_queue<Ntk> q;
      for ( auto i = 0u; i < tt.num_vars(); ++i )
      {
        if ( cube.get_mask( i ) )
        {
          const auto n = dest.pi_at( i );
          const auto f = dest.make_signal( n );
          const auto l = dest.get_context( n );
          q.push( { l, cube.get_bit( i ) ? f : dest.create_not( f ) } );
        }
      }
      auto [l ,s] = create_and_tree( dest, q );
      and_terms.push_back( s );
    }
    return dest.create_nary_xor( and_terms ); // TODO: use XOR tree
  }

private:
  Ntk const& ntk;
  params const& ps;
  stats& st;
  std::vector<TT> tts;
  mutable std::unordered_map<TT, std::vector<kitty::cube>, kitty::hash<TT>> sop_hash_;
};

} // namespace mockturtle::experimental
