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
  \file frontier.hpp
  \brief Frontier exploration

  \author Heinz Riener
*/

#pragma once

#include "../traits.hpp"

#include <vector>
#include <queue>

namespace mockturtle
{

namespace detail
{

template<typename Ntk>
class sort_by_pivot_distance
{
public:
  using node = node<Ntk>;

public:
  explicit sort_by_pivot_distance( Ntk const& ntk, node const& pivot )
    : ntk( ntk )
    , pivot( pivot )
  {
  }

  bool operator()( node const& a, node const& b ) const
  {
    auto a_level = ntk.level( a ) - ntk.level( pivot );
    if ( a_level < 0 )
      a_level = -a_level;
    auto b_level = ntk.level( b ) - ntk.level( pivot );
    if ( b_level < 0 )
      b_level = -a_level;

    /* sort by distance to pivot */
    if ( a_level < b_level )
      return true;
    else if ( a_level > b_level )
      return false;
    /* sort by node index if distance is equal */
    return a < b;
  }

private:
  Ntk const& ntk;
  node const& pivot;
}; /* sort_by_pivot_distance */

} /* namespace detail */

struct frontier_parameters
{
  /* skip nodes with many fanouts */
  uint32_t skip_fanout_limit{100};
};

/*! Dynamically growing frontier
 *
 * An experimental implementation of node frontieer that explores dynamically grows from a cut.
 *
 * Nodes are marked as explored using the current traversal ID:  If all nodes should be considered during exploration, the traversal ID can be incremented first.  If certain nodes should be excluded from exploration, they can be marked with the current traversal ID.
 */
template<class Ntk, class CostFn = detail::sort_by_pivot_distance<Ntk>>
class frontier
{
public:
  using node = node<Ntk>;
  using signal = signal<Ntk>;

public:
  explicit frontier( Ntk const& ntk, node const& pivot, std::vector<node> const& leaves,
                     frontier_parameters const ps = {} )
    : ntk( ntk )
    , pivot( pivot )
    , to_explore( CostFn( ntk, pivot ) )
    , ps( ps )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );
    static_assert( has_level_v<Ntk>, "Ntk does not implement the has_level method" );

    /* mark pivot as explored */
    ntk.set_visited( pivot, ntk.trav_id() );

    /* initialize fronier with leaves */
    for ( const auto& n : leaves )
    {
      if ( ntk.visited( n ) == ntk.trav_id() )
        continue;

      to_explore.push( n );
    }
  }

  explicit frontier( Ntk const& ntk, node const& pivot, std::vector<signal> const& leaves,
                     frontier_parameters const ps = {} )
    : ntk( ntk )
    , pivot( pivot )
    , to_explore( CostFn( ntk, pivot ) )
    , ps( ps )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );
    static_assert( has_level_v<Ntk>, "Ntk does not implement the has_level method" );

    /* mark pivot as explored */
    ntk.set_visited( pivot, ntk.trav_id() );

    /* initialize fronier with leaves */
    for ( const auto& l : leaves )
    {
      auto const& n = ntk.get_node( l );
      if ( ntk.visited( n ) == ntk.trav_id() )
        continue;

      to_explore.push( n );
    }
  }

  /*! Grows the frontier and invokes fn for each new divisor
   *
   * The algorithm terminates if no divisors are left to explore or
   * the divisor function signals termination by returning false.
   */
  template<typename Fn>
  bool grow( Fn&& fn )
  {
    node n;
    while ( true )
    {
      if ( to_explore.empty() )
        return false;

      n = to_explore.top();
      to_explore.pop();

      /* if not yet explored then break */
      if ( ntk.visited( n ) != ntk.trav_id() )
        break;
    }

    /* terminate if divisor functor returns false */
    if ( !fn( n ) )
      return false;

    /* explore fanin/fanout of this node */
    ntk.set_visited( n, ntk.trav_id() );
    explore_fanins( n );
    explore_fanouts( n );

    return true;
  }

private:
  /* explore the fanins of a node */
  void explore_fanins( node const& n )
  {
    ntk.foreach_fanin( n, [&]( const auto& fi ){
        auto const nn = ntk.get_node( fi );
        if ( ntk.visited( nn ) == ntk.trav_id() )
          return;

        to_explore.push( nn );
      });
  }

  /* explore the fanouts of a node */
  void explore_fanouts( node const& n )
  {
    /* skip nodes with many fanouts */
    if ( ntk.fanout_size( n ) > ps.skip_fanout_limit )
      return;

    ntk.foreach_fanout( n, [&]( const auto& fo ){
        if ( ntk.visited( fo ) != ntk.trav_id() && ntk.level( fo ) <= ntk.level( pivot ) )
        {
          if ( ntk.visited( fo ) == ntk.trav_id() )
            return;

          to_explore.push( fo );
        }
      });
  }

private:
  Ntk const& ntk;
  node const& pivot;
  std::priority_queue<node, std::vector<node>, detail::sort_by_pivot_distance<Ntk>> to_explore;

  frontier_parameters ps;
}; /* frontier */

} /* mockturtle */
