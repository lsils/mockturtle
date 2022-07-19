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
  \file oec_manager.hpp
  \brief A class for observability equivalence class management

  \author Siang-Yun Lee
*/

#pragma once

#include "positional_cube.hpp"
#include "cube.hpp"

#include <iostream>
#include <vector>
#include <utility>

namespace kitty
{

template<bool simple = false>
class oec_manager;

template<>
class oec_manager<true>
{
public:
  oec_manager() {}

  oec_manager( uint32_t num_pos ) : _num_pos( num_pos )
  {
    assert( num_pos < 32 );
    uint32_t max_oec = 1u << num_pos;
    _classes.resize( max_oec );
    for ( uint32_t i = 0u; i < max_oec; ++i )
    {
      _classes[i] = i;
    }
  }

  oec_manager<true>& operator=( oec_manager<true> const& other )
  {
    _num_pos = other._num_pos;
    _classes = other._classes;
    return *this;
  }

  void set_equivalent( uint32_t const& a, uint32_t const& b )
  {
    uint32_t repr_class = _classes.at( a );
    uint32_t to_be_replaced = _classes.at( b );
    for ( auto i = 0u; i < _classes.size(); ++i )
    {
      if ( _classes[i] == to_be_replaced )
        _classes[i] = repr_class;
    }
  }

  void set_equivalent( std::vector<bool> const& a, std::vector<bool> const& b )
  {
    set_equivalent( vector_bool_to_uint32( a ), vector_bool_to_uint32( b ) );
  }

  // fully assigned
  // not used --- maybe should use this for partial assignment
  void set_equivalent( cube const& a, cube const& b )
  {
    set_equivalent( cube_to_uint32( a ), cube_to_uint32( b ) );
  }

  // partially assigned
  // - : 0 and 1 are equivalent
  // x : split case and merge foreach
  void set_equivalent( positional_cube const& a, positional_cube const& b )
  {
    assert( false ); // not implemented yet
  }

  bool are_equivalent( uint32_t const& a, uint32_t const& b ) const
  {
    return _classes.at( a ) == _classes.at( b );
  }

  bool are_equivalent( std::vector<bool> const& a, std::vector<bool> const& b ) const
  {
    return are_equivalent( vector_bool_to_uint32( a ), vector_bool_to_uint32( b ) );
  }

  bool are_equivalent( cube const& a, cube const& b ) const
  {
    assert( a._mask == b._mask );
    return are_equivalent_rec( a, b, 0 );
  }

  uint32_t num_OECs() const
  {
    std::set<uint32_t> unique_ids;
    for ( auto const& id : _classes )
    {
      unique_ids.insert( id );
    }
    return unique_ids.size();
  }

  template<class Fn>
  void foreach_class( Fn&& fn ) const
  {
    std::unordered_map<uint32_t, std::vector<uint32_t>> class2pats;
    for ( auto pat = 0u; pat < _classes.size(); ++pat )
    {
      auto const& id = _classes[pat];
      class2pats.try_emplace( id );
      class2pats[id].emplace_back( pat );
    }
    
    for ( auto const& p : class2pats )
    {
      if ( !fn( p.second ) )
        break;
    }
  }

private:
  bool are_equivalent_rec( cube const& a, cube const& b, uint32_t i ) const
  {
    if ( i == _num_pos )
    {
      return are_equivalent( cube_to_uint32( a ), cube_to_uint32( b ) );
    }

    if ( a.get_mask( i ) )
    {
      return are_equivalent_rec( a, b, i + 1 );
    }
    else
    {
      cube a0 = a;
      a0.set_mask( i );
      cube b0 = b;
      b0.set_mask( i );
      if ( !are_equivalent_rec( a0, b0, i + 1 ) )
        return false;
      a0.set_bit( i );
      b0.set_bit( i );
      return are_equivalent_rec( a0, b0, i + 1 );
    }
  }

  uint32_t vector_bool_to_uint32( std::vector<bool> const& vec ) const
  {
    assert( vec.size() == _num_pos );
    uint32_t res{0u};
    for ( auto i = 0u; i < _num_pos; ++i )
    {
      if ( vec[i] )
        res |= 1u << i;
    }
    return res;
  }

  uint32_t cube_to_uint32( cube const& c ) const
  {
    assert( c.num_literals() == _num_pos ); // fully assigned
    return c._bits;
  }

private:
  uint32_t _num_pos;
  std::vector<uint32_t> _classes;
}; // oec_manager

template<>
class oec_manager<false>
{
public:
  oec_manager( uint32_t num_pos ) : _num_pos( num_pos )
  {}

  // c should have at least one -
  void set_equivalent( positional_cube const& c )
  {

  }

  // a, b should have x at the same bits
  void set_equivalent( positional_cube const& a, positional_cube const& b )
  {

  }

  void set_equivalent( uint32_t const& a, uint32_t const& b )
  {}

  void set_equivalent( std::vector<bool> const& a, std::vector<bool> const& b )
  {}

  // x : assume that a, b have the same value, but should be equivalent for all values
  // - : all in the same class
  bool are_equivalent( positional_cube const& a, positional_cube const& b ) const
  {
  }

  bool are_equivalent( std::vector<bool> const& a, std::vector<bool> const& b ) const
  {
  }

  bool are_equivalent( cube const& a, cube const& b ) const
  {
  }

private:
  // \forall m \in a, m \in b ?
  bool is_fully_contained( positional_cube const& a, positional_cube const& b ) const
  {

  }

  // \exists m \in a, m \in b ?
  // split a into contained part and non-contained part
  bool is_partially_contained( positional_cube const& a, positional_cube const& b ) const
  {
    
  }

private:
  uint32_t _num_pos;
  std::vector<std::pair<positional_cube, uint32_t>> _classes;
}; // oec_manager

} // namespace kitty