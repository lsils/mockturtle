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
  \file aqfp_fanout_resyn.hpp
  \brief AQFP fanout resynthesis strategy

  \author Dewmini Marakkalage 
*/

#pragma once

#include "../../traits.hpp"

#include <type_traits>
#include <vector>

namespace mockturtle
{

struct aqfp_fanout_resyn
{
  aqfp_fanout_resyn( uint32_t branching_factor ) : branching_factor( branching_factor ) {}

  /*! \brief Determines the relative levels of fanouts of a node assuming a nearly balanced splitter tree. */
  template<typename NtkSrc, typename NtkDest, typename FanoutNodeCallback, typename FanoutPoCallback>
  void operator()( const NtkSrc& ntk_src, node<NtkSrc> n, const NtkDest& ntk_dest, signal<NtkDest> f, uint32_t level_f,
                   FanoutNodeCallback&& fanout_node_fn, FanoutPoCallback&& fanout_co_fn )
  {
    static_assert( has_foreach_fanout_v<NtkSrc>, "NtkSource does not implement the foreach_fanout method" );
    static_assert( std::is_invocable_v<FanoutNodeCallback, node<NtkSrc>, uint32_t>, "FanoutNodeCallback is not callable with arguments (node, level)" );
    static_assert( std::is_invocable_v<FanoutPoCallback, uint32_t, uint32_t>, "FanoutNodeCallback is not callable with arguments (index, level)" );

    auto offsets = balanced_splitter_tree_offsets( ntk_src.fanout_size( n ) );

    std::vector<node<NtkSrc>> fanouts;
    ntk_src.foreach_fanout( n, [&]( auto fo ) { fanouts.push_back( fo ); } );
    std::sort( fanouts.begin(), fanouts.end(), [&]( auto f1, auto f2 ) {
      return ( ntk_src.depth() - ntk_src.level( f1 ) ) > ( ntk_src.depth() - ntk_src.level( f2 ) );
    } );

    auto n_dest = ntk_dest.get_node( f );
    auto no_splitters = ntk_dest.is_constant( n_dest ) || ntk_dest.is_ci( n_dest );

    uint32_t foind = 0u;
    for ( auto fo : fanouts )
    {
      fanout_node_fn( fo, no_splitters ? level_f : level_f + offsets[foind++] );
    }

    // remaining fanouts are either combinational outputs (primary outputs for register inputs)
    for ( auto i = foind; i < ntk_src.fanout_size( n ); i++ )
    {
      auto co_index = i - foind;
      fanout_co_fn( co_index, no_splitters ? level_f : level_f + offsets[foind++] );
    }
  }

private:
  uint32_t branching_factor;

  /*! \brief Determines the relative levels of the fanouts of a balanced splitter tree with `num_fanouts` many fanouts. */
  std::vector<uint32_t> balanced_splitter_tree_offsets( uint32_t num_fanouts )
  {
    if ( num_fanouts == 1u )
    {
      return { 0u };
    }

    // to get the minimum level, choose the splitters with max fanout size

    uint32_t num_splitters = 1u;
    uint32_t num_levels = 1u;
    uint32_t num_leaves = branching_factor;

    while ( num_leaves < num_fanouts )
    {
      num_splitters += num_leaves;
      num_leaves *= branching_factor;
      num_levels += 1u;
    }

    // we need at least `num_levels` levels, but we might not need all
    std::vector<uint32_t> result( num_fanouts, num_levels );
    uint32_t i = 0u;
    while ( num_leaves >= num_fanouts + ( branching_factor - 1 ) )
    {
      num_splitters--;
      num_leaves -= ( branching_factor - 1 );
      result[i++]--;
    }

    return result;
  };
};

} // namespace mockturtle
