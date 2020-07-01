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

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, bool> exp( "pattern_generation", "benchmark", "#PI", "size", "#pat", "#pat gen", "#const", "t_total", "t_sim", "t_SAT", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if ( benchmark != "iwls2005/mem_ctrl" ) continue;

    fmt::print( "[i] processing {}\n", benchmark );
    aig_network aig;
    lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) );
    auto size_before = aig.num_gates();

    patgen_params ps;
    patgen_stats st;

    uint32_t num_random_pattern = 256;
    std::string write_pats = "256sa1/" + benchmark + ".pat";

    ps.num_stuck_at = 1;
    //ps.observability_type1 = true;
    //ps.observability_type2 = true;
    //ps.odc_levels = 5;    
    ps.random_seed = 1689;
    ps.progress = false;
    //ps.verbose = true;

    partial_simulator sim( aig.num_pis(), num_random_pattern, ps.random_seed );

    pattern_generation( aig, sim, ps, &st );
    aig = cleanup_dangling( aig );

    sim.write_patterns( write_pats );

    const auto cec = benchmark == "hyp" ? true : abc_cec( aig, benchmark );
    exp( benchmark, aig.num_pis(), size_before, sim.num_bits(), st.num_generated_patterns, st.num_constant, to_seconds( st.time_total ), to_seconds( st.time_sim ), to_seconds( st.time_sat ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}
