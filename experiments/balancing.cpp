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
#include <mockturtle/algorithms/balancing.hpp>
#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <experiments.hpp>

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, double, bool> exp( "sop_balancing", "benchmark", "size_before", "depth_before", "size_after", "depth_after", "runtime", "equivalent" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );
    xag_network xag;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) ) != lorina::return_code::success )
    {
      continue;
    }

    const uint32_t size_before = xag.num_gates();
    const uint32_t depth_before = depth_view{ xag }.depth();

    lut_map_params ps;
    ps.cut_enumeration_ps.cut_size = 4u;
    lut_map_stats st;
    const xag_network balanced_xag = esop_balancing( xag, ps, &st );

    const uint32_t size_after = balanced_xag.num_gates();
    const uint32_t depth_after = depth_view{ balanced_xag }.depth();

    auto const cec = benchmark == "hyp" ? true : abc_cec( balanced_xag, benchmark );

    exp( benchmark, size_before, depth_before, size_after, depth_after, to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}