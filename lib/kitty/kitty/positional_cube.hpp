/* kitty: C++ truth table library
 * Copyright (C) 2017-2022  EPFL
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
  \file positional_cube.hpp
  \brief A positional cube data structure

  \author Siang-Yun Lee
*/

#pragma once

#include "hash.hpp"
#include "detail/mscfix.hpp"

#include <functional>
#include <iostream>
#include <string>

namespace kitty
{
class positional_cube
{
public:
  /*! \brief Constructs the empty cube
  */
  positional_cube() : _value( 0 ) {} /* NOLINT */

  /*! \brief Constructs a positional cube from zero and one

    Meanings of the values of (zero, one) at each bit position:
    00 is a don't-know (x) or not involved in the cube,
    01 is a positive literal (1), 10 is a negative literal (0), and
    11 is a don't-care (-), meaning that both 0 and 1 are accepted.

    \param zero Offset row
    \param one Onset row
  */
  positional_cube( uint32_t zero, uint32_t one ) : _zero( zero ), _one( one ) {} /* NOLINT */

  /*! \brief Constructs a cube from a string

    Each character corresponds to one literal in the cube.  Only up to first 32
    characters of the string will be considered, since this data structure
    cannot represent cubes with more than 32 literals.  A '1' in the string
    corresponds to a postive literal, a '0' corresponds to a negative literal,
    a '-' corresponds to a don't-care literal, and a 'x' corresponds to a
    don't-know (not involved) literal. If the string is shorter than 32 characters,
    the remaining literals are don't-knows.

    \param str String representing a cube
  */
  // cppcheck-suppress noExplicitConstructor
  positional_cube( const std::string& str ) /* NOLINT */
  {
    _zero = _one = 0u;

    auto p = str.begin();
    if ( p == str.end() )
    {
      return;
    }

    for ( uint64_t i = 1; i <= ( uint64_t( 1u ) << 32u ); i <<= 1 )
    {
      switch ( *p )
      {
      case '0':
        _zero |= i;
        break;
      case '1':
        _one |= i;
        break;
      case '-':
        _zero |= i;
        _one |= i;
        break;
      default: /* dont know (x) */
        break;
      }

      if ( ++p == str.end() )
      {
        return;
      }
    }
  }

  /*! \brief Returns number of concrete literals (0 or 1) */
  inline int num_concrete_literals() const
  {
    return __builtin_popcount( _zero ^ _one );
  }

  /*! \brief Returns number of known literals (0 or 1 or -) */
  inline int num_known_literals() const
  {
    return __builtin_popcount( _zero | _one );
  }

  /*! \brief Returns the difference to another cube */
  //inline int difference( const cube& that ) const
  //{
  //  return ( _bits ^ that._bits ) | ( _mask ^ that._mask );
  //}

  /*! \brief Returns the distance to another cube */
  //inline int distance( const cube& that ) const
  //{
  //  return __builtin_popcount( difference( that ) );
  //}

  /*! \brief Checks whether two cubes are equivalent */
  inline bool operator==( const cube& that ) const
  {
    return _value == that._value;
  }

  /*! \brief Checks whether two cubes are not equivalent */
  inline bool operator!=( const cube& that ) const
  {
    return _value != that._value;
  }

  /*! \brief Default comparison operator */
  inline bool operator<( const cube& that ) const
  {
    return _value < that._value;
  }

  /*! \brief Returns the negated cube (keep dont cares and dont knows) */
  inline cube operator~() const
  {
    return {_one, _zero};
  }

  /*! \brief Merges two cubes of distance-1 */
  //inline cube merge( const cube& that ) const
  //{
  //  const auto d = difference( that );
  //  return {_bits ^ ( ~that._bits & d ), _mask ^ ( that._mask & d )};
  //}

  /*! \brief Adds literal to cube */
  inline void add_literal( uint8_t var_index, bool polarity = true )
  {
    if ( polarity )
      set_one_bit( var_index );
    else
      set_zero_bit( var_index );
  }

  /*! \brief Removes literal from cube (set as x) */
  inline void remove_literal( uint8_t var_index )
  {
    set_dont_know( var_index );
  }

  /*! \brief Constructs the elementary cube representing a single variable (others are x) */
  static positional_cube nth_var_cube( uint8_t var_index )
  {
    const auto _bits = uint32_t( 1 ) << var_index;
    return {0u, _bits};
  }

  /*! \brief Constructs the elementary cube containing the first k positive literals */
  static positional_cube pos_cube( uint8_t k )
  {
    const uint32_t _bits = ( uint64_t( 1 ) << k ) - 1;
    return {0u, _bits};
  }

  /*! \brief Constructs the elementary cube containing the first k negative literals */
  static positional_cube neg_cube( uint8_t k )
  {
    const uint32_t _bits = ( uint64_t( 1 ) << k ) - 1;
    return {_bits, 0u};
  }

  /*! \brief Prints a cube */
  inline void print( unsigned length = 32u, std::ostream& os = std::cout ) const
  {
    for ( auto i = 0u; i < length; ++i )
    {
      os << ( maybe_zero( i ) ? ( maybe_one( i ) ? '-' : '0' ) : ( maybe_one( i ) ? '1' : 'x' ) );
    }
  }

  /*! \brief The bit at index may be 0 */
  inline bool maybe_zero( uint8_t index ) const
  {
    return ( ( _zero >> index ) & 1 ) != 0;
  }

  /*! \brief The bit at index may be 1 */
  inline bool maybe_one( uint8_t index ) const
  {
    return ( ( _one >> index ) & 1 ) != 0;
  }

  /*! \brief The bit at index is 0 */
  inline bool is_zero( uint8_t index ) const
  {
    return maybe_zero( index ) && !maybe_one( index );
  }

  /*! \brief The bit at index is 1 */
  inline bool is_one( uint8_t index ) const
  {
    return maybe_one( index ) && !maybe_zero( index );
  }

  /*! \brief The bit at index is dont care (-) */
  inline bool is_dont_care( uint8_t index ) const
  {
    return maybe_zero( index ) && maybe_one( index );
  }

  /*! \brief The bit at index is dont know (x) */
  inline bool is_dont_know( uint8_t index ) const
  {
    return !maybe_zero( index ) && !maybe_one( index );
  }

  /*! \brief Set as 0 bit at index  */
  inline void set_zero_bit( uint8_t index )
  {
    _zero |= ( 1 << index );
    _one &= ~( 1 << index );
  }

  /*! \brief Set as 1 bit at index  */
  inline void set_one_bit( uint8_t index )
  {
    _zero &= ~( 1 << index );
    _one |= ( 1 << index );
  }

  /*! \brief Set as dont care (-) bit at index  */
  inline void set_dont_care( uint8_t index )
  {
    _zero |= ( 1 << index );
    _one |= ( 1 << index );
  }

  /*! \brief Set as dont know (x) bit at index */
  inline void set_dont_know( uint8_t index )
  {
    _zero &= ~( 1 << index );
    _one &= ~( 1 << index );
  }

  /*! \brief Flips at index (0 <-> 1, x <-> -) */
  inline void flip( uint8_t index )
  {
    _zero ^= ( 1 << index );
    _one ^= ( 1 << index );
  }

  /* positional_cube data structure */
  union {
    struct
    {
      uint32_t _zero;
      uint32_t _one;
    };
    uint64_t _value;
  };
};

/*! \brief Prints all cubes in a vector

  \param cubes Vector of cubes
  \param length Number of variables in each cube
  \param os Output stream
*/
inline void print_cubes( const std::vector<positional_cube>& cubes, unsigned length = 32u, std::ostream& os = std::cout )
{
  for ( const auto& cube : cubes )
  {
    cube.print( length, os );
    os << '\n';
  }

  os << std::flush;
}

template<>
struct hash<positional_cube>
{
  std::size_t operator()( const positional_cube& c ) const
  {
    return std::hash<uint64_t>{}( c._value );
  }
};
} // namespace kitty