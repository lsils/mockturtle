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
  \file cofactor.hpp
  \brief Cofactor networks with respect to an input.

  \author Bruno Schmitt
*/

#pragma once

#include <iostream>
#include <type_traits>
#include <vector>
#include <fmt/format.h>

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

/*! \brief Factors a network with respect to a primary input.
 *
 * This method factors a network with respect pi.  It returns two networks: one
 * corresponding to when the pi is 0, and the other to when its 1.
 *
 * **Required network functions:**
 * - `get_node`
 * - `node_to_index`
 * - `get_constant`
 * - `create_pi`
 * - `create_po`
 * - `create_not`
 * - `is_complemented`
 * - `foreach_node`
 * - `foreach_pi`
 * - `foreach_po`
 * - `clone_node`
 * - `is_pi`
 * - `is_constant`
 *
 * \param ntk Logic network
 * \param pi Primary input
 */
template<class Ntk>
std::pair<Ntk, Ntk> factoring( Ntk const& ntk, signal<Ntk> const& pi )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
  static_assert( has_clone_node_v<Ntk>, "Ntk does not implement the clone_node method" );
  static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi method" );
  static_assert( has_create_po_v<Ntk>, "Ntk does not implement the create_po method" );
  static_assert( has_create_not_v<Ntk>, "Ntk does not implement the create_not method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  assert( ntk.is_pi( ntk.get_node( pi ) ) );

  Ntk f0_dest;
  Ntk f1_dest;
  std::vector<signal<Ntk>> dest_pis;
  ntk.foreach_pi( [&]( auto )
                  {
    dest_pis.push_back( f0_dest.create_pi() );
    f1_dest.create_pi(); } );

  node_map<signal<Ntk>, Ntk> orig_to_f0( ntk );
  node_map<signal<Ntk>, Ntk> orig_to_f1( ntk );
  orig_to_f0[ntk.get_constant( false )] = f0_dest.get_constant( false );
  orig_to_f1[ntk.get_constant( false )] = f1_dest.get_constant( false );

  /* create inputs in same order */
  auto it = dest_pis.begin();
  ntk.foreach_pi( [&]( auto node )
                  {
    orig_to_f0[node] = *it;
    orig_to_f1[node] = *it++; } );
  assert( it == dest_pis.end() );

  /* create inputs in same order */
  orig_to_f0[pi] = ntk.get_constant( false );
  orig_to_f1[pi] = ntk.get_constant( true );

  /* foreach node in topological order */
  topo_view topo( ntk );
  topo.foreach_node( [&]( auto node )
                     {
    if ( ntk.is_constant( node ) || ntk.is_pi( node ) )
      return;

    /* collect children */
    std::vector<signal<Ntk>> f0_children;
    std::vector<signal<Ntk>> f1_children;
    ntk.foreach_fanin( node, [&]( auto child, auto ) {
      const auto f0_child = orig_to_f0[child];
      const auto f1_child = orig_to_f1[child];
      if ( ntk.is_complemented( child ) )
      {
        f0_children.push_back( f0_dest.create_not( f0_child ) );
        f1_children.push_back( f1_dest.create_not( f1_child ) );
      }
      else
      {
        f0_children.push_back( f0_child );
        f1_children.push_back( f1_child );
      }
    } );

    orig_to_f0[node] = f0_dest.clone_node( ntk, node, f0_children );
    orig_to_f1[node] = f1_dest.clone_node( ntk, node, f1_children ); } );

  /* create outputs in same order */
  ntk.foreach_po( [&]( auto po )
                  {
    const auto f0_po = orig_to_f0[po];
    const auto f1_po = orig_to_f1[po];
    if ( ntk.is_complemented( po ) )
    {
      f0_dest.create_po( f0_dest.create_not( f0_po ) );
      f1_dest.create_po( f1_dest.create_not( f1_po ) );
    }
    else
    {
      f0_dest.create_po( f0_po );
      f1_dest.create_po( f1_po );
    } } );

  return { f0_dest, f1_dest };
}

