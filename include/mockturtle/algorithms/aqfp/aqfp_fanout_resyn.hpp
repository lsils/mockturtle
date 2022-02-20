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

#include <type_traits>
#include <vector>

#include "../../traits.hpp"
#include "aqfp_assumptions.hpp"

namespace mockturtle
{

/*! \brief A callback function to determine the fanout levels.
 *
 * This is intended to be used with AQFP or similar pipelined networks.
 * Levels are determined assuming that a nearly-balanced splitter tree
 * is used for each the considered fanout net.
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

     klut_network klut = ...;
     aqfp_assumptions assume = ...;

     aqfp_node_resyn node_resyn = ...;
     afp_fanout_resyn fanout_resyn{ assume };

     aqfp_network aqfp;
     auto res = mockturtle::aqfp_resynthesis( aqfp, klut, node_resyn, fanout_resyn );

   \endverbatim
 */
struct aqfp_fanout_resyn
{
  /*! \brief Default constructor.
   *
   * \param assume AQFP synthesis assumptions (only the fields `splitter_capacity` and `branc_pis` will be used)
   */
  aqfp_fanout_resyn( const aqfp_assumptions& assume )
      : splitter_capacity{ assume.splitter_capacity }, branch_pis{ assume.branch_pis } {}

  /*! \brief Determines the relative levels of fanouts of a node assuming a nearly balanced splitter tree.
   *
   * \param ntk_src Source network with `depth()` and `level()` member functions.
   * \param n Node in `ntk_src` for which the fanout levels are to be determined.
   * \param fanouts_n Fanout nodes of `n` in `ntk_dest`.
   * \param ntk_dest Destination network which is being synthesized as a pipelined network.
   * \param f The signal in `ntk_dest` that correspond to source node `n`.
   * \param level_f The level of `f` in `ntk_dest`.
   * \param fanout_node_fn Callback with arguments (source network node, level in destination network).
   * \param fanout_co_fn Callback with arguments (index of the combinational output, level in destination network).
   */
  template<typename NtkSrc, typename FOutsSrc, typename NtkDest, typename FanoutNodeCallback, typename FanoutPoCallback>
  void operator()( const NtkSrc& ntk_src, node<NtkSrc> n, FOutsSrc& fanouts_n, const NtkDest& ntk_dest, signal<NtkDest> f, uint32_t level_f,
                   FanoutNodeCallback&& fanout_node_fn, FanoutPoCallback&& fanout_co_fn )
  {
    static_assert( has_depth_v<NtkSrc>,
                   "The source network does not provide a method named depth()." );
    static_assert( has_level_v<NtkSrc>,
                   "The source network does not provide a method named level(node)." );
    static_assert( std::is_invocable_v<FanoutNodeCallback, node<NtkSrc>, uint32_t>,
                   "FanoutNodeCallback is not callable with arguments (node, level)" );
    static_assert( std::is_invocable_v<FanoutPoCallback, uint32_t, uint32_t>,
                   "FanoutNodeCallback is not callable with arguments (index, level)" );

    if ( ntk_src.fanout_size( n ) == 0 )
      return;

    auto offsets = balanced_splitter_tree_offsets( ntk_src.fanout_size( n ) );

    std::sort( fanouts_n.begin(), fanouts_n.end(), [&]( auto f1, auto f2 )
               { return ( ntk_src.depth() - ntk_src.level( f1 ) ) > ( ntk_src.depth() - ntk_src.level( f2 ) ) ||
                        ( ( ntk_src.depth() - ntk_src.level( f1 ) ) == ( ntk_src.depth() - ntk_src.level( f2 ) ) && ( f1 < f2 ) ); } );

    auto n_dest = ntk_dest.get_node( f );
    auto no_splitters = ntk_dest.is_constant( n_dest ) || ( !branch_pis && ntk_dest.is_ci( n_dest ) );

    uint32_t foind = 0u;
    for ( auto fo : fanouts_n )
    {
      fanout_node_fn( fo, no_splitters ? level_f : level_f + offsets[foind] );
      foind++;
    }

    // remaining fanouts are either combinational outputs (primary outputs or register inputs)
    for ( auto i = foind; i < ntk_src.fanout_size( n ); i++ )
    {
      auto co_index = i - foind;
      fanout_co_fn( co_index, no_splitters ? level_f : level_f + offsets[foind] );
      foind++;
    }
  }

private:
  uint32_t splitter_capacity;
  bool branch_pis;

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
    uint32_t num_leaves = splitter_capacity;

    while ( num_leaves < num_fanouts )
    {
      num_splitters += num_leaves;
      num_leaves *= splitter_capacity;
      num_levels += 1u;
    }

    // we need at least `num_levels` levels, but we might not need all
    std::vector<uint32_t> result( num_fanouts, num_levels );
    uint32_t i = 0u;
    while ( num_leaves >= num_fanouts + ( splitter_capacity - 1 ) )
    {
      num_splitters--;
      num_leaves -= ( splitter_capacity - 1 );
      result[i++]--;
    }

    return result;
  };
};

} // namespace mockturtle
