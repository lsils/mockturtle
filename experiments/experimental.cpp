#include "experiments.hpp"
#include <iostream>
#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/balancing.hpp>
#include <mockturtle/algorithms/balancing/esop_balancing.hpp>
#include <mockturtle/algorithms/balancing/sop_balancing.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/experimental/boolean_optimization.hpp>
#include <mockturtle/algorithms/experimental/costfn_resub.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <string>

int main()
{
  using namespace mockturtle;
  using namespace mockturtle::experimental;
  using namespace experiments;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "experimental", "benchmark", "size", "size gain", "depth", "depth gain", "runtime", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {

    if (benchmark != "sqrt") continue; // faster experiment
    fmt::print( "[i] processing {}\n", benchmark );

    // aig_network xag;
    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    assert( result == lorina::return_code::success );
    (void)result;

    costfn_resub_params ps;
    costfn_resub_stats st;
    ps.verbose = true;
    ps.wps.max_inserts = 3;
    // ps.wps.preserve_depth = true;
    // ps.wps.update_levels_lazily = true;

    using cost_t = typename std::pair<uint32_t, uint32_t>;

    /**
     * @brief 
     * The cost of each new node is calculated by node_cost_fn()
     * given the 2 fanins (for xag).
     */
    ps.rps.node_cost_fn = []( cost_t fanin_x, cost_t fanin_y, bool is_xor = false ) {
      (void) is_xor;
      auto [size_x, depth_x] = fanin_x;
      auto [size_y, depth_y] = fanin_y;
      return std::pair( size_x + size_y + 1, std::max( depth_x, depth_y ) + 1 );
    };

    /**
     * @brief 
     * For each problem, all possible solutions will be collected
     * Resyn solver will find the best solution according to the 
     * compare function
     */
    ps.rps.compare_cost_fn = []( cost_t fanin_x, cost_t fanin_y, bool is_critical = false ) {
      auto [size_x, depth_x] = fanin_x;
      auto [size_y, depth_y] = fanin_y;
      if ( is_critical )
      {
        return depth_x > depth_y || ( depth_x == depth_y && size_x > size_y );
      }
      return depth_x > depth_y || ( depth_x == depth_y && size_x > size_y );
      // return size_x > size_y || ( size_x == size_y && depth_x > depth_y );
    };

    depth_view _dntk( xag );
    uint32_t initial_size = _dntk.num_gates();
    uint32_t initial_depth = _dntk.depth();

    costfn_xag_heuristic_resub( xag, ps, &st );
    xag = cleanup_dangling( xag );

    // xag = balancing( xag, {sop_rebalancing<xag_network>{}} );

    depth_view dntk( xag );

    const auto cec = ps.dry_run || benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    exp( benchmark, initial_size, initial_size - xag.num_gates(), initial_depth, initial_depth - dntk.depth(), to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}