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
  \file mapping_view.hpp
  \brief Implements mapping methods to create mapped networks

  \author Mathias Soeken
*/

#pragma once

#include <vector>

#include "../networks/detail/foreach.hpp"
#include "../traits.hpp"
#include "../views/immutable_view.hpp"

namespace mockturtle
{

template<typename Ntk>
class mapping_view : public immutable_view<Ntk>
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  /*! \brief Default constructor.
   *
   * Constructs mapping view on another network.
   */
  mapping_view( Ntk const& ntk ) : immutable_view<Ntk>( ntk )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );

    mappings.resize( ntk.size(), 0 );
  }

  bool has_mapping()
  {
    return true;
  }

  bool is_mapped( node const& n ) const
  {
    return mappings[this->node_to_index( n )] != 0;
  }

  void clear_mapping()
  {
    mappings.clear();
    mappings.resize( this->size(), 0 );
  }

  template<typename LeavesIterator>
  void add_to_mapping( node const& n, LeavesIterator begin, LeavesIterator end )
  {
    /* set starting index of leafs */
    mappings[this->node_to_index( n )] = mappings.size();

    /* insert number of leafs */
    mappings.push_back( std::distance( begin, end ) );

    /* insert leaf indexes */
    while ( begin != end )
    {
      mappings.push_back( this->node_to_index( *begin++ ) );
    }
  }

  void remove_from_mapping( node const& n )
  {
    mappings[this->node_to_index( n )] = 0;
  }

  template<typename Fn>
  void foreach_lut_fanin( node const& n, Fn&& fn ) const
  {
    const auto it = mappings.begin() + mappings[this->node_to_index( n )];
    const auto size = *it++;
    foreach_element_transform( it, it + size,
                               [&]( auto i ) { return this->index_to_node( i ); }, fn );
  }

private:
  std::vector<uint32_t> mappings;
};

} // namespace mockturtle
