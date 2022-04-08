#include "experiments.hpp"
#include <mockturtle/algorithms/experimental/boolean_optimization.hpp>
#include <mockturtle/algorithms/experimental/window_resub.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/properties/mccost.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_minmc2.hpp>

#include <mockturtle/algorithms/experimental/sim_resub.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_aiger.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/algorithms/xmg_resub.hpp>

#include <lorina/aiger.hpp>
#include <iostream>
#include <string>

int main()
{
  using namespace mockturtle;
  using namespace mockturtle::experimental;
  using namespace experiments;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "experimental", "benchmark", "size", "#Gate", "#AND", "#Gate\'", "depth", "depth gain", "runtime", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    /*      ---------------     */
    /*                          */
    /*       Read Input         */
    /*       XAG network        */
    /*                          */
    /*      ---------------     */
    // if ( benchmark != "adder" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );
    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    // auto const result = lorina::read_verilog( "output.v", verilog_reader( xag ) );
    assert( result == lorina::return_code::success ); (void)result;

    /*      ---------------     */
    /*                          */
    /*       Read Input         */
    /*       XMG network        */
    /*                          */
    /*      ---------------     */
    // fmt::print( "[i] processing {}\n", benchmark );

    // xmg_network xmg;
    // auto const result_xmg = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xmg ) );
    // assert( result_xmg == lorina::return_code::success ); (void)result_xmg;

    // depth_view _dntk( xmg );
    // uint32_t initial_size = _dntk.num_gates();
    // uint32_t initial_depth = _dntk.depth();

    // const auto cec = benchmark == "hyp" ? true : abc_cec( xmg, benchmark );
    // depth_view dntk( xmg );
    
    // float run_time = 0;

 
    /*      ---------------     */
    /*                          */
    /*       Read Input         */
    /*       XMG resyn          */
    /*                          */
    /*      ---------------     */
    // xmg_resubstitution( xmg );
    // xmg = cleanup_dangling( xmg );

    // exp( benchmark, initial_size, 0, xmg.num_gates(), 0, initial_depth, dntk.depth(), run_time, cec );


    /*      ---------------     */
    /*                          */
    /*       Init  Status       */
    /*                          */
    /*      ---------------     */
    depth_view _dntk( xag );
    uint32_t initial_size = _dntk.num_gates();
    uint32_t initial_depth = _dntk.depth();

    float run_time = 0;

    uint32_t initial_num_xor = 0u;
    xag.foreach_gate( [&]( auto const& n ) { initial_num_xor += xag.is_xor(n); });


    /*      ---------------     */
    /*                          */
    /*       Optimization       */
    /*                          */
    /*      ---------------     */
    window_resub_params ps;
    window_resub_stats st;
    ps.verbose = true;
    ps.wps.max_inserts = 3u;
    ps.wps.max_pis = 8u;
    ps.wps.max_divisors = 150u;
    ps.wps.preserve_depth = false;
    ps.wps.update_levels_lazily = false;
    bool until_conv = false;
    while ( true )
    {
      auto num_gates = xag.num_gates();
      window_xag_heuristic_resub( xag, ps, &st );
      xag = cleanup_dangling( xag );
      run_time += to_seconds( st.time_total );
      if ( !until_conv || num_gates == xag.num_gates() )
      {
        break;
      } 
    }
    // write_verilog( xag, "output.v" );

    /*      ---------------     */
    /*                          */
    /*       ABC Cut            */
    /*                          */
    /*      ---------------     */
    // aig_network aig;
    // stopwatch<>::duration abc_run_time{0};
    // call_with_stopwatch( abc_run_time, [&](){
    //   std::system( ("abc -c \"read_aiger ../experiments/benchmarks/"+benchmark+".aig; drw; drw; drw; drw; drw; write_aiger output_tmp.aig\" ").c_str() );
    // });
    // lorina::read_aiger( "output_tmp.aig", aiger_reader( aig ) );
    // depth_view dntk( aig );
    // const auto cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    // exp( benchmark, 0, initial_size, aig.num_gates(), 0, initial_depth, initial_depth - dntk.depth(), to_seconds( abc_run_time ), cec );
    /*      ---------------     */
    /*                          */
    /*       Baseline NPN       */
    /*                          */
    /*      ---------------     */
    // bool until_conv = false;
    // while ( true )
    // {
    //   auto num_gates = xag.num_gates();
    //   xag_npn_resynthesis<xag_network> resyn;
    //   cut_rewriting_params ps;
    //   cut_rewriting_stats st;
    //   ps.cut_enumeration_ps.cut_size = 4;
    //   xag = cut_rewriting( xag, resyn, ps, &st );
    //   xag = cleanup_dangling( xag );
    //   run_time += to_seconds( st.time_total );
    //   if ( !until_conv || num_gates == xag.num_gates() )
    //   {
    //     break;
    //   }
    // }

    /*      ---------------     */
    /*                          */
    /*       Baseline MC        */
    /*                          */
    /*      ---------------     */
    // bool until_conv = false;
    // int max_iter = 10;
    // while ( true )
    // {
    //   auto num_gates = xag.num_gates();
    //   future::xag_minmc_resynthesis resyn;
    //   cut_rewriting_params ps;
    //   cut_rewriting_stats st;
    //   ps.cut_enumeration_ps.cut_size = 4; // 6 is too expansive
    //   ps.verbose = true;
    //   // cut_rewriting_with_compatibility_graph( xag, resyn, ps, &st, mc_cost<xag_network>() );
    //   xag = cut_rewriting<xag_network, decltype( resyn ), mc_cost<xag_network> >( xag, resyn, ps, &st );
    //   run_time += to_seconds( st.time_total );
    //   xag = cleanup_dangling( xag );
    //   if ( !until_conv || (max_iter-- == 0) || num_gates == xag.num_gates() )
    //   {
    //     break;
    //   } 
    // }

    /*      ---------------     */
    /*                          */
    /*       Final Status       */
    /*                          */
    /*      ---------------     */
    uint32_t final_num_xor = 0u;
    xag.foreach_gate( [&]( auto const& n ) { final_num_xor += xag.is_xor(n); });
    depth_view dntk( xag );

    const auto cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    exp( benchmark, initial_size - initial_num_xor, initial_size, xag.num_gates() - final_num_xor, xag.num_gates(), initial_depth, initial_depth - dntk.depth(), run_time, cec );
  }


  exp.save();
  exp.table();

  return 0;
}