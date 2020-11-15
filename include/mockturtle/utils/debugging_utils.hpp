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
  \file debugging_utils.hpp
  \brief Network debugging utils

  \author Heinz Riener
*/

#pragma once

namespace mockturtle
{

template<typename Ntk>
inline uint64_t count_dead_nodes( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  // static_assert( has_is_dead_v<Ntk>, "Ntk does not implement the is_dead function" );

  uint64_t counter{0};
  for ( uint64_t n = 0; n < ntk.size(); ++n )
  {
    if ( ntk.is_dead( n ) )
    {
      ++counter;
    }
  }
  return counter;
}

template<typename Ntk>
inline uint64_t count_dangling_roots( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size function" );

  uint64_t counter{0};
  for ( uint64_t n = 0; n < ntk.size(); ++n )
  {
    if ( ntk.fanout_size( n ) == 0 )
    {
      ++counter;
    }
  }
  return counter;
}

namespace detail
{

template<typename Ntk>
void count_reachable_dead_nodes_recur( Ntk const& ntk, typename Ntk::node const& n, std::vector<typename Ntk::node>& nodes )
{
  using signal = typename Ntk::signal;

  if ( ntk.current_color() == ntk.color( n ) )
  {
    return;
  }

  if ( ntk.is_dead( n ) )
  {
    if ( std::find( std::begin( nodes ), std::end( nodes ), n ) == std::end( nodes ) )
    {
      nodes.push_back( n );
    }
  }

  ntk.paint( n );
  ntk.foreach_fanin( n, [&]( signal const& fi ){
    count_reachable_dead_nodes_recur( ntk, ntk.get_node( fi ), nodes );
  });
}

} /* namespace detail */

template<typename Ntk>
inline uint64_t count_reachable_dead_nodes( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_color_v<Ntk>, "Ntk does not implement the color function" );
  static_assert( has_current_color_v<Ntk>, "Ntk does not implement the current_color function" );
  static_assert( has_foreach_co_v<Ntk>, "Ntk does not implement the foreach_co function" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin function" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node function" );
  // static_assert( has_is_dead_v<Ntk>, "Ntk does not implement the is_dead function" );
  static_assert( has_new_color_v<Ntk>, "Ntk does not implement the new_color function" );
  static_assert( has_paint_v<Ntk>, "Ntk does not implement the paint function" );

  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  ntk.new_color();

  std::vector<node> dead_nodes;
  ntk.foreach_co( [&]( signal const& po ){
    detail::count_reachable_dead_nodes_recur( ntk, ntk.get_node( po ), dead_nodes );
  });

  return dead_nodes.size();
}

namespace detail
{

template<typename Ntk>
bool network_is_acylic_recur( Ntk const& ntk, typename Ntk::node const& n )
{
  using signal = typename Ntk::signal;

  if ( ntk.color( n ) == ntk.current_color() )
  {
    return true;
  }

  if ( ntk.color( n ) == ntk.current_color() - 1 )
  {
    /* cycle detected at node n */
    return false;
  }

  ntk.paint( n, ntk.current_color() - 1 );
  ntk.foreach_fanin( n, [&]( signal const& fi ) {
    network_is_acylic_recur( ntk, ntk.get_node( fi ) );
  });
  ntk.paint( n, ntk.current_color() );
}

} /* namespace detail */

template<typename Ntk>
bool network_is_acylic( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_color_v<Ntk>, "Ntk does not implement the color function" );
  static_assert( has_current_color_v<Ntk>, "Ntk does not implement the current_color function" );
  static_assert( has_foreach_ci_v<Ntk>, "Ntk does not implement the foreach_ci function" );
  static_assert( has_foreach_co_v<Ntk>, "Ntk does not implement the foreach_co function" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin function" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant function" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node function" );
  static_assert( has_new_color_v<Ntk>, "Ntk does not implement the new_color function" );
  static_assert( has_paint_v<Ntk>, "Ntk does not implement the paint function" );

  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  ntk.new_color();
  ntk.new_color();

  ntk.paint( ntk.get_node( ntk.get_constant( false ) ) );
  ntk.foreach_ci( [&]( node const& n ){
    ntk.paint( n );
  });

  ntk.foreach_co( [&]( signal const& o ){
    if ( !detail::network_is_acylic_recur( ntk, ntk.get_node( o ) ) )
    {
      return false;
    }
  });

  return true;
}

} /* namespace mockturtle */
