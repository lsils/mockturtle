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

#include <iostream>

int main()
{
  using namespace mockturtle;
  using namespace experiments;

  std::vector<std::string> benchmarks( 
    {"adder1", "adder8", "mult8", "counter16", "counter32", "counter64", "counter128",
     "c17", "c432", "c499", "c880", "c1355", "c1908", "c2670", "c3540", "c5315", "c6288", "c7552",
     "sorter32", "sorter48", "alu32"} );
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool>
    exp( "buffer_insertion", "benchmark", "#gates", "depth", "#buffers", "ori. #JJs", "opt. #JJs", "depth_JJ", "verified" );

  for ( auto benchmark : benchmarks )
  {
    std::cout << "\n[i] processing " << benchmark << "\n";
    names_view<mig_network> ntk;
    lorina::text_diagnostics td;
    lorina::diagnostic_engine diag( &td );
    auto res = lorina::read_verilog( "testcase_iscas85/" + benchmark + ".v", verilog_reader( ntk ), &diag );
    if ( res != lorina::return_code::success )
    {
      std::cout << "read failed\n";
      continue;
    }
    ntk = cleanup_dangling( ntk );
    write_verilog( ntk, "testcase_iscas85/mockturtle/" + benchmark + ".v" );

    buffer_insertion_params ps;
    ps.scheduling = buffer_insertion_params::better;
    ps.optimization_effort = buffer_insertion_params::until_sat;
    ps.assume.splitter_capacity = 4u;
    ps.assume.branch_pis = true;
    ps.assume.balance_pis = true;
    ps.assume.balance_pos = true;
    
    buffer_insertion aqfp( ntk, ps );
    buffered_mig_network bufntk;
    uint32_t num_buffers = aqfp.run( bufntk );
    bool verified = verify_aqfp_buffer( bufntk, ps.assume );
    names_view named_bufntk{bufntk};
    restore_pio_names_by_order( ntk, named_bufntk );
    write_verilog( named_bufntk, "testcase_iscas85/mockturtle/" + benchmark + "_buffered.v" );

    depth_view d{ntk};
    depth_view d_buf{bufntk};

    ntk.foreach_po( [&](auto f){
      if ( ntk.fanout_size( ntk.get_node( f ) ) > 1 && !ntk.is_pi( ntk.get_node( f ) ) && d_buf.is_on_critical_path( ntk.get_node( f ) ) )
        std::cout << "[i] multi-fanout PO " << ntk.get_node( f ) << " on critical path (#FO = " << ntk.fanout_size( ntk.get_node( f ) ) << ")\n";
    });

    exp( benchmark, ntk.num_gates(), d.depth(), num_buffers, ntk.num_gates() * 6, ntk.num_gates() * 6 + num_buffers * 2, d_buf.depth(), verified );
  }

  exp.save();
  exp.table();

  return 0;
}
