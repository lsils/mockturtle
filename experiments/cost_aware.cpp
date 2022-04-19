#include <lorina/aiger.hpp>
#include <experiments.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/algorithms/experimental/costfn_window.hpp>
#include <mockturtle/algorithms/experimental/costfn_resyn.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/algorithms/cleanup.hpp>

using namespace mockturtle;

int main()
{

  using namespace mockturtle::experimental;
  using namespace experiments;

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
    cost_aware_optimization( ntk, and_cost<decltype( ntk )>(), ps );
    ntk = cleanup_dangling( ntk );

    _ngate = ntk.num_gates();
    _cost = cost_view( ntk, and_cost<decltype( ntk )>() ).get_cost();

    const auto cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    exp( benchmark, ngate, cost, _ngate, _cost, run_time, cec );
  }
  exp.save();
  exp.table();
  return 0;
}