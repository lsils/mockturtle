/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
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
  \file npn_cache.hpp
  \brief Cached NPN class computation

  \author Dewmini Marakkalage 
*/

#pragma once

#include <kitty/kitty.hpp>

#include <tuple>
#include <vector>

namespace mockturtle
{

/*! \brief Cache for mapping an N-input truthtable to the corresponding NPN class and the associated NPN transformation. */
template<uint32_t N = 4u>
class npn_cache
{
  using npn_info = std::tuple<uint64_t, uint32_t, std::vector<uint8_t>>;

public:
  npn_cache() : arr( 1ul << ( 1ul << N ) ), has( 1ul << ( 1ul << N ), false )
  {
    static_assert( N <= 4u, "N is too high! Try increasing this limit if memory is not a problem." );
  }

  npn_info operator()( uint64_t tt )
  {
    if ( has[tt] )
    {
      return arr[tt];
    }

    kitty::dynamic_truth_table dtt( N );
    dtt._bits[0] = tt;

    auto tmp = kitty::exact_npn_canonization( dtt );

    has[tt] = true;
    return ( arr[tt] = { std::get<0>( tmp )._bits[0] & 0xffff, std::get<1>( tmp ), std::get<2>( tmp ) } );
  }

private:
  std::vector<npn_info> arr;
  std::vector<bool> has;
};

} // namespace mockturtle
