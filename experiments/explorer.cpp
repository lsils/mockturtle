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

static const std::string benchmark_repo_path = "../../SCE-benchmarks";

/* AQFP benchmarks */
std::vector<std::string> aqfp_benchmarks = {
    "5xp1", "c1908", "c432", "c5315", "c880", "chkn", "count", "dist", "in5", "in6", "k2",
    "m3", "max512", "misex3", "mlp4", "prom2", "sqr6", "x1dn" };

std::string benchmark_aqfp_path( std::string const& benchmark_name )
{
  return fmt::format( "{}/MCNC/original/{}.v", benchmark_repo_path, benchmark_name );
}

int main00( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  (void)argc;
  std::string benchmark = std::string( argv[1] );
  {
    aig_network aig;
    //if ( lorina::read_verilog( benchmark, verilog_reader( mig ) ) != lorina::return_code::success )
    if ( lorina::read_aiger( benchmark, aiger_reader( aig ) ) != lorina::return_code::success )
    {
      std::cerr << "Cannot read " << benchmark << "\n";
      return -1;
    }

    aig = call_abc_script( aig, "&put; resyn2rs; resyn2rs; resyn2rs; &get" );
    mig_network mig;

    /* map AIG to MIG */
    {
      mig_npn_resynthesis resyn2{ true };
      exact_library<mig_network, mig_npn_resynthesis> exact_lib( resyn2 );

      map_params ps;
      ps.skip_delay_round = false;
      ps.required_time = std::numeric_limits<double>::max();
      mig = map( aig, exact_lib, ps );

      depth_view d( mig );
      fmt::print( "size {}, depth {}\n", mig.num_gates(), d.depth() );

      /* remap */
      ps.skip_delay_round = true;
      ps.area_flow_rounds = 2;
      mig = map( mig, exact_lib, ps );

      ps.area_flow_rounds = 1;
      ps.enable_logic_sharing = true; /* high-effort remap */
      mig = map( mig, exact_lib, ps );
    }

    /* simple MIG resub */
    {
      resubstitution_params ps;
      ps.max_pis = 8u;
      ps.max_inserts = 2u;
      uint32_t tmp;

      do
      {
        tmp = mig.num_gates();
        depth_view depth_mig{ mig };
        fanout_view fanout_mig{ depth_mig };
        mig_resubstitution( fanout_mig, ps );
        mig = cleanup_dangling( mig );
      } while ( mig.num_gates() > tmp );
    }

    {
      resubstitution_params ps;
      ps.max_pis = 8u;
      ps.max_inserts = std::numeric_limits<uint32_t>::max();

      depth_view depth_mig{ mig };
      fanout_view fanout_mig{ depth_mig };
      mig_resubstitution2( fanout_mig, ps );
      mig = cleanup_dangling( mig );
    }
    
    depth_view d( mig );
    fmt::print( "size {}, depth {}\n", mig.num_gates(), d.depth() );
  }

  return 0;
}

