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
  \file cover.hpp
  \brief Divisor cover

  \author Heinz Riener
*/

#pragma once

#include <kitty/kitty.hpp>

#include <optional>
#include <vector>

namespace mockturtle
{

struct greedy_covering_solver_parameters
{
  /* only consider divisor sets up to a certain size */
  uint32_t max_cover_size{3u};
};

class greedy_covering_solver
{
public:
  explicit greedy_covering_solver( greedy_covering_solver_parameters ps = {} )
    : ps( ps )
  {
  }

  std::optional<std::vector<uint32_t>> operator()( std::vector<kitty::partial_truth_table> const& matrix ) const
  {
    kitty::partial_truth_table result( matrix[0].num_bits() );
    result = ~result;

    std::vector<uint32_t> solution;
    for ( auto j = 0u; j < std::min( uint64_t( ps.max_cover_size ), uint64_t( matrix.size() ) ); ++j )
    {
      uint64_t best = 0u;
      uint32_t index = 0u;

      for ( auto i = 0u; i < matrix.size(); ++i )
      {
        auto const current = kitty::count_ones( result & matrix[i] );
        if ( best < current )
        {
          best = current;
          index = i;
        }
      }

      /* update result */
      solution.emplace_back( index );
      result = result & ~matrix[index];

      if ( kitty::count_ones( result ) == 0u )
      {
        /* solution found */
        return solution;
      }
    }

    /* no solution */
    return std::nullopt;
  }

private:
  greedy_covering_solver_parameters ps;
};

class divisor_cover
{
public:
  explicit divisor_cover( kitty::partial_truth_table const& target_function )
    : target_function( target_function )
  {
  }

  void add_divisor( kitty::partial_truth_table const& divisor_function )
  {
    kitty::partial_truth_table bitflip_signature;

    /* iterate over all bit pairs of the target function */
    for ( uint32_t j = 1u; j < target_function.num_bits(); ++j )
    {
      for ( uint32_t i = 0u; i < j; ++i )
      {
        /* check if the bit pair is distinguished by the target function */
        if ( get_bit( target_function, i ) != get_bit( target_function, j ) )
        {
          auto const diff = get_bit( divisor_function, i ) != get_bit( divisor_function, j );
          bitflip_signature.add_bit( diff );
        }
      }
    }

    matrix.emplace_back( bitflip_signature );
  }

  template<class CoverComputation, class Fn>
  void solve( CoverComputation const& solver, Fn&& fn ) const
  {
    auto const solution = solver( matrix );
    if ( solution )
    {
      /* invoke callback functor on candidate set */
      fn( *solution );
    }
  }

  uint64_t size() const
  {
    return matrix.size();
  }

  uint64_t signature_length() const
  {
    return matrix.size() > 0u ? matrix[0u].num_bits() : compute_signature_length();
  }

  uint64_t compute_signature_length() const
  {
    uint64_t length = 0;

    /* iterate over all bit pairs of the target function */
    for ( uint32_t j = 1u; j < target_function.num_bits(); ++j )
    {
      for ( uint32_t i = 0u; i < j; ++i )
      {
        /* check if the bit pair is distinguished by the target function */
        if ( get_bit( target_function, i ) != get_bit( target_function, j ) )
        {
          ++length;
        }
      }
    }

    return length;
  }

private:
  kitty::partial_truth_table const& target_function;
  std::vector<kitty::partial_truth_table> matrix;
}; /* divisor_cover */

} /* mockturtle */
