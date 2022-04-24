#include <lorina/aiger.hpp>
#include <experiments.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/algorithms/experimental/costfn_window.hpp>
#include <mockturtle/algorithms/experimental/costfn_resyn.hpp>
#include <mockturtle/algorithms/experimental/window_resub.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/testcase_minimizer.hpp>
#include <mockturtle/utils/debugging_utils.hpp>

using namespace mockturtle;

int main()
{

  using namespace mockturtle::experimental;
  using namespace experiments;

  /* run the actual experiments */
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "cost_aware", "benchmark", "#Gate", "Depth", "cost", "#Gate\'", "Depth\'", "cost\'", "runtime", "cec" );
  for ( auto const& benchmark : epfl_benchmarks() )
  {
    if ( benchmark != "ctrl" ) continue;
    uint32_t ngate = 0, cost = 0, depth = 0, _ngate = 0, _cost = 0, _depth = 0;
    float run_time = 0;

    fmt::print( "[i] processing {}\n", benchmark );
    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    assert( result == lorina::return_code::success ); (void)result;

    fanout_view ntk( xag );
    auto costfn = supp_cost<decltype( ntk )>();
    auto evalfn = supp_cost<decltype( ntk )>();
    ngate = ntk.num_gates();
    cost = cost_view( ntk, evalfn ).get_cost();
    depth = depth_view( ntk ).depth();

    cost_aware_params ps;
    cost_aware_stats st;
    cost_aware_optimization( ntk, costfn, ps, &st );
    xag = cleanup_dangling( xag );    

    run_time = to_seconds( st.time_total );

    fanout_view _ntk( xag );
    _ngate = _ntk.num_gates();
    _cost = cost_view( _ntk, evalfn ).get_cost();
    _depth = depth_view( _ntk ).depth();

    const auto cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    exp( benchmark, ngate, depth, cost, _ngate, _depth, _cost, run_time, cec );
  }
  exp.save();
  exp.table();
  return 0;
}