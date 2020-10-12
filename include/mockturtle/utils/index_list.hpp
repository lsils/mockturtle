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

/*! \brief An ABC-compatiable index list.
 *
 * Small network represented as a list of literals.  The
 * implementation supports AND and XOR gates and is compatiable with
 * ABC's encoding.
 */
struct abc_index_list
{
public:
  using element_type = uint64_t;

public:
  explicit abc_index_list( uint64_t num_pis = 0 )
    : _num_pis( num_pis )
    , values( /* add constant */{ 0u, 1u } )
  {}

  explicit abc_index_list( std::vector<element_type> const& values )
    : values( std::begin( values ), std::end( values ) )
  {
    /* parse the values to determine the number of inputs and outputs */
    auto i = 2u;
    for ( ; ( i+1 ) < values.size(); i+=2 )
    {
      if ( values.at( i ) == 0 && values.at( i + 1 ) == 0 )
      {
        ++_num_pis;
      }
      else
      {
        break;
      }
    }

    for ( ; ( i+1 ) < values.size(); i+=2 )
    {
      assert( !( values.at( i ) == 0 && values.at( i + 1 ) == 0 ) );
      if ( values.at( i ) == values.at( i+1 ) )
      {
        ++_num_pos;
      }
    }
  }

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
    return ( values.size() - ( ( 1 + _num_pis + _num_pos ) << 1u ) ) >> 1u;
  }

  uint64_t num_pis() const
  {
    return _num_pis;
  }

  uint64_t num_pos() const
  {
    return _num_pos;
  }

  template<typename Fn>
  void foreach_entry( Fn&& fn ) const
  {
    assert( ( values.size() % 2 ) == 0 );
    for ( uint64_t i = ( 1 + _num_pis ) << 1u; i < values.size() - ( _num_pos << 1 ); i += 2 )
    {
      fn( values.at( i ), values.at( i+1 ) );
    }
  }

  template<typename Fn>
  void foreach_po( Fn&& fn ) const
  {
    for ( uint64_t i = values.size() - _num_pos; i < values.size(); ++i )
    {
      fn( values.at( i ) );
    }
  }

  void add_inputs( uint64_t num_pis = 1u )
  {
    _num_pis += num_pis;
    for ( auto i = 0u; i < num_pis; ++i )
    {
      values.push_back( 0u );
      values.push_back( 0u );
    }
  }

  void add_and( element_type lit0, element_type lit1 )
  {
    assert( lit0 < lit1 );
    values.push_back( lit0 );
    values.push_back( lit1 );
  }

  void add_xor( element_type lit0, element_type lit1 )
  {
    assert( lit0 > lit1 );
    values.push_back( lit0 );
    values.push_back( lit1 );
  }

  void add_output( element_type lit )
  {
    ++_num_pos;
    values.push_back( lit );
    values.push_back( lit );
  }

private:
  uint64_t _num_pis{0};
  uint64_t _num_pos{0};
  std::vector<uint64_t> values;
};

/*! \brief Generates an abc_index_list from a network
 *
 * **Required network functions:**
 * - `foreach_fanin`
 * - `foreach_gate`
 * - `get_node`
 * - `is_complemented`
 * - `is_and`
 * - `is_xor`
 * - `node_to_index`
 * - `num_gates`
 * - `num_pis`
 * - `num_pos`
 *
 * \param indices An index list
 * \param ntk A logic network
 */
template<typename Ntk>
void encode( abc_index_list& indices, Ntk const& ntk )
{
  using node   = typename Ntk::node;
  using signal = typename Ntk::signal;

  /* inputs */
  indices.add_inputs( ntk.num_pis() );

  /* gates */
  ntk.foreach_gate( [&]( node const& n ){
    assert( ntk.is_and( n ) || ntk.is_xor( n ) );

    std::array<uint64_t, 2u> lits;
    ntk.foreach_fanin( n, [&]( signal const& fi, uint64_t index ){
      lits[index] = 2*ntk.node_to_index( ntk.get_node( fi ) ) + ntk.is_complemented( fi );
    });

    if ( ntk.is_and( n ) )
    {
      indices.add_and( lits[0u], lits[1u] );
    }
    else if ( ntk.is_xor( n ) )
    {
      indices.add_xor( lits[0u], lits[1u] );
    }
  });

  /* outputs */
  ntk.foreach_po( [&]( signal const& f ){
    indices.add_output( 2*ntk.node_to_index( ntk.get_node( f ) ) + ntk.is_complemented( f ) );
  });

  assert( indices.size() == ( 1u + ntk.num_pis() + ntk.num_gates() + ntk.num_pos() ) << 1u );
}

