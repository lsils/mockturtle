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
  \file buffered.hpp
  \brief Buffered networks implementation

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../traits.hpp"
#include "aig.hpp"
#include "crossed.hpp"
#include "mig.hpp"

#include <cassert>

namespace mockturtle
{

class buffered_aig_network : public aig_network
{
public:
  static constexpr bool is_buffered_network_type = true;

#pragma region Create unary functions
  signal create_buf( signal const& a )
  {
    const auto index = _storage->nodes.size();
    auto& node = _storage->nodes.emplace_back();
    node.children[0] = a;
    node.children[1] = !a;

    if ( index >= .9 * _storage->nodes.capacity() )
    {
      _storage->nodes.reserve( static_cast<uint64_t>( 3.1415f * index ) );
    }

    /* increase ref-count to children */
    _storage->nodes[a.index].data[0].h1++;

    for ( auto const& fn : _events->on_add )
    {
      ( *fn )( index );
    }

    return { index, 0 };
  }

  void invert( node const& n )
  {
    assert( !is_constant( n ) && !is_pi( n ) );
    assert( fanout_size( n ) == 0 );
    _storage->nodes[n].children[0].weight ^= 1;
    _storage->nodes[n].children[1].weight ^= 1;
  }
#pragma endregion

#pragma region Create arbitrary functions
  signal clone_node( aig_network const& other, node const& source, std::vector<signal> const& children )
  {
    (void)other;
    (void)source;
    assert( other.is_and( source ) );
    assert( children.size() == 2u );
    return create_and( children[0u], children[1u] );
  }
#pragma endregion

#pragma region Restructuring
  // disable restructuring
  std::optional<std::pair<node, signal>> replace_in_node( node const& n, node const& old_node, signal new_signal ) = delete;
  void replace_in_outputs( node const& old_node, signal const& new_signal ) = delete;
  void take_out_node( node const& n ) = delete;
  void substitute_node( node const& old_node, signal const& new_signal ) = delete;
  void substitute_nodes( std::list<std::pair<node, signal>> substitutions ) = delete;
#pragma endregion

#pragma region Structural properties
  uint32_t fanin_size( node const& n ) const
  {
    if ( is_constant( n ) || is_ci( n ) )
      return 0;
    else if ( is_buf( n ) )
      return 1;
    else
      return 2;
  }

  // including buffers, splitters, and inverters
  bool is_buf( node const& n ) const
  {
    return _storage->nodes[n].children[0].index == _storage->nodes[n].children[1].index && _storage->nodes[n].children[0].weight != _storage->nodes[n].children[1].weight;
  }

  bool is_not( node const& n ) const
  {
    return _storage->nodes[n].children[0].weight;
  }

  bool is_and( node const& n ) const
  {
    return n > 0 && !is_ci( n ) && !is_buf( n );
  }

#pragma endregion

#pragma region Functional properties
  kitty::dynamic_truth_table node_function( const node& n ) const
  {
    if ( is_buf( n ) )
    {
      kitty::dynamic_truth_table _buf( 1 );
      _buf._bits[0] = 0x2;
      return _buf;
    }

    kitty::dynamic_truth_table _and( 2 );
    _and._bits[0] = 0x8;
    return _and;
  }
#pragma endregion

#pragma region Node and signal iterators
  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    auto r = range<uint64_t>( 1u, _storage->nodes.size() ); /* start from 1 to avoid constant */
    detail::foreach_element_if(
        r.begin(), r.end(),
        [this]( auto n ) { return !is_ci( n ) && !is_dead( n ) && !is_buf( n ); },
        fn );
  }

