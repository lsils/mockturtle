/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
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
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>

#include <experiments.hpp>

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, float, float, float, bool> exp( "sim_resubstitution", "benchmark", "num_constant", "generated_patterns", "total time", "sim time", "SAT time", "equivalent" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    if ( benchmark == "hyp" || benchmark == "log2" ) continue;

    fmt::print( "[i] processing {}\n", benchmark );
    aig_network aig;
    lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) );

    simresub_params ps;
    simresub_stats st;

    ps.num_pattern_base = 15u;
    ps.num_reserved_blocks = 100u;
    ps.max_pis = 8u;
    ps.max_inserts = 1u;
    ps.progress = false;

    //const uint32_t size_before = aig.num_gates();
    sim_resubstitution( aig, ps, &st );

    aig = cleanup_dangling( aig );

    const auto cec = true; //benchmark == "hyp" ? true : abc_cec( aig, benchmark );

    exp( benchmark, st.num_constant, st.num_generated_patterns, to_seconds( st.time_total ), to_seconds( st.time_sim ), to_seconds( st.time_sat ), cec );
  }

  exp.save();
  exp.table();
  //exp.compare();

  return 0;
}
