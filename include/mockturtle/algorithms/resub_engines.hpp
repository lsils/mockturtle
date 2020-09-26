/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2020  EPFL
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
  \file resub_engines.hpp
  \brief Implements generalized resubstitution engine(s).

  \author Siang-Yun Lee
*/

#pragma once

#include <kitty/kitty.hpp>

namespace mockturtle
{
template<class TT, bool use_top_down = true>
class mig_resub_engine
{
  /*! \brief Internal data structure */
  struct maj_node
  {
    uint32_t id; /* ( id - divisors.size() ) / 2 = its position in maj_nodes */
    TT care; /* the care bits; not used in bottom-up approach */
    std::vector<uint32_t> fanins; /* ids of its three fanins */
  };

public:
  explicit mig_resub_engine( uint64_t num_divisors, uint64_t max_num_divisors = 50ul )
    : max_num_divisors( max_num_divisors ), counter( 2u ), divisors( ( num_divisors + 1 ) * 2 ), scores( ( num_divisors + 1 ) * 2 )
  { }

  template<class node_type, class truth_table_storage_type>
  void add_root( node_type const& node, truth_table_storage_type const& tts )
  {
    target = tts[node];
    divisors.at( 1u ) = get_const1();
    divisors.at( 0u ) = ~divisors.at( 1u ); /* const 0 */
  }

  template<class node_type, class truth_table_storage_type>
  void add_divisor( node_type const& node, truth_table_storage_type const& tts )
  {
    divisors.at( counter++ ) = tts[node];
    divisors.at( counter++ ) = ~tts[node];
  }

  template<class iterator_type, class truth_table_storage_type>
  void add_divisors( iterator_type begin, iterator_type end, truth_table_storage_type const& tts )
  { 
    // counter = 2u;
    while ( begin != end )
    {
      add_divisor( *begin, tts );
      ++begin;
    }
  }

  std::optional<std::vector<uint32_t>> compute_function( uint32_t num_inserts, bool use_XOR = false )
  {
    /* THINK: When size limit is reached, do we go back and try other choices if there are tied scores? */
    /* OPTIMIZE: Only save one of the polarities (the better one) of the divisors. Indexing also needs to be adjusted. */
    assert( use_XOR == false ); (void)use_XOR; // not supported yet

    uint64_t max_score = 0u;
    uint32_t max_i = 0u;
    for ( auto i = 0u; i < divisors.size(); ++i )
    {
      scores.at( i ) = kitty::count_zeros( target ^ divisors.at( i ) );
      if ( scores.at( i ) > max_score )
      {
        max_score = scores.at( i );
        max_i = i;
        if ( max_score == target.num_bits() )
        {
          break;
        }
      }
    }
    /* 0-resub (including constants) */
    if ( max_score == target.num_bits() )
    {
      return std::vector<uint32_t>( {max_i} );
    }

    if ( num_inserts == 0u )
    {
      return std::nullopt;
    }
    size_limit = num_inserts;

    /* the first node with the first fanin decided */
    maj_nodes.emplace_back( maj_node{uint32_t(divisors.size()), get_const1(), {max_i}} );

    if constexpr ( use_top_down )
    {
      return top_down_approach( 0u );
    }
    else
    {
      return bottom_up_approach( divisors.at( max_i ) );
    }
  }

private:
  TT get_const1() const { return target | ~target; }

