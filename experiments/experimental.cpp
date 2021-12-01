#include "experiments.hpp"
#include <iostream>
#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/experimental/boolean_optimization.hpp>
#include <mockturtle/algorithms/experimental/costfn_resub.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <string>

int main()
{
  using namespace mockturtle;
  using namespace mockturtle::experimental;
  using namespace experiments;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "experimental", "benchmark", "size", "size gain", "level", "level gain", "runtime", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {

    // if (benchmark != "log2") continue;
    fmt::print( "[i] processing {}\n", benchmark );

    // aig_network aig;
    xag_network aig;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) );
    assert( result == lorina::return_code::success );
    (void)result;

    depth_view d0{ aig };
    uint32_t initial_level = d0.depth();

    costfn_resub_params ps;
    costfn_resub_stats st;
    // ps.verbose = true;
    ps.wps.max_inserts = 3;
    ps.wps.preserve_depth = true;
    ps.wps.update_levels_lazily = true;

    // ps.wps.use_dont_cares = true; // TODO: fix the bug here

    using cost_t = typename std::pair<uint32_t, uint32_t>;

    ps.rps.node_cost_fn = [&]( cost_t fanin_x, cost_t fanin_y ) {
      auto [size_x, depth_x] = fanin_x;
      auto [size_y, depth_y] = fanin_y;
      return std::pair( size_x + size_y + 1, std::max( depth_x, depth_y ) + 1 );
    };

    costfn_xag_heuristic_resub( aig, ps, &st );
    aig = cleanup_dangling( aig );

    depth_view d1{ aig };
    const auto cec = ps.dry_run || benchmark == "hyp" ? true : abc_cec( aig, benchmark );
    exp( benchmark, st.initial_size, st.initial_size - aig.num_gates(), initial_level, initial_level - d1.depth(), to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}