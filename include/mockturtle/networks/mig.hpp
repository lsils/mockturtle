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
  \file mig.hpp
  \brief MIG logic network implementation

  \author Eleonora Testa 
*/

#pragma once

#include <memory>
#include <optional>
#include <stack>
#include <string>

#include <ez/direct_iterator.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>

#include "../traits.hpp"
#include "../utils/algorithm.hpp"
#include "detail/foreach.hpp"
#include "storage.hpp"

namespace mockturtle
{

struct mig_storage_data
{
  uint32_t trav_id = 0u;
};

/*! \brief MIG storage container

  MIGs have nodes with fan-in 3.  We split of one bit of the index pointer to
  store a complemented attribute.  Every node has 64-bit of additional data
  used for the following purposes:

  `data[0].h1`: Fan-out size
  `data[0].h2`: Application-specific value
  `data[1].h1`: Visited flag
*/

using mig_node = regular_node<3, 2, 1>;
using mig_storage = storage<mig_node,
                            mig_storage_data>;

class mig_network
{
public:
#pragma region Types and constructors
  static constexpr auto min_fanin_size = 3u;
  static constexpr auto max_fanin_size = 3u;

  using base_type = mig_network;
  using storage = std::shared_ptr<mig_storage>;
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

    signal( mig_storage::node_type::pointer_type const& p )
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

    operator mig_storage::node_type::pointer_type() const
    {
      return {index, complement};
    }
  };

  mig_network() : _storage( std::make_shared<mig_storage>() )
  {
  }

