#include "experiments.hpp"
#include <mockturtle/algorithms/experimental/boolean_optimization.hpp>
#include <mockturtle/algorithms/experimental/window_resub.hpp>
#include <mockturtle/algorithms/experimental/sim_resub.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <lorina/aiger.hpp>
#include <iostream>
#include <string>

int main()
{
  using namespace mockturtle;
  using namespace mockturtle::experimental;
  using namespace experiments;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "experimental", "benchmark", "size", "gain", "est. gain", "#sols", "runtime", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network aig;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) );
    assert( result == lorina::return_code::success ); (void)result;

    window_resub_params ps;
    window_resub_stats st;
    ps.verbose = true;
    ps.wps.max_inserts = 1;

    window_aig_enumerative_resub( aig, ps, &st );
    aig = cleanup_dangling( aig );

    const auto cec = ps.dry_run || benchmark == "hyp" ? true : abc_cec( aig, benchmark );
    exp( benchmark, st.initial_size, st.initial_size - aig.num_gates(), st.estimated_gain, st.num_solutions, to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}