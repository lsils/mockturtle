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
  \file index_list.hpp
  \brief Network representation as index lists

  \author Mathias Soeken
*/

#pragma once

#include "../networks/aig.hpp"
#include "../networks/xag.hpp"
#include "../traits.hpp"

#include <percy/percy.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <string>
#include <vector>


namespace mockturtle
{

class index_list
{
public:
  struct entry
  {
  public:
    explicit entry( kitty::dynamic_truth_table const& tt, uint32_t gate )
      : tt( tt )
      , gate( gate )
    {}

    bool operator==( entry const& other ) const
    {
      return tt == other.tt && gate == other.gate;
    }

    bool operator<( entry const& other ) const
    {
      return ( tt < other.tt ) || ( tt == other.tt && gate < other.gate );
    }

    kitty::dynamic_truth_table tt;
    uint32_t gate;
  }; /* entry */

  explicit index_list( uint32_t num_nodes, uint32_t num_inputs, uint32_t num_outputs )
    : num_nodes( num_nodes )
    , num_inputs( num_inputs )
    , num_outputs( num_outputs )
  {
  }

  void add( uint32_t d )
  {
    data.emplace_back( d );
  }

  std::string to_string() const
  {
    auto s = fmt::format( "{{{} << 16 | {} << 8 | {}", num_nodes, num_outputs, num_inputs );
    for ( const auto& d : data )
    {
      s += fmt::format( ", {}", d );
    }
    s += "}";
    return s;
  }

protected:
  uint32_t num_nodes;
  uint32_t num_inputs;
  uint32_t num_outputs;

  std::vector<uint32_t> data;
}; /* index_list */

/*! \brief Creates AND and XOR gates from binary index list.
 *
 * The `begin` iterator points to the first index of an index list that has the
 * following 32-bit unsigned integer elements.  It starts with a signature whose
 * partitioned into `| num_gates | num_pos | num_pis |`, where `num_gates`
 * accounts for the most-significant 16 bits, `num_pos` accounts for 8 bits, and
 * `num_pis` accounts for the least-significant 8 bits.  Afterwards, gates are
 * defined as literal indexes `(2 * i + c)`, where `i` is an index, with 0
 * indexing the constant 0, 1 to `num_pis` indexing the primary inputs, and all
 * successive indexes for the gates.  Gate literals come in pairs.  If the first
 * literal has a smaller value than the second one, an AND gate is created,
 * otherwise, an XOR gate is created.  Afterwards, all outputs are defined in
 * terms of literals.  The length of both the index list and the primary input
 * list can be derived from the signature, and therefore, no end iterators need
 * to be passed to this function.
 *
 * \param dest Network
 * \param begin Start iterator to index list
 * \param pi_begin Start iterator to primary inputs (in network)
 * \return Returns a vector of signals created based on index list
 *
 * Example: The following index list creates (x1 AND x2) XOR (x3 AND x4):
 * `{3 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 12, 10, 14}`
 */
template<class Ntk, class IndexIterator, class LeavesIterator>
std::vector<signal<Ntk>> create_from_binary_index_list( Ntk& dest, IndexIterator begin, LeavesIterator pi_begin )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_create_and_v<Ntk>, "Ntk does not implement the create_and method" );
  static_assert( has_create_xor_v<Ntk>, "Ntk does not implement the create_xor method" );
  static_assert( has_create_not_v<Ntk>, "Ntk does not implement the create_not method" );

  static_assert( std::is_same_v<std::decay_t<typename std::iterator_traits<IndexIterator>::value_type>, uint32_t>, "IndexIterator value_type must be uint32_t" );
  static_assert( std::is_same_v<std::decay_t<typename std::iterator_traits<LeavesIterator>::value_type>, signal<Ntk>>, "LeavesIterator value_type must be Ntk signal type" );

  const auto signature = *begin++;
  const auto num_pis = signature & 0xff;
  const auto num_pos = ( signature >> 8 ) & 0xff;
  const auto num_gates = signature >> 16;

  // initialize gate-signal list
  std::vector<signal<Ntk>> fs;
  fs.push_back( dest.get_constant( false ) );
  std::copy_n( pi_begin, num_pis, std::back_inserter( fs ) );

  for ( auto i = 0u; i < num_gates; ++i )
  {
    const auto signal1 = *begin++;
    const auto signal2 = *begin++;

    const auto c1 = signal1 % 2 ? dest.create_not( fs[signal1 / 2] ) : fs[signal1 / 2];
    const auto c2 = signal2 % 2 ? dest.create_not( fs[signal2 / 2] ) : fs[signal2 / 2];

    fs.push_back( signal1 > signal2 ? dest.create_xor( c1, c2 ) : dest.create_and( c1, c2 ) );
  }

  std::vector<signal<Ntk>> pos( num_pos );
  for ( auto i = 0u; i < num_pos; ++i )
  {
    const auto signal = *begin++;
    pos[i] = signal % 2 ? dest.create_not( fs[signal / 2] ) : fs[signal / 2];
  }

  return pos;
}

