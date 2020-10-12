/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2020  EPFL
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
  \brief List of indices to represent small networks.
  \author Heinz Riener
*/

#include <fmt/format.h>
#include <vector>
#include <array>

namespace mockturtle
{

/*! \brief Index list for majority-inverter graphs.
 *
 * Small network consisting of majority gates and inverters
 * represented as a list of indices.
 */
struct mig_index_list
{
public:
  using element_type = uint64_t;

public:
  explicit mig_index_list()
    : values( 2u, 0u )
  {}

  explicit mig_index_list( std::vector<element_type> const& values )
    : values( std::begin( values ), std::end( values ) )
  {}

  std::vector<element_type> raw() const
  {
    return values;
  }

  uint64_t size() const
  {
    return values.size();
  }

  uint64_t num_entries() const
  {
    return ( values.size() - 2u - num_pos() ) / 3u;
  }

  uint64_t num_pis() const
  {
    return values.at( 0u );
  }

  uint64_t num_pos() const
  {
    return values.at( 1u );
  }

  template<typename Fn>
  void foreach_entry( Fn&& fn ) const
  {
    assert( ( values.size() - 2u - num_pos() ) % 3 == 0 );
    for ( uint64_t i = 2u; i < values.size() - num_pos(); i += 3 )
    {
      fn( values.at( i ), values.at( i+1 ), values.at( i+2 ) );
    }
  }

  template<typename Fn>
  void foreach_po( Fn&& fn ) const
  {
    for ( uint64_t i = values.size() - num_pos(); i < values.size(); ++i )
    {
      fn( values.at( i ) );
    }
  }

  void add_input()
  {
    ++values.at( 0u );
  }

  void add_gate( element_type lit0, element_type lit1, element_type lit2 )
  {
    values.push_back( lit0 );
    values.push_back( lit1 );
    values.push_back( lit2 );
  }

  void add_output( element_type lit )
  {
    ++values.at( 1u );
    values.push_back( lit );
  }

private:
  std::vector<element_type> values;
};

/*! \brief Generates an mig_index_list from a network */
template<typename Ntk>
void encode( mig_index_list& indices, Ntk const& ntk )
{
  using node   = typename Ntk::node;
  using signal = typename Ntk::signal;

  /* inputs */
  for ( uint64_t i = 0; i < ntk.num_pis(); ++i )
  {
    indices.add_input();
  }

  /* gates */
  ntk.foreach_gate( [&]( node const& n ){
    assert( ntk.is_maj( n ) );

    std::array<uint64_t, 3u> lits;
    ntk.foreach_fanin( n, [&]( signal const& fi, uint64_t index ){
      lits[index] = 2*ntk.node_to_index( ntk.get_node( fi ) ) + ntk.is_complemented( fi );
    });
    indices.add_gate( lits[0u], lits[1u], lits[2u] );
  });

  /* outputs */
  ntk.foreach_po( [&]( signal const& f ){
    indices.add_output( 2*ntk.node_to_index( ntk.get_node( f ) ) + ntk.is_complemented( f ) );
  });

  assert( indices.size() == 2u + 3u*ntk.num_gates() + ntk.num_pos() );
}

/*! \brief Generates a network from a mig_index_list */
template<typename Ntk>
void decode( Ntk& ntk, mig_index_list const& indices )
{
  using signal = typename Ntk::signal;

  std::vector<signal> signals;
  for ( uint64_t i = 0; i < indices.num_pis(); ++i )
  {
    signals.push_back( ntk.create_pi() );
  }

  insert( ntk, std::begin( signals ), std::end( signals ), indices,
          [&]( signal const& s ){ ntk.create_po( s ); });
}

/*! \brief Inserts a mig_index_list into an existing network */
template<typename Ntk, typename BeginIter, typename EndIter, typename Fn>
void insert( Ntk& ntk, BeginIter begin, EndIter end, mig_index_list const& indices, Fn&& fn )
{
  using signal = typename Ntk::signal;

  std::vector<signal> signals;
  signals.emplace_back( ntk.get_constant( false ) );
  for ( auto it = begin; it != end; ++it )
  {
    signals.push_back( *it );
  }

  indices.foreach_entry( [&]( uint64_t lit0, uint64_t lit1, uint64_t lit2 ){
    uint64_t const i0 = lit0 >> 1;
    uint64_t const i1 = lit1 >> 1;
    uint64_t const i2 = lit2 >> 1;
    signal const s0 = ( lit0 % 2 ) ? !signals.at( i0 ) : signals.at( i0 );
    signal const s1 = ( lit1 % 2 ) ? !signals.at( i1 ) : signals.at( i1 );
    signal const s2 = ( lit2 % 2 ) ? !signals.at( i2 ) : signals.at( i2 );
    signals.push_back( ntk.create_maj( s0, s1, s2 ) );
  });

  indices.foreach_po( [&]( uint64_t lit ){
    uint64_t const i = lit >> 1;
    fn( ( lit % 2 ) ? !signals.at( i ) : signals.at( i ) );
  });
}

} /* mockturtle */
