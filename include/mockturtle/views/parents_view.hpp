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
  \file parents_view.hpp
  \brief Implements parents for a network

  \author Heinz Riener
*/

#pragma once

#include <cstdint>
#include <vector>

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "immutable_view.hpp"

namespace mockturtle
{

/*! \brief Implements `foreach_parent` methods for networks.
 *
 * This view computes the parents of each node of the network.
 * It implements the network interface method `foreach_parent`.  The
 * parents are computed at construction and can be recomputed by
 * calling the `update` method.
 *
 * **Required network functions:**
 * - `foreach_node`
 * - `foreach_fanin`
 *
 */
template<typename Ntk, bool has_parents_interface = has_foreach_parent_v<Ntk>>
class parents_view
{
};

template<typename Ntk>
class parents_view<Ntk, true> : public Ntk
{
public:
  parents_view( Ntk const& ntk ) : Ntk( ntk )
  {
  }
};

template<typename Ntk>
class parents_view<Ntk, false> : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node    = typename Ntk::node;
  using signal  = typename Ntk::signal;

  parents_view( Ntk const& ntk ) : Ntk( ntk ), _parents( ntk )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );

    update();
  }

  template<typename Fn>
  void foreach_parent( node const& n, Fn&& fn ) const
  {
    detail::foreach_element( _parents[n].begin(), _parents[n].end(), fn );
  }

  void update()
  {
    compute_parents();
  }

private:
  void compute_parents()
  {
    _parents.reset();

    this->foreach_gate( [&]( auto const& n ){
        this->foreach_fanin( n, [&]( auto const& c ){
            auto& parents = _parents[ c ];
            if ( std::find( parents.begin(), parents.end(), n ) == parents.end() )
            {
              parents.push_back( n );
            }
          });
      });
  }

  node_map<std::vector<node>, Ntk> _parents;
};

template<class T>
parents_view(T const&) -> parents_view<T>;

} // namespace mockturtle
