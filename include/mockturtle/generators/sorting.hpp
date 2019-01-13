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
  \file sorting.hpp
  \brief Generate sorting networks

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>

namespace mockturtle
{

/*! \brief Generates sorting network based on bubble sort.
 *
 * The functor is called for every comparator in the network.  The arguments
 * to the functor are two integers that define on which lines the comparator
 * acts.
 *
 * \param n Number of elements to sort
 * \param compare_fn Functor
 */
template<class Fn>
void bubble_sorting_network( uint32_t n, Fn&& compare_fn )
{
  if ( n <= 1 )
  {
    return;
  }
  for ( auto c = n - 1; c >= 1; --c )
  {
    for ( auto j = 0u; j < c; ++j )
    {
      compare_fn( j, j + 1 );
    }
  }
}

/*! \brief Generates sorting network based on insertion sort.
 *
 * The functor is called for every comparator in the network.  The arguments
 * to the functor are two integers that define on which lines the comparator
 * acts.
 *
 * \param n Number of elements to sort
 * \param compare_fn Functor
 */
template<class Fn>
void insertion_sorting_network( uint32_t n, Fn&& compare_fn )
{
  if ( n <= 1 )
  {
    return;
  }
  for ( auto c = 1u; c < n; ++c )
  {
    for ( int j = c - 1; j >= 0; --j )
    {
      compare_fn( j, j + 1 );
    }
  }
}

} // namespace mockturtle
