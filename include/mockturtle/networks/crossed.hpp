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
  \file crossed.hpp
  \brief Implements networks with crossing cells

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../traits.hpp"
#include "klut.hpp"

namespace mockturtle
{

/* Version 1: annotate information of crossing fanout ordereing with different signals
 * Share the same `klut_storage_data` as regular k-LUT network,
 * but define a different `crossed_klut_storage_node` with a weight field in the pointer.
 */

/*! \brief k-LUT node
 *
 * `data[0].h1`: Fan-out size
 * `data[0].h2`: Application-specific value
 * `data[1].h1`: Function literal in truth table cache
 * `data[1].h2`: Visited flags
 */
struct crossed_klut_storage_node : mixed_fanin_node<2, 1>
{
  bool operator==( crossed_klut_storage_node const& other ) const
  {
    return data[1].h1 == other.data[1].h1 && children == other.children;
  }
};
using crossed_klut_storage = storage<crossed_klut_storage_node, klut_storage_data>;

class crossed_klut_network
{
public:
#pragma region Types and constructors
  static constexpr auto min_fanin_size = 1;
  static constexpr auto max_fanin_size = 32;

  using base_type = crossed_klut_network;
  using storage = std::shared_ptr<crossed_klut_storage>;
  using node = uint64_t;
  using signal = crossed_klut_storage_node::pointer_type;

  crossed_klut_network()
      : _storage( std::make_shared<crossed_klut_storage>() ),
        _events( std::make_shared<decltype( _events )::element_type>() )
  {
    _init();
  }

  crossed_klut_network( std::shared_ptr<crossed_klut_storage> storage )
      : _storage( storage ),
        _events( std::make_shared<decltype( _events )::element_type>() )
  {
    _init();
  }
#pragma endregion

protected:
  static constexpr uint32_t literal_crossing = 0xffffffff;

  inline void _init()
  {
    /* reserve the second node for constant 1 */
    _storage->nodes.emplace_back();

    /* reserve some truth tables for nodes */
    kitty::dynamic_truth_table tt_zero( 0 );
    _storage->data.cache.insert( tt_zero );

    static uint64_t _not = 0x1;
    kitty::dynamic_truth_table tt_not( 1 );
    kitty::create_from_words( tt_not, &_not, &_not + 1 );
    _storage->data.cache.insert( tt_not );

    static uint64_t _and = 0x8;
    kitty::dynamic_truth_table tt_and( 2 );
    kitty::create_from_words( tt_and, &_and, &_and + 1 );
    _storage->data.cache.insert( tt_and );

    static uint64_t _or = 0xe;
    kitty::dynamic_truth_table tt_or( 2 );
    kitty::create_from_words( tt_or, &_or, &_or + 1 );
    _storage->data.cache.insert( tt_or );

    static uint64_t _lt = 0x4;
    kitty::dynamic_truth_table tt_lt( 2 );
    kitty::create_from_words( tt_lt, &_lt, &_lt + 1 );
    _storage->data.cache.insert( tt_lt );

    static uint64_t _le = 0xd;
    kitty::dynamic_truth_table tt_le( 2 );
    kitty::create_from_words( tt_le, &_le, &_le + 1 );
    _storage->data.cache.insert( tt_le );

    static uint64_t _xor = 0x6;
    kitty::dynamic_truth_table tt_xor( 2 );
    kitty::create_from_words( tt_xor, &_xor, &_xor + 1 );
    _storage->data.cache.insert( tt_xor );

    static uint64_t _maj = 0xe8;
    kitty::dynamic_truth_table tt_maj( 3 );
    kitty::create_from_words( tt_maj, &_maj, &_maj + 1 );
    _storage->data.cache.insert( tt_maj );

    static uint64_t _ite = 0xd8;
    kitty::dynamic_truth_table tt_ite( 3 );
    kitty::create_from_words( tt_ite, &_ite, &_ite + 1 );
    _storage->data.cache.insert( tt_ite );

    static uint64_t _xor3 = 0x96;
    kitty::dynamic_truth_table tt_xor3( 3 );
    kitty::create_from_words( tt_xor3, &_xor3, &_xor3 + 1 );
    _storage->data.cache.insert( tt_xor3 );

    /* truth tables for constants */
    _storage->nodes[0].data[1].h1 = 0;
    _storage->nodes[1].data[1].h1 = 1;
  }

public:
#pragma region Primary I / O and constants
  signal get_constant( bool value = false ) const
  {
    return value ? signal( 1, 0 ) : signal( 0, 0 );
  }