  template<typename Fn>
  void foreach_fanin( node const& n, Fn&& fn ) const
  {
    if ( n == 0 || is_ci( n ) )
      return;

    static_assert( detail::is_callable_without_index_v<Fn, signal, bool> ||
                   detail::is_callable_with_index_v<Fn, signal, bool> ||
                   detail::is_callable_without_index_v<Fn, signal, void> ||
                   detail::is_callable_with_index_v<Fn, signal, void> );

    /* we don't use foreach_element here to have better performance */
    if ( is_buf( n ) )
    {
      if constexpr ( detail::is_callable_without_index_v<Fn, signal, bool> )
      {
        fn( signal{ _storage->nodes[n].children[0] } );
      }
      else if constexpr ( detail::is_callable_with_index_v<Fn, signal, bool> )
      {
        fn( signal{ _storage->nodes[n].children[0] }, 0 );
      }
      else if constexpr ( detail::is_callable_without_index_v<Fn, signal, void> )
      {
        fn( signal{ _storage->nodes[n].children[0] } );
      }
      else if constexpr ( detail::is_callable_with_index_v<Fn, signal, void> )
      {
        fn( signal{ _storage->nodes[n].children[0] }, 0 );
      }
    }
    else
    {
      if constexpr ( detail::is_callable_without_index_v<Fn, signal, bool> )
      {
        if ( !fn( signal{ _storage->nodes[n].children[0] } ) )
          return;
        fn( signal{ _storage->nodes[n].children[1] } );
      }
      else if constexpr ( detail::is_callable_with_index_v<Fn, signal, bool> )
      {
        if ( !fn( signal{ _storage->nodes[n].children[0] }, 0 ) )
          return;
        fn( signal{ _storage->nodes[n].children[1] }, 1 );
      }
      else if constexpr ( detail::is_callable_without_index_v<Fn, signal, void> )
      {
        fn( signal{ _storage->nodes[n].children[0] } );
        fn( signal{ _storage->nodes[n].children[1] } );
      }
      else if constexpr ( detail::is_callable_with_index_v<Fn, signal, void> )
      {
        fn( signal{ _storage->nodes[n].children[0] }, 0 );
        fn( signal{ _storage->nodes[n].children[1] }, 1 );
      }
    }
  }
#pragma endregion

#pragma region Value simulation
  template<typename Iterator>
  iterates_over_t<Iterator, bool>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    (void)end;

    assert( n != 0 && !is_ci( n ) );

    if ( is_buf( n ) )
      return is_complemented( _storage->nodes[n].children[0] ) ? !( *begin ) : *begin;

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

    assert( n != 0 && !is_ci( n ) );

    if ( is_buf( n ) )
      return is_complemented( _storage->nodes[n].children[0] ) ? ~( *begin ) : *begin;

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];

    auto tt1 = *begin++;
    auto tt2 = *begin++;

    return ( c1.weight ? ~tt1 : tt1 ) & ( c2.weight ? ~tt2 : tt2 );
  }

  /*! \brief Re-compute the last block. */
  template<typename Iterator>
  void compute( node const& n, kitty::partial_truth_table& result, Iterator begin, Iterator end ) const
  {
    static_assert( iterates_over_v<Iterator, kitty::partial_truth_table>, "begin and end have to iterate over partial_truth_tables" );

    (void)end;
    assert( n != 0 && !is_ci( n ) );

    if ( is_buf( n ) )
    {
      result.resize( begin->num_bits() );
      result._bits.back() = is_complemented( _storage->nodes[n].children[0] ) ? ~( begin->_bits.back() ) : begin->_bits.back();
      result.mask_bits();
      return;
    }

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];

    auto tt1 = *begin++;
    auto tt2 = *begin++;

    assert( tt1.num_bits() > 0 && "truth tables must not be empty" );
    assert( tt1.num_bits() == tt2.num_bits() );
    assert( tt1.num_bits() >= result.num_bits() );
    assert( result.num_blocks() == tt1.num_blocks() || ( result.num_blocks() == tt1.num_blocks() - 1 && result.num_bits() % 64 == 0 ) );

    result.resize( tt1.num_bits() );
    result._bits.back() = ( c1.weight ? ~( tt1._bits.back() ) : tt1._bits.back() ) & ( c2.weight ? ~( tt2._bits.back() ) : tt2._bits.back() );
    result.mask_bits();
  }
#pragma endregion
}; /* buffered_aig_network */

