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
  \file partial_truth_table.hpp
  \brief Implements partial_truth_table

  \author Siang-Yun Lee
*/

#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>

#include "detail/constants.hpp"
#include "traits.hpp"
#include "operations.hpp"

namespace kitty
{

/*! Truth table in which number of variables is known at runtime.
*/
struct partial_truth_table
{
  /*! Standard constructor.
    \param num_bits Number of bits in use initially
    \param reserved_bits Number of bits to be reserved (at least) initially
  */
  explicit partial_truth_table( int num_bits, int reserved_bits = 0 )
      : _bits( ( num_bits + reserved_bits ) ? ( (( num_bits + reserved_bits - 1 ) >> 6) + 1 ) : 0 ),
        _num_bits( num_bits )
  {
  }

  /*! Empty constructor.

    Creates an empty truth table. It has no bit in use. This constructor is
    only used for convenience, if algorithms require the existence of default
    constructable classes.
   */
  partial_truth_table() : _num_bits( 0 ) {}

  /*! Constructs a new dynamic truth table instance with the same number of bits and blocks. */
  inline partial_truth_table construct() const
  {
    return partial_truth_table( _num_bits, ( _bits.size() << 6 ) - _num_bits );
  }

  /*! Returns number of (allocated) blocks.
   */
  inline auto num_blocks() const noexcept { return _bits.size(); }

  /*! Returns number of (used) blocks.
   */
  inline unsigned num_used_blocks() const noexcept { return _num_bits ? ( ( ( _num_bits - 1 ) >> 6 ) + 1 ) : 0; }

  /*! Returns number of (used) bits.
   */
  inline auto num_bits() const noexcept { return _num_bits; }

  /*! \brief Begin iterator to bits.
   */
  inline auto begin() noexcept { return _bits.begin(); }

  /*! \brief End iterator to bits.
   */
  inline auto end() noexcept { return _bits.end(); }

  /*! \brief Begin iterator to bits.
   */
  inline auto begin() const noexcept { return _bits.begin(); }

  /*! \brief End iterator to bits.
   */
  inline auto end() const noexcept { return _bits.end(); }

  /*! \brief Reverse begin iterator to bits.
   */
  inline auto rbegin() noexcept { return _bits.rbegin(); }

  /*! \brief Reverse end iterator to bits.
   */
  inline auto rend() noexcept { return _bits.rend(); }

  /*! \brief Constant begin iterator to bits.
   */
  inline auto cbegin() const noexcept { return _bits.cbegin(); }

  /*! \brief Constant end iterator to bits.
   */
  inline auto cend() const noexcept { return _bits.cend(); }

  /*! \brief Constant reverse begin iterator to bits.
   */
  inline auto crbegin() const noexcept { return _bits.crbegin(); }

  /*! \brief Constant teverse end iterator to bits.
   */
  inline auto crend() const noexcept { return _bits.crend(); }

  /*! \brief Assign other truth table.

    This replaces the current truth table with another truth table.  The truth
    table type is arbitrary.  The vector of bits is resized accordingly.

    \param other Other truth table
  */
  template<class TT, typename = std::enable_if_t<is_truth_table<TT>::value>>
  partial_truth_table& operator=( const TT& other )
  {
    _bits.resize( other.num_blocks() );
    std::copy( other.begin(), other.end(), begin() );
    _num_bits = 1 << other.num_vars();
    /* TODO: partial_truth_table can be assigned with other types of (complete) truth tables,
             but we should somehow prevent assigning partial_truth_table to other types. */

    return *this;
  }

  /*! Masks the number of valid truth table bits.

    If there are reserved blocks or if not all the bits in the last block are used up,
    we block out the remaining bits (fill with zero).
    Bits are used from LSB.
  */
  inline void mask_bits() noexcept
  {
    for ( auto i = num_used_blocks(); i < _bits.size(); ++i )
    {
      _bits[i] &= 0u;
    }
    if ( _num_bits % 64 )
    {
      _bits[num_used_blocks() - 1] &= 0xFFFFFFFFFFFFFFFF >> ( 64 - (_num_bits % 64) );
    }
  }