  mig_network( std::shared_ptr<mig_storage> storage ) : _storage( storage )
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
    node.children[0].data = node.children[1].data = node.children[2].data = ~static_cast<uint64_t>( 0 );
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
    return _storage->nodes[n].children[0].data == ~static_cast<uint64_t>( 0 ) && _storage->nodes[n].children[1].data == ~static_cast<uint64_t>( 0 ) && _storage->nodes[n].children[2].data == ~static_cast<uint64_t>( 0 );
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

#pragma region Create binary / ternary functions
  signal create_maj( signal a, signal b, signal c )
  {
    /* order inputs */
    if ( a.index > b.index )
    {
      std::swap( a, b );
      if ( b.index > c.index )
        std::swap( b, c );
      if ( a.index > b.index )
        std::swap( a, b );
    }
    else
    {
      if ( b.index > c.index )
        std::swap( b, c );
      if ( a.index > b.index )
        std::swap( a, b );
    }

    /* trivial cases */
    if ( a.index == b.index )
    {
      return ( a.complement == b.complement ) ? a : c;
    }
    else if ( b.index == c.index )
    {
      return ( b.complement == c.complement ) ? b : a;
    }

    /*  complemented edges minimization */
    auto node_complement = false;
    if ( static_cast<unsigned>( a.complement ) + static_cast<unsigned>( b.complement ) +
             static_cast<unsigned>( c.complement ) >=
         2u )
    {
      node_complement = true;
      a.complement = !a.complement;
      b.complement = !b.complement;
      c.complement = !c.complement;
    }

    storage::element_type::node_type node;
    node.children[0] = a;
    node.children[1] = b;
    node.children[2] = c;

    /* structural hashing */
    const auto it = _storage->hash.find( node );
    if ( it != _storage->hash.end() )
    {
      return {it->second, node_complement};
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
    _storage->nodes[c.index].data[0].h1++;

    return {index, node_complement};
  }

  signal create_and( signal const& a, signal const& b )
  {
    return create_maj( get_constant( false ), a, b );
  }

  signal create_nand( signal const& a, signal const& b )
  {
    return !create_and( a, b );
  }

  signal create_or( signal const& a, signal const& b )
  {
    return create_maj( get_constant( true ), a, b );
  }

  signal create_nor( signal const& a, signal const& b )
  {
    return !create_or( a, b );
  }

  signal create_xor( signal const& a, signal const& b )
  {
    const auto fcompl = a.complement ^ b.complement;
    const auto c1 = create_and( +a, -b );
    const auto c2 = create_and( +b, -a );
    return create_and( !c1, !c2 ) ^ !fcompl;
  }

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

#pragma region Create nary functions
  signal create_nary_and( std::vector<signal> const& fs )
  {
    return tree_reduce( fs.begin(), fs.end(), get_constant( true ), [this]( auto const& a, auto const& b ) { return create_and( a, b ); } );
  }

  signal create_nary_or( std::vector<signal> const& fs )
  {
    return tree_reduce( fs.begin(), fs.end(), get_constant( false ), [this]( auto const& a, auto const& b ) { return create_or( a, b ); } );
  }

  signal create_nary_xor( std::vector<signal> const& fs )
  {
    return tree_reduce( fs.begin(), fs.end(), get_constant( false ), [this]( auto const& a, auto const& b ) { return create_xor( a, b ); } );
  }
#pragma endregion

#pragma region Create arbitrary functions
  signal clone_node( mig_network const& other, node const& source, std::vector<signal> const& children )
  {
    (void)other;
    (void)source;
    assert( children.size() == 3u );
    return create_maj( children[0u], children[1u], children[2u] );
  }
#pragma endregion

#pragma region Restructuring
  std::optional<std::pair<node, signal>> _replace_in_node( node const& n, node const& old_node, signal new_signal )
  {
    auto& node = _storage->nodes[n];

    uint32_t fanin = 0u;
    for ( auto i = 0u; i < 4u; ++i )
    {
      if ( i == 3u )
      {
        return std::nullopt;
      }

      if ( node.children[i].index == old_node )
      {
        fanin = i;
        new_signal.complement ^= node.children[i].weight;
        break;
      }
    }

    // determine potential new children of node n
    signal child2 = new_signal;
    signal child1 = node.children[(fanin + 1 ) % 3];
    signal child0 = node.children[(fanin + 2 ) % 3];

    if ( child0.index > child1.index )
    {
      std::swap( child0, child1 );
    }
    if ( child1.index > child2.index )
    {
      std::swap( child1, child2 );
    }
    if ( child0.index > child1.index )
    {
      std::swap( child0, child1 );
    }

    assert( child0.index <= child1.index );
    assert( child1.index <= child2.index );

    // check for trivial cases?
    if ( child0.index == child1.index )
    {
      const auto diff_pol = child0.complement != child1.complement;
      return std::make_pair( n, diff_pol ? child2 : child0 );
    }
    else if ( child1.index == child2.index )
    {
      const auto diff_pol = child1.complement != child2.complement;
      return std::make_pair( n, diff_pol ? child0 : child1 );
    }

    // node already in hash table
    storage::element_type::node_type _hash_obj;
    _hash_obj.children[0] = child0;
    _hash_obj.children[1] = child1;
    _hash_obj.children[2] = child2;
    if ( const auto it = _storage->hash.find( _hash_obj ); it != _storage->hash.end() )
    {
      return std::make_pair( n, signal( it->second, 0 ) );
    }

    // erase old node in hash table
    _storage->hash.erase( node );

    // insert updated node into hash table
    node.children[0] = child0;
    node.children[1] = child1;
    node.children[2] = child2;
    _storage->hash[node] = n;

    // update the reference counter of the new signal
    _storage->nodes[new_signal.index].data[0].h1++;

    return std::nullopt;
  }

  void _replace_in_outputs( node const& old_node, signal const& new_signal )
  {
    for ( auto& output : _storage->outputs )
    {
      if ( output.index == old_node )
      {
        output.index = new_signal.index;
        output.weight ^= new_signal.complement;

        // increment fan-in of new node
        _storage->nodes[new_signal.index].data[0].h1++;
      }
    }
  }

  void _take_out_node( node const& n )
  {
    auto& nobj = _storage->nodes[n];
    nobj.data[0].h1 = 0;
    _storage->hash.erase( nobj );

    if ( n != 0 && !is_pi( n ) )
    {
      for ( auto i = 0u; i < 3u; ++i )
      {
        if ( _storage->nodes[nobj.children[i].index].data[0].h1 == 0 )
        {
          continue;
        }
        if ( --_storage->nodes[nobj.children[i].index].data[0].h1 == 0 )
        {
          _take_out_node( nobj.children[i].index );
        }
      }
    }
  }

  void substitute_node( node const& old_node, signal const& new_signal )
  {
    std::stack<std::pair<node, signal>> to_substitute;
    to_substitute.push( {old_node, new_signal} );

    while ( !to_substitute.empty() )
    {
      const auto [_old, _new] = to_substitute.top();
      to_substitute.pop();

      for ( auto idx = 1u; idx < _storage->nodes.size(); ++idx )
      {
        if ( is_pi( idx ) )
          continue; /* ignore PIs */

        if ( const auto repl = _replace_in_node( idx, _old, _new ); repl )
        {
          to_substitute.push( *repl );
        }
      }

      /* check outputs */
      _replace_in_outputs( _old, _new );

      // reset fan-in of old node
      _take_out_node( _old );
    }
  }

  void substitute_node_of_parents( std::vector<node> const& parents, node const& old_node, signal const& new_signal )
  {
    for ( auto& p : parents )
    {
      auto& n = _storage->nodes[p];
      for ( auto& child : n.children )
      {
        if ( child.index == old_node )
        {
          child.index = new_signal.index;
          child.weight ^= new_signal.complement;

          // increment fan-in of new node
          _storage->nodes[new_signal.index].data[0].h1++;

          // decrement fan-in of old node
          _storage->nodes[old_node].data[0].h1--;
        }
      }
    }

    /* check outputs */
    for ( auto& output : _storage->outputs )
    {
      if ( output.index == old_node )
      {
        output.index = new_signal.index;
        output.weight ^= new_signal.complement;

        // increment fan-in of new node
        _storage->nodes[new_signal.index].data[0].h1++;

        // decrement fan-in of old node
        _storage->nodes[old_node].data[0].h1--;
      }
    }
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
    return 3;
  }

  uint32_t fanout_size( node const& n ) const
  {
    return _storage->nodes[n].data[0].h1;
  }

  bool is_and( node const& n ) const
  {
    (void)n;
    return false;
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
    return n > 0 && !is_pi( n );
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
    kitty::dynamic_truth_table _maj( 3 );
    _maj._bits[0] = 0xe8;
    return _maj;
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
    detail::foreach_element_if( ez::make_direct_iterator<uint64_t>( 1 ), // start from 1 to avoid constant
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

    // we don't use foreach_element here to have better performance
    if constexpr ( detail::is_callable_without_index_v<Fn, signal, bool> )
    {
      if ( !fn( signal{_storage->nodes[n].children[0]} ) )
        return;
      if ( !fn( signal{_storage->nodes[n].children[1]} ) )
        return;
      fn( signal{_storage->nodes[n].children[2]} );
    }
    else if constexpr ( detail::is_callable_with_index_v<Fn, signal, bool> )
    {
      if ( !fn( signal{_storage->nodes[n].children[0]}, 0 ) )
        return;
      if ( !fn( signal{_storage->nodes[n].children[1]}, 1 ) )
        return;
      fn( signal{_storage->nodes[n].children[2]}, 2 );
    }
    else if constexpr ( detail::is_callable_without_index_v<Fn, signal, void> )
    {
      fn( signal{_storage->nodes[n].children[0]} );
      fn( signal{_storage->nodes[n].children[1]} );
      fn( signal{_storage->nodes[n].children[2]} );
    }
    else if constexpr ( detail::is_callable_with_index_v<Fn, signal, void> )
    {
      fn( signal{_storage->nodes[n].children[0]}, 0 );
      fn( signal{_storage->nodes[n].children[1]}, 1 );
      fn( signal{_storage->nodes[n].children[2]}, 2 );
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
    auto const& c3 = _storage->nodes[n].children[2];

    auto v1 = *begin++;
    auto v2 = *begin++;
    auto v3 = *begin++;

    return ( ( v1 ^ c1.weight ) && ( v2 ^ c2.weight ) ) || ( ( v3 ^ c3.weight ) && ( v1 ^ c1.weight ) ) || ( ( v3 ^ c3.weight ) && ( v2 ^ c2.weight ) );
  }

  template<typename Iterator>
  iterates_over_truth_table_t<Iterator>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    (void)end;

    assert( n != 0 && !is_pi( n ) );

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];
    auto const& c3 = _storage->nodes[n].children[2];

    auto tt1 = *begin++;
    auto tt2 = *begin++;
    auto tt3 = *begin++;

    return kitty::ternary_majority( c1.weight ? ~tt1 : tt1, c2.weight ? ~tt2 : tt2, c3.weight ? ~tt3 : tt3 );
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

  uint32_t trav_id() const
  {
    return _storage->data.trav_id;
  }

  void incr_trav_id() const
  {
    ++_storage->data.trav_id;
  }
#pragma endregion

#pragma region General methods
  void update()
  {
  }
#pragma endregion

public:
  std::shared_ptr<mig_storage> _storage;
};

} // namespace mockturtle

namespace std
{

template<>
struct hash<mockturtle::mig_network::signal>
{
  uint64_t operator()( mockturtle::mig_network::signal const &s ) const noexcept
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