class buffered_mig_network : public mig_network
{
public:
  static constexpr bool is_buffered_network_type = true;

#pragma region Create unary functions
  signal create_buf( signal const& a )
  {
    const auto index = _storage->nodes.size();
    auto& node = _storage->nodes.emplace_back();
    node.children[0] = a;
    node.children[1] = !a;
    // node.children[2] = a; // not used

    if ( index >= .9 * _storage->nodes.capacity() )
    {
      _storage->nodes.reserve( static_cast<uint64_t>( 3.1415f * index ) );
    }

    /* increase ref-count to children */
    _storage->nodes[a.index].data[0].h1++;

    for ( auto const& fn : _events->on_add )
    {
      ( *fn )( index );
    }

    return { index, 0 };
  }

  void invert( node const& n )
  {
    assert( !is_constant( n ) && !is_pi( n ) );
    assert( fanout_size( n ) == 0 );
    _storage->nodes[n].children[0].weight ^= 1;
    _storage->nodes[n].children[1].weight ^= 1;
    _storage->nodes[n].children[2].weight ^= 1;
  }
#pragma endregion

#pragma region Create arbitrary functions
  signal clone_node( mig_network const& other, node const& source, std::vector<signal> const& children )
  {
    (void)other;
    (void)source;
    assert( other.is_maj( source ) );
    assert( children.size() == 3u );
    return create_maj( children[0u], children[1u], children[2u] );
  }
#pragma endregion

#pragma region Restructuring
  // disable restructuring
  std::optional<std::pair<node, signal>> replace_in_node( node const& n, node const& old_node, signal new_signal ) = delete;
  void replace_in_outputs( node const& old_node, signal const& new_signal ) = delete;
  void take_out_node( node const& n ) = delete;
  void substitute_node( node const& old_node, signal const& new_signal ) = delete;
  void substitute_nodes( std::list<std::pair<node, signal>> substitutions ) = delete;
#pragma endregion

#pragma region Structural properties
  uint32_t fanin_size( node const& n ) const
  {
    if ( is_constant( n ) || is_ci( n ) )
      return 0;
    else if ( is_buf( n ) )
      return 1;
    else
      return 3;
  }

  // including buffers, splitters, and inverters
  bool is_buf( node const& n ) const
  {
    return _storage->nodes[n].children[0].index == _storage->nodes[n].children[1].index && _storage->nodes[n].children[0].weight != _storage->nodes[n].children[1].weight;
  }

  bool is_not( node const& n ) const
  {
    return _storage->nodes[n].children[0].weight;
  }

