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
  \file cost_view.hpp
  \brief Implements get_cost for a network

  \author Hanyu Wang
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

struct cost_view_params
{
  /*! \brief Take complemented edges into account for depth computation. */
  bool count_complements{false};
};

/*! \brief Implements `get_cost` methods for networks.
 */
template<class Ntk, class NodeCostFn, class cost_t, bool has_cost_interface = has_cost_v<Ntk>>
class cost_view
{
};

template<class Ntk, class NodeCostFn, class cost_t>
class cost_view<Ntk, NodeCostFn, cost_t, true> : public Ntk
{
public:
  cost_view( Ntk const& ntk, cost_view_params const& ps = {} ) : Ntk( ntk )
  {
    (void)ps;
  }
};

template<class Ntk, class NodeCostFn, class cost_t>
class cost_view<Ntk, NodeCostFn, cost_t, false> : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit cost_view( NodeCostFn const& cost_fn = {}, cost_view_params const& ps = {} )
    : Ntk()
    , _ps( ps )
    , _cost_val( *this )
    , _cost_fn( cost_fn )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
    static_assert( has_visited_v<Ntk>, "Ntk does not implement the visited method" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
  }

  /*! \brief Standard constructor.
   *
   * \param ntk Base network
   */
  explicit cost_view( Ntk const& ntk, NodeCostFn const& cost_fn = {}, cost_view_params const& ps = {} )
    : Ntk( ntk )
    , _ps( ps )
    , _cost_val( ntk )
    , _cost_fn( cost_fn )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
    static_assert( has_visited_v<Ntk>, "Ntk does not implement the visited method" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );

    update_cost();
  }

  /*! \brief Copy constructor. */
  explicit cost_view( cost_view<Ntk, NodeCostFn, cost_t, false> const& other )
    : Ntk( other )
    , _ps( other._ps )
    , _cost_val( other._cost_val )
    , _cost_fn( other._cost_fn )
  {
  }

  cost_view<Ntk, NodeCostFn, cost_t, false>& operator=( cost_view<Ntk, NodeCostFn, cost_t, false> const& other )
  {
    /* update the base class */
    this->_storage = other._storage;
    this->_events = other._events;

    /* copy */
    _ps = other._ps;
    _cost_val = other._cost_val;
    _cost_fn = other._cost_fn;

    return *this;
  }

  /**
   * @brief Get the dissipate cost of a single node
   * 
   * @param n 
   * @return cost_t 
   */
  cost_t get_dissipate_cost( node const& n ) const
  {
    return _cost_val[n];
  }

  /**
   * @brief Get the cost of the whole network
   * 
   * @return cost_t 
   */
  uint32_t get_cost() const
  {
    return _cost;
  }

  uint32_t get_accumulate_cost( node const& n ) const
  {
    uint32_t tmp_cost{};
    _cost_fn( *this, n, tmp_cost );
    return tmp_cost;
  }

  void set_cost_val( node const& n, uint32_t cost_val )
  {
    _cost_val[n] = cost_val;
  }

  void update_cost()
  {
    _cost_val.reset( 0 );
    this->incr_trav_id();
    compute_cost();
  }

  void create_po( signal const& f )
  {
    Ntk::create_po( f );
  }
  void create_po( signal const& f, std::string name )
  {
    Ntk::create_po( f, name );
  }
private:
  cost_t compute_cost( node const& n )
  {
    if ( this->visited( n ) == this->trav_id() )
    {
      return _cost_val[n];
    }
    this->set_visited( n, this->trav_id() );
    if ( this->is_constant( n ) )
    {
      return _cost_val[n] = 0;
    }
    if ( this->is_pi( n ) )
    {
      return _cost_val[n] = _cost_fn( *this, n, _cost );
    }
    this->foreach_fanin( n, [&]( auto const& f ) {
      compute_cost( this->get_node( f ) );
    } );
    return _cost_val[n] = _cost_fn( *this, n, _cost );
  }

  void compute_cost()
  {
    _cost = cost_t{}; /* must define the zero initialization */
    this->foreach_po( [&]( auto const& f ) {
      compute_cost( this->get_node( f ) );
    } );
  }

  cost_view_params _ps;
  node_map<cost_t, Ntk> _cost_val;
  uint32_t _cost;
  NodeCostFn _cost_fn;
};

// template<class T>
// cost_view( T const& )->cost_view<T>;

template<class T, class NodeCostFn>
cost_view( T const&, NodeCostFn const& )->cost_view<T, NodeCostFn, cost<NodeCostFn>>;

} // namespace mockturtle