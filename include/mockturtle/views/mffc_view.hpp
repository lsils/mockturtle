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
  \file cut_view.hpp
  \brief Implements an isolated view on a single cut in a network

  \author Mathias Soeken
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "../networks/detail/foreach.hpp"
#include "../traits.hpp"
#include "immutable_view.hpp"

#include <sparsepp/spp.h>

namespace mockturtle
{

/*! \brief Implements an isolated view on the MFFC of a node.
 *
 * The network is constructed from a given root node which is traversed towards
 * the primary inputs.  Nodes are collected as long they only fanout into nodes
 * which are already among the visited nodes.  Therefore the final view only
 * has outgoing edges to nodes not in the view from the given root node or from
 * the newly generated primary inputs.
 * 
 * The view reimplements the methods `size`, `num_pis`, `num_pos`, `foreach_pi`,
 * `foreach_po`, `foreach_node`, `foreach_gate`, `is_pi`, `node_to_index`, and
 * `index_to_node`.
 *
 * **Required network functions:**
 * - `get_node`
 * - `clear_values`
 * - `set_value`
 * - `decr_value`
 * - `value`
 * - `fanout_size`
 * - `foreach_node`
 * - `foreach_fanin`
 * - `is_constant`
 * - `node_to_index`
 */
template<typename Ntk>
class mffc_view : public immutable_view<Ntk>
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  explicit mffc_view( Ntk const& ntk, node const& root )
      : immutable_view<Ntk>( ntk ), _root( root )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
    static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
    static_assert( has_decr_value_v<Ntk>, "Ntk does not implement the decr_value method" );
    static_assert( has_value_v<Ntk>, "Ntk does not implement the value method" );
    static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
    static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );

    this->clear_values();
    Ntk::foreach_node( [&]( auto const& n ) {
      if ( this->is_constant( n ) )
      {
        _constants.push_back( n );
        _node_to_index.emplace( n, _node_to_index.size() );
      }
      this->set_value( n, Ntk::fanout_size( n ) );
    } );
    _num_constants = _constants.size();

    _leaves.reserve( 16 );
    _nodes.reserve( _limit );
    _inner.reserve( _limit );
    update();
  }

  inline auto size() const { return _num_constants + _num_leaves + _inner.size(); }
  inline auto num_pis() const { return _num_leaves; }
  inline auto num_pos() const { return _empty ? 0u : 1u; }
  inline auto num_gates() const { return _inner.size(); }

  inline bool is_pi( node const& pi ) const
  {
    return std::find( _leaves.begin(), _leaves.end(), pi ) != _leaves.end();
  }

  template<typename Fn>
  void foreach_pi( Fn&& fn ) const
  {
    detail::foreach_element( _leaves.begin(), _leaves.end(), fn );
  }

  template<typename Fn>
  void foreach_po( Fn&& fn ) const
  {
    if ( _empty ) return;
    std::vector<signal> signals( 1, this->make_signal( _root ) );
    detail::foreach_element( signals.begin(), signals.end(), fn );
  }

  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    detail::foreach_element( _inner.begin(), _inner.end(), fn );
  }

  template<typename Fn>
  void foreach_node( Fn&& fn ) const
  {
    detail::foreach_element( _constants.begin(), _constants.end(), fn );
    detail::foreach_element( _leaves.begin(), _leaves.end(), fn, _num_constants );
    detail::foreach_element( _inner.begin(), _inner.end(), fn, _num_constants + _num_leaves );
  }

  inline auto index_to_node( uint32_t index ) const
  {
    if ( index < _num_constants )
    {
      return _constants[index];
    }
    if ( index < _num_constants + _num_leaves )
    {
      return _leaves[index - _num_constants];
    }
    return _inner[index - _num_constants - _num_leaves];
  }

  inline auto node_to_index( node const& n ) const { return _node_to_index.at( n ); }

  void update()
  {
    _leaves.clear();
    _inner.clear();
    if ( collect( _root ) )
    {
      _empty = false;
      compute_sets();
    }
    else
    {
      _empty = true;
    }
    _num_leaves = _leaves.size();
  }

private:
  bool collect( node const& n )
  {
    if ( Ntk::is_constant( n ) || Ntk::is_pi( n ) )
    {
      return true;
    }

    /* we break from the loop over the fanins, if we find that _nodes contains
       too many nodes; the return value is stored in ret_val */
    bool ret_val = true;
    this->foreach_fanin( n, [&]( auto const& f ) {
      _nodes.push_back( this->get_node( f ) );
      if ( this->decr_value( this->get_node( f ) ) == 0 && ( _nodes.size() > _limit || !collect( this->get_node( f ) ) ) )
      {
        ret_val = false;
        return false;
      }
      return true;
    } );

    return ret_val;
  }

  void compute_sets()
  {
    //std::sort( _nodes.begin(), _nodes.end(),
    //           [&]( auto const& n1, auto const& n2 ) { return static_cast<Ntk*>( this )->node_to_index( n1 ) < static_cast<Ntk*>( this )->node_to_index( n2 ); } );
    std::sort( _nodes.begin(), _nodes.end() );

    for ( auto const& n : _nodes )
    {
      if ( Ntk::is_constant( n ) )
      {
        continue;
      }

      if ( this->value( n ) > 0 || Ntk::is_pi( n ) ) /* PI candidate */
      {
        if ( _leaves.empty() || _leaves.back() != n )
        {
          _leaves.push_back( n );
        }
      }
      else
      {
        if ( _inner.empty() || _inner.back() != n )
        {
          _inner.push_back( n );
        }
      }
    }

    for ( auto const& n : _leaves )
    {
      _node_to_index.emplace( n, _node_to_index.size() );
    }
    for ( auto const& n : _inner )
    {
      _node_to_index.emplace( n, _node_to_index.size() );
    }

    _inner.push_back( _root );
    _node_to_index.emplace( _root, _node_to_index.size() );
  }

public:
  std::vector<node> _nodes, _constants, _leaves, _inner;
  unsigned _num_constants{0}, _num_leaves{0};
  spp::sparse_hash_map<node, uint32_t> _node_to_index;
  node _root;
  bool _empty{true};
  uint32_t _limit{30};
};

template<class T>
mffc_view(T const&, typename T::node const&) -> mffc_view<T>;


} /* namespace mockturtle */