  bool is_maj( node const& n ) const
  {
    return n > 0 && !is_ci( n ) && !is_buf( n );
  }

#pragma endregion

#pragma region Functional properties
  kitty::dynamic_truth_table node_function( const node& n ) const
  {
    if ( is_buf( n ) )
    {
      kitty::dynamic_truth_table _buf( 1 );
      _buf._bits[0] = 0x2;
      return _buf;
    }

    kitty::dynamic_truth_table _maj( 3 );
    _maj._bits[0] = 0xe8;
    return _maj;
  }
#pragma endregion

#pragma region Node and signal iterators
  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    auto r = range<uint64_t>( 1u, _storage->nodes.size() ); /* start from 1 to avoid constant */
    detail::foreach_element_if(
        r.begin(), r.end(),
        [this]( auto n ) { return !is_ci( n ) && !is_dead( n ) && !is_buf( n ); },
        fn );
  }

  template<typename Fn>
  void foreach_fanin( node const& n, Fn&& fn ) const
  {
    if ( n == 0 || is_ci( n ) )
      return;

    static_assert( detail::is_callable_without_index_v<Fn, signal, bool> ||
                   detail::is_callable_with_index_v<Fn, signal, bool> ||
                   detail::is_callable_without_index_v<Fn, signal, void> ||
                   detail::is_callable_with_index_v<Fn, signal, void> );

    /* we don't use foreach_element here to have better performance */
    if ( is_buf( n ) )
    {
      if constexpr ( detail::is_callable_without_index_v<Fn, signal, bool> )
      {
        fn( signal{ _storage->nodes[n].children[0] } );
      }
      else if constexpr ( detail::is_callable_with_index_v<Fn, signal, bool> )
      {
        fn( signal{ _storage->nodes[n].children[0] }, 0 );
      }
      else if constexpr ( detail::is_callable_without_index_v<Fn, signal, void> )
      {
        fn( signal{ _storage->nodes[n].children[0] } );
      }
      else if constexpr ( detail::is_callable_with_index_v<Fn, signal, void> )
      {
        fn( signal{ _storage->nodes[n].children[0] }, 0 );
      }
    }
    else
    {
      if constexpr ( detail::is_callable_without_index_v<Fn, signal, bool> )
      {
        if ( !fn( signal{ _storage->nodes[n].children[0] } ) )
          return;
        if ( !fn( signal{ _storage->nodes[n].children[1] } ) )
          return;
        fn( signal{ _storage->nodes[n].children[2] } );
      }
      else if constexpr ( detail::is_callable_with_index_v<Fn, signal, bool> )
      {
        if ( !fn( signal{ _storage->nodes[n].children[0] }, 0 ) )
          return;
        if ( !fn( signal{ _storage->nodes[n].children[1] }, 1 ) )
          return;
        fn( signal{ _storage->nodes[n].children[2] }, 2 );
      }
      else if constexpr ( detail::is_callable_without_index_v<Fn, signal, void> )
      {
        fn( signal{ _storage->nodes[n].children[0] } );
        fn( signal{ _storage->nodes[n].children[1] } );
        fn( signal{ _storage->nodes[n].children[2] } );
      }
      else if constexpr ( detail::is_callable_with_index_v<Fn, signal, void> )
      {
        fn( signal{ _storage->nodes[n].children[0] }, 0 );
        fn( signal{ _storage->nodes[n].children[1] }, 1 );
        fn( signal{ _storage->nodes[n].children[2] }, 2 );
      }
    }
  }
#pragma endregion

#pragma region Value simulation
  template<typename Iterator>
  iterates_over_t<Iterator, bool>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    (void)end;

    assert( n != 0 && !is_ci( n ) );

    if ( is_buf( n ) )
      return is_complemented( _storage->nodes[n].children[0] ) ? !( *begin ) : *begin;

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

    assert( n != 0 && !is_ci( n ) );

    if ( is_buf( n ) )
      return is_complemented( _storage->nodes[n].children[0] ) ? ~( *begin ) : *begin;

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];
    auto const& c3 = _storage->nodes[n].children[2];

    auto tt1 = *begin++;
    auto tt2 = *begin++;
    auto tt3 = *begin++;

    return kitty::ternary_majority( c1.weight ? ~tt1 : tt1, c2.weight ? ~tt2 : tt2, c3.weight ? ~tt3 : tt3 );
  }

  /*! \brief Re-compute the last block. */
  template<typename Iterator>
  void compute( node const& n, kitty::partial_truth_table& result, Iterator begin, Iterator end ) const
  {
    static_assert( iterates_over_v<Iterator, kitty::partial_truth_table>, "begin and end have to iterate over partial_truth_tables" );

    (void)end;
    assert( n != 0 && !is_ci( n ) );

    if ( is_buf( n ) )
    {
      result.resize( begin->num_bits() );
      result._bits.back() = is_complemented( _storage->nodes[n].children[0] ) ? ~( begin->_bits.back() ) : begin->_bits.back();
      result.mask_bits();
      return;
    }

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];
    auto const& c3 = _storage->nodes[n].children[2];

    auto tt1 = *begin++;
    auto tt2 = *begin++;
    auto tt3 = *begin++;

    assert( tt1.num_bits() > 0 && "truth tables must not be empty" );
    assert( tt1.num_bits() == tt2.num_bits() );
    assert( tt1.num_bits() == tt3.num_bits() );
    assert( tt1.num_bits() >= result.num_bits() );
    assert( result.num_blocks() == tt1.num_blocks() || ( result.num_blocks() == tt1.num_blocks() - 1 && result.num_bits() % 64 == 0 ) );

    result.resize( tt1.num_bits() );
    result._bits.back() =
        ( ( c1.weight ? ~tt1._bits.back() : tt1._bits.back() ) & ( c2.weight ? ~tt2._bits.back() : tt2._bits.back() ) ) |
        ( ( c1.weight ? ~tt1._bits.back() : tt1._bits.back() ) & ( c3.weight ? ~tt3._bits.back() : tt3._bits.back() ) ) |
        ( ( c2.weight ? ~tt2._bits.back() : tt2._bits.back() ) & ( c3.weight ? ~tt3._bits.back() : tt3._bits.back() ) );
    result.mask_bits();
  }
