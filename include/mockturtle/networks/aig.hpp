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
  \file aig.hpp
  \brief AIG logic network implementation

  \author Mathias Soeken
  \author Heinz Riener
*/

#pragma once

#include <memory>
#include <string>

#include <ez/direct_iterator.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>

#include "../traits.hpp"
#include "detail/foreach.hpp"
#include "storage.hpp"

namespace mockturtle
{

/*! \brief Hash function for AIGs (from ABC) */
template<class Node>
struct aig_hash
{
  uint64_t operator()( Node const& n ) const
  {
    uint64_t seed = -2011;
    seed += n.children[0].index * 7937;
    seed += n.children[1].index * 2971;
    seed += n.children[0].weight * 911;
    seed += n.children[1].weight * 353;
    return seed;
  }
};

/*! \brief AIG storage container

  AIGs have nodes with fan-in 2.  We split of one bit of the index pointer to
  store a complemented attribute.  Every node has 64-bit of additional data
  used for the following purposes:

  `data[0].h1`: Fan-out size
  `data[0].h2`: Application-specific value
  `data[1].h1`: Visited flag
*/
using aig_storage = storage<regular_node<2, 2, 1>,
                            empty_storage_data,
                            aig_hash<regular_node<2, 2, 1>>>;

class aig_network
{
public:
#pragma region Types and constructors
  static constexpr auto min_fanin_size = 2u;
  static constexpr auto max_fanin_size = 2u;

  using storage = std::shared_ptr<aig_storage>;
  using node = uint64_t;

  struct signal
  {
    signal() = default;

    signal( uint64_t index, uint64_t complement )
        : complement( complement ), index( index )
    {
    }

    explicit signal( uint64_t data )
        : data( data )
    {
    }

    signal( aig_storage::node_type::pointer_type const& p )
        : complement( p.weight ), index( p.index )
    {
    }

    union {
      struct
      {
        uint64_t complement : 1;
        uint64_t index : 63;
      };
      uint64_t data;
    };

    signal operator!() const
    {
      return signal( data ^ 1 );
    }

    signal operator+() const
    {
      return {index, 0};
    }

    signal operator-() const
    {
      return {index, 1};
    }

    signal operator^( bool complement ) const
    {
      return signal( data ^ ( complement ? 1 : 0 ) );
    }

    bool operator==( signal const& other ) const
    {
      return data == other.data;
    }

    bool operator!=( signal const& other ) const
    {
      return data != other.data;
    }

    bool operator<( signal const& other ) const
    {
      return data < other.data;
    }

    operator aig_storage::node_type::pointer_type() const
    {
      return {index, complement};
    }
  };

  aig_network() : _storage( std::make_shared<aig_storage>() )
  {
  }

  aig_network( std::shared_ptr<aig_storage> storage ) : _storage( storage )
  {
  }
#pragma endregion

#pragma region Primary I / O and constants
  signal get_constant( bool value ) const
  {
    return {0, static_cast<uint64_t>( value ? 1 : 0 )};
  }

  signal create_pi( std::string const& name = {} )
  {
    (void)name;

    const auto index = _storage->nodes.size();
    auto& node = _storage->nodes.emplace_back();
    node.children[0].data = node.children[1].data = ~static_cast<uint64_t>( 0 );
    _storage->inputs.emplace_back( index );
    return {index, 0};
  }

  void create_po( signal const& f, std::string const& name = {} )
  {
    (void)name;

    /* increase ref-count to children */
    _storage->nodes[f.index].data[0].h1++;

    _storage->outputs.emplace_back( f.index, f.complement );
  }

  bool is_constant( node const& n ) const
  {
    return n == 0;
  }

  bool is_pi( node const& n ) const
  {
    return _storage->nodes[n].children[0].data == ~static_cast<uint64_t>( 0 ) && _storage->nodes[n].children[1].data == ~static_cast<uint64_t>( 0 );
  }

  bool constant_value( node const& n ) const
  {
    (void)n;
    return false;
  }
#pragma endregion

#pragma region Create unary functions
  signal create_buf( signal const& a )
  {
    return a;
  }

