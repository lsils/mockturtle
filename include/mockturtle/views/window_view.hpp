/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
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
  \file window_view.hpp
  \brief Implements an isolated view on a window in a network

  \author Heinz Riener
*/

#pragma once

#include "../traits.hpp"
#include "../networks/detail/foreach.hpp"
#include "immutable_view.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

namespace mockturtle
{

/*! \brief Identify outputs using reference counting
 *
 * Identify outputs using a reference counting approach.  The
 * algorithm counts the references of the fanins of all nodes and
 * compares them with the fanout_sizes of the respective nodes.  If
 * reference count and fanout_size do not match, then the node is
 * references outside of the node set and the respective is identified
 * as an output.
 *
 * \param inputs Inputs of a window
 * \param nodes Inner nodes of a window (i.e., the intersection of
 *              inputs and nodes is assumed to be empty)
 * \param refs Reference counters (in the size of the network and
 *             initialized to 0)
 * \return Output signals of the window
 */
template<typename Ntk>
std::vector<typename Ntk::signal> find_outputs( Ntk const& ntk,
                                                std::vector<typename Ntk::node> const& inputs,
                                                std::vector<typename Ntk::node> const& nodes,
                                                std::vector<uint32_t>& refs )
{
  using signal = typename Ntk::signal;
  std::vector<signal> outputs;

  /* create a new traversal ID */
  ntk.incr_trav_id();

  /* mark the inputs visited */
  for ( auto const& i : inputs )
  {
    ntk.set_visited( i, ntk.trav_id() );
  }

  /* reference fanins of nodes */
  for ( auto const& n : nodes )
  {
    if ( ntk.visited( n ) == ntk.trav_id() )
    {
      continue;
    }

    assert( !ntk.is_constant( n ) && !ntk.is_pi( n ) );
    ntk.foreach_fanin( n, [&]( signal const& fi ){
      refs[ntk.get_node( fi )] += 1;
    });
  }

  /* if the fanout_size of a node does not match the reference count,
     the node has fanout outside of the window is an output */
  for ( const auto& n : nodes )
  {
    if ( ntk.visited( n ) == ntk.trav_id() )
    {
      continue;
    }

    if ( ntk.fanout_size( n ) != refs[n] )
    {
      outputs.emplace_back( ntk.make_signal( n ) );
    }
  }

  /* dereference fanins of nodes */
  for ( auto const& n : nodes )
  {
    if ( ntk.visited( n ) == ntk.trav_id() )
    {
      continue;
    }

    assert( !ntk.is_constant( n ) && !ntk.is_pi( n ) );
    ntk.foreach_fanin( n, [&]( signal const& fi ){
      refs[ntk.get_node( fi )] -= 1;
    });
  }

  return outputs;
}

/*! \brief Implements an isolated view on a window in a network.
 *
 * This view creates a network from a window in a large network.  The
 * window is specified by three parameters:
 *   1.) `inputs` are the common support of all window nodes, they do
 *       not overlap with `gates` (i.e., the intersection of `inputs` and
 *       `gates` is the empty set).
 *   2.) `gates` are the nodes in the window, supported by the
 *       `inputs` (i.e., `gates` are in the transitive fanout of the
 *       `inputs`,
 *   3.) `outputs` are signals (regular or complemented nodes)
 *        pointing to nodes in `gates` or `inputs`.  Not all fanouts
 *        of an output node are already part of the window.
 *
 * The second parameter `gates` has to be passed and is not
 * automatically computed (for example in contrast to `cut_view`),
 * because there are different strategies to construct a window from a
 * support set.  The outputs could be automatically computed.
 *
 * The window_view implements one new API method:
 *   1.) `belongs_to_window`: takes a node as input and returns true
 *       if and only if this node is a constant, an input, or an inner
 *       node of the window
 */
template<typename Ntk>
class new_window_view : public immutable_view<Ntk>
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  explicit new_window_view( Ntk const& ntk, std::vector<node> const& inputs, std::vector<signal> const& outputs, std::vector<node> const& gates )
    : immutable_view<Ntk>( ntk )
    , _inputs( inputs )
    , _outputs( outputs )
  {
    construct( inputs, gates );
  }

  explicit new_window_view( Ntk const& ntk, std::vector<node> const& inputs, std::vector<node> const& outputs, std::vector<node> const& gates )
    : immutable_view<Ntk>( ntk )
    , _inputs( inputs )
  {
    construct( inputs, gates );

    /* convert output nodes to signals */
    std::transform( std::begin( outputs ), std::end( outputs ), std::back_inserter( _outputs ),
                    [this]( node const& n ){
                      return this->make_signal( n );
                    });
  }

#pragma region Window
  inline bool belongs_to_window( node const& n ) const
  {
    return std::find( std::begin( _nodes ), std::end( _nodes ), n ) != std::end( _nodes );
  }
#pragma endregion

#pragma region Structural properties
  inline uint32_t size() const
  {
    return static_cast<uint32_t>( _nodes.size() );
  }