  signal create_pi()
  {
    const auto index = _storage->nodes.size();
    _storage->nodes.emplace_back();
    _storage->inputs.emplace_back( index );
    _storage->nodes[index].data[1].h1 = 2;
    return signal( index, 0 );
  }

  uint32_t create_po( signal const& f )
  {
    /* increase ref-count to children */
    _storage->nodes[f.index].data[0].h1++;
    auto const po_index = static_cast<uint32_t>( _storage->outputs.size() );
    _storage->outputs.emplace_back( f );
    return po_index;
  }

  bool is_combinational() const
  {
    return true;
  }

  bool is_constant( node const& n ) const
  {
    return n <= 1;
  }

  bool is_ci( node const& n ) const
  {
    return std::find( _storage->inputs.begin(), _storage->inputs.end(), n ) != _storage->inputs.end();
  }

  bool is_pi( node const& n ) const
  {
    return std::find( _storage->inputs.begin(), _storage->inputs.end(), n ) != _storage->inputs.end();
  }

  bool constant_value( node const& n ) const
  {
    return n == 1;
  }
#pragma endregion

#pragma region Create unary functions
  signal create_buf( signal const& a )
  {
    return a;
  }

  signal create_not( signal const& a )
  {
    return _create_node( { a }, 3 );
  }
#pragma endregion

#pragma region Create binary functions
  signal create_and( signal a, signal b )
  {
    return _create_node( { a, b }, 4 );
  }

  signal create_nand( signal a, signal b )
  {
    return _create_node( { a, b }, 5 );
  }

  signal create_or( signal a, signal b )
  {
    return _create_node( { a, b }, 6 );
  }

  signal create_lt( signal a, signal b )
  {
    return _create_node( { a, b }, 8 );
  }

  signal create_le( signal a, signal b )
  {
    return _create_node( { a, b }, 11 );
  }

  signal create_xor( signal a, signal b )
  {
    return _create_node( { a, b }, 12 );
  }
#pragma endregion

#pragma region Create ternary functions
  signal create_maj( signal a, signal b, signal c )
  {
    return _create_node( { a, b, c }, 14 );
  }

  signal create_ite( signal a, signal b, signal c )
  {
    return _create_node( { a, b, c }, 16 );
  }

