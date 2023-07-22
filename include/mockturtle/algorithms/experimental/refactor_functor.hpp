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
#include "../../algorithms/cut_enumeration.hpp"
#include "../../algorithms/simulation.hpp"
#include "../../utils/index_list.hpp"

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
    TT const& target = ntk.is_complemented( po ) ? ~tts[po] : tts[po];

    cut_enumeration_params ps;
    ps.cut_size = ntk.num_pis();
    cut_enumeration_stats st;

    const auto cuts = cut_enumeration<Ntk, true>( ntk, ps, &st );
    for ( auto& cut : cuts.cuts( ntk.node_to_index( ntk.get_node( po ) ) ) )
    {
      if ( cut->size() == 1u || kitty::is_const0( cuts.truth_table( *cut ) ) )
      {
        continue;
      }
      TT const& tt = cuts.truth_table( *cut );
      std::vector<signal> leaves( cut->size() );
      std::transform( cut->begin(), cut->end(), std::begin( leaves ), [&]( auto leaf ) { return ntk.make_signal( ntk.index_to_node( leaf ) ); } );
      assert( leaves.size() == tt.num_vars() );
      auto const s = create_esop_function( ntk, tt, leaves );
      evalfn( s );
      auto const _s = create_sop_function( ntk, tt, leaves );
      evalfn( _s );
    }

    // kitty::print_hex( target ); fmt::print( " : {}\n", target.num_vars() );
  }

private:
  std::vector<kitty::cube> create_esop_form( TT const& func ) const
  {
    if ( auto it = esop_hash_.find( func ); it != esop_hash_.end() )
    {
      return it->second;
    }
    else
    {
      return esop_hash_[func] = mockturtle::exorcism( func ); // TODO generalize
    }
  }

  std::vector<kitty::cube> create_sop_form( TT const& func ) const
  {
    if ( auto it = sop_hash_.find( func ); it != sop_hash_.end() )
    {
      return it->second;
    }
    else
    {
      return sop_hash_[func] = kitty::isop( func ); // TODO generalize
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

  cotext_signal_pair<Ntk> create_or_tree( Ntk& dest, cotext_signal_queue<Ntk>& queue ) const
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
      const auto s = dest.create_or( s1, s2 ); /* context propagate automatically */
      queue.push( { dest.get_context( dest.get_node( s ) ), s } );
    }
    return queue.top();
  }

  cotext_signal_pair<Ntk> create_xor_tree( Ntk& dest, cotext_signal_queue<Ntk>& queue ) const
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
      const auto s = dest.create_xor( s1, s2 ); /* context propagate automatically */
      queue.push( { dest.get_context( dest.get_node( s ) ), s } );
    }
    return queue.top();
  }

  /* esop rebalancing */
  signal create_esop_function( Ntk& dest, TT const& tt, std::vector<signal> const& leaves ) const
  {
    const auto esop = create_esop_form( tt );

    cotext_signal_queue<Ntk> _q;

    for ( auto const& cube : esop )
    {
      cotext_signal_queue<Ntk> q;
      for ( auto i = 0u; i < tt.num_vars(); ++i )
      {
        if ( cube.get_mask( i ) )
        {
          const auto f = leaves[i];
          const auto l = dest.get_context( dest.get_node( f ) );
          q.push( { l, cube.get_bit( i ) ? f : dest.create_not( f ) } );
        }
      }
      auto [l, s] = create_and_tree( dest, q );
      _q.push( { l, s } );
    }
    auto [_l, _s] = create_xor_tree( dest, _q );
    return _s;
  }

  /* sop rebalancing */
  signal create_sop_function( Ntk& dest, TT const& tt, std::vector<signal> const& leaves ) const
  {
    const auto sop = create_sop_form( tt );

    cotext_signal_queue<Ntk> _q;

    for ( auto const& cube : sop )
    {
      cotext_signal_queue<Ntk> q;
      for ( auto i = 0u; i < tt.num_vars(); ++i )
      {
        if ( cube.get_mask( i ) )
        {
          const auto f = leaves[i];
          const auto l = dest.get_context( dest.get_node( f ) );
          q.push( { l, cube.get_bit( i ) ? f : dest.create_not( f ) } );
        }
      }
      auto [l, s] = create_and_tree( dest, q );
      _q.push( { l, s } );
    }
    auto [_l, _s] = create_or_tree( dest, _q );
    return _s;
  }

private:
  mutable std::unordered_map<TT, std::vector<kitty::cube>, kitty::hash<TT>> esop_hash_;
  mutable std::unordered_map<TT, std::vector<kitty::cube>, kitty::hash<TT>> sop_hash_;
};

} // namespace mockturtle::experimental