  inline uint32_t num_cis() const
  {
    return num_pis();
  }

  inline uint32_t num_cos() const
  {
    return num_pos();
  }

  inline uint32_t num_latches() const
  {
    return 0u;
  }

  inline uint32_t num_pis() const
  {
    return static_cast<uint32_t>( _inputs.size() );
  }

  inline uint32_t num_pos() const
  {
    return static_cast<uint32_t>( _outputs.size() );
  }

  inline uint32_t num_registers() const
  {
    return 0u;
  }

  inline uint32_t num_gates() const
  {
    return static_cast<uint32_t>( _nodes.size() - _inputs.size() - 1u );
  }

  inline uint32_t node_to_index( node const& n ) const
  {
    return _node_to_index.at( n );
  }

  inline node index_to_node( uint32_t index ) const
  {
    return _nodes[index];
  }

  inline bool is_pi( node const& n ) const
  {
    return std::find( std::begin( _inputs ), std::end( _inputs ), n ) != std::end( _inputs );
  }

  inline bool is_ci( node const& n ) const
  {
    return is_pi( n );
  }

#pragma endregion

#pragma region Node and signal iterators
  template<typename Fn>
  void foreach_pi( Fn&& fn ) const
  {
    detail::foreach_element( std::begin( _inputs ), std::end( _inputs ), fn );
  }

  template<typename Fn>
  void foreach_po( Fn&& fn ) const
  {
    detail::foreach_element( std::begin( _outputs ), std::end( _outputs ), fn );
  }

  template<typename Fn>
  void foreach_ci( Fn&& fn ) const
  {
    foreach_pi( fn );
  }

  template<typename Fn>
  void foreach_co( Fn&& fn ) const
  {
    foreach_po( fn );
  }

  template<typename Fn>
  void foreach_ro( Fn&& fn ) const
  {
    (void)fn;
  }

  template<typename Fn>
  void foreach_ri( Fn&& fn ) const
  {
    (void)fn;
  }

  template<typename Fn>
  void foreach_register( Fn&& fn ) const
  {
    (void)fn;
  }

  template<typename Fn>
  void foreach_node( Fn&& fn ) const
  {
    detail::foreach_element( std::begin( _nodes ), std::end( _nodes ), fn );
  }

  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    detail::foreach_element( std::begin( _nodes ) + 1u + _inputs.size(), std::end( _nodes ), fn );
  }

  template<typename Fn>
  void foreach_fanin( node const& n, Fn&& fn ) const
  {
    /* constants and inputs do not have fanins */
    if ( this->is_constant( n ) ||
         std::find( std::begin( _inputs ), std::end( _inputs ), n ) != std::end( _inputs ) )
    {
      return;
    }

    /* if it's not a window input, the node has to be a window node */
    assert( std::find( std::begin( _nodes ) + 1 + _inputs.size(), std::end( _nodes ), n ) != std::end( _nodes ) );
    immutable_view<Ntk>::foreach_fanin( n, fn );
  }
#pragma endregion

protected:
  void construct( std::vector<node> const& inputs, std::vector<node> const& gates )
  {
    /* copy constant to nodes */
    _nodes.emplace_back( this->get_node( this->get_constant( false ) ) );

    /* copy inputs to nodes */
    std::copy( std::begin( inputs ), std::end( inputs ), std::back_inserter( _nodes ) );

    /* copy gates to nodes */
    std::copy( std::begin( gates ), std::end( gates ), std::back_inserter( _nodes ) );

    /* create a mapping from node id (index in the original network) to window index */
    for ( uint32_t index = 0; index < _nodes.size(); ++index )
    {
      _node_to_index[_nodes.at( index )] = index;
    }
  }

protected:
  std::vector<node> _inputs;
  std::vector<signal> _outputs;
  std::vector<node> _nodes;
  std::unordered_map<node, uint32_t> _node_to_index;
}; /* new_window_view */

/*! \brief Implements an isolated view on a window in a network.
 *
 */
