/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
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
  \file mffc_utils.hpp
  \brief Utility functions for DAG-aware reference counting and
         MFFC-size computation

  \author Mathias Soeken
*/

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "../../traits.hpp"
#include "../../utils/cost_functions.hpp"

namespace mockturtle::detail
{

template<typename Ntk, typename TermCond, class NodeCostFn = unit_cost<Ntk>>
uint32_t recursive_deref( Ntk const& ntk, node<Ntk> const& n, TermCond const& terminate )
{
  /* terminate? */
  if ( terminate( n ) )
    return 0;

  /* recursively collect nodes */
  uint32_t value = NodeCostFn{}( ntk, n );
  ntk.foreach_fanin( n, [&]( auto const& s ) {
    if ( ntk.decr_value( ntk.get_node( s ) ) == 0 )
    {
      value += recursive_deref<Ntk, TermCond, NodeCostFn>( ntk, ntk.get_node( s ), terminate );
    }
  } );
  return value;
}

template<typename Ntk, typename TermCond, class NodeCostFn = unit_cost<Ntk>>
uint32_t recursive_ref( Ntk const& ntk, node<Ntk> const& n, TermCond const& terminate )
{
  /* terminate? */
  if ( terminate( n ) )
    return 0;

  /* recursively collect nodes */
  uint32_t value = NodeCostFn{}( ntk, n );
  ntk.foreach_fanin( n, [&]( auto const& s ) {
    if ( ntk.incr_value( ntk.get_node( s ) ) == 0 )
    {
      value += recursive_ref<Ntk, TermCond, NodeCostFn>( ntk, ntk.get_node( s ), terminate );
    }
  } );
  return value;
}

template<typename Ntk, typename LeavesIterator, class NodeCostFn = unit_cost<Ntk>>
uint32_t recursive_deref( Ntk const& ntk, node<Ntk> const& n, LeavesIterator begin, LeavesIterator end )
{
  const auto terminate = [&]( auto const& n ) { return std::find( begin, end, n ) != end; };
  return recursive_deref<Ntk, decltype( terminate ), NodeCostFn>( ntk, n, terminate );
}

template<typename Ntk, typename LeavesIterator, class NodeCostFn = unit_cost<Ntk>>
uint32_t recursive_ref( Ntk const& ntk, node<Ntk> const& n, LeavesIterator begin, LeavesIterator end )
{
  const auto terminate = [&]( auto const& n ) { return std::find( begin, end, n ) != end; };
  return recursive_ref<Ntk, decltype( terminate ), NodeCostFn>( ntk, n, terminate );
}

template<typename Ntk, class NodeCostFn = unit_cost<Ntk>>
uint32_t recursive_deref( Ntk const& ntk, node<Ntk> const& n )
{
  const auto terminate = [&]( auto const& n ) { return ntk.is_constant( n ) || ntk.is_pi( n ); };
  return recursive_deref<Ntk, decltype( terminate ), NodeCostFn>( ntk, n, terminate );
}

template<typename Ntk, class NodeCostFn = unit_cost<Ntk>>
uint32_t recursive_ref( Ntk const& ntk, node<Ntk> const& n )
{
  const auto terminate = [&]( auto const& n ) { return ntk.is_constant( n ) || ntk.is_pi( n ); };
  return recursive_ref<Ntk, decltype( terminate ), NodeCostFn>( ntk, n, terminate );
}

template<typename Ntk, class NodeCostFn = unit_cost<Ntk>>
uint32_t mffc_size( Ntk const& ntk, node<Ntk> const& n )
{
  auto v1 = recursive_deref<Ntk, NodeCostFn>( ntk, n );
  auto v2 = recursive_ref<Ntk, NodeCostFn>( ntk, n );
  assert( v1 == v2 );
  (void)v2;
  return v1;
}

} /* namespace mockturtle::detail */