  signal create_xor3( signal a, signal b, signal c )
  {
    return _create_node( { a, b, c }, 18 );
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
  signal _create_node( std::vector<signal> const& children, uint32_t literal )
  {
    storage::element_type::node_type node;
    std::copy( children.begin(), children.end(), std::back_inserter( node.children ) );
    node.data[1].h1 = literal;

    const auto it = _storage->hash.find( node );
    if ( it != _storage->hash.end() )
    {
      return it->second;
    }

    const auto index = _storage->nodes.size();
    _storage->nodes.push_back( node );
    _storage->hash[node] = index;

    /* increase ref-count to children */
    for ( auto c : children )
    {
      _storage->nodes[c.index].data[0].h1++;
    }

    for ( auto const& fn : _events->on_add )
    {
      ( *fn )( index );
    }

    return signal( index, 0 );
  }

  signal create_node( std::vector<signal> const& children, kitty::dynamic_truth_table const& function )
  {
    if ( children.size() == 0u )
    {
      assert( function.num_vars() == 0u );
      return get_constant( !kitty::is_const0( function ) );
    }
    return _create_node( children, _storage->data.cache.insert( function ) );
  }

  signal clone_node( crossed_klut_network const& other, node const& source, std::vector<signal> const& children )
  {
    assert( !other.is_crossing( source ) );
    assert( !children.empty() );
    const auto tt = other._storage->data.cache[other._storage->nodes[source].data[1].h1];
    return create_node( children, tt );
  }

  signal clone_node( klut_network const& other, node const& source, std::vector<signal> const& children )
  {
    assert( !children.empty() );
    const auto tt = other._storage->data.cache[other._storage->nodes[source].data[1].h1];
    return create_node( children, tt );
  }
#pragma endregion

#pragma region Crossings
  /*! \brief Create a crossing cell
   *
   * \return A pair `(out1, out2)` of two signals to be used as fanouts of the crossing,
   * where `in1` connects to `out1` and `in2` connects to `out2`.
   */
  std::pair<signal, signal> create_crossing( signal const& in1, signal const& in2 )
  {
    storage::element_type::node_type node;
    node.children.emplace_back( in1 );
    node.children.emplace_back( in2 );
    node.data[1].h1 = literal_crossing;

    const auto index = _storage->nodes.size();
    _storage->nodes.push_back( node );

    /* increase ref-count to children */
    _storage->nodes[in1.index].data[0].h1++;
    _storage->nodes[in2.index].data[0].h1++;

    /* TODO: not sure if this is wanted/needed? */
    for ( auto const& fn : _events->on_add )
    {
      ( *fn )( index );
    }

    return std::make_pair( signal( index, 0 ), signal( index, 1 ) );
  }

  /*! \brief Insert a crossing cell on two wires
   *
   * After this operation, the network will not be in a topological order.
   *
   * \param in1 A fanin signal of the node `out1`
   * \param in2 A fanin signal of the node `out2`
   * \return The created crossing cell. No more fanouts should be added to it.
   */
  node insert_crossing( signal const& in1, signal const& in2, node const& out1, node const& out2 )
  {
    uint32_t fanin_index1 = std::numeric_limits<uint32_t>::max();
    foreach_fanin( out1, [&]( auto const& f, auto i ){
        if ( f == in1 )
        {
          fanin_index1 = i;
          return false;
        }
        return true;
    } );
    assert( fanin_index1 != std::numeric_limits<uint32_t>::max() );

    uint32_t fanin_index2 = std::numeric_limits<uint32_t>::max();
    foreach_fanin( out2, [&]( auto const& f, auto i ){
        if ( f == in2 )
        {
          fanin_index2 = i;
          return false;
        }
        return true;
    } );
    assert( fanin_index2 != std::numeric_limits<uint32_t>::max() );

    auto [fout1, fout2] = create_crossing( in1, in2 );
    _storage->nodes[out1].children[fanin_index1] = fout1;
    _storage->nodes[out2].children[fanin_index2] = fout2;

    /* decrease ref-count to children (was increased in `create_crossing`) */
    _storage->nodes[in1.index].data[0].h1++;
    _storage->nodes[in2.index].data[0].h1++;

    return get_node( fout1 );
  }

  bool is_crossing( node const& n ) const
  {
    return _storage->nodes[n].data[1].h1 == literal_crossing;
  }

  /*! \brief Get the real fanin signal ignoring all crossings in between
   *
   * \param f A signal pointing to a crossing
   * \return The corresponding signal pointing to a non-crossing node
   */
  signal ignore_crossings( signal const& f ) const
  {
    if ( !is_crossing( get_node( f ) ) )
      return f;
    return ignore_crossings( _storage->nodes[f.index].children[f.weight] );
  }

  /*! \brief Iterate through the real fanins of a node, ignoring all crossings in between */
  template<typename Fn>
  void foreach_fanin_ignore_crossings( node const& n, Fn&& fn ) const
  {
    if ( n == 0 || is_ci( n ) )
      return;

    using IteratorType = decltype( _storage->nodes[n].children.begin() );
    detail::foreach_element_transform<IteratorType, signal>(
        _storage->nodes[n].children.begin(), _storage->nodes[n].children.end(), [this]( auto f ) { return ignore_crossings( f ); }, fn );
  }
#pragma endregion

#pragma region Structural properties
  auto size() const
  {
    return static_cast<uint32_t>( _storage->nodes.size() );
  }

  auto num_cis() const
  {
    return static_cast<uint32_t>( _storage->inputs.size() );
  }

  auto num_cos() const
  {
    return static_cast<uint32_t>( _storage->outputs.size() );
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
    return static_cast<uint32_t>( _storage->nodes.size() - _storage->inputs.size() - 2 );
  }

  uint32_t fanin_size( node const& n ) const
  {
    return static_cast<uint32_t>( _storage->nodes[n].children.size() );
  }

  uint32_t fanout_size( node const& n ) const
  {
    return _storage->nodes[n].data[0].h1;
  }

  bool is_function( node const& n ) const
  {
    return n > 1 && !is_ci( n ) && !is_crossing( n );
  }
#pragma endregion

#pragma region Functional properties
  kitty::dynamic_truth_table node_function( const node& n ) const
  {
    assert( !is_crossing( n ) );
    return _storage->data.cache[_storage->nodes[n].data[1].h1];
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
    (void)f;
    return false;
  }

  uint32_t node_to_index( node const& n ) const
  {
    return static_cast<uint32_t>( n );
  }

  node index_to_node( uint32_t index ) const
  {
    return index;
  }

  node ci_at( uint32_t index ) const
  {
    assert( index < _storage->inputs.size() );
    return *( _storage->inputs.begin() + index );
  }

  signal co_at( uint32_t index ) const
  {
    assert( index < _storage->outputs.size() );
    return ( _storage->outputs.begin() + index )->index;
  }

  node pi_at( uint32_t index ) const
  {
    assert( index < _storage->inputs.size() );
    return *( _storage->inputs.begin() + index );
  }

  signal po_at( uint32_t index ) const
  {
    assert( index < _storage->outputs.size() );
    return ( _storage->outputs.begin() + index )->index;
  }
#pragma endregion

#pragma region Node and signal iterators
  template<typename Fn>
  void foreach_node( Fn&& fn ) const
  {
    auto r = range<uint64_t>( _storage->nodes.size() );
    detail::foreach_element( r.begin(), r.end(), fn );
  }

  template<typename Fn>
  void foreach_ci( Fn&& fn ) const
  {
    detail::foreach_element( _storage->inputs.begin(), _storage->inputs.end(), fn );
  }

  template<typename Fn>
  void foreach_co( Fn&& fn ) const
  {
    detail::foreach_element( _storage->outputs.begin(), _storage->outputs.end(), fn );
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

  /* TODO: Should crossings be included? */
  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    auto r = range<uint64_t>( 2u, _storage->nodes.size() ); /* start from 2 to avoid constants */
    detail::foreach_element_if(
        r.begin(), r.end(),
        [this]( auto n ) { return !is_ci( n ); },
        fn );
  }

  template<typename Fn>
  void foreach_fanin( node const& n, Fn&& fn ) const
  {
    if ( n == 0 || is_ci( n ) )
      return;

    detail::foreach_element( _storage->nodes[n].children.begin(), _storage->nodes[n].children.end(), fn );
  }
#pragma endregion

#pragma region Simulate values
  template<typename Iterator>
  iterates_over_t<Iterator, bool>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    assert( !is_crossing( n ) );
    uint32_t index{ 0 };
    while ( begin != end )
    {
      index <<= 1;
      index ^= *begin++ ? 1 : 0;
    }
    return kitty::get_bit( _storage->data.cache[_storage->nodes[n].data[1].h1], index );
  }

