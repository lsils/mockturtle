/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
  \file dont_touch_aig.hpp
  \brief AIG logic network with built-in "don't touch" mechanism

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "aig.hpp"

namespace mockturtle
{

/*! \brief AIG storage container

  AIGs have nodes with fan-in 2.  We split of one bit of the index pointer to
  store a complemented attribute.  Every node has 64-bit of additional data
  used for the following purposes:

  `data[0].h1`: Fan-out size (we use MSB to indicate whether a node is dead)
  `data[0].h2`: Application-specific value
  `data[1].h1`: Visited flag
  `data[1].h2`: &0x1: Is terminal node (PI or CI), &0x2: Is don't touch
*/
// Storage type is the same as aig_network, but usage of data[1].h2 is extended.

class dont_touch_aig_network : public aig_network
{
public:

#pragma region Primary I / O and constants
  signal create_pi()
  {
    const auto index = _storage->nodes.size();
    auto& node = _storage->nodes.emplace_back();
    node.children[0].data = node.children[1].data = _storage->inputs.size();
    node.data[1].h2 = 3; // mark as PI, and dont touch
    _storage->inputs.emplace_back( index );
    return { index, 0 };
  }

  bool is_ci( node const& n ) const
  {
    return _storage->nodes[n].data[1].h2 & 0x1;
  }

  bool is_pi( node const& n ) const
  {
    return _storage->nodes[n].data[1].h2 & 0x1 && !is_constant( n );
  }
#pragma endregion

#pragma region Create binary functions
  signal create_and_dont_touch( signal a, signal b )
  {
    /* order inputs */
    if ( a.index > b.index )
    {
      std::swap( a, b );
    }

    // don't check for trivial cases
    // don't check or add in strash
    // i.e. a new node is always created

    storage::element_type::node_type node;
    node.children[0] = a;
    node.children[1] = b;

    node.data[1].h2 = 2; // mark as dont touch

    const auto index = _storage->nodes.size();

    if ( index >= .9 * _storage->nodes.capacity() )
    {
      _storage->nodes.reserve( static_cast<uint64_t>( 3.1415f * index ) );
      _storage->hash.reserve( static_cast<uint64_t>( 3.1415f * index ) );
    }

    _storage->nodes.push_back( node );

    /* increase ref-count to children */
    _storage->nodes[a.index].data[0].h1++;
    _storage->nodes[b.index].data[0].h1++;

    for ( auto const& fn : _events->on_add )
    {
      ( *fn )( index );
    }

    return { index, 0 };
  }

  signal create_or_dont_touch( signal const& a, signal const& b )
  {
    return !create_and_dont_touch( !a, !b );
  }

  signal create_xor_dont_touch( signal const& a, signal const& b )
  {
    const auto fcompl = a.complement ^ b.complement;
    const auto c1 = create_and_dont_touch( +a, -b );
    const auto c2 = create_and_dont_touch( +b, -a );
    return create_and_dont_touch( !c1, !c2 ) ^ !fcompl;
  }
#pragma endregion

#pragma region Createy ternary functions
  signal create_maj_dont_touch( signal const& a, signal const& b, signal const& c )
  {
    return create_or_dont_touch( create_and_dont_touch( a, b ), create_and_dont_touch( c, create_or_dont_touch( a, b ) ) );
  }

  signal create_xor3_dont_touch( signal const& a, signal const& b, signal const& c )
  {
    return create_xor_dont_touch( create_xor_dont_touch( a, b ), c );
  }
#pragma endregion

#pragma region Create arbitrary functions
  signal clone_node( dont_touch_aig_network const& other, node const& source, std::vector<signal> const& children )
  {
    assert( children.size() == 2u );
    if ( other.is_dont_touch( source ) )
      return create_and_dont_touch( children[0u], children[1u] );
    else
      return create_and( children[0u], children[1u] );
  }

  signal clone_node( aig_network const& other, node const& source, std::vector<signal> const& children )
  {
    (void)other;
    (void)source;
    assert( children.size() == 2u );
    return create_and( children[0u], children[1u] );
  }
#pragma endregion

#pragma region Restructuring
  bool is_dont_touch( node const& n ) const
  {
    return _storage->nodes[n].data[1].h2 & 0x2;
  }

  bool is_dont_touch( signal const& f ) const
  {
    return is_dont_touch( get_node( f ) );
  }

