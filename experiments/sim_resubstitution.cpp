/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2020  EPFL
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
#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/algorithms/pattern_generation.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>

#include <experiments.hpp>

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, bool> exp( "sim_resubstitution", "benchmark", "#PI", "size", "gain", "#pat", "#cex", "#divk", "t_total", "t_structural", "t_sim", "t_SAT", "t_k", "cec" );

  //for ( auto const& benchmark : epfl_benchmarks() )
  for ( auto const& benchmark : iwls_benchmarks() )
  {
    //if ( benchmark.substr( 0, 4 ) == "leon" ) continue;

    fmt::print( "[i] processing {}\n", benchmark );
    aig_network aig;
    lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) );
    //if ( aig.num_gates() > 20000 ) continue;

    simresub_params ps;
    simresub_stats st;

    ps.max_pis = 10u;
    ps.max_divisors = 150u;
    ps.max_inserts = 1u;
    //ps.progress = true;

    bool useExternal = true;
    //ps.write_pats = "patCEX/" + benchmark + ".pat";

    pattern_generation_stats st_pat;
    partial_simulator sim(1,1);
    if ( useExternal )
    {
      sim = partial_simulator( "1024sa1/" + benchmark + ".pat" );
      //sim = partial_simulator( "patCEX/" + benchmark + ".pat" );
    }
    else
    {
      pattern_generation_params ps_pat;
      ps_pat.random_seed = 1689;
      //ps_pat.num_stuck_at = 0;
      //ps_pat.odc_levels = 5;
      ps_pat.progress = true;
      sim = partial_simulator( aig.num_pis(), 256 );
      pattern_generation( aig, sim, ps_pat, &st_pat );
    }

    const uint32_t num_total_patterns = sim.num_bits();
    const uint32_t size0 = aig.num_gates();
    sim_resubstitution( aig, sim, ps, &st );
    aig = cleanup_dangling( aig );

    const auto cec = abc_cec( aig, benchmark );
    exp( benchmark, aig.num_pis(), size0, size0 - aig.num_gates(), num_total_patterns, st.num_cex, st.num_resub, to_seconds( st.time_total ), to_seconds( st.time_divs ) + to_seconds( st.time_mffc ) + to_seconds( st.time_cut ) + to_seconds( st.time_callback ), to_seconds( st.time_sim ), to_seconds( st.time_sat ), to_seconds( st.time_compute_function ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}
