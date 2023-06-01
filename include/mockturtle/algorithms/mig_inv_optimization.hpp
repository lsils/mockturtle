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
  \file mig_algebraic_rewriting.hpp
  \brief MIG algebraric rewriting

  \author Heinz Riener
  \author Mathias Soeken
*/

#pragma once

#include "../utils/stopwatch.hpp"
#include "../views/fanout_view.hpp"

#include <iostream>
#include <optional>

namespace mockturtle
{

/*! \brief Statistics for mig_inv_optimization. */
struct mig_inv_optimization_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{ 0 };

  int num_inverted{ 0 };

  int num_two_level_inverted{ 0 };

  int total_gain{ 0 };
};

namespace detail
{

template<class Ntk>
class mig_inv_optimization_impl
{
public:
  mig_inv_optimization_impl( Ntk& ntk, mig_inv_optimization_stats& st )
      : ntk( ntk ), st( st )
  {
  }

  void run()
  {
    stopwatch t( st.time_total );

    minimize();
  }

  void minimize()
  {
    bool changed = true;
    while ( changed )
    {
      changed = false;
      ntk.foreach_gate( [&]( auto const& f ) {
        int _gain = gain( f );
        if ( _gain > 0 )
        {
          st.num_inverted++;
          st.total_gain += _gain;
          changed = true;
          invert_node( f );
        }
        else if ( two_level_gain( f ) > 0 )
        {
          st.num_two_level_inverted++;
          st.total_gain += _gain;
          changed = true;
          ntk.foreach_fanout( f, [&]( auto const& parent ) {
            int _subgain = 0;
            _subgain += gain( f );
            if ( is_complemented_parent( parent, f ) )
              _subgain -= 2;
            else
              _subgain += 2;
            if ( _subgain > 0 )
            {
              st.total_gain += _subgain;
              invert_node( parent );
            }
          } );
          invert_node( f );
        }
      } )
    }
  }

  int two_level_gain( node<Ntk> n )
  {
    int _gain = 0;
    _gain += gain( n );

    ntk.foreach_fanout( n, [&]( auto const& f ) {
      int _subgain = 0;
      _subgain += gain( f );
      if ( is_complemented_parent( f, n ) )
        _subgain -= 2;
      else
        _subgain += 2;
      if ( _subgain > 0 )
      {
        _gain += _subgain;
      }
    } );

    return _gain;
  }

  int gain( node<Ntk> n )
  {
    if ( ntk.is_dead( n ) )
    {
      std::cerr << "node" << n << " is dead\n";
      return 0;
    }
    int _gain = 0;
    ntk.foreach_fanin( n, [&]( auto const& f ) {
      if ( ntk.is_pi( f ) )
        return;
      update_gain_is_complemented( f, _gain );
    } );
    ntk.foreach_fanout( n, [&]( auto const& parent ) {
      if ( is_complemented_parent( parent, n ) )
        _gain++;
      else
        _gain--;
    } );
    ntk.foreach_po( [&]( auto const& f ) {
      if ( ntk.get_node( f ) == n )
        update_gain_is_complemented( f, _gain );
    } );
    return _gain;
  }

  void update_gain_is_complemented( signal<Ntk> f, int& _gain )
  {
    if ( ntk.is_complemented( f ) )
      _gain++;
    else
      _gain--;
  }

  bool is_complemented_parent( node<Ntk> parent, node<Ntk> child )
  {
    ntk.foreach_fanin( parent, [&]( auto const& f ) {
      if ( f == child )
      {
        return ntk.is_complemented( f );
      }
    } );
  }

  void invert_node( node<Ntk> n )
  {
    signal<Ntk> a, b, c;
    ntk.foreach_fanin( n, [&]( auto const& f, auto idx ) {
      if ( idx == 0 )
        a = f;
      else if ( idx == 1 )
        b = f;
      else if ( idx == 2 )
        c = f;
    } );
    signal<Ntk> new_node = !create_maj_directly( !a, !b, !c );
    ntk.substitute_node( n, new_node );
    ntk.replace_in_outputs( n, new_node );
  }

  signal<Ntk> create_maj_directly( signal<Ntk> a, signal<Ntk> b, signal<Ntk> c )
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

    storage::element_type::node_type node;
    node.children[0] = a;
    node.children[1] = b;
    node.children[2] = c;

    /* structural hashing */
    const auto it = _storage->hash.find( node );
    if ( it != _storage->hash.end() )
    {
      return { it->second, node_complement };
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

    for ( auto const& fn : _events->on_add )
    {
      ( *fn )( index );
    }

    return { index, 0 };
  }

private:
  Ntk& ntk;
  mig_inv_optimization_stats& st;
};

} // namespace detail

/*! \brief Majority algebraic depth rewriting.
 *
 * This algorithm tries to rewrite a network with majority gates for depth
 * optimization using the associativity and distributivity rule in
 * majority-of-3 logic.  It can be applied to networks other than MIGs, but
 * only considers pairs of nodes which both implement the majority-of-3
 * function.
 *
 * **Required network functions:**
 * - `get_node`
 * - `level`
 * - `update_levels`
 * - `create_maj`
 * - `substitute_node`
 * - `foreach_node`
 * - `foreach_po`
 * - `foreach_fanin`
 * - `is_maj`
 * - `clear_values`
 * - `set_value`
 * - `value`
 * - `fanout_size`
 *
   \verbatim embed:rst

  .. note::

      The implementation of this algorithm was heavily inspired by an
      implementation from Luca Amarù.
   \endverbatim
 */
template<class Ntk>
void mig_inv_optimization( Ntk& ntk, mig_inv_optimization_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_level_v<Ntk>, "Ntk does not implement the level method" );
  static_assert( has_create_maj_v<Ntk>, "Ntk does not implement the create_maj method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the substitute_node method" );
  static_assert( has_update_levels_v<Ntk>, "Ntk does not implement the update_levels method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_is_maj_v<Ntk>, "Ntk does not implement the is_maj method" );
  static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
  static_assert( has_value_v<Ntk>, "Ntk does not implement the value method" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );

  mig_inv_optimization_stats st;
  fanout_view<Ntk> fo_ntk( ntk );
  detail::mig_inv_optimization_impl<fanout_view<Ntk>> p( ntk, st );
  p.run();

  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */