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
#include <lorina/blif.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/experimental/fast_cec.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/algorithms/klut_to_graph.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/io/write_aiger.hpp>
#include <mockturtle/networks/aig.hpp>

#include <experiments.hpp>

int main()
{
  using namespace experiments;
  using namespace mockturtle;
  using namespace mockturtle::experimental;

  experiment<std::string, double, double, bool> exp( "equivalence_checking", "benchmark", "abc cec", "new cec", "equivalent" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    if ( benchmark == "hyp" ) continue;
    if ( benchmark == "div" ) continue;
    // if ( benchmark != "sin" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );
    aig_network aig1;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig1 ) ) != lorina::return_code::success )
    {
      continue;
    }
    
    aig_network aig2;
    if ( false )
    {
      klut_network klut;
      if ( lorina::read_blif( "../experiments/cec_benchmarks/"+benchmark+".blif", blif_reader( klut ) ) != lorina::return_code::success )
      {
        continue;
      }
      aig2 = convert_klut_to_graph<aig_network, klut_network>( klut );
    }
    else
    {
      aig_network aig = aig1;
      xag_npn_resynthesis<aig_network> resyn;
      cut_rewriting_params ps;
      ps.cut_enumeration_ps.cut_size = 4;
      ps.progress = true;
      aig2 = cut_rewriting( aig, resyn, ps );
    }
    if ( false )
    {
      aig_network miter_ = *reduced_miter<aig_network>( aig1, aig2 );
      write_aiger( miter_, "../experiments/miters/" + benchmark + "_miter.aig" );
      continue;
    }

    stopwatch<>::duration time_fast_cec{ 0 };
    fast_cec_stats cst;
    fast_cec_params cps;
    cps.verbose = true;
    bool cec_fast = call_with_stopwatch( time_fast_cec, [&]() { 
      aig_network miter_ = *reduced_miter<aig_network>( aig1, aig2 );
      fmt::print("[i] miter #gates = {}\n", miter_.num_gates() );

      return *fast_cec( miter_, cps, &cst ); 
    } );

    stopwatch<>::duration time_abc{ 0 };
    bool cec_abc = call_with_stopwatch( time_abc, [&]() { 
      return abc_cec( aig2, benchmark ); 
    } );

    exp( benchmark, to_seconds( time_abc ), to_seconds( time_fast_cec ), ( cec_fast == cec_abc ) );
  }

  exp.save();
  exp.table();

  return 0;
}
