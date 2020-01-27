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
#include <lorina/verilog.hpp>
#include <mockturtle/algorithms/xmg_resub.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/xmg_npn.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/properties/xmgcost.hpp>


#include <experiments.hpp>

int main()
{
    using namespace experiments;
    using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, float, float, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool > exp( "xmg_resubstitution", "benchmark", "size_before", "size_before_cr", "size_after", "runtime_resub", "runtime_rewrite", "total_xor3", "actual_xor3", "actual_xor2", "total_maj", "actual_maj", "remaining_maj","equivalent" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if (benchmark != "div") 
    //    continue;
    fmt::print( "[i] processing {}\n", benchmark );
    xmg_network xmg;
    lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xmg ) );

    // Preparing for the xmg_cost calculation 
    xmg_cost_params xmg_ps;

    // For XMG Resubstitution
    resubstitution_params ps;
    resubstitution_stats st;
    ps.max_pis = 8u;
    ps.max_inserts = 1u;  // Discuss with Heinz once.

    const uint32_t size_before = xmg.num_gates();
    xmg_resubstitution(xmg, ps, &st);

    xmg = cleanup_dangling( xmg );
    
    num_gate_profile(xmg,xmg_ps);
    xmg_ps.report();
    const auto cec2 = benchmark == "hyp" ? true : abc_cec( xmg, benchmark );
    std:: cout << "Equivalence after resubstitution = \n" <<  cec2 << std::endl;
    // rewriting
    xmg_npn_resynthesis resyn;

    cut_rewriting_params cr_ps;
    cr_ps.cut_enumeration_ps.cut_size = 4;
    cr_ps.progress = true;

    uint32_t size_before_cr = xmg.num_gates();
    cut_rewriting_stats cr_st;
    cut_rewriting( xmg, resyn, cr_ps, &cr_st );
    xmg = cleanup_dangling( xmg );

    num_gate_profile(xmg,xmg_ps);

    // For Rewriting 
    // Check for ABC equivalence
    const auto cec = benchmark == "hyp" ? true : abc_cec( xmg, benchmark );
    
    // Figure out how to integrate the xmg_cost.hpp as well  
    exp( benchmark, size_before, size_before_cr, xmg.num_gates(), to_seconds( st.time_total ), to_seconds( cr_st.time_total), xmg_ps.total_xor3, xmg_ps.actual_xor3, xmg_ps.actual_xor2, xmg_ps.total_maj, xmg_ps.actual_maj, xmg_ps.remaining_maj, cec );
    
  }
  
  exp.save();
  exp.table();
  return 0;
}
