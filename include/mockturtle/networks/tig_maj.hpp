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
  \file tig.hpp
  \brief  mux-inverter graph logic network implementation

  \author Dewmini Marakkalage
  */

#pragma once

#include "tig.hpp"
#include <kitty/kitty.hpp>

namespace mockturtle
{

template<>
struct compute_function<three_input_function::majority>
{
  template<typename T>
  std::enable_if_t<kitty::is_truth_table<T>::value, T> operator()( T a, T b, T c )
  {
    return kitty::ternary_majority( a, b, c );
  }

  template<typename T>
  std::enable_if_t<std::is_integral<T>::value, T> operator()( T a, T b, T c )
  {
    return ( a & b ) | ( b & c ) | ( a & c );
  }
};

using mig_signal = tig_network<three_input_function::majority>::signal;
using mig_network2 = tig_network<three_input_function::majority>;

template<>
kitty::dynamic_truth_table mig_network2::node_function( const node& n ) const
{
  (void)n;
  kitty::dynamic_truth_table _maj( 3 );
  _maj._bits[0] = 0xe8;
  return _maj;
}

template<>
bool mig_network2::is_maj( node const& n ) const
{
  return n > 0 && !is_ci( n );
}

template<>
mig_network2::normalization_result mig_network2::normalized_fanins( mig_signal a, mig_signal b, mig_signal c )
{
  mig_network2::normalization_result res;
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
    return { false, { ( a.complement == b.complement ) ? a : c } };
  }
  else if ( b.index == c.index )
  {
    return { false, { ( b.complement == c.complement ) ? b : a } };
  }

  /*  complemented edges minimization */
  auto node_complement = false;
  if ( static_cast<unsigned>( a.complement ) + static_cast<unsigned>( b.complement ) +
           static_cast<unsigned>( c.complement ) >=
       2u )
  {
    node_complement = true;
    a.complement = !a.complement;
    b.complement = !b.complement;
    c.complement = !c.complement;
  }
  return { node_complement, { a, b, c } };
}

template<>
mig_signal mig_network2::create_maj( mig_signal const& a, mig_signal const& b, mig_signal const& c )
{
  return create_gate( a, b, c );
}

template<>
mig_signal mig_network2::create_and( mig_signal const& a, mig_signal const& b )
{
  return create_maj( get_constant( false ), a, b );
}

} // namespace mockturtle