/*! \brief Creates AND and XOR gates from binary index list.
 *
 * An out-of-place variant for create_from_binary_index_list.
 */
template<class Ntk, class IndexIterator>
Ntk create_from_binary_index_list( IndexIterator begin )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi method" );
  static_assert( has_create_po_v<Ntk>, "Ntk does not implement the create_po method" );

  static_assert( std::is_same_v<std::decay_t<typename std::iterator_traits<IndexIterator>::value_type>, uint32_t>, "IndexIterator value_type must be uint32_t" );

  Ntk ntk;
  std::vector<signal<Ntk>> pis( *begin & 0xff );
  std::generate( pis.begin(), pis.end(), [&]() { return ntk.create_pi(); } );
  for ( auto const& f : create_from_binary_index_list<Ntk>( ntk, begin, pis.begin() ) )
  {
    ntk.create_po( f );
  }
  return ntk;
}

namespace detail
{

template<class Ntk>
std::string to_index_list( Ntk const& ntk )
{
  static_assert( std::is_same_v<typename Ntk::base_type, xag_network> || std::is_same_v<typename Ntk::base_type, aig_network>, "Ntk must be XAG or AIG" );

  /* compute signature */
  auto s = fmt::format( "{{{} << 16 | {} << 8 | {}", ntk.num_gates(), ntk.num_pos(), ntk.num_pis() );

  ntk.foreach_pi( [&]( auto const& n, auto i ) {
    if ( ntk.node_to_index( n ) != i + 1 ) {
      fmt::print( "[e] network is not in normalized index order (violated by PI {})\n", i + 1 );
      std::abort();
    }
  });

  ntk.foreach_gate( [&]( auto const& n, auto i ) {
    if ( ntk.node_to_index( n ) != ntk.num_pis() + i + 1 )
    {
      fmt::print( "[e] network is not in normalized index order (violated by node {})\n", ntk.node_to_index( n ) );
      std::abort();
    }

    ntk.foreach_fanin( n, [&]( auto const& f ) {
      if ( ntk.node_to_index( ntk.get_node( f ) ) > ntk.node_to_index( n ) )
      {
        fmt::print( "[e] node {} not in topological order\n", ntk.node_to_index( n ) );
        std::abort();
      }
      s += fmt::format( ", {}", 2 * ntk.node_to_index( ntk.get_node( f ) ) + ( ntk.is_complemented( f ) ? 1 : 0 ) );
    } );
  } );

  ntk.foreach_po( [&]( auto const& f ) {
    s += fmt::format( ", {}", 2 * ntk.node_to_index( ntk.get_node( f ) ) + ( ntk.is_complemented( f ) ? 1 : 0 ) );
  });

  s += "}";

  return s;
}

template<typename Fn, typename T>
void compute_permutations( Fn&& fn, std::vector<T>& vs, uint32_t k )
{
  do
  {
    fn( vs );
    std::reverse( vs.begin() + k, vs.end() );
  }
  while ( std::next_permutation( vs.begin(), vs.end() ) );
}

template<class IndexListType>
IndexListType index_list_from_chain( percy::chain const& chain )
{
  /* precompute possible operator functions */
  std::vector<index_list::entry> functions;

  kitty::dynamic_truth_table const0{3}, x0{3}, x1{3}, x2{3};
  kitty::create_nth_var( x0, 0u );
  kitty::create_nth_var( x1, 1u );
  kitty::create_nth_var( x2, 2u );

  /* prepare elementary functions */
  std::vector<kitty::dynamic_truth_table> elementaries = { const0, ~const0, x0, ~x0, x1, ~x1, x2, ~x2 };

  std::vector<uint32_t> indices;
  for ( auto i = 0u; i < elementaries.size(); ++i )
    indices.emplace_back( i );
  std::sort( std::begin( indices ), std::end( indices) );

  auto store_gate_functions = [&]( std::vector<uint32_t> const& vs ){
    /* maj function */
    {
      auto const f = kitty::ternary_majority( elementaries[vs[0u]], elementaries[vs[1u]], elementaries[vs[2u]] );
      auto const entry = index_list::entry( f, ( 0u << 3 | (vs[0u] % 2) << 2 | (vs[1u] % 2) << 1) | (vs[2u] % 2) );
      if ( std::find( std::begin( functions ), std::end( functions ), entry ) == std::end( functions ) )
      {
        functions.emplace_back( entry );
      }
    }

    /* xor3 function */
    {
      auto const f = elementaries[vs[0u]] ^ elementaries[vs[1u]] ^ elementaries[vs[2u]];
      auto const entry = index_list::entry( f, ( 1u << 3 | (vs[0u] % 2) << 2 | (vs[1u] % 2) << 1) | (vs[2u] % 2) );
      if ( std::find( std::begin( functions ), std::end( functions ), entry ) == std::end( functions ) )
      {
        functions.emplace_back( entry );
      }
    }
  };

  compute_permutations( store_gate_functions, indices, 3u );

  /* generate the index list */
  IndexListType index_list( chain.get_nr_steps(), chain.get_nr_outputs(), chain.get_nr_inputs() );
  for ( auto i = 0; i < chain.get_nr_steps(); ++i )
  {
    auto const& step = chain.get_step( i );
    auto const& op = chain.get_operator( i );

    auto const it = std::find_if( std::begin( functions ), std::end( functions ),
                                  [&]( auto const& entry ){
                                    return entry.tt == op;
                                  });
    assert( it != std::end( functions ) );

    std::array<uint32_t, 3u> complemented = { ( it->gate >> 2u ) & 1, ( it->gate >> 1u ) & 1, ( it->gate >> 0u ) & 1 };
    auto const gate_kind = ( it->gate >> 3u ) & 1;
    switch ( gate_kind )
    {
    case 0u:
      {
        // fmt::print( "<{} {} {}>\n",
        //             2*(step[0u]+1) + complemented[0u],
        //             2*(step[1u]+1) + complemented[1u],
        //             2*(step[2u]+1) + complemented[2u] );
        index_list.add( 2*(step[0u]+1) + complemented[0u] );
        index_list.add( 2*(step[1u]+1) + complemented[1u] );
        index_list.add( 2*(step[2u]+1) + complemented[2u] );
        break;
      }
    case 1u:
      {
        // fmt::print( "{{{} {} {}}}\n",
        //             2*( step[2u] + 1 ) + complemented[2u],
        //             2*( step[1u] + 1 ) + complemented[1u],
        //             2*( step[0u] + 1 ) + complemented[0u] );
        index_list.add( 2*(step[0u]+1) + complemented[0u] );
        index_list.add( 2*(step[1u]+1) + complemented[1u] );
        index_list.add( 2*(step[2u]+1) + complemented[2u] );
        break;
      }
    default:
      std::abort();
    }
  }
  return index_list;
}

}

} /* namespace mockturtle */
