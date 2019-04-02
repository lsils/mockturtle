/* kitty: C++ truth table library
 * Copyright (C) 2017-2019  EPFL
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
  \file decomposition.hpp
  \brief Check decomposition properties of a function

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>

#include "constructors.hpp"
#include "operations.hpp"

namespace kitty
{

enum class top_decomposition
{
  none,
  and_,
  or_,
  lt_,
  le_,
  xor_
};

enum class bottom_decomposition
{
  none,
  and_,
  or_,
  lt_,
  le_,
  xor_
};

/*! \brief Checks, whether function is top disjoint decomposable

  \verbatim embed:rst
  Checks whether the input function ``tt`` can be represented by the function
  :math:`f = g(h(X_1), a)`, where :math:`a \notin X_1`.  The return value
  is :math:`g`:

  * ``top_decomposition::and_``: :math:`g = a \land h(X_1)`
  * ``top_decomposition::or_``: :math:`g = a \lor h(X_1)`
  * ``top_decomposition::lt_``: :math:`g = \bar a \land h(X_1)`
  * ``top_decomposition::le_``: :math:`g = \bar a \lor h(X_1)`
  * ``top_decomposition::xor_``: :math:`g = a \oplus h(X_1)`
  * ``top_decomposition::none``: decomposition does not exist
  
  The function can return the remainder function :math:`h`, whic will not depend
  on :math:`a`.
  \endverbatim

  \param tt Input function \f$f\f$
  \param var_index Variable \f$a\f$
  \param func If not ``null`` and decomposition exists, its value is assigned the remainder \f$h\f$
*/
template<class TT>
top_decomposition is_top_decomposable( const TT& tt, uint32_t var_index, TT* func = nullptr )
{
  auto var = tt.construct();
  kitty::create_nth_var( var, var_index );

  if ( implies( tt, var ) )
  {
    if ( func )
    {
      *func = cofactor1( tt, var_index );
    }
    return top_decomposition::and_;
  }
  else if ( implies( var, tt ) )
  {
    if ( func )
    {
      *func = cofactor0( tt, var_index );
    }
    return top_decomposition::or_;
  }
  else if ( implies( tt, ~var ) )
  {
    if ( func )
    {
      *func = cofactor0( tt, var_index );
    }
    return top_decomposition::lt_;
  }
  else if ( implies( ~var, tt ) )
  {
    if ( func )
    {
      *func = cofactor1( tt, var_index );
    }
    return top_decomposition::le_;
  }

  /* try XOR */
  const auto co0 = cofactor0( tt, var_index );
  const auto co1 = cofactor1( tt, var_index );

  if ( equal( co0, ~co1 ) )
  {
    if ( func )
    {
      *func = co0;
    }
    return top_decomposition::xor_;
  }

  return top_decomposition::none;
}

/*! \brief Checks, whether function is bottom disjoint decomposable

  \verbatim embed:rst
  Checks whether the input function ``tt`` can be represented by the function
  :math:`f = h(X_1, g(a, b))`, where :math:`a, b \notin X_1`.  The return value
  is :math:`g`:

  * ``bottom_decomposition::and_``: :math:`g = a \land b`
  * ``bottom_decomposition::or_``: :math:`g = a \lor b`
  * ``bottom_decomposition::lt_``: :math:`g = \bar a \land b`
  * ``bottom_decomposition::le_``: :math:`g = \bar a \lor b`
  * ``bottom_decomposition::xor_``: :math:`g = a \oplus b`
  * ``bottom_decomposition::none``: decomposition does not exist
  
  The function can return the remainder function :math:`h` in where :math:`g`
  is substituted by :math:`a`.  The remainder function will not depend on
  :math:`b`.
  \endverbatim

  \param tt Input function \f$f\f$
  \param var_index1 Variable \f$a\f$
  \param var_index2 Variable \f$b\f$
  \param func If not ``null`` and decomposition exists, its value is assigned the remainder \f$h\f$
*/
template<class TT>
bottom_decomposition is_bottom_decomposable( const TT& tt, uint32_t var_index1, uint32_t var_index2, TT* func = nullptr )
{
  const auto tt0 = cofactor0( tt, var_index1 );
  const auto tt1 = cofactor1( tt, var_index1 );

  const auto tt00 = cofactor0( tt0, var_index2 );
  const auto tt01 = cofactor1( tt0, var_index2 );
  const auto tt10 = cofactor0( tt1, var_index2 );
  const auto tt11 = cofactor1( tt1, var_index2 );

  const auto eq01 = equal( tt00, tt01 );
  const auto eq02 = equal( tt00, tt10 );
  const auto eq03 = equal( tt00, tt11 );
  const auto eq12 = equal( tt01, tt10 );
  const auto eq13 = equal( tt01, tt11 );
  const auto eq23 = equal( tt10, tt11 );

  const auto num_pairs =
      static_cast<uint32_t>( eq01 ) +
      static_cast<uint32_t>( eq02 ) +
      static_cast<uint32_t>( eq03 ) +
      static_cast<uint32_t>( eq12 ) +
      static_cast<uint32_t>( eq13 ) +
      static_cast<uint32_t>( eq23 );

  if ( num_pairs != 2u && num_pairs != 3 )
  {
    return bottom_decomposition::none;
  }

  if ( !eq01 && !eq02 && !eq03 ) // 00 is different
  {
    if ( func )
    {
      *func = mux_var( var_index1, tt11, tt00 );
    }
    return bottom_decomposition::or_;
  }
  else if ( !eq01 && !eq12 && !eq13 ) // 01 is different
  {
    if ( func )
    {
      *func = mux_var( var_index1, tt01, tt10 );
    }
    return bottom_decomposition::lt_;
  }
  else if ( !eq02 && !eq12 && !eq23 ) // 10 is different
  {
    if ( func )
    {
      *func = mux_var( var_index1, tt01, tt10 );
    }
    return bottom_decomposition::le_;
  }
  else if ( !eq03 && !eq13 && !eq23 ) // 11 is different
  {
    if ( func )
    {
      *func = mux_var( var_index1, tt11, tt00 );
    }
    return bottom_decomposition::and_;
  }
  else // XOR
  {
    if ( func )
    {
      *func = mux_var( var_index1, tt01, tt00 );
    }
    return bottom_decomposition::xor_;
  }

  return bottom_decomposition::none;
}

} // namespace kitty
