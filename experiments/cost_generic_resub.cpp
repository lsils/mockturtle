/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include <experiments.hpp>
#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/balancing.hpp>
#include <mockturtle/algorithms/balancing/esop_balancing.hpp>
#include <mockturtle/algorithms/balancing/sop_balancing.hpp>
#include <mockturtle/algorithms/experimental/cost_generic_resub.hpp>
#include <mockturtle/algorithms/functional_reduction.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_minmc2.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/utils/stopwatch.hpp>

using namespace mockturtle;

int main()
{

  using namespace mockturtle::experimental;
  using namespace experiments;

  std::string results_dir = "../experiments/results";

  /* run the actual experiments */
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "cost_generic_resub", "benchmark", "c1", "c2", "c3", "c4", "_c1", "_c2", "_c3", "_c4", "runtime", "cec" );
  for ( auto const& benchmark : iwls_benchmarks() )
  {
    /* skip these for "collect all" experiments */
    // if (benchmark == "hyp") continue; 
    // if (benchmark == "mem_ctrl") continue;
    // if (benchmark == "log2") continue;
    // if (benchmark == "sin") continue;
    float run_time = 0;

    fmt::print( "[i] processing {}\n", benchmark );
    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    assert( result == lorina::return_code::success );
    (void)result;

    /* fraig */
    // functional_reduction( xag );
    // xag = cleanup_dangling( xag );

    auto costfn_1 = xag_size_cost_function<xag_network>();
    auto costfn_2 = xag_depth_cost_function<xag_network>();
    auto costfn_3 = xag_multiplicative_complexity_cost_function<xag_network>();
    auto costfn_4 = xag_t_depth_cost_function<xag_network>();

    std::string costfn_str = "mc";
    auto costfn = costfn_3;

    auto c1 = cost_view( xag, costfn_1 ).get_cost();
    auto c2 = cost_view( xag, costfn_2 ).get_cost();
    auto c3 = cost_view( xag, costfn_3 ).get_cost();
    auto c4 = cost_view( xag, costfn_4 ).get_cost();

    cost_generic_resub_params ps;
    cost_generic_resub_stats st;
    ps.verbose = false;
    ps.rps.max_solutions = 1; /* = 1: collect one, =0: collect all */
    // ps.rps.use_esop = true; /* true: use esop, false: no esop */

    stopwatch<>::duration time_tot{ 0 };

    future::xag_minmc_resynthesis resyn;
    call_with_stopwatch( time_tot, [&]() {
      cost_generic_resub( xag, costfn, ps, &st );

      xag = cut_rewriting<xag_network, decltype( resyn ), mc_cost<xag_network>>( xag, resyn, ps, nullptr );

      // xag = balancing( xag, { sop_rebalancing<xag_network>{} } );
      // xag = balancing( xag, { esop_rebalancing<xag_network>{} } );
      xag = cleanup_dangling( xag );
    } );

    write_verilog( xag, fmt::format("{}/{}_{}.v", results_dir, benchmark, costfn_str ) );

    run_time = to_seconds( time_tot );

    // fmt::print( "{},{},{},{:>5.2f}\n", iter++, xag.num_gates(), cost_after, run_time );

    auto _c1 = cost_view( xag, costfn_1 ).get_cost();
    auto _c2 = cost_view( xag, costfn_2 ).get_cost();
    auto _c3 = cost_view( xag, costfn_3 ).get_cost();
    auto _c4 = cost_view( xag, costfn_4 ).get_cost();

    auto cec = true;
    // cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    // cec = xag.num_gates() > 10000 ? true : abc_cec( xag, benchmark );
    exp( benchmark, c1, c2, c3, c4, _c1, _c2, _c3, _c4, run_time, cec );
  }
  exp.save();
  exp.table();
  return 0;
}