  void take_out_node( node const& n )
  {
    /* we cannot delete CIs, constants, or already dead nodes */
    if ( n == 0 || is_dont_touch( n ) || is_ci( n ) || is_dead( n ) )
      return;

    /* delete the node (ignoring its current fanout_size) */
    auto& nobj = _storage->nodes[n];
    nobj.data[0].h1 = UINT32_C( 0x80000000 ); /* fanout size 0, but dead */
    _storage->hash.erase( nobj );

    for ( auto const& fn : _events->on_delete )
    {
      ( *fn )( n );
    }

    /* if the node has been deleted, then deref fanout_size of
       fanins and try to take them out if their fanout_size become 0 */
    for ( auto i = 0u; i < 2u; ++i )
    {
      if ( fanout_size( nobj.children[i].index ) == 0 )
      {
        continue;
      }
      if ( decr_fanout_size( nobj.children[i].index ) == 0 )
      {
        take_out_node( nobj.children[i].index );
      }
    }
  }

  bool is_fanin( node const& parent, node const& child ) const
  {
    auto& nobj = _storage->nodes[parent];
    return nobj.children[0].index == child || nobj.children[1].index == child;
  }

  void replace_in_node_no_restrash( node const& n, node const& old_node, signal new_signal )
  {
    if ( !is_fanin( n, old_node ) )
      return;
    auto& node = _storage->nodes[n];

    signal child0 = node.children[0];
    signal child1 = node.children[1];

    if ( node.children[0].index == old_node )
    {
      child0 = child0.complement ? !new_signal : new_signal;
      // increase the reference counter of the new signal
      _storage->nodes[new_signal.index].data[0].h1++;
    }
    if ( node.children[1].index == old_node )
    {
      child1 = child1.complement ? !new_signal : new_signal;
      // increase the reference counter of the new signal
      _storage->nodes[new_signal.index].data[0].h1++;
    }

    if ( child0.index > child1.index )
    {
      std::swap( child0, child1 );
    }

    // don't check for trivial cases
    // also don't check/erase/add in strash

    // remember before
    const auto old_child0 = signal{ node.children[0] };
    const auto old_child1 = signal{ node.children[1] };

    // update node
    node.children[0] = child0;
    node.children[1] = child1;

    for ( auto const& fn : _events->on_modified )
    {
      ( *fn )( n, { old_child0, old_child1 } );
    }
  }

  void substitute_node( node const& old_node, signal const& new_signal )
  {
    if ( is_dont_touch( old_node ) )
    {
      assert( false );
      return;
    }

    std::unordered_map<node, signal> old_to_new;
    std::stack<std::pair<node, signal>> to_substitute;
    to_substitute.push( { old_node, new_signal } );

    while ( !to_substitute.empty() )
    {
      const auto [_old, _curr] = to_substitute.top();
      to_substitute.pop();
      assert( !is_dont_touch( _old ) );

      signal _new = _curr;
      /* find the real new node */
      if ( is_dead( get_node( _new ) ) )
      {
        auto it = old_to_new.find( get_node( _new ) );
        while ( it != old_to_new.end() )
        {
          _new = is_complemented( _new ) ? create_not( it->second ) : it->second;
          it = old_to_new.find( get_node( _new ) );
        }
      }
      /* revive */
      if ( is_dead( get_node( _new ) ) )
      {
        revive_node( get_node( _new ) );
      }

      for ( auto idx = 1u; idx < _storage->nodes.size(); ++idx )
      {
        if ( is_ci( idx ) || is_dead( idx ) )
          continue; /* ignore CIs */

        if ( is_fanin( idx, _old ) )
        {
          if ( is_dont_touch( idx ) )
            replace_in_node_no_restrash( idx, _old, _new );
          else if ( const auto repl = replace_in_node( idx, _old, _new ); repl )
          {
            to_substitute.push( *repl );
          }
        }
      }

      /* check outputs */
      replace_in_outputs( _old, _new );

      /* recursively reset old node */
      if ( _old != _new.index )
      {
        old_to_new.insert( { _old, _new } );
        take_out_node( _old );
      }
    }
  }
#pragma endregion

#pragma region Structural properties
  auto size() const
  {
    return static_cast<uint32_t>( _storage->nodes.size() );
  }

  auto num_hashed_gates() const
  {
    return static_cast<uint32_t>( _storage->hash.size() );
  }

  uint32_t num_gates() const
  {
    uint32_t count = 0u;
    for ( auto idx = 1u; idx < _storage->nodes.size(); ++idx )
    {
      if ( !is_ci( idx ) && !is_dead( idx ) )
        ++count;
    }
    return count;
  }

  uint32_t num_dont_touch_gates() const
  {
    uint32_t count = 0u;
    for ( auto idx = 1u; idx < _storage->nodes.size(); ++idx )
    {
      if ( !is_ci( idx ) && !is_dead( idx ) && is_dont_touch( idx ) )
        ++count;
    }
    return count;
  }
#pragma endregion
};

} // namespace mockturtle
