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

#include "../traits.hpp"

namespace mockturtle
{

template<class T, class Ntk>
class node_map
{
public:
  explicit node_map( Ntk const& ntk )
      : ntk( ntk ),
        data( ntk.size() )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );
  }

  auto& operator[]( node<Ntk> const& n )
  {
    return data[ntk.node_to_index( n )];
  }

  auto const& operator[]( node<Ntk> const& n ) const
  {
    return data[ntk.node_to_index( n )];
  }

  template<typename _Ntk = Ntk, typename = std::enable_if_t<!std::is_same_v<signal<_Ntk>, node<_Ntk>>>>
  auto& operator[]( signal<Ntk> const& f )
  {
    return data[ntk.node_to_index( ntk.get_node( f ) )];
  }

  template<typename _Ntk = Ntk, typename = std::enable_if_t<!std::is_same_v<signal<_Ntk>, node<_Ntk>>>>
  auto const& operator[]( signal<Ntk> const& f ) const
  {
    return data[ntk.node_to_index( ntk.get_node( f ) )];
  }

private:
  Ntk const& ntk;
  std::vector<T> data;
};

} /* namespace mockturtle */
