#include <lorina/aiger.hpp>
#include <experiments.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/algorithms/experimental/costfn_window.hpp>
#include <mockturtle/algorithms/experimental/costfn_resyn.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/testcase_minimizer.hpp>
#include <mockturtle/utils/debugging_utils.hpp>

using namespace mockturtle;

int main()
{

  using namespace mockturtle::experimental;
  using namespace experiments;

  /* use debug tool to debug the algorithm */
  if ( false )
  {
    /* Use this lambda function for debugging mockturtle algorithms */
    auto opt = []( xag_network xag ) -> bool {
      fanout_view ntk( xag );
      cost_aware_params ps;
      cost_aware_stats st;
      cost_aware_optimization( ntk, and_cost<decltype( ntk )>(), ps, &st );
      ntk = cleanup_dangling( ntk );
      return !abc_cec( xag, "voter" ); // return true if buggy
    };

    testcase_minimizer_params ps;
    ps.file_format = testcase_minimizer_params::aiger;
    ps.path = "."; // current directory
    ps.init_case = "../experiments/benchmarks/voter"; // the buggy 
    ps.minimized_case = ps.init_case + "_minimized";
    ps.max_size = 0;
    ps.verbose = true;
    testcase_minimizer<xag_network> minimizer( ps );
    minimizer.run( opt );
    return 0;
  }
  /* see what happens with the buggy testcase */
  if( false )
  {
    xag_network xag;
    auto const result = lorina::read_aiger( "voter_minimized.aig", aiger_reader( xag ) );
    assert( result == lorina::return_code::success ); (void)result;
    fanout_view ntk( xag );
    
    cost_aware_params ps;
    cost_aware_stats st;
    cost_aware_optimization( ntk, and_cost<decltype( ntk )>(), ps, &st );
    ntk = cleanup_dangling( ntk );
    write_verilog( xag, "output.v" );
    return 0;
  }
  /* run the actual experiments */
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "cost_aware", "benchmark", "#Gate", "cost", "#Gate\'", "cost\'", "runtime", "cec" );
  for ( auto const& benchmark : epfl_benchmarks() )
  {
    uint32_t ngate = 0, cost = 0, _ngate = 0, _cost = 0;
    float run_time = 0;

    fmt::print( "[i] processing {}\n", benchmark );
    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    assert( result == lorina::return_code::success ); (void)result;

    fanout_view ntk( xag );
    ngate = ntk.num_gates();
    cost = cost_view( ntk, and_cost<decltype( ntk )>() ).get_cost();

    cost_aware_params ps;
    cost_aware_stats st;

    cost_aware_optimization( ntk, and_cost<decltype( ntk )>(), ps, &st );
    ntk = cleanup_dangling( ntk );

    run_time = to_seconds( st.time_total );

    _ngate = ntk.num_gates();
    _cost = cost_view( ntk, and_cost<decltype( ntk )>() ).get_cost();

    const auto cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    exp( benchmark, ngate, cost, _ngate, _cost, run_time, cec );
  }
  exp.save();
  exp.table();
  return 0;
}