#pragma endregion
}; /* buffered_mig_network */

class buffered_crossed_klut_network : public crossed_klut_network
{
public:
  static constexpr bool is_buffered_network_type = true;

#pragma region Create unary functions
  signal create_buf( signal const& a )
  {
    return _create_node( { a }, 2 );
  }

  void invert( node const& n )
  {
    if ( _storage->nodes[n].data[1].h1 == 2 )
      _storage->nodes[n].data[1].h1 = 3;
    else if ( _storage->nodes[n].data[1].h1 == 3 )
      _storage->nodes[n].data[1].h1 = 2;
    else
      assert( false );
  }
#pragma endregion

#pragma region Crossings
  /*! \brief Merges two buffer nodes into a crossing cell
   *
   * After this operation, the network will not be in a topological order. Additionally, buf1 and buf2 will be dangling.
   *
   * \param buf1 First buffer node.
   * \param buf2 Second buffer node.
   * \return The created crossing cell.
   */
  node merge_into_crossing( node const& buf1, node const& buf2 )
  {
    assert( is_buf( buf1 ) && is_buf( buf2 ) );

    auto const& in_buf1 = _storage->nodes[buf1].children[0];
    auto const& in_buf2 = _storage->nodes[buf2].children[0];

    node out_buf1{}, out_buf2{};
    uint32_t fanin_index1 = std::numeric_limits<uint32_t>::max(), fanin_index2 = std::numeric_limits<uint32_t>::max();
    foreach_node( [&]( node const& n ) {
      foreach_fanin( n, [&]( auto const& f, auto i ) {
        if ( auto const fin = get_node( f ); fin == buf1 )
        {
          out_buf1 = n;
          fanin_index1 = i;
        }
        else if ( fin == buf2 )
        {
          out_buf2 = n;
          fanin_index2 = i;
        }
      } );
    } );
    assert( out_buf1 != 0 && out_buf2 != 0 );
    assert( fanin_index1 != std::numeric_limits<uint32_t>::max() && fanin_index2 != std::numeric_limits<uint32_t>::max() );

    auto const [fout1, fout2] = create_crossing( in_buf1, in_buf2 );

    _storage->nodes[out_buf1].children[fanin_index1] = fout1;
    _storage->nodes[out_buf2].children[fanin_index2] = fout2;

    /* decrease ref-count to children (was increased in `create_crossing`) */
    _storage->nodes[in_buf1.index].data[0].h1--;
    _storage->nodes[in_buf2.index].data[0].h1--;

    _storage->nodes[buf1].children.clear();
    _storage->nodes[buf2].children.clear();

    return get_node( fout1 );
  }

#pragma endregion

#pragma region Structural properties
  // including buffers, splitters, and inverters
  bool is_buf( node const& n ) const
  {
    return _storage->nodes[n].data[1].h1 == 2 || _storage->nodes[n].data[1].h1 == 3;
  }

  bool is_not( node const& n ) const
  {
    return _storage->nodes[n].data[1].h1 == 3;
  }
#pragma endregion

#pragma region Node and signal iterators
  /* Note: crossings are included; buffers, splitters, inverters are not */
  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    auto r = range<uint64_t>( 2u, _storage->nodes.size() ); /* start from 2 to avoid constants */
    detail::foreach_element_if(
        r.begin(), r.end(),
        [this]( auto n ) { return !is_ci( n ) && !is_buf( n ); },
        fn );
  }
#pragma endregion
}; /* buffered_crossed_klut_network */

} // namespace mockturtle