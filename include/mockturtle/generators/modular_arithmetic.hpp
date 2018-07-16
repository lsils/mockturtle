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
  \file modular_arithmetic.hpp
  \brief Generate modular arithmetic logic networks

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <vector>

#include "../traits.hpp"
#include "arithmetic.hpp"
#include "control.hpp"

namespace mockturtle
{

/*! \brief Creates modular adder
 *
 * Given two input words of the same size *k*, this function creates a circuit
 * that computes *k* output signals that represent \f$(a + b) \bmod 2^k\f$.
 * The first input word `a` is overriden and stores the output signals.
 */
template<class Ntk>
inline void modular_adder_inplace( Ntk& ntk, std::vector<signal<Ntk>>& a, std::vector<signal<Ntk>> const& b )
{
  auto carry = ntk.get_constant( false );
  carry_ripple_adder_inplace( ntk, a, b, carry );
}

/*! \brief Creates modular adder
 *
 * Given two input words of the same size *k*, this function creates a circuit
 * that computes *k* output signals that represent \f$(a + b) \bmod (2^k -
 * c)\f$.  The first input word `a` is overriden and stores the output signals.
 */
template<class Ntk>
inline void modular_adder_inplace( Ntk& ntk, std::vector<signal<Ntk>>& a, std::vector<signal<Ntk>> const& b, uint64_t c )
{
  /* c must be smaller than 2^k */
  assert( c < ( 1 << a.size() ) );

  /* refer to simpler case */
  if ( c == 0 )
  {
    modular_adder_inplace( ntk, a, b );
    return;
  }

  const auto word = constant_word( ntk, c, static_cast<uint32_t>( a.size() ) );
  auto carry = ntk.get_constant( false );
  carry_ripple_adder_inplace( ntk, a, word, carry );

  carry = ntk.get_constant( false );
  carry_ripple_adder_inplace( ntk, a, b, carry );

  std::vector<signal<Ntk>> sum( a.begin(), a.end() );
  auto carry_inv = ntk.get_constant( true );
  carry_ripple_subtractor_inplace( ntk, a, word, carry_inv );

  mux_inplace( ntk, !carry, a, sum );
}

/*! \brief Creates modular subtractor
 *
 * Given two input words of the same size *k*, this function creates a circuit
 * that computes *k* output signals that represent \f$(a - b) \bmod 2^k\f$.
 * The first input word `a` is overriden and stores the output signals.
 */
template<class Ntk>
inline void modular_subtractor_inplace( Ntk& ntk, std::vector<signal<Ntk>>& a, std::vector<signal<Ntk>> const& b )
{
  auto carry = ntk.get_constant( true );
  carry_ripple_subtractor_inplace( ntk, a, b, carry );
}

/*! \brief Creates modular subtractor
 *
 * Given two input words of the same size *k*, this function creates a circuit
 * that computes *k* output signals that represent \f$(a - b) \bmod (2^k -
 * c)\f$.  The first input word `a` is overriden and stores the output signals.
 */
template<class Ntk>
inline void modular_subtractor_inplace( Ntk& ntk, std::vector<signal<Ntk>>& a, std::vector<signal<Ntk>> const& b, uint64_t c )
{
  /* c must be smaller than 2^k */
  assert( c < ( 1 << a.size() ) );

  /* refer to simpler case */
  if ( c == 0 )
  {
    modular_subtractor_inplace( ntk, a, b );
    return;
  }

  auto carry = ntk.get_constant( true );
  carry_ripple_subtractor_inplace( ntk, a, b, carry );

  const auto word = constant_word( ntk, c, static_cast<uint32_t>( a.size() ) );
  std::vector<signal<Ntk>> sum( a.begin(), a.end() );
  auto carry_inv = ntk.get_constant( true );
  carry_ripple_subtractor_inplace( ntk, sum, word, carry_inv );

  mux_inplace( ntk, carry, a, sum );
}

} // namespace mockturtle
