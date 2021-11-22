#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/buffered.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/views/names_view.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/algorithms/aqfp/buffer_insertion.hpp>
#include <mockturtle/algorithms/aqfp/buffer_verification.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/utils/name_utils.hpp>
#include <lorina/verilog.hpp>
#include <lorina/aiger.hpp>
#include <lorina/diagnostics.hpp>
#include "experiments.hpp"
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>

#include <iostream>

int main( int argc, char* argv[] )
{
  std::string run_only_one = "";
  if ( argc == 2 )
    run_only_one = std::string( argv[1] );

  using namespace mockturtle;
  using namespace experiments;
  /* NOTE 1: To run the "optimal" insertion, please clone and build Z3: https://github.com/Z3Prover/z3
   * And have `z3` available as a system call
   */

  /* NOTE 2: Please clone this repository: https://github.com/lsils/SCE-benchmarks
   * And put in the following string the relative path from your build path to SCE-benchmarks/ISCAS/strashed/
   */
  std::string benchmark_path = "../../SCE-benchmarks/ISCAS/strashed/";
  //std::string benchmark_path = "../../SCE-benchmarks/MCNC/original/";
  //std::string benchmark_path = "../../SCE-benchmarks/EPFL/MIGs/";
  static const std::string benchmarks_iscas[] = {
    "adder1", "adder8", "mult8", "counter16", "counter32", "counter64", "counter128",
    "c17", "c432", "c499", "c880", "c1355", "c1908", "c2670", "c3540", "c5315", "c6288", "c7552",
    "sorter32", "sorter48", "alu32"};
  static const std::string benchmarks_mcnc[] = {
    /*"5xp1",*/ "c1908", "c432", "c5315", "c880", "chkn", "count", "dist", "in5", "in6", "k2",
    "m3", "max512", "misex3", "mlp4", "prom2", "sqr6", "x1dn"};
  const auto benchmarks_epfl = experiments::epfl_benchmarks();

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, bool>
    exp( "buffer_insertion", "benchmark", "#gates", "depth", "#buffers", "max FO", "opt. #JJs", "depth_JJ", "runtime", "verified" );

  buffer_insertion_params ps;
  ps.scheduling = buffer_insertion_params::better;
  ps.optimization_effort = buffer_insertion_params::until_sat;
  ps.assume.splitter_capacity = 4u;
  ps.assume.branch_pis = true;
  ps.assume.balance_pis = true;
  ps.assume.balance_pos = true;

  if ( argc == 3 )
  {
    ps.assume.splitter_capacity = std::stoi( argv[1] );
    uint32_t arg = std::stoi( argv[2] );
    ps.assume.branch_pis = arg >= 100;
    ps.assume.balance_pis = ( arg % 100 ) >= 10;
    ps.assume.balance_pos = arg % 10;
  }

  uint32_t total_buffers{0}, total_depth{0};
  for ( auto benchmark : benchmarks_iscas )
  {
    if ( run_only_one != "" && benchmark != run_only_one ) continue;
    if ( benchmark == "hyp" && run_only_one != "hyp" ) continue;
    //std::cout << "\n[i] processing " << benchmark << "\n";
    names_view<mig_network> ntk;
    lorina::text_diagnostics td;
    lorina::diagnostic_engine diag( &td );
    auto res = lorina::read_verilog( benchmark_path + benchmark + ".v", verilog_reader( ntk ), &diag );
    if ( res != lorina::return_code::success )
    {
      std::cout << "read failed\n";
      continue;
    }
    ntk.set_network_name( benchmark );

    stopwatch<>::duration t{0};    
    buffer_insertion aqfp( ntk, ps );
    buffered_mig_network bufntk;
    uint32_t num_buffers = call_with_stopwatch( t, [&]() {
      return aqfp.dry_run();
    });
    aqfp.dump_buffered_network( bufntk );
    bool verified = verify_aqfp_buffer( bufntk, ps.assume );

    //names_view named_bufntk{bufntk};
    //restore_pio_names_by_order( ntk, named_bufntk );
    //write_verilog( named_bufntk, benchmark_path + "../best_insertion/" + benchmark + "_buffered.v" ); 
    
    depth_view d{ntk};
    depth_view d_buf{bufntk};

    total_buffers += num_buffers;
    total_depth += d_buf.depth();

    uint32_t max_fanout{0};
    ntk.foreach_node( [&]( auto const& n ){
      if ( !ntk.is_constant( n ) )
      max_fanout = std::max( max_fanout, ntk.fanout_size( n ) );
    });

    exp( benchmark, ntk.num_gates(), d.depth(), num_buffers, max_fanout, ntk.num_gates() * 6 + num_buffers * 2, d_buf.depth(), to_seconds( t ), verified );
  }

  exp.save();
  //exp.table();

  std::cout << "[i] total buffers = " << total_buffers << ", total depth = " << total_depth << "\n";

  return 0;
}
