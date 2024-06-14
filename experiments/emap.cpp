/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2024  EPFL
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

#include <iostream>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <lorina/aiger.hpp>
#include <lorina/genlib.hpp>
#include <mockturtle/algorithms/aig_balancing.hpp>
#include <mockturtle/algorithms/emap.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/block.hpp>
#include <mockturtle/utils/name_utils.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/cell_view.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/names_view.hpp>

#include <experiments.hpp>

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, double, uint32_t, double, uint32_t, float, bool> exp(
      "emap", "benchmark", "size", "area_after", "depth", "delay_after", "multioutput", "runtime", "cec" );

  /* library to map to technology */
  fmt::print( "[i] processing technology library\n" );
  std::string library = "multioutput";
  std::vector<gate> gates;
  std::ifstream in( cell_libraries_path( library ) );

  if ( lorina::read_genlib( in, genlib_reader( gates ) ) != lorina::return_code::success )
  {
    return 1;
  }

  tech_library_params tps;
  tps.ignore_symmetries = false; // set to true to drastically speed-up mapping with minor delay increase
  tps.verbose = true;
  tech_library<9> tech_lib( gates, tps );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    names_view<aig_network> aig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* remove structural redundancies */
    aig_balancing_params bps;
    bps.minimize_levels = false;
    bps.fast_mode = true;
    aig_balance( aig, bps );

    const uint32_t size_before = aig.num_gates();
    const uint32_t depth_before = depth_view( aig ).depth();

    emap_params ps;
    ps.matching_mode = emap_params::hybrid;
    ps.area_oriented_mapping = false;
    ps.map_multioutput = true;
    ps.relax_required = 0;
    emap_stats st;
    cell_view<block_network> res = emap<9>( aig, tech_lib, ps, &st );

    names_view res_names{ res };
    restore_network_name( aig, res_names );
    restore_pio_names_by_order( aig, res_names );
    const auto cec = benchmark == "hyp" ? true : abc_cec_mapped_cell( res_names, benchmark, library );

    /* write verilog netlist */
    // write_verilog_with_cell( res_names, benchmark + "_mapped.v" );

    exp( benchmark, size_before, res.compute_area(), depth_before, res.compute_worst_delay(), st.multioutput_gates, to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}