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
  \file depth_view.hpp
  \brief Implements depth and level for a network

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <vector>

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "immutable_view.hpp"

namespace mockturtle
{

/*! \brief Implements `depth` and `get_level` methods for networks.
 *
 * This view computes the level of each node and also the depth of
 * the network.  It implements the network interface methods
 * `get_level` and `depth`.  The levels are computed at construction
 * and can be recomputed by calling the `update` method.
 *
 * **Required network functions:**
 * - `size`
 * - `get_node`
 * - `clear_visited`
 * - `visited`
 * - `set_visited`
 * - `foreach_fanin`
 * - `foreach_po`
 *
 * Example
 *
   \verbatim embed:rst

   .. code-block:: c++

      // create network somehow
      aig_network aig = ...;

      // create a depth view on the network
      depth_view aig_depth{aig};

      // print depth
      std::cout << "Depth: " << aig_depth.depth() << "\n";
   \endverbatim
 */
template<typename Ntk, bool has_depth_interface = has_depth_v<Ntk> && has_level_v<Ntk>>
class depth_view
{
};

template<typename Ntk>
class depth_view<Ntk, true> : public Ntk
{
public:
  depth_view( Ntk const& ntk ) : Ntk( ntk )
  {
  }
};

template<typename Ntk>
class depth_view<Ntk, false> : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  depth_view( Ntk const& ntk ) : Ntk( ntk ), _levels( ntk )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_clear_visited_v<Ntk>, "Ntk does not implement the clear_visited method" );
    static_assert( has_visited_v<Ntk>, "Ntk does not implement the visited method" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );

    update();
  }

  uint32_t depth() const
  {
    return _depth;
  }

  uint32_t level( node const& n ) const
  {
    return _levels[n];
  }

  void update()
  {
    _levels.reset( 0 );
    compute_levels();
  }

private:
  uint32_t compute_levels( node const& n )
  {
    if ( this->visited( n ) )
      return _levels[n];

    if ( this->is_constant( n ) || this->is_pi( n ) )
    {
      this->set_visited( n, 1 );
      return _levels[n] = 0;
    }

    uint32_t level{0};
    this->foreach_fanin( n, [&]( auto const& f ) {
      level = std::max( level, compute_levels( this->get_node( f ) ) );
    } );

    this->set_visited( n, 1 );
    return _levels[n] = level + 1;
  }

  void compute_levels()
  {
    this->clear_visited();
    _depth = 0;
    this->foreach_po( [&]( auto const& f ) {
      _depth = std::max( _depth, compute_levels( this->get_node( f ) ) );
    } );
  }

  node_map<uint32_t, Ntk> _levels;
  uint32_t _depth;
};

template<class T>
depth_view(T const&) -> depth_view<T>;

} // namespace mockturtle
