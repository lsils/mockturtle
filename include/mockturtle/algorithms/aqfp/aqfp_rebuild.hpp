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
  \file aqfp_rebuild.hpp
  \brief Rebuilds buffer-splitter tree in AQFP networks

  \author Alessandro Tempia Calvino
*/

#pragma once

#include <algorithm>
#include <list>
#include <vector>

#include "../../networks/buffered.hpp"
#include "../../networks/generic.hpp"
#include "../../utils/node_map.hpp"
#include "../../utils/stopwatch.hpp"
#include "../../views/depth_view.hpp"
#include "../../views/fanout_view.hpp"
#include "../../views/topo_view.hpp"
#include "aqfp_assumptions.hpp"
#include "aqfp_cleanup.hpp"
#include "buffer_insertion.hpp"
#include "buffer_verification.hpp"

namespace mockturtle
{

struct aqfp_reconstruct_params
{
  /*! \brief AQFP buffer insertion parameters. */
  buffer_insertion_params buffer_insertion_ps{};

  /*! \brief Randomize topological order. */
  bool det_randomization{ false };

  /*! \brief Seed for random selection of splitters to relocate. */
  std::default_random_engine::result_type seed{ 1 };

  /*! \brief Be verbose. */
  bool verbose{ false };
};

struct aqfp_reconstruct_stats
{
  /*! \brief Number of buffers and splitters after reconstruction. */
  uint32_t num_buffers{ 0 };

  /*! \brief Total runtime */
  stopwatch<>::duration total_time{ 0 };

  /*! \brief Report stats */
  void report()
  {
    std::cout << fmt::format( "[i] Buffers = {}\t Total time = {}\n", num_buffers, to_seconds( total_time ) );
  }
};

namespace detail
{

class aqfp_reconstruct_impl
{
public:
  using node = typename aqfp_network::node;
  using signal = typename aqfp_network::signal;

public:
  explicit aqfp_reconstruct_impl( buffered_aqfp_network const& ntk, aqfp_reconstruct_params const& ps, aqfp_reconstruct_stats& st )
      : _ntk( ntk ), _ps( ps ), _num_buffers( num_buffers ), _runtime( runtime )
  {
  }

  buffered_aqfp_network run()
  {
    stopwatch( st.total_time );

    /* save the level of each node */
    depth_view ntk_level{ _ntk };

    /* create a network removing the splitter trees */
    aqfp_network clean_ntk;
    node_map<signal, buffered_aqfp_network> old2new( _ntk );
    remove_splitter_trees( clean_ntk, old2new );

    /* compute the node level on the new network */
    node_map<uint32_t, aqfp_network> levels( clean_ntk );

    if ( _ps.buffer_insertion_ps.assume.branch_pis )
    {
      /* gates are in a fixed position */
      _ntk.foreach_gate( [&]( auto const& n ) {
        levels[old2new[n]] = ntk_level.level( n );
      } );
    }
    else
    {
      /* gates are not in a fixed position */
      /* gates are scheduled ALAP */

      /* if not balance POs, POs are scheduled ASAP */
      auto const levels_guess = schedule_buffered_network( _ntk, _ps.buffer_insertion_ps.assume );
      _ntk.foreach_gate( [&]( auto const& n ) {
        levels[old2new[n]] = levels_guess[n];
      } );
    }

    /* recompute splitter trees and return the new buffered network */
    buffered_aqfp_network res;
    buffer_insertion buf_inst( clean_ntk, levels, _ps.buffer_insertion_ps );
    st.num_buffers = buf_inst.run( res );
    return res;
  }

private:
  void remove_splitter_trees( aqfp_network& res, node_map<signal, buffered_aqfp_network>& old2new )
  {
    topo_view_params tps;
    tps.deterministic_randomization = _ps.det_randomization;
    tps.seed = _ps.seed;
    topo_view topo{ _ntk, tps };

    old2new[_ntk.get_constant( false )] = res.get_constant( false );

    _ntk.foreach_pi( [&]( auto const& n ) {
      old2new[n] = res.create_pi();
    } );

    topo.foreach_node( [&]( auto const& n ) {
      if ( _ntk.is_pi( n ) || _ntk.is_constant( n ) )
        return;

      std::vector<signal> children;
      _ntk.foreach_fanin( n, [&]( auto const& f ) {
        children.push_back( old2new[f] ^ _ntk.is_complemented( f ) );
      } );

      if ( _ntk.is_buf( n ) )
      {
        old2new[n] = children[0];
      }
      else if ( children.size() == 3 )
      {
        old2new[n] = res.create_maj( children[0], children[1], children[2] );
      }
      else
      {
        old2new[n] = res.create_maj( children );
      }
    } );

    _ntk.foreach_po( [&]( auto const& f ) {
      res.create_po( old2new[f] ^ _ntk.is_complemented( f ) );
    } );
  }

private:
  buffered_aqfp_network const& _ntk;
  aqfp_reconstruct_params const& _ps;
  aqfp_reconstruct_stats& st;
};

} /* namespace detail */

/*! \brief Rebuilds buffer/splitter trees in an AQFP network.
 *
 * This function rebuilds buffer/splitter trees in an AQFP network.
 *
 * \param ntk Buffered AQFP network
 */
buffered_aqfp_network aqfp_reconstruct( buffered_aqfp_network const& ntk, aqfp_reconstruct_params const& ps = {}, aqfp_reconstruct_stats* pst = nullptr )
{
  aqfp_reconstruct_stats st;

  detail::aqfp_reconstruct_impl p( ntk, ps, st );
  auto res = p.run();

  if ( pst )
    *pst = st;

  if ( ps.verbose )
    st.report();

  return res;
}

} // namespace mockturtle