/*! \brief Inserts an abc_index_list into an existing network
 *
 * **Required network functions:**
 * - `get_constant`
 * - `create_and`
 * - `create_xor`
 *
 * \param ntk A logic network
 * \param begin Begin iterator of signal inputs
 * \param end End iterator of signal inputs
 * \param indices An index list
 * \param fn Callback function
 */
template<typename Ntk, typename BeginIter, typename EndIter, typename Fn>
void insert( Ntk& ntk, BeginIter begin, EndIter end, abc_index_list const& indices, Fn&& fn )
{
  using signal = typename Ntk::signal;

  std::vector<signal> signals;
  signals.emplace_back( ntk.get_constant( false ) );
  for ( auto it = begin; it != end; ++it )
  {
    signals.push_back( *it );
  }

  indices.foreach_entry( [&]( uint64_t lit0, uint64_t lit1 ){
    assert( lit0 != lit1 );

    uint64_t const i0 = lit0 >> 1;
    uint64_t const i1 = lit1 >> 1;
    signal const s0 = ( lit0 % 2 ) ? !signals.at( i0 ) : signals.at( i0 );
    signal const s1 = ( lit1 % 2 ) ? !signals.at( i1 ) : signals.at( i1 );

    if ( lit0 < lit1 )
    {
      signals.push_back( ntk.create_and( s0, s1 ) );
    }
    else if ( lit0 > lit1 )
    {
      signals.push_back( ntk.create_xor( s0, s1 ) );
    }
  });

  indices.foreach_po( [&]( uint64_t lit ){
    uint64_t const i = lit >> 1;
    fn( ( lit % 2 ) ? !signals.at( i ) : signals.at( i ) );
  });
}

/*! \brief Index list for majority-inverter graphs.
 *
 * Small network consisting of majority gates and inverters
 * represented as a list of literals.
 */
struct mig_index_list
{
public:
  using element_type = uint64_t;

public:
  explicit mig_index_list( uint64_t num_pis = 0 )
    : values( {num_pis, 0u} )
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

  void add_inputs( uint64_t num_pis = 1u )
  {
    values.at( 0u ) += num_pis ;
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

/*! \brief Generates a mig_index_list from a network
 *
 * The function requires `ntk` to consist of majority gates.
 *
 * **Required network functions:**
 * - `foreach_fanin`
 * - `foreach_gate`
 * - `get_node`
 * - `is_complemented`
 * - `is_maj`
 * - `node_to_index`
 * - `num_gates`
 * - `num_pis`
 * - `num_pos`
 *
 * \param indices An index list
 * \param ntk A logic network
 */
template<typename Ntk>
void encode( mig_index_list& indices, Ntk const& ntk )
{
  using node   = typename Ntk::node;
  using signal = typename Ntk::signal;

  /* inputs */
  indices.add_inputs( ntk.num_pis() );

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

/*! \brief Inserts a mig_index_list into an existing network
 *
 * **Required network functions:**
 * - `get_constant`
 * - `create_maj`
 *
 * \param ntk A logic network
 * \param begin Begin iterator of signal inputs
 * \param end End iterator of signal inputs
 * \param indices An index list
 * \param fn Callback function
 */
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

/*! \brief Generates a network from an index_list
 *
 * **Required network functions:**
 * - `create_pi`
 * - `create_po`
 *
 * \param ntk A logic network
 * \param indices An index list
 */
template<typename Ntk, typename IndexList>
void decode( Ntk& ntk, IndexList const& indices )
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

} /* mockturtle */
