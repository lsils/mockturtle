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
  \file mux_resyn.hpp
  \brief Implements resynthesis methods for MuxIGs.

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../../utils/index_list.hpp"
#include "../../utils/null_utils.hpp"

#include <fmt/format.h>
#include <kitty/kitty.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace mockturtle
{

/*! \brief Logic resynthesis engine for MuxIGs with top-down decomposition.
 *
 */
template<class TT>
class mux_resyn
{
public:
  using stats = null_stats;
  using index_list_t = muxig_index_list;
  using truth_table_t = TT;

public:
  explicit mux_resyn( stats& st )
      : st( st )
  {
  }

  template<class iterator_type, class truth_table_storage_type>
  std::optional<index_list_t> operator()( TT const& target, TT const& care, iterator_type begin, iterator_type end, truth_table_storage_type const& tts, uint32_t max_size = std::numeric_limits<uint32_t>::max() )
  {
    normalized.emplace_back( ~target ); // 0 XNOR target = ~target
    normalized.emplace_back( target );  // 1 XNOR target = target

    while ( begin != end )
    {
      auto const& tt = tts[*begin];
      assert( tt.num_bits() == target.num_bits() );
      normalized.emplace_back( tt ^ normalized[0] ); // tt XNOR target = tt XOR ~target
      normalized.emplace_back( tt ^ target );        // ~tt XNOR target = tt XOR target
      divisors.emplace_back( tt );
      ++begin;
    }
    size_limit = max_size;
    num_bits = kitty::count_ones( care );
    //scores.resize( normalized.size() );

    for ( auto i = 0u; i < normalized.size(); ++i )
    {
      if ( kitty::is_const0( ~normalized.at( i ) & care ) )
      {
        /* 0-resub (including constants) */
        muxig_index_list index_list( divisors.size() );
        index_list.add_output( i );
        return index_list;
      }
    }

    if ( size_limit == 0u )
    {
      return std::nullopt;
    }

    return compute_function( care );
  }

private:
  std::optional<index_list_t> compute_function( TT const& care )
  {
    uint32_t chosen_i{0}, chosen_s{0}, max_score{0};
    for ( auto i = 0u; i < normalized.size(); ++i )
    {
      TT covered_and_cared = normalized.at( i ) & care;
      for ( auto s = 0u; s < divisors.size(); ++s )
      {
        uint32_t score = kitty::count_ones( covered_and_cared & divisors.at( s ) );
        if ( score > max_score )
        {
          max_score = score;
          chosen_i = i;
          chosen_s = s * 2;
        }

        score = kitty::count_ones( covered_and_cared & ~divisors.at( s ) );
        if ( score > max_score )
        {
          max_score = score;
          chosen_i = i;
          chosen_s = s * 2 + 1;
        }
      }
    }

    TT tt_s = chosen_s % 2 ? ~divisors.at( chosen_s / 2 ) : divisors.at( chosen_s / 2 );
    // choose the best for care & ~tt_s

    // uncovered: care & ~normalized( chosen_i ) & tt_s
    // uncovered: care & ~normalized( chosen_j ) & ~tt_s

  }

private:
  uint32_t size_limit;
  uint32_t num_bits;

  std::vector<TT> divisors;
  std::vector<TT> normalized;
  //std::vector<uint32_t> scores;

  stats& st;
}; /* mux_resyn */

} /* namespace mockturtle */