  std::optional<std::vector<uint32_t>> bottom_up_approach( TT const& function_i )
  {
    /* THINK: Should we consider reusing newly-built nodes (nodes in maj_nodes) in addition to divisors? */
    assert( maj_nodes.back().fanins.size() == 1u );

    /* the second fanin */
    uint64_t max_score = 0u;
    uint32_t max_j = 0u;
    auto const not_covered_by_i = target ^ function_i;
    for ( auto j = 0u; j < divisors.size(); ++j )
    {
      auto const covered_by_j = ~( target ^ divisors.at( j ) );
      scores.at( j ) = kitty::count_ones( covered_by_j ) + kitty::count_ones( not_covered_by_i & covered_by_j );
      if ( scores.at( j ) > max_score )
      {
        max_score = scores.at( j );
        max_j = j;
      }
    }
    maj_nodes.back().fanins.emplace_back( max_j );

    /* the third fanin */
    max_score = 0u;
    uint32_t max_k = 0u;
    auto const disagree_in_ij = function_i ^ divisors.at( max_j );
    for ( auto k = 0u; k < divisors.size(); ++k )
    {      
      scores.at( k ) = kitty::count_ones( ~( target ^ divisors.at( k ) ) & disagree_in_ij );
      if ( scores.at( k ) > max_score )
      {
        max_score = scores.at( k );
        max_k = k;
      }
    }
    maj_nodes.back().fanins.emplace_back( max_k );

    auto const current_function = kitty::ternary_majority( function_i, divisors.at( max_j ), divisors.at( max_k ) );
    if ( current_function == target )
    {
      std::vector<uint32_t> index_list;
      for ( auto& n : maj_nodes )
      {
        index_list.emplace_back( n.fanins.at( 0u ) );
        index_list.emplace_back( n.fanins.at( 1u ) );
        index_list.emplace_back( n.fanins.at( 2u ) );
      }
      index_list.emplace_back( maj_nodes.back().id );
      return index_list;
    }
    else if ( maj_nodes.size() < size_limit )
    {
      maj_nodes.emplace_back( maj_node{maj_nodes.back().id + 2u, current_function, {maj_nodes.back().id}} );
      return bottom_up_approach( current_function );
    }
    else
    {
      return std::nullopt;
    }
  }

  std::optional<std::vector<uint32_t>> top_down_approach( uint32_t node_position )
  {
    maj_node const& current_node = maj_nodes.at( node_position );
    TT const& care = current_node.care;
    assert( current_node.fanins.size() == 1u );

    /* the second fanin */
    uint64_t max_score = 0u;
    uint32_t max_j = 0u;
    auto const not_covered_by_i = target ^ function_i;
    for ( auto j = 0u; j < divisors.size(); ++j )
    {
      auto const covered_by_j = ~( target ^ divisors.at( j ) );
      scores.at( j ) = kitty::count_ones( covered_by_j ) + kitty::count_ones( not_covered_by_i & covered_by_j );
      if ( scores.at( j ) > max_score )
      {
        max_score = scores.at( j );
        max_j = j;
      }
    }
    current_node.fanins.emplace_back( max_j );

    /* the third fanin */
    max_score = 0u;
    uint32_t max_k = 0u;
    auto const not_covered_by_j = target ^ divisors.at( max_j );
    for ( auto k = 0u; k < divisors.size(); ++k )
    {
      auto const covered_by_k = ~( target ^ divisors.at( k ) );
      scores.at( k ) = kitty::count_ones( covered_by_k & not_covered_by_i ) + kitty::count_ones( covered_by_k & not_covered_by_j );
      if ( scores.at( k ) > max_score )
      {
        max_score = scores.at( k );
        max_k = k;
      }
    }
    current_node.fanins.emplace_back( max_k );

    auto const current_function = kitty::ternary_majority( function_i, divisors.at( max_j ), divisors.at( max_k ) );
    if ( current_function == target )
    {
      std::vector<uint32_t> index_list;
      // TODO
      return index_list;
    }
    else if ( maj_nodes.size() < size_limit )
    {
      maj_nodes.emplace_back( maj_node{maj_nodes.back().id + 2u, care & ( ( target ^ divisors.at( max_j ) ) + ( target ^ divisors.at( max_k ) ) ), {current_node.fanins[0]}} );
      current_node.fanins[0] = maj_nodes.back().id + 2u;
      // refine this new node. if not successful, try refining the second fanin (breadth-first)
    }
    else
    {
      return std::nullopt;
    }
  }

  bool refine()

private:
  uint64_t max_num_divisors;
  uint64_t counter;
  uint32_t size_limit;

  TT target;
  std::vector<TT> divisors;
  std::vector<uint64_t> scores;
  std::vector<maj_node> maj_nodes;
}; /* mig_resub_engine */

} /* namespace mockturtle */
