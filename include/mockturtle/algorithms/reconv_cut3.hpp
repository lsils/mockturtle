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
  \file reconv_cut3.hpp
  \brief Reconvergence-driven cut

  \author Heinz Riener
*/

#pragma once

#include "../traits.hpp"

#include <cassert>
#include <optional>
#include <vector>

namespace mockturtle
{

struct reconv_cut_parameters
{
  /*! \brief maximum size of a reconv. cut */
  uint32_t node_size_max{8u};

  /*! \brief skip nodes with many fanouts */
  uint32_t node_fan_stop{100000u};
};

struct reconv_cut_statistics
{
  /*! \brief number of calls */
  uint32_t num_calls{0u};

  /*! \brief total number of leaves */
  uint32_t total_leaves{0u};

  void reset()
  {
    *this = {};
  }
};

template<typename Ntk>
class reconv_cut_computation
{
public:
  using node = typename Ntk::node;

public:
  explicit reconv_cut_computation( Ntk const& ntk, reconv_cut_parameters const& ps, reconv_cut_statistics& st )
    : ntk( ntk )
    , ps( ps )
    , st( st )
  {
  }

  std::vector<node> run( node const& pivot )
  {
    ++st.num_calls;

    ntk.incr_trav_id();

    /* start the visited nodes and mark them */
    visited.clear();
    visited.push_back( pivot );
    ntk.set_visited( pivot, 1 );
    ntk.foreach_fanin( pivot, [&]( const auto& f ) {
        auto const& n = ntk.get_node( f );
        if ( n == 0 )
          return true;
        visited.push_back( n );
        ntk.set_visited( n, ntk.trav_id() );
        return true;
      });

    /* start the cut */
    leaves.clear();
    ntk.foreach_fanin( pivot, [&]( const auto& f ) {
        auto const& n = ntk.get_node( f );
        if ( n == 0 )
          return true;
        leaves.push_back( n );
        return true;
      } );

    if ( leaves.size() > ps.node_size_max )
    {
      /* special case: cut already overflows at the current node
         because the cut size limit is very low */
      leaves.clear();
      return {};
    }

    /* compute the cut */
    while ( build_cut() );
    assert( leaves.size() <= ps.node_size_max );

    /* update statistics */
    st.total_leaves += leaves.size();
    return leaves;
  }

protected:
  bool build_cut()
  {
    uint32_t best_cost{100u};

    std::optional<node> best_fanin;
    int best_pos;

    /* evaluate fanins of the cut */
    auto pos = 0;
    for ( const auto& l : leaves )
    {
      uint32_t const cost_curr = leaf_costs( l );
      if ( best_cost > cost_curr ||
           ( best_cost == cost_curr && best_fanin && ntk.level( l ) > ntk.level( *best_fanin ) ) )
      {
        best_cost = cost_curr;
        best_fanin = std::make_optional( l );
        best_pos = pos;
      }

      if ( best_cost == 0 )
        break;

      ++pos;
    }

    if ( !best_fanin )
      return false;

    // assert( best_cost < max_fanin_of_graph_structure );
    if ( leaves.size() - 1 + best_cost > ps.node_size_max )
      return false;

    /* remove the best node from the array */
    leaves.erase( leaves.begin() + best_pos );

    /* add the fanins of best to leaves and visited */
    ntk.foreach_fanin( *best_fanin, [&]( const auto& f ){
        auto const& n = ntk.get_node( f );
        if ( n != 0 && ( ntk.visited( n ) != ntk.trav_id() ) )
          {
            ntk.set_visited( n, ntk.trav_id() );
            visited.push_back( n );
            leaves.push_back( n );
          }
      });

    assert( leaves.size() <= ps.node_size_max );
    return true;
  }

  uint32_t leaf_costs( node const &node ) const
  {
    /* make sure the node is in the construction zone */
    assert( ntk.visited( node ) == ntk.trav_id() );

    /* cannot expand over the PI node */
    if ( ntk.is_constant( node ) || ntk.is_pi( node ) )
      return 999;

    /* get the cost of the cone */
    uint32_t cost = 0u;
    ntk.foreach_fanin( node, [&]( const auto& f ){
        cost += ( ntk.visited( ntk.get_node( f ) ) == ntk.trav_id() ) ? 0 : 1;
      } );

    /* always accept if the number of leaves does not increase */
    if ( cost < ntk.fanin_size( node ) )
      return cost;

    /* skip nodes with many fanouts */
    if ( ntk.fanout_size( node ) > ps.node_fan_stop )
      return 999;

    /* return the number of nodes that will be on the leaves if this node is removed */
    return cost;
  }

protected:
  Ntk const& ntk;
  reconv_cut_parameters const& ps;
  reconv_cut_statistics& st;

  std::vector<node> visited;
  std::vector<node> leaves;
};

template<typename Ntk>
std::vector<node<Ntk>> reconv_driven_cut( Ntk const& ntk, node<Ntk> const& pivot, reconv_cut_parameters const& ps, reconv_cut_statistics& st )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );
  static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method" );
  static_assert( has_level_v<Ntk>, "Ntk does not implement the level method" );

  return reconv_cut_computation<Ntk>( ntk, ps, st ).run( pivot );
}

template<typename Ntk>
std::vector<node<Ntk>> reconv_driven_cut( Ntk const& ntk, node<Ntk> const& pivot, reconv_cut_parameters const& ps )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );
  static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method" );
  static_assert( has_level_v<Ntk>, "Ntk does not implement the level method" );

  reconv_cut_statistics st;
  return reconv_cut_computation<Ntk>( ntk, ps, st ).run( pivot );
}

} /* namespace mockturtle */
