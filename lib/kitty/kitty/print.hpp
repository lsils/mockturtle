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
  \file print.hpp
  \brief Implements functions to print truth tables

  \author Mathias Soeken
*/

#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "algorithm.hpp"

namespace kitty
{

/*! \brief Prints truth table in binary representation

  The most-significant bit will be the first character of the string.

  \param tt Truth table
  \param os Output stream
*/
template<typename TT>
void print_binary( const TT& tt, std::ostream& os = std::cout )
{
  for_each_block_reversed( tt, [&tt, &os]( auto word ) {
    std::string chunk( std::min<uint64_t>( tt.num_bits(), 64 ), '0' );
    auto it = chunk.rbegin();
    while ( word && it != chunk.rend() )
    {
      if ( word & 1 )
      {
        *it = '1';
      }
      ++it;
      word >>= 1;
    }
    os << chunk;
  } );
}

/*! \brief Prints truth table in hexadecimal representation

  The most-significant bit will be the first character of the string.

  \param tt Truth table
  \param os Output stream
*/
template<typename TT>
void print_hex( const TT& tt, std::ostream& os = std::cout )
{
  const auto chunk_size = std::min<uint64_t>( tt.num_vars() <= 1 ? 1 : ( tt.num_bits() >> 2 ), 16 );
  for_each_block_reversed( tt, [&os, chunk_size]( auto word ) {
    std::string chunk( chunk_size, '0' );
    auto it = chunk.rbegin();
    while ( word && it != chunk.rend() )
    {
      auto hex = word & 0xf;
      if ( hex < 10 )
      {
        *it = '0' + hex;
      }
      else
      {
        *it = 'a' + ( hex - 10 );
      }
      ++it;
      word >>= 4;
    }
    os << chunk;
  } );
}

/*! \brief Prints truth table in raw binary presentation (for file I/O)

  This function is useful to store large truth tables in binary files
  or `std::stringstream`. Each word is stored into 8 characters.

  \param tt Truth table
  \param os Output stream
*/
template<typename TT>
void print_raw( const TT& tt, std::ostream& os )
{
  for_each_block( tt, [&os]( auto word ) {
    os.write( reinterpret_cast<char*>( &word ), sizeof( word ) );
  } );
}

/*! \brief Returns truth table as a string in binary representation

  Calls `print_binary` internally on a string stream.

  \param tt Truth table
*/
template<typename TT>
inline std::string to_binary( const TT& tt )
{
  std::stringstream st;
  print_binary( tt, st );
  return st.str();
}

/*! \brief Returns truth table as a string in hexadecimal representation

  Calls `print_hex` internally on a string stream.

  \param tt Truth table
*/
template<typename TT>
inline std::string to_hex( const TT& tt )
{
  std::stringstream st;
  print_hex( tt, st );
  return st.str();
}

} /* namespace kitty */