  signal create_not( signal const& a )
  {
    return !a;
  }
#pragma endregion

#pragma region Create binary functions
  signal create_and( signal a, signal b )
  {
    /* order inputs */
    if ( a.index > b.index )
    {
      std::swap( a, b );
    }

    /* trivial cases */
    if ( a.index == b.index )
    {
      return ( a.complement == b.complement ) ? a : get_constant( false );
    }
    else if ( a.index == 0 )
    {
      return a.complement ? b : get_constant( false );
    }

    storage::element_type::node_type node;
    node.children[0] = a;
    node.children[1] = b;

    /* structural hashing */
    const auto it = _storage->hash.find( node );
    if ( it != _storage->hash.end() )
    {
      return {it->second, 0};
    }

    const auto index = _storage->nodes.size();

    if ( index >= .9 * _storage->nodes.capacity() )
    {
      _storage->nodes.reserve( static_cast<uint64_t>( 3.1415f * index ) );
      _storage->hash.reserve( static_cast<uint64_t>( 3.1415f * index ) );
    }

    _storage->nodes.push_back( node );

    _storage->hash[node] = index;

    /* increase ref-count to children */
    _storage->nodes[a.index].data[0].h1++;
    _storage->nodes[b.index].data[0].h1++;

    return {index, 0};
  }

  signal create_nand( signal const& a, signal const& b )
  {
    return !create_and( a, b );
  }

  signal create_or( signal const& a, signal const& b )
  {
    return !create_and( !a, !b );
  }

  signal create_nor( signal const& a, signal const& b )
  {
    return create_and( !a, !b );
  }

  signal create_xor( signal const& a, signal const& b )
  {
    const auto fcompl = a.complement ^ b.complement;
    const auto c1 = create_and( +a, -b );
    const auto c2 = create_and( +b, -a );
    return create_and( !c1, !c2 ) ^ !fcompl;
  }

  signal create_xnor( signal const& a, signal const& b )
  {
    return !create_xor( a, b );
  }
#pragma endregion

#pragma region Createy ternary functions
  signal create_ite( signal cond, signal f_then, signal f_else )
  {
    bool f_compl{false};
    if ( f_then.index < f_else.index )
    {
      std::swap( f_then, f_else );
      cond.complement ^= 1;
    }
    if ( f_then.complement )
    {
      f_then.complement = 0;
      f_else.complement ^= 1;
      f_compl = true;
    }

    return create_and( !create_and( !cond, f_else ), !create_and( cond, f_then ) ) ^ !f_compl;
  }
#pragma endregion

#pragma region Create arbitrary functions
  signal clone_node( aig_network const& other, node const& source, std::vector<signal> const& children )
  {
    (void)other;
    (void)source;
    assert( children.size() == 2u );
    return create_and( children[0u], children[1u] );
  }
#pragma endregion

#pragma region Structural properties
  auto size() const
  {
    return static_cast<uint32_t>( _storage->nodes.size() );
  }

  auto num_pis() const
  {
    return static_cast<uint32_t>( _storage->inputs.size() );
  }

  auto num_pos() const
  {
    return static_cast<uint32_t>( _storage->outputs.size() );
  }

  auto num_gates() const
  {
    return static_cast<uint32_t>( _storage->nodes.size() - _storage->inputs.size() - 1 );
  }

  uint32_t fanin_size( node const& n ) const
  {
    if ( is_constant( n ) || is_pi( n ) )
      return 0;
    return 2;
  }

  uint32_t fanout_size( node const& n ) const
  {
    return _storage->nodes[n].data[0].h1;
  }

  bool is_and( node const& n ) const
  {
    return n > 0 && !is_pi( n );
  }

  bool is_or( node const& n ) const
  {
    (void)n;
    return false;
  }

  bool is_xor( node const& n ) const
  {
    (void)n;
    return false;
  }

  bool is_maj( node const& n ) const
  {
    (void)n;
    return false;
  }

  bool is_ite( node const& n ) const
  {
    (void)n;
    return false;
  }

  bool is_xor3( node const& n ) const
  {
    (void)n;
    return false;
  }
#pragma endregion

#pragma region Functional properties
  kitty::dynamic_truth_table node_function( const node& n ) const
  {
    (void)n;
    kitty::dynamic_truth_table _and( 2 );
    _and._bits[0] = 0x8;
    return _and;
  }
#pragma endregion

#pragma region Nodes and signals
  node get_node( signal const& f ) const
  {
    return f.index;
  }

  signal make_signal( node const& n ) const
  {
    return signal( n, 0 );
  }

  bool is_complemented( signal const& f ) const
  {
    return f.complement;
  }

  uint32_t node_to_index( node const& n ) const
  {
    return static_cast<uint32_t>( n );
  }

  node index_to_node( uint32_t index ) const
  {
    return index;
  }
#pragma endregion

#pragma region Node and signal iterators
  template<typename Fn>
  void foreach_node( Fn&& fn ) const
  {
    detail::foreach_element( ez::make_direct_iterator<uint64_t>( 0 ),
                             ez::make_direct_iterator<uint64_t>( _storage->nodes.size() ),
                             fn );
  }

