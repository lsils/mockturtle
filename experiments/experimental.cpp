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

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "experimental", "benchmark", "size", "size gain", "depth", "depth gain", "runtime", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    // if ( benchmark != "priority" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    assert( result == lorina::return_code::success ); (void)result;

    depth_view _dntk( xag );
    uint32_t initial_size = _dntk.num_gates();
    uint32_t initial_depth = _dntk.depth();

    float run_time = 0;

    while ( true )
    {
      window_resub_params ps;
      window_resub_stats st;
      ps.verbose = true;
      ps.wps.max_inserts = 3;
      ps.wps.preserve_depth = true;
      ps.wps.update_levels_lazily = ps.wps.preserve_depth;
      uint32_t curr_size = xag.num_gates();
      window_xag_heuristic_resub( xag, ps, &st );
      xag = cleanup_dangling( xag );
      run_time += to_seconds( st.time_total );
      if ( ps.wps.preserve_depth == true || xag.num_gates() == curr_size )
      {
        break;
      }
    }

    depth_view dntk( xag );

    const auto cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    exp( benchmark, initial_size, initial_size - xag.num_gates(), initial_depth, initial_depth - dntk.depth(), run_time, cec );
  }

  exp.save();
  exp.table();

  return 0;
}