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
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>

#include <experiments.hpp>

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, bool> exp( "sim_resubstitution", "benchmark", "#PI", "size", "size_after", "#pat aug", "#cex", "#const", "#div0", "#div1", "total time", "sim time", "SAT time", "sub. time", "equivalent" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    if ( benchmark == "hyp" || benchmark == "mem_ctrl" || benchmark == "log2" || benchmark == "div" || benchmark == "sqrt") continue;
    //if ( benchmark != "sin" ) continue;

    fmt::print( "[i] processing {}\n", benchmark );
    aig_network aig, orig;
    lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) );
    lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( orig ) );

    simresub_params ps;
    simresub_stats st;

    ps.num_pattern_base = (benchmark=="div")? 17u: 15u;
    ps.num_reserved_blocks = (benchmark=="div")? 200u: 100u;
    // assert ( ps.num_reserved_blocks < (1 << (num_pattern_base-6)) );
    ps.max_pis = 10u; //100u; //8u;
    ps.max_divisors = 500u;
    ps.max_inserts = 1u;
    ps.progress = false;

    sim_resubstitution( aig, ps, &st );
    aig = cleanup_dangling( aig );
    //orig = cleanup_dangling( orig );

    const auto cec = benchmark == "hyp" ? true : abc_cec( aig, benchmark );
    //equivalence_checking_params cec_ps;
    //cec_ps.conflict_limit = 100;
    //const auto cec_res = equivalence_checking( *miter<aig_network>( orig, aig ), cec_ps );
    //const auto cec = true; //cec_res && *cec_res;
    std::cout << "num_total_divisors = " << st.num_total_divisors << std::endl;
    exp( benchmark, aig.num_pis(), orig.num_gates(), aig.num_gates(), st.num_generated_patterns, st.num_cex, st.num_constant, st.num_div0_accepts, st.num_div1_accepts, to_seconds( st.time_total ), to_seconds( st.time_sim ), to_seconds( st.time_sat ), to_seconds( st.time_substitute ), cec );
  }

  exp.save();
  exp.table();
  //exp.compare();

  return 0;
}