  template<typename Fn>
  void foreach_pi( Fn&& fn ) const
  {
    detail::foreach_element( _storage->inputs.begin(), _storage->inputs.end(), fn );
  }

  template<typename Fn>
  void foreach_po( Fn&& fn ) const
  {
    detail::foreach_element( _storage->outputs.begin(), _storage->outputs.end(), fn );
  }

  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    detail::foreach_element_if( ez::make_direct_iterator<uint64_t>( 1 ), /* start from 1 to avoid constant */
                                ez::make_direct_iterator<uint64_t>( _storage->nodes.size() ),
                                [this]( auto n ) { return !is_pi( n ); },
                                fn );
  }

  template<typename Fn>
  void foreach_fanin( node const& n, Fn&& fn ) const
  {
    if ( n == 0 || is_pi( n ) )
      return;

    static_assert( detail::is_callable_without_index_v<Fn, signal, bool> ||
                   detail::is_callable_with_index_v<Fn, signal, bool> ||
                   detail::is_callable_without_index_v<Fn, signal, void> ||
                   detail::is_callable_with_index_v<Fn, signal, void> );

    /* we don't use foreach_element here to have better performance */
    if constexpr ( detail::is_callable_without_index_v<Fn, signal, bool> )
    {
      if ( !fn( signal{_storage->nodes[n].children[0]} ) )
        return;
      fn( signal{_storage->nodes[n].children[1]} );
    }
    else if constexpr ( detail::is_callable_with_index_v<Fn, signal, bool> )
    {
      if ( !fn( signal{_storage->nodes[n].children[0]}, 0 ) )
        return;
      fn( signal{_storage->nodes[n].children[1]}, 1 );
    }
    else if constexpr ( detail::is_callable_without_index_v<Fn, signal, void> )
    {
      fn( signal{_storage->nodes[n].children[0]} );
      fn( signal{_storage->nodes[n].children[1]} );
    }
    else if constexpr ( detail::is_callable_with_index_v<Fn, signal, void> )
    {
      fn( signal{_storage->nodes[n].children[0]}, 0 );
      fn( signal{_storage->nodes[n].children[1]}, 1 );
    }
  }
#pragma endregion

#pragma region Value simulation
  template<typename Iterator>
  iterates_over_t<Iterator, bool>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    (void)end;

    assert( n != 0 && !is_pi( n ) );

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];

    auto v1 = *begin++;
    auto v2 = *begin++;

    return ( v1 ^ c1.weight ) && ( v2 ^ c2.weight );
  }

  template<typename Iterator>
  iterates_over_truth_table_t<Iterator>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    (void)end;

    assert( n != 0 && !is_pi( n ) );

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];

    auto tt1 = *begin++;
    auto tt2 = *begin++;

    return ( c1.weight ? ~tt1 : tt1 ) & ( c2.weight ? ~tt2 : tt2 );
  }
#pragma endregion

#pragma region Custom node values
  void clear_values() const
  {
    std::for_each( _storage->nodes.begin(), _storage->nodes.end(), []( auto& n ) { n.data[0].h2 = 0; } );
  }

  auto value( node const& n ) const
  {
    return _storage->nodes[n].data[0].h2;
  }

  void set_value( node const& n, uint32_t v ) const
  {
    _storage->nodes[n].data[0].h2 = v;
  }

  auto incr_value( node const& n ) const
  {
    return _storage->nodes[n].data[0].h2++;
  }

  auto decr_value( node const& n ) const
  {
    return --_storage->nodes[n].data[0].h2;
  }
#pragma endregion

#pragma region Visited flags
  void clear_visited() const
  {
    std::for_each( _storage->nodes.begin(), _storage->nodes.end(), []( auto& n ) { n.data[1].h1 = 0; } );
  }

  auto visited( node const& n ) const
  {
    return _storage->nodes[n].data[1].h1;
  }

  void set_visited( node const& n, uint32_t v ) const
  {
    _storage->nodes[n].data[1].h1 = v;
  }
#pragma endregion

#pragma region General methods
  void update()
  {
  }
#pragma endregion

public:
  std::shared_ptr<aig_storage> _storage;
};

} // namespace mockturtle

namespace std
{

template<>
struct hash<mockturtle::aig_network::signal>
{
  uint64_t operator()( mockturtle::aig_network::signal const &s ) const noexcept
  {
    uint64_t k = s.data;
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccd;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53;
    k ^= k >> 33;
    return k;
  }
}; /* hash */

} // namespace std

