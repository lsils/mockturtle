/* kitty: C++ truth table library
 * Copyright (C) 2017-2018  EPFL
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
  \file properties.hpp
  \brief Implements property checks for Boolean function

  \author Mathias Soeken
*/

#pragma once

#include <cinttypes>
#include <utility>
#include <vector>

#include "bit_operations.hpp"
#include "operations.hpp"
#include "operators.hpp"

namespace kitty
{

/*! \brief Returns the Chow parameter of a function

  The Chow parameters is a set of values \f$N(f), \Sigma(f)\f$, where \f$N(f)\f$
  is the size of the ON-set, and \f$\Sigma(f)\f$ is the sum of all input
  assignments in the ON-set.  For example for \f$f = x_1 \lor x_2\f$ the
  function returns \f$(3, (2,2))\f$.

  \param tt Truth table
*/
template<typename TT>
std::pair<uint32_t, std::vector<uint32_t>> chow_parameters( const TT& tt )
{
  assert( tt.num_vars() <= 32 );

  const auto n = tt.num_vars();
  const auto nf = count_ones( tt );

  std::vector<uint32_t> sf( n, 0u );
  for_each_one_bit( tt, [&sf]( auto minterm ) {
    for ( auto i = 0u; minterm; ++i )
    {
      if ( minterm & 1 )
      {
        ++sf[i];
      }
      minterm >>= 1;
    }
  } );

  return {nf, sf};
}

/*! \brief Checks whether a function is canalizing

  \param tt Truth table
*/
template<typename TT>
bool is_canalizing( const TT& tt )
{
  uint32_t f1or{}, f0or{};
  uint32_t f1and, f0and;

  uint32_t max = static_cast<uint32_t>( ( uint64_t( 1 ) << tt.num_vars() ) - 1 );
  f1and = f0and = max;

  for ( uint32_t i = 0u; i < tt.num_bits(); ++i )
  {
    if ( get_bit( tt, i ) == 0 )
    {
      f0and &= i;
      f0or |= i;
    }
    else
    {
      f1and &= i;
      f1or |= i;
    }

    if ( f0and == 0 && f1and == 0 && f0or == max && f1or == max )
    {
      return false;
    }
  }

  return true;
}

/*! \brief Checks whether a function is Horn

  A function is Horn, if it can be represented using Horn clauses.

  \param tt Truth table
*/
template<typename TT>
bool is_horn( const TT& tt )
{
  for ( uint32_t i = 1u; i < tt.num_bits(); ++i )
  {
    for ( uint32_t j = 0u; j < i; ++j )
    {
      if ( get_bit( tt, j ) && get_bit( tt, i ) && !get_bit( tt, i & j ) )
      {
        return false;
      }
    }
  }

  return true;
}

/*! \brief Checks whether a function is Krom

  A function is Krom, if it can be represented using Krom clauses.

  \param tt Truth table
*/
template<typename TT>
bool is_krom( const TT& tt )
{
  for ( uint32_t i = 2u; i < tt.num_bits(); ++i )
  {
    for ( uint32_t j = 1u; j < i; ++j )
    {
      for ( uint32_t k = 0u; k < j; ++k )
      {
        const auto maj = ( i & j ) | ( i & k ) | ( j & k );
        if ( get_bit( tt, k ) && get_bit( tt, j ) && get_bit( tt, i ) && !get_bit( tt, maj ) )
        {
          return false;
        }
      }
    }
  }

  return true;
}

/*! \brief Checks whether a function is symmetric in a pair of variables

  A function is symmetric in two variables, if it is invariant to swapping them.

  \param tt Truth table
  \param var_index1 Index of first variable
  \param var_index2 Index of second variable
*/
template<typename TT>
bool is_symmetric_in( const TT& tt, uint8_t var_index1, uint8_t var_index2 )
{
  return tt == swap( tt, var_index1, var_index2 );
}

} // namespace kitty