int main0( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, bool, bool> exp( "deepsyn0", "benchmark", "#JJ", "JJ depth", "MIG size", "MIG depth", "cec", "verified" );

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

    aig_network aig = cleanup_dangling<mig_network, aig_network>( ntk );
    aig = call_abc_script( aig, "&deepsyn -I 10 -J 50 -T 1000 -S 111 -t" );
    mig_network mig;

    /* map AIG to MIG */
    {
      mig_npn_resynthesis resyn2{ true };
      exact_library<mig_network, mig_npn_resynthesis> exact_lib( resyn2 );

      map_params ps;
      ps.skip_delay_round = true;
      ps.required_time = std::numeric_limits<double>::max();
      mig = map( aig, exact_lib, ps );

      /* remap */
      ps.area_flow_rounds = 2;
      mig = map( mig, exact_lib, ps );

      ps.area_flow_rounds = 1;
      ps.enable_logic_sharing = true; /* high-effort remap */
      mig = map( mig, exact_lib, ps );
    }

    /* simple MIG resub */
    {
      resubstitution_params ps;
      ps.max_pis = 8u;
      ps.max_inserts = 2u;
      uint32_t tmp;

      do
      {
        tmp = mig.num_gates();
        depth_view depth_mig{ mig };
        fanout_view fanout_mig{ depth_mig };
        mig_resubstitution( fanout_mig, ps );
        mig = cleanup_dangling( mig );
      } while ( mig.num_gates() > tmp );
    }

    {
      resubstitution_params ps;
      ps.max_pis = 8u;
      ps.max_inserts = std::numeric_limits<uint32_t>::max();

      depth_view depth_mig{ mig };
      fanout_view fanout_mig{ depth_mig };
      mig_resubstitution2( fanout_mig, ps );
      mig = cleanup_dangling( mig );
    }
    
    depth_view d( mig );
    bool cec = abc_cec_impl( mig, benchmark_aqfp_path( benchmark ) );

    buffer_insertion_params bps;
    bps.assume.balance_cios = true;
    bps.assume.splitter_capacity = 4;
    bps.assume.ci_phases = {0};
    bps.scheduling = buffer_insertion_params::better_depth;
    bps.optimization_effort = buffer_insertion_params::none;
    buffer_insertion buf_inst( mig, bps );

    buffered_mig_network buffered_mig;
    auto num_buffers = buf_inst.run( buffered_mig );
    auto JJ_depth = buf_inst.depth();
    bool bv = verify_aqfp_buffer( buffered_mig, bps.assume, buf_inst.pi_levels() );
    //write_verilog( buffered_mig, benchmark + ".v" );

    exp( benchmark, mig.num_gates() * 6 + num_buffers * 2, JJ_depth, mig.num_gates(), d.depth(), cec, bv );
  }

  exp.save();
  exp.table();

  return 0;
}

int main( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool, bool> exp( "deepsyn_aqfp", "benchmark", "#JJ", "JJ depth", "JJ EDP", "MIG size", "MIG depth", "cec", "verified" );

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
    ps.num_restarts = 5;
    ps.random_seed = 3252;
    ps.max_steps_no_impr = 50;
    ps.timeout = 100;
    ps.compressing_scripts_per_step = 3;
    ps.verbose = true;
    //ps.very_verbose = true;

    mig_network opt = deepsyn_aqfp( ntk, ps );
    depth_view d( opt );

    bool cec = abc_cec_impl( opt, benchmark_aqfp_path( benchmark ) );

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
    auto JJ_ADP = JJ_depth * JJ_count;
    bool bv = verify_aqfp_buffer( buffered_mig, bps.assume, buf_inst.pi_levels() );
    //write_verilog( buffered_mig, benchmark + ".v" );

    exp( benchmark, JJ_count, JJ_depth, JJ_ADP, opt.num_gates(), d.depth(), cec, bv );
  }

  exp.save();
  exp.table();

  return 0;
}

int main2( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, bool> exp( "deepsyn_mig", "benchmark", "size_before", "size_after", "depth", "cec" );

  //for ( auto const& benchmark : aqfp_benchmarks )
  for ( auto const& benchmark : epfl_benchmarks() )
  {
    if ( argc == 2 && benchmark != std::string( argv[1] ) ) continue;
    if ( benchmark == "hyp" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    using Ntk = mig_network;
    Ntk ntk;
    //if ( lorina::read_verilog( benchmark_aqfp_path( benchmark ), verilog_reader( ntk ) ) != lorina::return_code::success )
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( ntk ) ) != lorina::return_code::success )
    {
      std::cerr << "Cannot read " << benchmark << "\n";
      return -1;
    }

    explorer_params ps;
    ps.num_restarts = 3;
    ps.random_seed = 42124;
    ps.timeout = 1000;
    ps.max_steps_no_impr = 50;
    ps.compressing_scripts_per_step = 1;
    ps.verbose = true;
    //ps.very_verbose = true;

    Ntk opt = deepsyn_mig( ntk, ps );
    bool const cec = ( benchmark == "hyp" ) ? true : abc_cec_impl( opt, benchmark_path( benchmark ) );
    depth_view d( opt );

    exp( benchmark, ntk.num_gates(), opt.num_gates(), d.depth(), cec );
  }

  exp.save();
  exp.table();

  return 0;
}