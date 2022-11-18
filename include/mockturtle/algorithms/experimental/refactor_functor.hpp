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
  \file refactor_functor.hpp
  \brief Solver of cost-aware resynthesis problem.
         Given a resynthesis problem and the cost function, returns
         the solution with (1) correct functionality (2) lower cost.

         This solver is cost-generic.

  \author Hanyu Wang
*/

#pragma once

#include "../../algorithms/cleanup.hpp"
#include "../../utils/index_list.hpp"
#include "../../algorithms/simulation.hpp"

#include <algorithm>
#include <optional>
#include <queue>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace mockturtle::experimental
{

template<class Ntk, class TT>
class refactor_functor
{
public:
  using signal = typename Ntk::signal;
  using context_t = typename Ntk::context_t;

public:
  explicit refactor_functor()
  {
  }

  template<typename Fn>
  void operator()( Ntk& ntk, Fn&& evalfn )
  {
    assert( ntk.num_pos() == 1 && "network does not have single PO" );
    default_simulator<kitty::dynamic_truth_table> sim( ntk.num_pis() );
    const auto tts = simulate_nodes<kitty::dynamic_truth_table>( ntk, sim );
    auto const po = ntk.po_at( 0 );
    TT target = ntk.is_complemented( po )? ~tts[po] : tts[po];
    // kitty::print_hex( target ); fmt::print( " : {}\n", target.num_vars() );
    auto const s = create_function( ntk, target );
    evalfn( s );
  }
private:

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
  mutable std::unordered_map<TT, std::vector<kitty::cube>, kitty::hash<TT>> sop_hash_;

};


}