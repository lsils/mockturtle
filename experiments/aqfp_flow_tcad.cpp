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

/*
  \file aqfp_flow_aspdac.cpp
  \brief AQFP synthesis flow

  This file contains the code to reproduce the experiment (Table I)
  in the following paper:
  "Depth-optimal Buffer and Splitter Insertion and Optimization in AQFP Circuits",
  ASP-DAC 2023, by Alessandro Tempia Calvino and Giovanni De Micheli.

  This version runs on the ISCAS benchmarks. The benchmarks for Table 1 can be
  downloaded at https://github.com/lsils/SCE-benchmarks
 */

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <mockturtle/algorithms/aqfp/aqfp_cleanup.hpp>
#include <mockturtle/algorithms/aqfp/aqfp_rebuild.hpp>
#include <mockturtle/algorithms/aqfp/aqfp_retiming.hpp>
#include <mockturtle/algorithms/aqfp/buffer_insertion.hpp>
#include <mockturtle/algorithms/aqfp/buffer_verification.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aqfp.hpp>
#include <mockturtle/networks/buffered.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <experiments.hpp>

using namespace mockturtle;

struct global_stats
{
  uint32_t num_jjs{ 0 };
  uint32_t num_bufs{ 0 };
  uint32_t jj_depth{ 0 };
  double time{ 0 };
};

buffered_aqfp_network aqfp_buffer_insertion( aqfp_network const& aqfp, buffer_insertion_params const& ps, global_stats& st )
{
  /* buffer insertion: ALAP */
  stopwatch<>::duration time_insertion{ 0 };
  buffer_insertion buf_inst( aqfp, ps );
  buffered_aqfp_network buffered_aqfp;
  st.num_bufs = call_with_stopwatch( time_insertion, [&]() { return buf_inst.run( buffered_aqfp ); } );
  st.num_jjs = aqfp.num_gates() * 6 + st.num_bufs * 2;
  st.jj_depth = buf_inst.depth();
  st.time = to_seconds( time_insertion );

  return buffered_aqfp;
}

buffered_aqfp_network aqfp_buffer_optimize( buffered_aqfp_network& start, aqfp_assumptions_legacy const& aqfp_ps, bool direction, global_stats& st )
{
  /* retiming params */
  aqfp_retiming_params aps;
  aps.aqfp_assumptions_ps = aqfp_ps;
  aps.backwards_first = direction;
  aps.iterations = 250;
  aps.retime_splitters = true;

  /* chunk movement params */
  buffer_insertion_params buf_ps;
  buf_ps.scheduling = buffer_insertion_params::provided;
  buf_ps.optimization_effort = buffer_insertion_params::one_pass;
  buf_ps.max_chunk_size = 100;
  buf_ps.assume = legacy_to_realistic( aqfp_ps );

  /* aqfp network */
  buffered_aqfp_network buffered_aqfp;

  /* first retiming */
  {
    aqfp_retiming_stats ast;
    auto buf_aqfp_ret = aqfp_retiming( start, aps, &ast );
    st.time += to_seconds( ast.time_total );
    buffered_aqfp = buf_aqfp_ret;
  }

  /* repeat loop */
  uint32_t iterations = 10;
  aps.det_randomization = true;
  while ( iterations-- > 0 )
  {
    uint32_t size_previous = buffered_aqfp.size();

    /* chunk movement */
    aqfp_reconstruct_params reconstruct_ps;
    aqfp_reconstruct_stats reconstruct_st;
    reconstruct_ps.buffer_insertion_ps = buf_ps;
    auto buf_aqfp_chunk = aqfp_reconstruct( buffered_aqfp, reconstruct_ps, &reconstruct_st );
    st.time += to_seconds( reconstruct_st.total_time );

    /* retiming */
    aqfp_retiming_stats ast;
    auto buf_aqfp_ret = aqfp_retiming( buf_aqfp_chunk, aps, &ast );
    st.time += to_seconds( ast.time_total );

    if ( buf_aqfp_ret.size() >= size_previous )
      break;

    buffered_aqfp = buf_aqfp_ret;
  }

  /* compute JJ cost */
  st.num_jjs = 0;
  st.num_bufs = 0;
  st.jj_depth = depth_view<buffered_aqfp_network>( buffered_aqfp ).depth();

  buffered_aqfp.foreach_node( [&]( auto const& n ) {
    if ( buffered_aqfp.is_pi( n ) || buffered_aqfp.is_constant( n ) )
      return;
    if ( buffered_aqfp.is_buf( n ) )
    {
      st.num_bufs++;
      st.num_jjs += 2;
    }
    else
    {
      st.num_jjs += 6;
    }
  } );

  return buffered_aqfp;
}