namespace detail
{

template<class Ntk>
signal<Ntk> select_variable( Ntk const& ntk, std::vector<signal<Ntk>>& pis )
{
  uint32_t selected_variable_idx = 0;
  auto min_num_gates = std::numeric_limits<uint32_t>::max();
  for ( uint32_t i = 0; i < pis.size(); ++i )
  {
    auto [f0, f1] = factoring( ntk, pis.at( i ) );
    uint32_t num_gates = f0.num_gates() + f1.num_gates();
    if ( min_num_gates > num_gates )
    {
      selected_variable_idx = i;
      min_num_gates = num_gates;
    }
  }
  signal<Ntk> selected_variable = pis.at( selected_variable_idx );
  pis.erase( pis.begin() + selected_variable_idx );
  return selected_variable;
}

template<class Ntk>
void factoring_rec( int32_t begin, int32_t end, std::vector<signal<Ntk>> pis, std::vector<std::vector<signal<Ntk>>>& cubes, std::vector<Ntk>& factored_networks )
{
  if ( ( end - begin ) < 1 )
    return;
  int32_t middle = ( begin + end ) / 2;
  auto ntk = factored_networks.at( begin );
  signal<Ntk> selected_variable = select_variable( factored_networks.at( begin ), pis );
  for ( int32_t i = 0; i <= ( middle - begin ); ++i )
  {
    auto [f0, f1] = factoring( ntk, selected_variable );
    factored_networks.at( begin + i ) = f0.clone();
    cubes.at( begin + i ).push_back( ntk.create_not( selected_variable ) );
    factored_networks.at( middle + 1 + i ) = f1.clone();
    cubes.at( middle + 1 + i ).push_back( selected_variable );
  }
  factoring_rec( begin, middle, pis, cubes, factored_networks );
  factoring_rec( middle + 1, end, pis, cubes, factored_networks );
}

} // namespace detail

/*! \brief Factors a network with respect to `n` primary inputs.
 *
 * This method factors a network with respect to a single-cube factor with of 
 * `n` primary inputs.  It returns 2 ** n networks and their respective
 * single-cube factors.
 *
 * **Required network functions:**
 * - `clone`
 * - `get_node`
 * - `node_to_index`
 * - `get_constant`
 * - `create_pi`
 * - `create_po`
 * - `create_not`
 * - `is_complemented`
 * - `foreach_node`
 * - `foreach_pi`
 * - `foreach_po`
 * - `clone_node`
 * - `is_pi`
 * - `is_constant`
 *
 * \param ntk Logic network
 * \param num_vars Number of primary inputs in the single-cube factor
 */
template<class Ntk>
std::pair<std::vector<std::vector<signal<Ntk>>>, std::vector<Ntk>>
factoring( Ntk const& ntk, uint32_t num_vars )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( is_clonable_v<Ntk>, "Ntk is not clonable" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
  static_assert( has_clone_node_v<Ntk>, "Ntk does not implement the clone_node method" );
  static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi method" );
  static_assert( has_create_po_v<Ntk>, "Ntk does not implement the create_po method" );
  static_assert( has_create_not_v<Ntk>, "Ntk does not implement the create_not method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  
  using Cube = std::vector<signal<Ntk>>;
  std::vector<Cube> cubes;
  std::vector<Ntk> factored_networks;
  cubes.resize( 1 << num_vars );
  factored_networks.resize( 1 << num_vars );

  factored_networks.at( 0 ) = ntk.clone();
  std::vector<signal<Ntk>> pis;
  ntk.foreach_pi( [&]( auto node ) { pis.push_back( ntk.make_signal( node ) ); } );
  detail::factoring_rec( 0, factored_networks.size() - 1, pis, cubes, factored_networks );
  return { cubes, factored_networks };
}

} // namespace mockturtle