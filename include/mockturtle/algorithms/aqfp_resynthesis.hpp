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
  \file aqfp_resynthesis.hpp
  \brief Resynthesis of path balanced networks

  \author Heinz Riener
  \author Mathias Soeken
  \author Max Austin
  \author Dewmini Marakkalage 
*/

#pragma once

#include <iostream>
#include <unordered_map>

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/depth_view.hpp"
#include "../views/fanout_view.hpp"
#include "../views/topo_view.hpp"

#include <fmt/format.h>

namespace mockturtle
{

/*! \brief Parameters for aqfp_resynthesis.
 *
 * The data structure `aqfp_resynthesis_params` holds configurable parameters
 * with default arguments for `node_resynthesis`.
 */
struct aqfp_resynthesis_params
{
  bool verbose{ false };
};

/*! \brief Statistics of aqfp_resynthesis.
 *
 * The data structure `aqfp_resynthesis_stats` holds data collected during aqfp_resynthesis.  
 */
struct aqfp_resynthesis_stats
{
  stopwatch<>::duration time_total{ 0 };

  void report() const
  {
    std::cout << fmt::format( "[i] total time = {:>8.2f} secs\n", to_seconds( time_total ) );
  }
};

/*! \brief Results of aqfp_resynthesis.
 *
 * The data structure `aqfp_resynthesis_result` holds the resulting level assignment of nodes 
 * and the level of the critical primary output.
 */
template<typename NtkDest>
struct aqfp_resynthesis_result
{
  std::unordered_map<node<NtkDest>, uint32_t> node_level;
  uint32_t po_level = 0u;
};

namespace detail
{

template<typename NtkDest, typename NtkSrc, typename NodeResynFn, typename FanoutResynFn>
class aqfp_resynthesis_impl
{
public:
  aqfp_resynthesis_impl(
      NtkDest& ntk_dest,
      NtkSrc const& ntk_src,
      NodeResynFn&& node_resyn_fn,
      FanoutResynFn&& fanout_resyn_fn,
      aqfp_resynthesis_params const& ps,
      aqfp_resynthesis_stats& st )
      : ntk_dest( ntk_dest ),
        ntk_src( ntk_src ),
        node_resyn_fn( node_resyn_fn ),
        fanout_resyn_fn( fanout_resyn_fn ),
        ps( ps ),
        st( st )
  {
  }

