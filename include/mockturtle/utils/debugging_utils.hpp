/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
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

#include "../algorithms/simulation.hpp"
#include "../views/topo_view.hpp"

#include <kitty/kitty.hpp>

#include <algorithm>
#include <vector>

namespace mockturtle
{

template<typename Ntk>
inline void print( Ntk const& ntk )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  for ( uint32_t n = 0; n < ntk.size(); ++n )
  {
    std::cout << n;

    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
    {
      std::cout << std::endl;
      continue;
    }

    std::cout << " = ";

    ntk.foreach_fanin( n, [&]( signal const& fi ){
      std::cout << ( ntk.is_complemented( fi ) ? "~" : "" ) << ntk.get_node( fi ) << " ";
    });
    std::cout << " ; [level = " << int32_t( ntk.level( n ) ) << "]" << " [dead = " << ntk.is_dead( n ) << "]" << " [ref = " << ntk.fanout_size( n ) << "]" << std::endl;
  }

  ntk.foreach_co( [&]( signal const& s ){
    std::cout << "o " << ( ntk.is_complemented( s ) ? "~" : "" ) << ntk.get_node( s ) << std::endl;
  });
}

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
void count_reachable_dead_nodes_from_node_recur( Ntk const& ntk, typename Ntk::node const& n, std::vector<typename Ntk::node>& nodes )
{
  using node = typename Ntk::node;

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
  ntk.foreach_fanout( n, [&]( node const& fo ){
    count_reachable_dead_nodes_from_node_recur( ntk, fo, nodes );
  });
}

} /* namespace detail */

template<typename Ntk>
inline uint64_t count_reachable_dead_nodes_from_node( Ntk const& ntk, typename Ntk::node const& n )
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
  detail::count_reachable_dead_nodes_from_node_recur( ntk, n, dead_nodes );

  return dead_nodes.size();
}

namespace detail
{

template<typename Ntk>
void count_nodes_with_dead_fanins_recur( Ntk const& ntk, typename Ntk::node const& n, std::vector<typename Ntk::node>& nodes )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  if ( ntk.current_color() == ntk.color( n ) )
  {
    return;
  }
  ntk.paint( n );

  ntk.foreach_fanin( n, [&]( signal const& s ){
    if ( ntk.is_dead( ntk.get_node( s ) ) )
    {
      if ( std::find( std::begin( nodes ), std::end( nodes ), n ) == std::end( nodes ) )
      {
        nodes.push_back( n );
      }
    }
  });

  ntk.foreach_fanout( n, [&]( node const& fo ){
    count_nodes_with_dead_fanins_recur( ntk, fo, nodes );
  });
}

} /* namespace detail */

template<typename Ntk>
uint64_t count_nodes_with_dead_fanins( Ntk const& ntk, typename Ntk::node const& n )
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

  ntk.new_color();

  std::vector<node> nodes_with_dead_fanins;
  detail::count_nodes_with_dead_fanins_recur( ntk, n, nodes_with_dead_fanins );

  return nodes_with_dead_fanins.size();
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

  bool result{true};
  ntk.foreach_fanin( n, [&]( signal const& fi ) {
    if ( !network_is_acylic_recur( ntk, ntk.get_node( fi ) ) )
    {
      result = false;
      return false;
    }
    return true;
  });
  ntk.paint( n, ntk.current_color() );

  return result;
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

  bool result{true};
  ntk.foreach_co( [&]( signal const& o ){
    if ( !detail::network_is_acylic_recur( ntk, ntk.get_node( o ) ) )
    {
      result = false;
      return false;
    }
    return true;
  });

  return result;
}

template<typename Ntk>
bool check_network_levels( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the size function" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant function" );
  static_assert( has_is_ci_v<Ntk>, "Ntk does not implement the is_ci function" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node function" );
  // static_assert( has_is_dead_v<Ntk>, "Ntk does not implement the is_dead function" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin function" );
  static_assert( has_level_v<Ntk>, "Ntk does not implement the level function" );
  static_assert( has_depth_v<Ntk>, "Ntk does not implement the depth function" );

  using signal = typename Ntk::signal;

  uint32_t max = 0;
  for ( uint32_t i = 0u; i < ntk.size(); ++i )
  {
    if ( ntk.is_constant( i ) || ntk.is_ci( i ) || ntk.is_dead( i ) )
    {
      continue;
    }

    uint32_t max_fanin_level = 0;
    ntk.foreach_fanin( i, [&]( signal fi ){
      if ( ntk.level( ntk.get_node( fi ) ) > max_fanin_level )
      {
        max_fanin_level = ntk.level( ntk.get_node( fi ) );
      }
    });

    /* the node's level has not been correctly computed */
    if ( ntk.level( i ) != max_fanin_level + 1 )
    {
      return false;
    } 
      
    if ( ntk.level( i ) > max )
    {
      max = ntk.level( i );
    }
  }

  /* the network's depth has not been correctly computed */
  if ( ntk.depth() != max )
  {
    return false;
  }

  return true;
}

template<typename Ntk>
bool check_fanouts( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the size function" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node function" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin function" );
  static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout function" );
  static_assert( has_foreach_co_v<Ntk>, "Ntk does not implement the foreach_co function" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size function" );

  using node   = typename Ntk::node;
  using signal = typename Ntk::signal;

  for ( auto i = 0u; i < ntk.size(); ++i )
  {
    uint32_t fanout_counter{0};

    bool fanout_error = false;
    ntk.foreach_fanout( i, [&]( node fo ){
      ++fanout_counter;

      /* check the fanins of the fanout  */
      bool found = false;
      ntk.foreach_fanin( fo, [&]( signal fi ){
        if ( ntk.get_node( fi ) == i )
        {
          found = true;
          return false;
        }
        return true;
      });

      /* if errors have been detected, then terminate */
      if ( !found )
      {
        fanout_error = true;
        return false;
      }

      return true;
    });

    /* report error */
    if ( fanout_error )
    {
      return false;
    }

     /* update the fanout counter by considering outputs */
    ntk.foreach_co( [&]( signal f ){
      if ( ntk.get_node( f ) == i )
      {
        ++fanout_counter;
      }
    });

    /* report error fanout_size does not match with the counter */
    if ( fanout_counter != ntk.fanout_size( i ) )
    {
      return false;
    }
  }

  return true;
}

template<typename Ntk, typename NtkWin>
bool check_window_equivalence( Ntk const& ntk, std::vector<typename Ntk::node> const& inputs, std::vector<typename Ntk::signal> const& outputs, std::vector<typename Ntk::node> const& gates, NtkWin const& win_opt )
{
  NtkWin win;
  clone_subnetwork( ntk, inputs, outputs, gates, win );
  topo_view topo_win{win_opt};
  assert( win.num_pis() == win_opt.num_pis() );
  assert( win.num_pos() == win_opt.num_pos() );

  default_simulator<kitty::dynamic_truth_table> sim( inputs.size() );
  auto const tts1 = simulate<kitty::dynamic_truth_table, NtkWin>( win, sim );
  auto const tts2 = simulate<kitty::dynamic_truth_table, topo_view<NtkWin>>( topo_win, sim );
  for ( auto i = 0u; i < tts1.size(); ++i )
  {
    if ( tts1[i] != tts2[i] )
    {
      return false;
    }
  }
  return true;
}

} /* namespace mockturtle */
