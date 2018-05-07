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
  \file arithmetic.hpp
  \brief Generate arithmetic logic networks

  \author Mathias Soeken
*/

#pragma once

#include <utility>
#include <vector>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>

#include "../traits.hpp"

namespace mockturtle
{

template<typename Ntk>
inline std::pair<signal<Ntk>, signal<Ntk>> full_adder( Ntk& network, const signal<Ntk>& a, const signal<Ntk>& b, const signal<Ntk>& c )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  /* specialization for LUT-ish networks */
  if constexpr ( has_create_node_v<Ntk> )
  {
    kitty::dynamic_truth_table tt_maj( 3u ), tt_xor( 3u );
    kitty::create_from_hex_string( tt_maj, "e8" );
    kitty::create_from_hex_string( tt_xor, "96" );

    const auto sum = network.create_node( {a, b, c}, tt_xor );
    const auto carry = network.create_node( {a, b, c}, tt_maj );

    return {sum, carry};
  }
  else
  {
    static_assert( has_create_and_v<Ntk>, "Ntk does not implement the create_and method" );
    static_assert( has_create_nor_v<Ntk>, "Ntk does not implement the create_nor method" );
    static_assert( has_create_or_v<Ntk>, "Ntk does not implement the create_or method" );

    const auto w1 = network.create_and( a, b );
    const auto w2 = network.create_nor( a, b );
    const auto w3 = network.create_nor( w1, w2 );
    const auto w4 = network.create_and( c, w3 );
    const auto w5 = network.create_nor( c, w3 );
    const auto sum = network.create_nor( w4, w5 );
    const auto carry = network.create_or( w1, w4 );

    return {sum, carry};
  }
}

template<typename Ntk>
inline void carry_ripple_adder_inplace( Ntk& network, std::vector<signal<Ntk>>& a, const std::vector<signal<Ntk>>& b, signal<Ntk>& carry )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  auto pa = a.begin();
  for ( auto pb = b.begin(); pa != a.end(); ++pa, ++pb )
  {
    std::tie( *pa, carry ) = full_adder( network, *pa, *pb, carry );
  }
}

}