template<typename Ntk>
class window_view : public immutable_view<Ntk>
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  explicit window_view( Ntk const& ntk, std::vector<node> const& leaves, std::vector<node> const& pivots, bool auto_extend = true )
    : immutable_view<Ntk>( ntk )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_visited_v<Ntk>, "Ntk does not implement the visited method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
    static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
    static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
    static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method" );

    this->incr_trav_id();

    /* constants */
    add_node( this->get_node( this->get_constant( false ) ) );
    this->set_visited( this->get_node( this->get_constant( false ) ), this->trav_id() );
    if ( this->get_node( this->get_constant( true ) ) != this->get_node( this->get_constant( false ) ) )
    {
      add_node( this->get_node( this->get_constant( true ) ) );
      this->set_visited( this->get_node( this->get_constant( true ) ), this->trav_id() );
      ++_num_constants;
    }

    /* primary inputs */
    for ( auto const& leaf : leaves )
    {
      if ( this->visited( leaf ) == this->trav_id() )
        continue;
      this->set_visited( leaf, this->trav_id() );

      add_node( leaf );
      ++_num_leaves;
    }

    for ( auto const& p : pivots )
    {
      traverse( p );
    }

    if ( auto_extend )
    {
      extend( ntk );
    }

    add_roots( ntk );
  }

  inline auto size() const { return _nodes.size(); }
  inline auto num_pis() const { return _num_leaves; }
  inline auto num_pos() const { return _roots.size(); }
  inline auto num_gates() const { return _nodes.size() - _num_leaves - _num_constants; }

  inline auto node_to_index( const node& n ) const { return _node_to_index.at( n ); }
  inline auto index_to_node( uint32_t index ) const { return _nodes[index]; }

  inline bool is_pi( node const& pi ) const
  {
    const auto beg = _nodes.begin() + _num_constants;
    return std::find( beg, beg + _num_leaves, pi ) != beg + _num_leaves;
  }

  template<typename Fn>
  void foreach_pi( Fn&& fn ) const
  {
    detail::foreach_element( _nodes.begin() + _num_constants, _nodes.begin() + _num_constants + _num_leaves, fn );
  }

  template<typename Fn>
  void foreach_po( Fn&& fn ) const
  {
    detail::foreach_element( _roots.begin(), _roots.end(), fn );
  }

  template<typename Fn>
  void foreach_node( Fn&& fn ) const
  {
    detail::foreach_element( _nodes.begin(), _nodes.end(), fn );
  }

  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    detail::foreach_element( _nodes.begin() + _num_constants + _num_leaves, _nodes.end(), fn );
  }

  uint32_t fanout_size( node const& n ) const
  {
    return _fanout_size.at( node_to_index( n ) );
  }

private:
  void add_node( node const& n )
  {
    _node_to_index[n] = static_cast<unsigned int>( _nodes.size() );
    _nodes.push_back( n );

    auto fanout_counter = 0;
    this->foreach_fanin( n, [&]( const auto& f ) {
        if ( std::find( _nodes.begin(), _nodes.end(), this->get_node( f ) ) != _nodes.end() )
        {
          fanout_counter++;
        }
      });
    _fanout_size.push_back( fanout_counter );
  }

  void traverse( node const& n )
  {
    if ( this->visited( n ) == this->trav_id() )
      return;
    this->set_visited( n, this->trav_id() );

    this->foreach_fanin( n, [&]( const auto& f ) {
      traverse( this->get_node( f ) );
    } );

    add_node( n );
  }

  void extend( Ntk const& ntk )
  {
    std::set<node> new_nodes;
    do
    {
      new_nodes.clear();
      for ( const auto& n : _nodes )
      {
        ntk.foreach_fanout( n, [&]( auto const& p ){
            /* skip node if it is already in _nodes */
            if ( std::find( _nodes.begin(), _nodes.end(), p ) != _nodes.end() ) return;

            auto all_children_in_nodes = true;
            ntk.foreach_fanin( p, [&]( auto const& s ){
                auto const& child = ntk.get_node( s );
                if ( std::find( _nodes.begin(), _nodes.end(), child ) == _nodes.end() )
                {
                  all_children_in_nodes = false;
                  return false;
                }
                return true;
              });

            if ( all_children_in_nodes )
            {
              assert( p != 0 );
              assert( !is_pi( p ) );
              new_nodes.insert( p );
            }
          });
      }

      for ( const auto& n : new_nodes )
      {
        add_node( n );
      }
    } while ( !new_nodes.empty() );
  }

  void add_roots( Ntk const& ntk )
  {
    /* compute po nodes */
    std::vector<node> pos;
    ntk.foreach_po( [&]( auto const& s ){
        pos.push_back( ntk.get_node( s ) );
      });

    /* compute window outputs */
    for ( const auto& n : _nodes )
    {
      // if ( ntk.is_constant( n ) || ntk.is_pi( n ) ) continue;

      if ( std::find( pos.begin(), pos.end(), n ) != pos.end() )
      {
        auto s = this->make_signal( n );
        if ( std::find( _roots.begin(), _roots.end(), s ) == _roots.end() )
        {
          _roots.push_back( s );
        }
        continue;
      }

      ntk.foreach_fanout( n, [&]( auto const& p ){
          if ( std::find( _nodes.begin(), _nodes.end(), p ) == _nodes.end() )
          {
            auto s = this->make_signal( n );
            if ( std::find( _roots.begin(), _roots.end(), s ) == _roots.end() )
            {
              _roots.push_back( s );
              return false;
            }
          }
          return true;
      });
    }
  }

public:
  unsigned _num_constants{1};
  unsigned _num_leaves{0};
  std::vector<node> _nodes;
  std::unordered_map<node, uint32_t> _node_to_index;
  std::vector<signal> _roots;
  std::vector<unsigned> _fanout_size;
};

} /* namespace mockturtle */