int main()
{
  using namespace experiments;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, double, uint32_t, uint32_t, uint32_t, double, bool> exp(
      "aqfp_retiming", "Bench", "Size_init", "Depth_init", "B/S_sched", "JJs_sched", "Depth_sched", "Time_sched (s)", "B/S_fin", "JJs_fin", "Depth_fin", "Time (s)", "cec" );

  uint32_t total_jjs = 0;
  uint32_t total_bufs = 0;

  for ( auto const& benchmark : iscas_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    mig_network mig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( mig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* MIG-based logic optimization can be added here */
    auto mig_opt = cleanup_dangling( mig );

    const uint32_t size_before = mig_opt.num_gates();
    const uint32_t depth_before = depth_view( mig_opt ).depth();

    /* convert MIG network to AQFP */
    aqfp_network aqfp = cleanup_dangling<mig_network, aqfp_network>( mig_opt );

    aqfp_assumptions_legacy aqfp_ps;
    aqfp_ps.splitter_capacity = 4;
    aqfp_ps.branch_pis = true;
    aqfp_ps.balance_pis = true;
    aqfp_ps.balance_pos = true;

    /* Buffer insertion params */
    buffer_insertion_params buf_ps;
    buf_ps.optimization_effort = buffer_insertion_params::none;
    buf_ps.max_chunk_size = 100;
    buf_ps.assume = legacy_to_realistic( aqfp_ps );

    /* Buffer insertion: ALAP */
    global_stats alap_stats;
    buf_ps.scheduling = buffer_insertion_params::ALAP_depth;
    buffered_aqfp_network buffered_aqfp_alap = aqfp_buffer_insertion( aqfp, buf_ps, alap_stats );

    /* Buffer insertion: ASAP */
    global_stats asap_stats;
    buf_ps.scheduling = buffer_insertion_params::ASAP_depth;
    buffered_aqfp_network buffered_aqfp_asap = aqfp_buffer_insertion( aqfp, buf_ps, asap_stats );

    /* save the results for the best one */
    global_stats best_stats;
    if ( asap_stats.num_bufs < alap_stats.num_bufs )
    {
      best_stats = asap_stats;
    }
    else
    {
      best_stats = alap_stats;
    }
    best_stats.time = alap_stats.time + asap_stats.time;

    double total_runtime = best_stats.time;

    /* Optimize ALAP */
    global_stats alap_opt_stats;
    buffered_aqfp_network buffered_aqfp_alap_opt = aqfp_buffer_optimize( buffered_aqfp_alap, aqfp_ps, false, alap_opt_stats );

    /* Optimize ASAP */
    global_stats asap_opt_stats;
    buffered_aqfp_network buffered_aqfp_asap_opt = aqfp_buffer_optimize( buffered_aqfp_asap, aqfp_ps, true, asap_opt_stats );

    total_runtime += alap_opt_stats.time + asap_opt_stats.time;

    /* Commit the best */
    global_stats best_opt_stats;
    buffered_aqfp_network buffered_aqfp_best;
    if ( asap_opt_stats.num_bufs < alap_opt_stats.num_bufs )
    {
      buffered_aqfp_best = buffered_aqfp_asap_opt;
      best_opt_stats = asap_opt_stats;
    }
    else
    {
      buffered_aqfp_best = buffered_aqfp_alap_opt;
      best_opt_stats = alap_opt_stats;
    }

    total_bufs += best_opt_stats.num_bufs;
    total_jjs += best_opt_stats.num_jjs;

    /* cec */
    auto cec = abc_cec( buffered_aqfp_best, benchmark );
    std::vector<uint32_t> pi_levels;
    for ( auto i = 0u; i < buffered_aqfp_best.num_pis(); ++i )
      pi_levels.emplace_back( 0 );
    cec &= verify_aqfp_buffer( buffered_aqfp_best, aqfp_ps, pi_levels );

    exp( benchmark, size_before, depth_before, best_stats.num_bufs, best_stats.num_jjs, best_stats.jj_depth, best_stats.time, best_opt_stats.num_bufs, best_opt_stats.num_jjs, best_opt_stats.jj_depth, total_runtime, cec );
  }

  exp.save();
  exp.table();

  std::cout << fmt::format( "[i] Total B/S = {} \tTotal JJs = {}\n", total_bufs, total_jjs );

  return 0;
}
