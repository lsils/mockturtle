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

#include <mockturtle/algorithms/rewrite.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <lorina/aiger.hpp>

#include <experiments.hpp>
#include <fmt/format.h>
#include <string>

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, float, bool>
    exp( "refactoring", "benchmark", "size_before", "size_after", "depth_before", "depth_after", "runtime", "equivalent" );

  xag_npn_resynthesis<aig_network, aig_network, xag_npn_db_kind::aig_complete> resyn;
  exact_library_params eps;
  eps.np_classification = false;
  exact_library<aig_network, decltype( resyn )> exact_lib( resyn, eps );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network aig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    rewrite_params ps;
    rewrite_stats st;

    uint32_t const size_before = aig.num_gates();
    uint32_t const depth_before = depth_view( aig ).depth();

    rewrite( aig, exact_lib, ps, &st );

    bool const cec = benchmark == "hyp" ? true : abc_cec( aig, benchmark );
    exp( benchmark, size_before, aig.num_gates(), depth_before, depth_view( aig ).depth(), to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}