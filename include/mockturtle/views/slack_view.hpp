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
  \file slack_view.hpp
  \brief Implements require time for a network

  \author Hanyu
  \author Mathias Soeken
*/

#pragma once

#include "../traits.hpp"
#include "../utils/cost_functions.hpp"
#include "../utils/node_map.hpp"
#include "../networks/events.hpp"
#include "immutable_view.hpp"

#include <cstdint>
#include <vector>

namespace mockturtle
{

struct slack_view_params
{
  /*! \brief Take complemented edges into account for depth computation. */
  bool count_complements{false};

  /*! \brief Whether POs have costs. */
  bool po_cost{false};
};

/*! \brief Implements `slack` and `required` methods for networks.

 * **Required network functions:**
 * - `size`
 * - `get_node`
 * - `visited`
 * - `set_visited`
 * - `foreach_fanin`
 * - `foreach_po`
 * - `foreach_pi`
 *
 * Example
 *
   \verbatim embed:rst

   .. code-block:: c++

      // create network somehow
      aig_network aig = ...;

      // create a depth view on the network
      slack_view aig_depth{aig};

      // print depth
      std::cout << "Depth: " << aig_depth.depth() << "\n";
   \endverbatim
 */
template<class Ntk, class NodeCostFn = unit_cost<Ntk>, bool has_slack_interface = has_slack_v<Ntk>>
class slack_view
{
};

template<class Ntk, class NodeCostFn>
class slack_view<Ntk, NodeCostFn, true> : public Ntk
{
public:
  slack_view( Ntk const& ntk, slack_view_params const& ps = {} ) : Ntk( ntk )
  {
    (void)ps;
  }
};

template<class Ntk, class NodeCostFn>
class slack_view<Ntk, NodeCostFn, false> : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit slack_view( NodeCostFn const& cost_fn = {}, slack_view_params const& ps = {} )
    : Ntk()
    , _ps( ps )
    , _required( *this )
    , _cost_fn( cost_fn )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
    static_assert( has_visited_v<Ntk>, "Ntk does not implement the visited method" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method" );
  }

  /*! \brief Standard constructor.
   *
   * \param ntk Base network
   */
  explicit slack_view( Ntk const& ntk, NodeCostFn const& cost_fn = {}, slack_view_params const& ps = {} )
    : Ntk( ntk )
    , _ps( ps )
    , _required( ntk )
    , _cost_fn( cost_fn )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
    static_assert( has_visited_v<Ntk>, "Ntk does not implement the visited method" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method" );

    update_requires();
  }

  /*! \brief Copy constructor. */
  explicit slack_view( slack_view<Ntk, NodeCostFn, false> const& other )
    : Ntk( other )
    , _ps( other._ps )
    , _required( other._required )
    , _cost_fn( other._cost_fn )
  {
  }

  slack_view<Ntk, NodeCostFn, false>& operator=( slack_view<Ntk, NodeCostFn, false> const& other )
  {
    /* update the base class */
    this->_storage = other._storage;

    /* copy */
    _ps = other._ps;
    _required = other._required;
    _cost_fn = other._cost_fn;

    return *this;
  }

  uint32_t required() const
  {
    return _required;
  }

  void set_required( node const& n, uint32_t required )
  {
    _required[n] = required;
  }

  void update_requires()
  {
    _required.reset( 0 );
    this->incr_trav_id();
    compute_requires();
  }

  void create_po( signal const& f )
  {
    Ntk::create_po( f );
  }

private:
  uint32_t compute_requires( node const& n )
  {
    if ( this->visited( n ) == this->trav_id() )
    {
      return _required[n];
    }
    this->set_visited( n, this->trav_id() );

    if ( this->is_constant( n ) )
    {
      return _required[n] = 0;
    }
    if ( this->is_pi( n ) )
    {
      assert( !_ps.po_cost || _cost_fn( *this, n ) >= 1 );
      return _required[n] = _ps.po_cost ? _cost_fn( *this, n ) : 1;
    }

    uint32_t require{0};
    this->foreach_fanout( n, [&]( auto const& f ) {
      auto crequire = compute_requires( f ) + _cost_fn( *this, f );
      require = std::max( require, crequire );
    } );

    return _required[n] = require;
  }

  void compute_requires()
  {
    this->foreach_pi( [&]( auto const& f ) {
        compute_requires( f );
    } );
  }

  slack_view_params _ps;
  node_map<uint32_t, Ntk> _required;
  NodeCostFn _cost_fn;
};

template<class T>
slack_view( T const& )->slack_view<T>;

template<class T, class NodeCostFn = unit_cost<T>>
slack_view( T const&, NodeCostFn const&, slack_view_params const& )->slack_view<T, NodeCostFn>;

} // namespace mockturtle
