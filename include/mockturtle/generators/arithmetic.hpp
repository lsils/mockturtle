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
#include "control.hpp"

namespace mockturtle
{

/*! \brief Inserts a full adder into a network.
 *
 * Inserts a full adder for three inputs (two 1-bit operands and one carry)
 * into the network and returns a pair of sum and carry bit.
 *
 * By default creates a seven 2-input gate network composed of AND, NOR, and OR
 * gates.  If network has `create_node` function, creates two 3-input gate
 * network.
 *
 * \param ntk Network
 * \param a First input operand
 * \param b Second input operand
 * \param c Carry
 * \return Pair of sum (`first`) and carry (`second`)
 */
template<typename Ntk>
inline std::pair<signal<Ntk>, signal<Ntk>> full_adder( Ntk& ntk, const signal<Ntk>& a, const signal<Ntk>& b, const signal<Ntk>& c )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  /* specialization for LUT-ish networks */
  if constexpr ( has_create_node_v<Ntk> )
  {
    kitty::dynamic_truth_table tt_maj( 3u ), tt_xor( 3u );
    kitty::create_from_hex_string( tt_maj, "e8" );
    kitty::create_from_hex_string( tt_xor, "96" );

    const auto sum = ntk.create_node( {a, b, c}, tt_xor );
    const auto carry = ntk.create_node( {a, b, c}, tt_maj );

    return {sum, carry};
  }
  else
  {
    static_assert( has_create_and_v<Ntk>, "Ntk does not implement the create_and method" );
    static_assert( has_create_nor_v<Ntk>, "Ntk does not implement the create_nor method" );
    static_assert( has_create_or_v<Ntk>, "Ntk does not implement the create_or method" );

    const auto w1 = ntk.create_and( a, b );
    const auto w2 = ntk.create_nor( a, b );
    const auto w3 = ntk.create_nor( w1, w2 );
    const auto w4 = ntk.create_and( c, w3 );
    const auto w5 = ntk.create_nor( c, w3 );
    const auto sum = ntk.create_nor( w4, w5 );
    const auto carry = ntk.create_or( w1, w4 );

    return {sum, carry};
  }
}

/*! \brief Creates carry ripple adder structure.
 *
 * Creates a carry ripple structure composed of full adders.  The vectors `a`
 * and `b` must have the same size.  The resulting sum bits are eventually
 * stored in `a` and the carry bit will be overriden to store the output carry
 * bit.
 *
 * \param a First input operand, will also have the output after the call
 * \param b Second input operand
 * \param carry Carry bit, will also have the output carry after the call
 */
template<typename Ntk>
inline void carry_ripple_adder_inplace( Ntk& ntk, std::vector<signal<Ntk>>& a, std::vector<signal<Ntk>> const& b, signal<Ntk>& carry )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  assert( a.size() == b.size() );

  auto pa = a.begin();
  for ( auto pb = b.begin(); pa != a.end(); ++pa, ++pb )
  {
    std::tie( *pa, carry ) = full_adder( ntk, *pa, *pb, carry );
  }
}

/*! \brief Creates carry ripple subtractor structure.
 *
 * Creates a carry ripple structure composed of full adders.  The vectors `a`
 * and `b` must have the same size.  The resulting sum bits are eventually
 * stored in `a` and the carry bit will be overriden to store the output carry
 * bit.  The inputs in `b` are inverted to realize substraction with full
 * adders.  The carry bit must be passed in inverted state to the subtractor.
 *
 * \param a First input operand, will also have the output after the call
 * \param b Second input operand
 * \param carry Carry bit, will also have the output carry after the call
 */
template<typename Ntk>
inline void carry_ripple_subtractor_inplace( Ntk& ntk, std::vector<signal<Ntk>>& a, const std::vector<signal<Ntk>>& b, signal<Ntk>& carry )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_create_not_v<Ntk>, "Ntk does not implement the create_not method" );

  assert( a.size() == b.size() );

  auto pa = a.begin();
  for ( auto pb = b.begin(); pa != a.end(); ++pa, ++pb )
  {
    std::tie( *pa, carry ) = full_adder( ntk, *pa, ntk.create_not( *pb ), carry );
  }
}

/*! \brief Creates a classical multiplier using full adders.
 *
 * The vectors `a` and `b` must not have the same size.  The function creates
 * the multiplier in `ntk` and returns output signals, whose size is the summed
 * sizes of `a` and `b`.
 *
 * \param ntk Network
 * \param a First input operand
 * \param b Second input operand
 */
template<typename Ntk>
inline std::vector<signal<Ntk>> carry_ripple_multiplier( Ntk& ntk, std::vector<signal<Ntk>> const& a, std::vector<signal<Ntk>> const& b )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_create_and_v<Ntk>, "Ntk does not implement the create_and method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );

  auto res = constant_word( ntk, 0, a.size() + b.size() );
  auto tmp = constant_word( ntk, 0, a.size() * 2 );

  for ( auto j = 0u; j < b.size(); ++j )
  {
    for ( auto i = 0u; i < a.size(); ++i )
    {
      std::tie( i ? tmp[a.size() + i - 1] : res[j], tmp[i] ) = full_adder( ntk, ntk.create_and( a[i], b[j] ), tmp[a.size() + i], tmp[i] );
    }
  }

  auto carry = tmp.back() = ntk.get_constant( false );
  for ( auto i = 0u; i < a.size(); ++i )
  {
    std::tie( res[b.size() + i], carry ) = full_adder( ntk, tmp[i], tmp[a.size() + i], carry );
  }

  return res;
}

} // namespace mockturtle