  inline void resize( int num_bits ) noexcept
  {
    _num_bits = num_bits;

    unsigned needed_blocks = num_bits ? ( (( num_bits - 1 ) >> 6) + 1 ) : 0;
    if ( needed_blocks > _bits.size() )
    {
      _bits.resize( needed_blocks, 0u );
    }

    mask_bits();
  }

  // void reserve()

  inline void add_bit( bool bit ) noexcept
  {
    resize( _num_bits + 1 );
    if ( bit )
      _bits[num_used_blocks() - 1] |= 1 << ( _num_bits % 64 - 1 );
  }

  inline void add_bits( std::vector<bool>& bits ) noexcept
  {
    for ( unsigned i = 0; i < bits.size(); ++i )
      add_bit( bits.at(i) );
  }

  /* \param num_bits Number of bits in `bits` to be added (count from LSB) */
  inline void add_bits( uint64_t bits, int num_bits = 64 ) noexcept
  {
    assert( num_bits <= 64 );
    
    if ( ( _num_bits % 64 ) + num_bits <= 64 ) /* no need for a new block */
    {
      _bits[num_used_blocks() - 1] |= bits << ( _num_bits % 64 );
      _num_bits += num_bits;
    }
    else
    {
      auto first_half_len = 64 - ( _num_bits % 64 );
      _bits[num_used_blocks() - 1] |= bits << ( _num_bits % 64 );
      resize( _num_bits + num_bits );
      _bits[num_used_blocks() - 1] |= ( bits & ( 0xFFFFFFFFFFFFFFFF >> ( 64 - num_bits ) ) ) >> first_half_len;
    }
  }

