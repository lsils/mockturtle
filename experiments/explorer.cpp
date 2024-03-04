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

#ifdef ENABLE_ABC

#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/explorer.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/algorithms/aqfp/buffer_insertion.hpp>
#include <mockturtle/algorithms/aqfp/buffer_verification.hpp>
#include <mockturtle/networks/buffered.hpp>

#include <experiments.hpp>
#include <fmt/format.h>
#include <string>

using namespace mockturtle;

static const std::string benchmark_repo_path = "../../SCE-benchmarks";

/* AQFP benchmarks */
std::vector<std::string> aqfp_benchmarks = {
    "5xp1", "c1908", "c432", "c5315", "c880", "chkn", "count", "dist", "in5", "in6", "k2",
    "m3", "max512", "misex3", "mlp4", "prom2", "sqr6", "x1dn" };

std::string benchmark_aqfp_path( std::string const& benchmark_name )
{
  return fmt::format( "{}/MCNC/original/{}.v", benchmark_repo_path, benchmark_name );
}

mig_network ale_flow( aig_network const& ntk )
{
    aig_network aig = call_abc_script( ntk, "&c2rs" );
    mig_network _ntk = cleanup_dangling<aig_network, mig_network>( aig );
    mig_npn_resynthesis resyn{ true };
    exact_library_params eps;
    eps.np_classification = false;
    eps.compute_dc_classes = true;
    exact_library<mig_network> exact_lib( resyn, eps );
    
    map_params mps;
    mps.skip_delay_round = true;
    mps.required_time = std::numeric_limits<double>::max();
    mps.ela_rounds = 2;
    mps.enable_logic_sharing = true;
    mps.use_dont_cares = true;
    mps.window_size = 12;
    mps.logic_sharing_cut_limit = 1;
    mps.cut_enumeration_ps.cut_limit = 8;
    
    rewrite_params rwps;
    rwps.allow_zero_gain = true;
    rwps.window_size = 8;

    resubstitution_params rsps;
    rsps.max_inserts = 2;
    rsps.max_pis = 8;
    
    for ( int i = 0; i < 3; ++i )
    {
      auto tmp = map( _ntk, exact_lib, mps );
      if ( tmp.size() >= _ntk.size() )
        break;
      _ntk = tmp;
    }

    for ( int i = 0; i < 3; ++i )
    {
      rwps.use_dont_cares = i == 1;
      auto size_before = _ntk.size();
      rewrite( _ntk, exact_lib, rwps );
      _ntk = cleanup_dangling( _ntk );
      if ( _ntk.size() >= size_before )
        break;
    }

    while ( true )
    {
      auto size_before = _ntk.size();
      auto mig_resub = cleanup_dangling( _ntk );

      depth_view depth_mig{ mig_resub };
      fanout_view fanout_mig{ depth_mig };

      mig_resubstitution( fanout_mig, rsps );
      mig_resub = cleanup_dangling( mig_resub );

      if ( mig_resub.size() >= size_before )
        break;
      _ntk = mig_resub;
    }

    rsps.max_inserts = std::numeric_limits<uint32_t>::max();
    sim_resubstitution( _ntk, rsps );
    _ntk = cleanup_dangling( _ntk );
    return _ntk;
}

int main_aqfp( int argc, char* argv[] )
{
  using namespace experiments;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, bool, bool> exp( "deepsyn_aqfp", "benchmark", "#JJ", "JJ depth", "JJ EDP", "MIG size", "MIG depth", "t total", "t eval", "cec", "verified" );

  for ( auto const& benchmark : aqfp_benchmarks )
  {
    if ( argc == 2 && benchmark != std::string( argv[1] ) ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    mig_network ntk;
    if ( lorina::read_verilog( benchmark_aqfp_path( benchmark ), verilog_reader( ntk ) ) != lorina::return_code::success )
    {
      std::cerr << "Cannot read " << benchmark << "\n";
      return -1;
    }

    explorer_params ps;
    explorer_stats st;
    ps.num_restarts = 5;
    ps.random_seed = 3252;
    ps.max_steps_no_impr = 50;
    ps.timeout = 100;
    ps.compressing_scripts_per_step = 3;
    ps.verbose = true;
    //ps.very_verbose = true;

    mig_network opt = deepsyn_aqfp( ntk, ps, &st );
    depth_view d( opt );

    aqfp_assumptions_legacy aqfp_ps;
    aqfp_ps.splitter_capacity = 4;
    aqfp_ps.branch_pis = true;
    aqfp_ps.balance_pis = true;
    aqfp_ps.balance_pos = true;

    buffer_insertion_params bps;
    bps.assume = legacy_to_realistic( aqfp_ps );
    bps.scheduling = buffer_insertion_params::better_depth;
    bps.optimization_effort = buffer_insertion_params::until_sat;
    buffer_insertion buf_inst( opt, bps );

    buffered_mig_network buffered_mig;
    auto num_buffers = buf_inst.run( buffered_mig );
    auto JJ_depth = buf_inst.depth();
    auto JJ_count = opt.num_gates() * 6 + num_buffers * 2;
    auto JJ_EDP = JJ_depth * JJ_count;
    bool cec = abc_cec_impl( buffered_mig, benchmark_aqfp_path( benchmark ) );
    bool bv = verify_aqfp_buffer( buffered_mig, bps.assume, buf_inst.pi_levels() );
    //write_verilog( buffered_mig, benchmark + ".v" );

    exp( benchmark, JJ_count, JJ_depth, JJ_EDP, opt.num_gates(), d.depth(), to_seconds(st.time_total), to_seconds(st.time_evaluate), cec, bv );
  }

  exp.save();
  exp.table();

  return 0;
}

int main( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, float, bool> exp( "deepsyn_mig", "benchmark", "size_before", "size_after", "depth", "runtime", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    if ( argc == 2 && benchmark != std::string( argv[1] ) ) continue;
    //if ( benchmark == "hyp" && argc == 1 ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    using Ntk = mig_network;
    Ntk ntk;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( ntk ) ) != lorina::return_code::success )
    {
      std::cerr << "Cannot read " << benchmark << "\n";
      return -1;
    }

    explorer_params ps;
    ps.num_restarts = 4;
    ps.random_seed = 42124;
    ps.timeout = std::max(1000u, ntk.num_gates()/10);
    ps.max_steps_no_impr = 50;
    ps.compressing_scripts_per_step = 1;
    ps.verbose = true;
    //ps.very_verbose = true;

    stopwatch<>::duration rt{0};
    Ntk opt = call_with_stopwatch( rt, [&](){ return deepsyn_mig_v1( ntk, ps ); } );
    //write_verilog( opt, "best_MIGs/" + benchmark + ".v" );
    bool const cec = ( benchmark == "hyp" ) ? true : abc_cec_impl( opt, benchmark_path( benchmark ) );
    depth_view d( opt );

    exp( benchmark, ntk.num_gates(), opt.num_gates(), d.depth(), to_seconds(rt), cec );
  }

  exp.save();
  exp.table();

  return 0;
}
#else
int main()
{}
#endif