  template<typename Iterator>
  iterates_over_truth_table_t<Iterator>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    assert( !is_crossing( n ) );
    const auto nfanin = _storage->nodes[n].children.size();

    std::vector<typename Iterator::value_type> tts( begin, end );

    assert( nfanin != 0 );
    assert( tts.size() == nfanin );

    /* resulting truth table has the same size as any of the children */
    auto result = tts.front().construct();
    const auto gate_tt = _storage->data.cache[_storage->nodes[n].data[1].h1];

    for ( uint32_t i = 0u; i < static_cast<uint32_t>( result.num_bits() ); ++i )
    {
      uint32_t pattern = 0u;
      for ( auto j = 0u; j < nfanin; ++j )
      {
        pattern |= kitty::get_bit( tts[j], i ) << j;
      }
      if ( kitty::get_bit( gate_tt, pattern ) )
      {
        kitty::set_bit( result, i );
      }
    }

    return result;
  }
#pragma endregion

#pragma region Custom node values
  void clear_values() const
  {
    std::for_each( _storage->nodes.begin(), _storage->nodes.end(), []( auto& n ) { n.data[0].h2 = 0; } );
  }

  uint32_t value( node const& n ) const
  {
    return _storage->nodes[n].data[0].h2;
  }

  void set_value( node const& n, uint32_t v ) const
  {
    _storage->nodes[n].data[0].h2 = v;
  }

  uint32_t incr_value( node const& n ) const
  {
    return static_cast<uint32_t>( _storage->nodes[n].data[0].h2++ );
  }

  uint32_t decr_value( node const& n ) const
  {
    return static_cast<uint32_t>( --_storage->nodes[n].data[0].h2 );
  }
#pragma endregion

#pragma region Visited flags
  void clear_visited() const
  {
    std::for_each( _storage->nodes.begin(), _storage->nodes.end(), []( auto& n ) { n.data[1].h2 = 0; } );
  }

  auto visited( node const& n ) const
  {
    return _storage->nodes[n].data[1].h2;
  }

  void set_visited( node const& n, uint32_t v ) const
  {
    _storage->nodes[n].data[1].h2 = v;
  }

  uint32_t trav_id() const
  {
    return _storage->trav_id;
  }

  void incr_trav_id() const
  {
    ++_storage->trav_id;
  }
#pragma endregion

#pragma region General methods
  auto& events() const
  {
    return *_events;
  }
#pragma endregion

public:
  std::shared_ptr<crossed_klut_storage> _storage;
  std::shared_ptr<network_events<base_type>> _events;
};

} // namespace mockturtle