/* mockturtle: C++ logic network library
 * Copyright (C) 2018  EPFL
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
  \file node_map.hpp
  \brief A vector-based map indexed by network nodes

  \author Mathias Soeken
*/

#pragma once

#include <vector>
#include <cassert>

#include "../traits.hpp"

namespace mockturtle
{

/*! \brief Associative container network nodes
 *
 * This container helps to store values associated to nodes in a network.  The
 * container is initialized with a network to derive the size according to the
 * number of nodes.  The container can be accessed via nodes, or indirectly
 * via signals, from which the corresponding node is derived.
 * 
 * The implementation uses a vector as underlying data structure which is
 * indexed by the node's index.
 * 
 * **Required network functions:**
 * - `size`
 * - `get_node`
 * - `node_to_index`
 * 
 * Example
 * 
   \verbatim embed:rst

   .. code-block:: c++

      aig_network aig = ...
      node_map<std::string, aig_network> node_names( aig );
      aig.foreach_node( [&]( auto n ) {
        node_names[n] = "some string";
      } );
   \endverbatim
 */
template<class T, class Ntk>
class node_map
{
public:
  using reference = typename std::vector<T>::reference;
  using const_reference = typename std::vector<T>::const_reference;
public:
  /*! \brief Default constructor. */
  explicit node_map( Ntk const& ntk )
      : ntk( ntk ),
        data( ntk.size() )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );
  }

  /*! \brief Constructor with default value.
   *
   * Initializes all values in the container to `init_value`.
   */
  node_map( Ntk const& ntk, T const& init_value )
      : ntk( ntk ),
        data( ntk.size(), init_value )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );
  }

  /*! \brief Mutable access to value by node. */
  reference operator[]( node<Ntk> const& n )
  {
    return data[ntk.node_to_index( n )];
  }

  /*! \brief Constant access to value by node. */
  const_reference operator[]( node<Ntk> const& n ) const
  {
    assert( ntk.node_to_index( n ) < data.size() && "index out of bounds" );
    return data[ntk.node_to_index( n )];
  }

  /*! \brief Mutable access to value by signal.
   *
   * This method derives the node from the signal.  If the node and signal type
   * are the same in the network implementation, this method is disabled.
   */
  template<typename _Ntk = Ntk, typename = std::enable_if_t<!std::is_same_v<signal<_Ntk>, node<_Ntk>>>>
  reference operator[]( signal<Ntk> const& f )
  {
    assert( ntk.node_to_index( ntk.get_node( f ) ) < data.size() && "index out of bounds" );
    return data[ntk.node_to_index( ntk.get_node( f ) )];
  }

  /*! \brief Constant access to value by signal.
   *
   * This method derives the node from the signal.  If the node and signal type
   * are the same in the network implementation, this method is disabled.
   */
  template<typename _Ntk = Ntk, typename = std::enable_if_t<!std::is_same_v<signal<_Ntk>, node<_Ntk>>>>
  const_reference operator[]( signal<Ntk> const& f ) const
  {
    assert( ntk.node_to_index( ntk.get_node( f ) ) < data.size() && "index out of bounds" );
    return data[ntk.node_to_index( ntk.get_node( f ) )];
  }

  /*! \brief Resets the size of the map.
   *
   * This function should be called, if the network changed in size.  Then, the
   * map is cleared, and resized to the current network's size.  All values are
   * initialized with `init_value`.
   * 
   * \param init_value Initialization value after resize
   */
  void reset( T const& init_value = {} )
  {
    data.clear();
    data.resize( ntk.size(), init_value );
  }

private:
  Ntk const& ntk;
  std::vector<T> data;
};

} /* namespace mockturtle */
