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
template<class Ntk, class NodeCostFn = and_cost<Ntk>, class cost_t = uint32_t, bool has_cost_interface = has_cost_v<Ntk>>
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
  using costfn_t = NodeCostFn;

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

    add_event = Ntk::events().register_add_event( [this]( auto const& n ) { on_add( n ); } );
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
    add_event = Ntk::events().register_add_event( [this]( auto const& n ) { on_add( n ); } );
  }

  /*! \brief Copy constructor. */
  explicit cost_view( cost_view<Ntk, NodeCostFn, cost_t, false> const& other )
    : Ntk( other )
    , _ps( other._ps )
    , _cost_val( other._cost_val )
    , _cost_fn( other._cost_fn )
  {
    add_event = Ntk::events().register_add_event( [this]( auto const& n ) { on_add( n ); } );
  }

  cost_view<Ntk, NodeCostFn, cost_t, false>& operator=( cost_view<Ntk, NodeCostFn, cost_t, false> const& other )
  {
    /* delete the event of this network */
    Ntk::events().release_add_event( add_event );

    /* update the base class */
    this->_storage = other._storage;
    this->_events = other._events;

    /* copy */
    _ps = other._ps;
    _cost_val = other._cost_val;
    _cost_fn = other._cost_fn;

    add_event = Ntk::events().register_add_event( [this]( auto const& n ) { on_add( n ); } );

    return *this;
  }

  ~cost_view()
  {
    Ntk::events().release_add_event( add_event );
  }

  /**
   * @brief Get the accumulate cost of a single node
   * 
   * @param n 
   * @return cost_t 
   */
  cost_t get_cost_val( node const& n ) const
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

  /**
   * @brief Get the cost of the whole network
   * 
   * @return cost_t 
   */
  uint32_t get_tmp_cost()
  {
    uint32_t _tmp_cost = 0u; /* must define the zero initialization */
    this->foreach_po( [&]( auto const& f ) {
      compute_cost( this->get_node( f ), _tmp_cost );
    } );
    return _tmp_cost;
  }

  /**
   * @brief Get the cost of one single node
   * 
   * @return cost_t 
   */
  uint32_t get_cost( node const& n )
  {
    uint32_t _c = 0u;
    this->incr_trav_id();
    compute_cost( n, _c );
    return _c;
  }

  /**
   * @brief Get the cost of one single node starting from divisors
   * 
   * @return cost_t 
   */
  uint32_t get_cost( node const& n, std::vector<signal> const& divs )
  {
    uint32_t _c = 0u;
    this->incr_trav_id();
    for ( signal s : divs )
    {
      this->set_visited( this->get_node( s ), this->trav_id() );
    }
    compute_cost( n, _c );
    return _c;
  }

  uint32_t get_dissipate_cost( node const& n ) const
  {
    uint32_t tmp_cost{};
    _cost_fn( *this, n, tmp_cost );
    return tmp_cost;
  }

  void set_cost_val( node const& n, cost_t cost_val )
  {
    _cost_val[n] = cost_val;
    this->set_visited( n, this->trav_id() );
  }

  void update_cost()
  {
    _cost_val.reset( cost_t{} );
    this->incr_trav_id();
    compute_cost();
  }

  void on_add( node const& n )
  {
    _cost_val.resize();

    std::vector<cost_t> fanin_costs;
    this->foreach_fanin( n, [&]( auto const& f ) {
      fanin_costs.emplace_back( _cost_val[this->get_node( f )] );
    } );
    _cost_val[n] = _cost_fn( *this, n, _cost, fanin_costs );
  }
  signal create_pi( cost_t pi_cost )
  {
    signal s = Ntk::create_pi();
    _cost_val.resize();
    set_cost_val( this->get_node( s ), pi_cost );
    return s;
  }

  /**
   * @brief Create a pi
   * this is required by the cleanup_dangling method
   * @return signal 
   */
  signal create_pi()
  {
    signal s = Ntk::create_pi();
    _cost_val.resize();
    return s;
  }
  signal create_pi( std::string name )
  {
    signal s = Ntk::create_pi( name );
    _cost_val.resize();
    return s;
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
  cost_t compute_cost( node const& n, uint32_t& _c )
  {
    if ( this->visited( n ) == this->trav_id() )
    {
      return _cost_val[n];
    }
    this->set_visited( n, this->trav_id() );
    if ( this->is_constant( n ) )
    {
      return _cost_val[n] = cost_t{};
    }
    if ( this->is_pi( n ) )
    {
      return _cost_val[n] = _cost_fn( *this, n, _cost );
    }
    std::vector<cost_t> fanin_costs;
    this->foreach_fanin( n, [&]( auto const& f ) {
      fanin_costs.emplace_back( compute_cost( this->get_node( f ), _c ) );
    } );
    return _cost_val[n] = _cost_fn( *this, n, _c, fanin_costs );
  }
  void compute_cost()
  {
    _cost = 0u; /* must define the zero initialization */
    this->foreach_po( [&]( auto const& f ) {
      compute_cost( this->get_node( f ), _cost );
    } );
  }

  cost_view_params _ps;
  node_map<cost_t, Ntk> _cost_val;
  uint32_t _cost;
  NodeCostFn _cost_fn;

  std::shared_ptr<typename network_events<Ntk>::add_event_type> add_event;

};

// template<class T, class NodeCostFn>
// cost_view( T const& )->cost_view<T, NodeCostFn>;

template<class T, class NodeCostFn>
cost_view( T const&, NodeCostFn const& )->cost_view<T, NodeCostFn, typename NodeCostFn::cost_t>;

} // namespace mockturtle