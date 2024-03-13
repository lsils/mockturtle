/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2023  EPFL
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

#include <string>
#include <vector>

#include <fmt/format.h>
#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/experimental/decompose_multioutput.hpp>
#include <mockturtle/algorithms/aig_balancing.hpp>
#include <mockturtle/algorithms/extract_adders.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/dont_touch_aig.hpp>
#include <mockturtle/networks/block.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>

#include <experiments.hpp>

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool> exp(
      "map_adders", "benchmark", "size", "HA", "FA", "|bntk1|", "|dt-aig|", "#dt", 
      "|dt-aig-opt|", "#dt-opt", "HA2", "FA2", "|bntk2|", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network aig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* Remove structural redundancies (increases the number of discoverable HAs/FAs) */
    aig_balancing_params bps;
    bps.minimize_levels = false;
    bps.fast_mode = false;
    aig_balance( aig, bps );

    const uint32_t size_before = aig.num_gates();

    /* Map HAs/FAs */
    extract_adders_params ps;
    extract_adders_stats st;
    block_network bntk1 = extract_adders( aig, ps, &st );

    /* check correctness */
    //decompose_multioutput_params dps;
    //dps.set_multioutput_as_dont_touch = true;
    //dont_touch_view<aig_network> dt_aig = decompose_multioutput<block_network, dont_touch_view<aig_network>>( bntk1 );
    //dont_touch_view<aig_network> dt_aig = extract_adders2( aig, ps );
    dont_touch_aig_network dt_aig = extract_adders3( aig, ps );

    resubstitution_params rps;

    // rps.pattern_filename = "1024sa1/" + benchmark + ".pat";
    rps.max_inserts = 20;
    rps.max_pis = 8;
    rps.max_divisors = std::numeric_limits<uint32_t>::max();

    const uint32_t dt_aig_size_before = dt_aig.num_gates();
    const uint32_t dt_before = dt_aig.num_dont_touch_gates();
    sim_resubstitution( dt_aig, rps );
    dt_aig = cleanup_dangling( dt_aig );

    extract_adders_stats st2;
    block_network bntk2 = extract_adders( dt_aig, ps, &st2 );

    bool const cec = benchmark == "hyp" ? true : abc_cec( dt_aig, benchmark );

    exp( benchmark, size_before, st.mapped_ha, st.mapped_fa, bntk1.num_gates(),
          dt_aig_size_before, dt_before,
          dt_aig.num_gates(), dt_aig.num_dont_touch_gates(),
          st2.mapped_ha, st2.mapped_fa, bntk2.num_gates(),
          cec );
  }

  exp.save();
  exp.table();

  return 0;
}