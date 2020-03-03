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

  /*! Constructs a new dynamic truth table instance with the same number of variables. */
  inline partial_truth_table construct() const
  {
    return partial_truth_table( _num_bits );
  }

  /*! Returns number of (allocated) blocks.
   */
  inline auto num_blocks() const noexcept { return _bits.size(); }

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
  */
  inline void mask_bits() noexcept
  {
    unsigned s = _num_bits ? ( (( _num_bits - 1 ) >> 6) + 1 ) : 0;
    for ( auto i = s; i < _bits.size(); ++i )
    {
      _bits[i] &= 0u;
    }
    if ( _num_bits % 64 )
    {
      _bits[s-1] &= 0xFFFFFFFFFFFFFFFF << (_num_bits % 64);
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

  /*! \cond PRIVATE */
public: /* fields */
  std::vector<uint64_t> _bits;
  int _num_bits;
  /*! \endcond */
};

template<>
struct is_truth_table<kitty::partial_truth_table> : std::true_type {};


/*************************************************************************
* Stuff originally in operators.hpp and operations.hpp and algorithm.hpp *
**************************************************************************/

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

  return std::equal( first.begin(), first.end(), second.begin(), op );
}

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

} // namespace kitty
