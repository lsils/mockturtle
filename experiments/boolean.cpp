#include "experiments.hpp"
#include <mockturtle/algorithms/experimental/boolean_optimization.hpp>
#include <mockturtle/algorithms/experimental/window_resub.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <lorina/aiger.hpp>
#include <iostream>
#include <string>

int main()
{
  using namespace mockturtle;
  using namespace experiments;

  experiment<std::string, uint32_t, uint32_t, float, bool> exp( "new_resub", "benchmark", "size_before", "size_after", "runtime", "equivalent" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network aig;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) );
    assert( result == lorina::return_code::success ); (void)result;

    using wps_t = typename experimental::complete_tt_windowing_params;
    using ps_t = typename experimental::boolean_optimization_params<wps_t>;
    ps_t ps;
    ps.verbose = true;
    //ps.dry_run = true;
    ps.wps.max_inserts = 1;

    using wst_t = typename experimental::complete_tt_windowing_stats;
    using st_t = typename experimental::boolean_optimization_stats<wst_t>;
    st_t st;

    const uint32_t size_before = aig.num_gates();
    window_xag_heuristic_resub( aig, ps, &st );
    aig = cleanup_dangling( aig );

    const auto cec = benchmark == "hyp" ? true : abc_cec( aig, benchmark );
    exp( benchmark, size_before, aig.num_gates(), to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}