  /*! \cond PRIVATE */
public: /* fields */
  std::vector<uint64_t> _bits;
  int _num_bits;
  /*! \endcond */
};

template<>
struct is_truth_table<kitty::partial_truth_table> : std::true_type {};


/************************************
* Stuff originally in algorithm.hpp *
*************************************/

/*! \brief Perform bitwise binary operation on two truth tables

  The dimensions of `first` and `second` must match.  This is ensured
  at compile-time for static truth tables, but at run-time for dynamic
  truth tables.

  \param first First truth table
  \param second Second truth table
  \param op Binary operation that takes as input two words (`uint64_t`) and returns a word

  \return new constructed truth table of same type and dimensions
 */
template<typename Fn>
auto binary_operation( const partial_truth_table& first, const partial_truth_table& second, Fn&& op )
{
  assert( first.num_bits() == second.num_bits() );

  auto result = first.construct();
  std::transform( first.cbegin(), first.cbegin() + first.num_used_blocks(), second.cbegin(), result.begin(), op );
  result.mask_bits();
  return result;
}

/*! \brief Perform bitwise ternary operation on three truth tables

  The dimensions of `first`, `second`, and `third` must match.  This
  is ensured at compile-time for static truth tables, but at run-time
  for dynamic truth tables.

  \param first First truth table
  \param second Second truth table
  \param third Third truth table
  \param op Ternary operation that takes as input two words (`uint64_t`) and returns a word

  \return new constructed truth table of same type and dimensions
 */
template<typename Fn>
auto ternary_operation( const partial_truth_table& first, const partial_truth_table& second, const partial_truth_table& third, Fn&& op )
{
  assert( first.num_bits() == second.num_bits() && second.num_bits() == third.num_bits() );

  auto result = first.construct();
  auto it1 = first.cbegin();
  const auto it1_e = first.cbegin() + first.num_used_blocks();
  auto it2 = second.cbegin();
  auto it3 = third.cbegin();
  auto it = result.begin();

  while ( it1 != it1_e )
  {
    *it++ = op( *it1++, *it2++, *it3++ );
  }

  result.mask_bits();
  return result;
}

/*! \brief Computes a predicate based on two truth tables

  The dimensions of `first` and `second` must match.  This is ensured
  at compile-time for static truth tables, but at run-time for dynamic
  truth tables.

  \param first First truth table
  \param second Second truth table
  \param op Binary operation that takes as input two words (`uint64_t`) and returns a Boolean

  \return true or false based on the predicate
 */
template<typename Fn>
bool binary_predicate( const partial_truth_table& first, const partial_truth_table& second, Fn&& op )
{
  assert( first.num_bits() == second.num_bits() );

  return std::equal( first.begin(), first.begin() + first.num_used_blocks(), second.begin(), op );
}

/**************************************
* Stuff originally in operations.hpp  *
* - Many other functions may not work *
*   either, but they maybe also don't *
*   make sense for partial TT.        *
***************************************/

/*! \brief Checks whether two truth tables are equal

  \param first First truth table
  \param second Second truth table
*/
inline bool equal( const partial_truth_table& first, const partial_truth_table& second )
{
  if ( first.num_bits() != second.num_bits() )
  {
    return false;
  }

  return binary_predicate( first, second, std::equal_to<>() );
}

/************************************
* Stuff originally in operators.hpp *
* - The same as dynamic_truth_table.*
*************************************/

/*! \brief Operator for unary_not */
inline partial_truth_table operator~( const partial_truth_table& tt )
{
  return unary_not( tt );
}

/*! \brief Operator for binary_and */
inline partial_truth_table operator&( const partial_truth_table& first, const partial_truth_table& second )
{
  return binary_and( first, second );
}

/*! \brief Operator for binary_and and assign */
inline void operator&=( partial_truth_table& first, const partial_truth_table& second )
{
  first = binary_and( first, second );
}

/*! \brief Operator for binary_or */
inline partial_truth_table operator|( const partial_truth_table& first, const partial_truth_table& second )
{
  return binary_or( first, second );
}

/*! \brief Operator for binary_or and assign */
inline void operator|=( partial_truth_table& first, const partial_truth_table& second )
{
  first = binary_or( first, second );
}

/*! \brief Operator for binary_xor */
inline partial_truth_table operator^( const partial_truth_table& first, const partial_truth_table& second )
{
  return binary_xor( first, second );
}

/*! \brief Operator for binary_xor and assign */
inline void operator^=( partial_truth_table& first, const partial_truth_table& second )
{
  first = binary_xor( first, second );
}

/*! \brief Operator for equal */
inline bool operator==( const partial_truth_table& first, const partial_truth_table& second )
{
  return equal( first, second );
}

/*! \brief Operator for not equal */
inline bool operator!=( const partial_truth_table& first, const partial_truth_table& second )
{
  return !equal( first, second );
}

/*! \brief Operator for less_than */
inline bool operator<( const partial_truth_table& first, const partial_truth_table& second )
{
  return less_than( first, second );
}

/*
  Need to overwrite the following functions in operations.hpp
  in order to allow shift operators:
  - shift_left
  - shift_left_inplace
  - shift_right
  - shift_right_inplace
*/

///*! \brief Operator for left_shift */
//inline partial_truth_table operator<<( const partial_truth_table& tt, uint64_t shift )
//{
//  return shift_left( tt, shift );
//}
//
///*! \brief Operator for left_shift_inplace */
//inline void operator<<=( partial_truth_table& tt, uint64_t shift )
//{
//  shift_left_inplace( tt, shift );
//}
//
///*! \brief Operator for right_shift */
//inline partial_truth_table operator>>( const partial_truth_table& tt, uint64_t shift )
//{
//  return shift_right( tt, shift );
//}
//
///*! \brief Operator for right_shift_inplace */
//inline void operator>>=( partial_truth_table& tt, uint64_t shift )
//{
//  shift_right_inplace( tt, shift );
//}

/*****************************************
* Stuff originally in bit_operations.hpp *
* - find_first_bit_difference and        *
*   find_last_bit_difference should be   *
*   overwritten; others should work.     *
******************************************/


} // namespace kitty
