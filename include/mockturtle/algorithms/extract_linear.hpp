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
  \file extract_linear.hpp
  \brief Extract linear subcircuit in XAGs

  \author Mathias Soeken
*/

#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "../networks/xag.hpp"
#include "../utils/node_map.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

inline std::pair<xag_network, std::vector<std::array<xag_network::signal, 3>>>
extract_linear_circuit( xag_network const& xag )
{
  xag_network dest;
  std::vector<std::array<xag_network::signal, 3>> and_tuples;
  node_map<xag_network::signal, xag_network> old_to_new( xag );

  old_to_new[xag.get_constant( false )] = dest.get_constant( false );
  xag.foreach_pi( [&]( auto const& n ) {
    old_to_new[n] = dest.create_pi();
  } );

  topo_view topo{xag};
  topo.foreach_node( [&]( auto const& n ) {
    if ( xag.is_constant( n ) || xag.is_pi( n ) )
      return;

    if ( xag.is_and( n ) )
    {
      std::array<xag_network::signal, 3> signal_tuple;
      xag.foreach_fanin( n, [&]( auto const& f, auto i ) {
        signal_tuple[i] = f;
      } );
      const auto and_pi = dest.create_pi();
      old_to_new[n] = and_pi;
      signal_tuple[2] = and_pi;
      and_tuples.push_back( signal_tuple );
    }
    else /* if ( xag.is_xor( n ) ) */
    {
      std::array<xag_network::signal, 2> children;
      xag.foreach_fanin( n, [&]( auto const& f, auto i ) {
        children[i] = old_to_new[f] ^ xag.is_complemented( f );
      } );
      old_to_new[n] = dest.create_xor( children[0], children[1] );
    }
  } );

  xag.foreach_po( [&]( auto const& f ) {
    dest.create_po( old_to_new[f] ^ xag.is_complemented( f ) );
  } );
  for ( auto const& [a, b, _] : and_tuples )
  {
    dest.create_po( a );
    dest.create_po( b );
  }

  return {dest, and_tuples};
}

namespace detail
{

struct merge_linear_circuit_impl
{
public:
  merge_linear_circuit_impl( xag_network const& xag, uint32_t num_and_gates )
      : xag( xag ),
        num_and_gates( num_and_gates ),
        old_to_new( xag ),
        and_pi( xag )
  {
  }

  xag_network run()
  {
    old_to_new[xag.get_constant( false )] = dest.get_constant( false );

    uint32_t orig_pis = xag.num_pis() - num_and_gates;
    orig_pos = xag.num_pos() - 2 * num_and_gates;

    xag.foreach_pi( [&]( auto const& n, auto i ) {
      if ( i < orig_pis )
      {
        old_to_new[n] = dest.create_pi();
      }
      else
      {
        and_pi[n] = i - orig_pis;
      }
    } );

    xag.foreach_po( [&]( auto const& f, auto i ) {
      if ( i == orig_pos )
        return false;

      dest.create_po( run_rec( f ) );

      return true;
    } );

    return dest;
  }

private:
  xag_network::signal run_rec( xag_network::signal const& f )
  {
    if ( old_to_new.has( xag.get_node( f ) ) )
    {
      return old_to_new[f] ^ xag.is_complemented( f );
    }

    if ( and_pi.has( xag.get_node( f ) ) )
    {
      const auto pi_index = and_pi[f];
      const auto c1 = run_rec( xag.po_at( orig_pos + 2 * pi_index ) );
      const auto c2 = run_rec( xag.po_at( orig_pos + 2 * pi_index + 1 ) );
      return old_to_new[f] = dest.create_and( c1, c2 ) ^ xag.is_complemented( f );
    }

    const auto n = xag.get_node( f );
    assert( xag.is_xor( n ) );
    std::array<xag_network::signal, 2> children;
    xag.foreach_fanin( n, [&]( auto const& f, auto i ) {
      children[i] = run_rec( f );
    } );
    return old_to_new[f] = dest.create_xor( children[0], children[1] ) ^ xag.is_complemented( f );
  }

private:
  xag_network dest;
  xag_network const& xag;
  uint32_t num_and_gates;
  uint32_t orig_pos;
  unordered_node_map<xag_network::signal, xag_network> old_to_new;
  unordered_node_map<uint32_t, xag_network> and_pi;
};

} // namespace detail

inline xag_network merge_linear_circuit( xag_network const& xag, uint32_t num_and_gates )
{
  return detail::merge_linear_circuit_impl( xag, num_and_gates ).run();
}

} /* namespace mockturtle */