  aqfp_resynthesis_result<NtkDest> run()
  {
    stopwatch t( st.time_total );

    node_map<signal<NtkDest>, NtkSrc> node2new( ntk_src );
    node_map<uint32_t, NtkSrc> level_of_src_node( ntk_src );

    std::unordered_map<node<NtkDest>, uint32_t> level_of_node;  
    std::map<std::pair<node<NtkSrc>, node<NtkSrc>>, uint32_t> level_for_fanout;
    uint32_t critical_po_level = 0;

    /* map constants */
    auto c0 = ntk_dest.get_constant( false );
    node2new[ntk_src.get_node( ntk_src.get_constant( false ) )] = c0;
    level_of_node[ntk_dest.get_node( c0 )] = 0u;

    if ( ntk_src.get_node( ntk_src.get_constant( true ) ) != ntk_src.get_node( ntk_src.get_constant( false ) ) )
    {
      auto c1 = ntk_dest.get_constant( true );
      node2new[ntk_src.get_node( ntk_src.get_constant( true ) )] = c1;
      level_of_node[ntk_dest.get_node( c1 )] = 0u;
    }

    /* map primary inputs */
    ntk_src.foreach_pi( [&]( auto n ) {
      auto pi = ntk_dest.create_pi();
      node2new[n] = pi;
      level_of_node[ntk_dest.get_node( pi )] = 0u;

      if constexpr ( has_has_name_v<NtkSrc> && has_get_name_v<NtkSrc> && has_set_name_v<NtkDest> )
      {
        if ( ntk_src.has_name( ntk_src.make_signal( n ) ) )
          ntk_dest.set_name( node2new[n], ntk_src.get_name( ntk_src.make_signal( n ) ) );
      }
    } );

    /* map register outputs */
    ntk_src.foreach_ro( [&]( auto n ) {
      auto ro = ntk_dest.create_ro();
      node2new[n] = ro;
      level_of_node[ntk_dest.get_node( ro )] = 0u;

      ntk_dest._storage->latch_information[ntk_dest.get_node( node2new[n] )] = ntk_src._storage->latch_information[n];
      if constexpr ( has_has_name_v<NtkSrc> && has_get_name_v<NtkSrc> && has_set_name_v<NtkDest> )
      {
        if ( ntk_src.has_name( ntk_src.make_signal( n ) ) )
          ntk_dest.set_name( node2new[n], ntk_src.get_name( ntk_src.make_signal( n ) ) );
      }
    } );

    fanout_view ntk_fanout{ ntk_src };
    depth_view ntk_depth{ ntk_fanout };
    topo_view ntk_topo{ ntk_depth };

    /* map nodes */
    ntk_topo.foreach_node( [&]( auto n ) {
      if ( ntk_topo.is_constant( n ) || ntk_topo.is_ci( n ) )
        return;

      /* synthesize node `n` */
      std::vector<std::pair<signal<NtkDest>, uint32_t>> children;
      ntk_topo.foreach_fanin( n, [&]( auto const& f ) {
        children.push_back( { ntk_topo.is_complemented( f ) ? ntk_dest.create_not( node2new[f] ) : node2new[f], level_for_fanout[{ ntk_topo.get_node( f ), n }] } );
      } );

      auto performed_resyn = false;

      auto level_update_callback =
          [&]( const auto& n, uint32_t level ) {
            if ( !level_of_node.count( n ) )
            {
              level_of_node[n] = level;
            }
          };

      auto resyn_performed_callback =
          [&]( const auto& f, auto new_level ) {
            node2new[n] = f;
            level_of_src_node[n] = new_level;

            if constexpr ( has_has_name_v<NtkSrc> && has_get_name_v<NtkSrc> && has_set_name_v<NtkDest> )
            {
              if ( ntk_topo.has_name( ntk_topo.make_signal( n ) ) )
                ntk_dest.set_name( f, ntk_topo.get_name( ntk_topo.make_signal( n ) ) );
            }

            performed_resyn = true;
          };

      node_resyn_fn( ntk_dest, ntk_topo.node_function( n ), children.begin(), children.end(), level_update_callback, resyn_performed_callback );

      if ( !performed_resyn )
      {
        fmt::print( "[e] could not perform resynthesis for node {} in node_resynthesis\n", ntk_topo.node_to_index( n ) );
        std::abort();
      }

      /* synthesize fanout net of `n`*/
      auto fanout_node_callback = [&]( const auto& f, const auto& level ) {
        level_for_fanout[{ n, f }] = level;
      };

      auto fanout_po_callback = [&]( const auto& index, const auto& level ) {
        (void)index;
        critical_po_level = std::max( critical_po_level, level );
      };

      fanout_resyn_fn( ntk_topo, n, ntk_dest, node2new[n], level_of_src_node[n], fanout_node_callback, fanout_po_callback );
    } );

    /* map primary outputs */
    ntk_src.foreach_po( [&]( auto const& f, auto index ) {
      (void)index;

      auto const o = ntk_src.is_complemented( f ) ? !node2new[f] : node2new[f];
      ntk_dest.create_po( o );

      if constexpr ( has_has_output_name_v<NtkSrc> && has_get_output_name_v<NtkSrc> && has_set_output_name_v<NtkDest> )
      {
        if ( ntk_src.has_output_name( index ) )
        {
          ntk_dest.set_output_name( index, ntk_src.get_output_name( index ) );
        }
      }
    } );

    ntk_src.foreach_ri( [&]( auto const& f, auto index ) {
      (void)index;

      auto const o = ntk_src.is_complemented( f ) ? ntk_dest.create_not( node2new[f] ) : node2new[f];
      ntk_dest.create_ri( o );

      if constexpr ( has_has_output_name_v<NtkSrc> && has_get_output_name_v<NtkSrc> && has_set_output_name_v<NtkDest> )
      {
        if ( ntk_src.has_output_name( index ) )
        {
          ntk_dest.set_output_name( index + ntk_src.num_pos(), ntk_src.get_output_name( index + ntk_src.num_pos() ) );
        }
      }
    } );

    return { level_of_node, critical_po_level };
  }

private:
  NtkDest& ntk_dest;
  NtkSrc const& ntk_src;
  NodeResynFn&& node_resyn_fn;
  FanoutResynFn&& fanout_resyn_fn;
  aqfp_resynthesis_params const& ps;
  aqfp_resynthesis_stats& st;
};

} /* namespace detail */

/*! \brief Path balanced resynthesis algorithm. */
template<class NtkDest, class NtkSrc, class NodeResynFn, class FanoutResynFn>
aqfp_resynthesis_result<NtkDest> aqfp_resynthesis(
    NtkDest& ntk_dest,
    NtkSrc const& ntk_src,
    NodeResynFn&& node_resyn_fn,
    FanoutResynFn&& fanout_resyn_fn,
    aqfp_resynthesis_params const& ps = { false },
    aqfp_resynthesis_stats* pst = nullptr )
{
  static_assert( is_network_type_v<NtkSrc>, "NtkSrc is not a network type" );
  static_assert( is_network_type_v<NtkDest>, "NtkDest is not a network type" );

  static_assert( has_get_node_v<NtkSrc>, "NtkSrc does not implement the get_node method" );
  static_assert( has_get_constant_v<NtkSrc>, "NtkSrc does not implement the get_constant method" );
  static_assert( has_foreach_pi_v<NtkSrc>, "NtkSrc does not implement the foreach_pi method" );
  static_assert( has_foreach_node_v<NtkSrc>, "NtkSrc does not implement the foreach_node method" );
  static_assert( has_is_constant_v<NtkSrc>, "NtkSrc does not implement the is_constant method" );
  static_assert( has_is_pi_v<NtkSrc>, "NtkSrc does not implement the is_pi method" );
  static_assert( has_is_complemented_v<NtkSrc>, "NtkSrc does not implement the is_complemented method" );
  static_assert( has_foreach_fanin_v<NtkSrc>, "NtkSrc does not implement the foreach_fanin method" );
  static_assert( has_node_function_v<NtkSrc>, "NtkSrc does not implement the node_function method" );
  static_assert( has_foreach_po_v<NtkSrc>, "NtkSrc does not implement the foreach_po method" );

  static_assert( has_get_constant_v<NtkDest>, "NtkDest does not implement the get_constant method" );
  static_assert( has_create_pi_v<NtkDest>, "NtkDest does not implement the create_pi method" );
  static_assert( has_create_not_v<NtkDest>, "NtkDest does not implement the create_not method" );
  static_assert( has_create_po_v<NtkDest>, "NtkDest does not implement the create_po method" );

  aqfp_resynthesis_stats st;

  detail::aqfp_resynthesis_impl<NtkDest, NtkSrc, NodeResynFn, FanoutResynFn> p( ntk_dest, ntk_src, node_resyn_fn, fanout_resyn_fn, ps, st );
  auto result = p.run();

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }

  return result;
}

} // namespace mockturtle
