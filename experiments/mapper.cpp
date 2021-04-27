/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
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
#include <lorina/genlib.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/views/depth_view.hpp>


#include <experiments.hpp>

std::string const mcnc_library =  "GATE   inv1    1 O=!a;           PIN * INV 1 999 0.9 0.3 0.9 0.3\n"
                                  "GATE   inv2    2 O=!a;           PIN * INV 2 999 1.0 0.1 1.0 0.1\n"
                                  "GATE   inv3    3 O=!a;           PIN * INV 3 999 1.1 0.09 1.1 0.09\n"
                                  "GATE   inv4    4 O=!a;           PIN * INV 4 999 1.2 0.07 1.2 0.07\n"
                                  "GATE   nand2   2 O=!(ab);        PIN * INV 1 999 1.0 0.2 1.0 0.2\n"
                                  "GATE   nand3   3 O=!(abc);	      PIN * INV 1 999 1.1 0.3 1.1 0.3\n"
                                  "GATE   nand4   4 O=!(abcd);      PIN * INV 1 999 1.4 0.4 1.4 0.4\n"
                                  "GATE   nor2    2 O=!{ab};        PIN * INV 1 999 1.4 0.5 1.4 0.5\n"
                                  "GATE   nor3    3 O=!{abc};       PIN * INV 1 999 2.4 0.7 2.4 0.7\n"
                                  "GATE   nor4    4 O=!{abcd};      PIN * INV 1 999 3.8 1.0 3.8 1.0\n"
                                  "GATE   and2    3 O=(ab);         PIN * NONINV 1 999 1.9 0.3 1.9 0.3\n"
                                  "GATE   or2     3 O={ab};         PIN * NONINV 1 999 2.4 0.3 2.4 0.3\n"
                                  "GATE   xor2a   5 O=[ab];         PIN * UNKNOWN 2 999 1.9 0.5 1.9 0.5\n"
                                  "#GATE  xor2b   5 O=[ab];         PIN * UNKNOWN 2 999 1.9 0.5 1.9 0.5\n"
                                  "GATE   xnor2a  5 O=![ab];        PIN * UNKNOWN 2 999 2.1 0.5 2.1 0.5\n"
                                  "#GATE  xnor2b  5 O=![ab];        PIN * UNKNOWN 2 999 2.1 0.5 2.1 0.5\n"
                                  "GATE   aoi21   3 O=!{(ab)c};     PIN * INV 1 999 1.6 0.4 1.6 0.4\n"
                                  "GATE   aoi22   4 O=!{(ab)(cd)};  PIN * INV 1 999 2.0 0.4 2.0 0.4\n"
                                  "GATE   oai21   3 O=!({ab}c);     PIN * INV 1 999 1.6 0.4 1.6 0.4\n"
                                  "GATE   oai22   4 O=!({ab}{cd});  PIN * INV 1 999 2.0 0.4 2.0 0.4\n"
                                  "GATE   buf     2 O=a;            PIN * NONINV 1 999 1.0 0.0 1.0 0.0\n"
                                  "GATE   zero    0 O=0;\n"
                                  "GATE   one     0 O=1;";

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, double, uint32_t, float, float, bool> exp( "mapper", "benchmark", "size", "area_after", "depth", "delay_after", "runtime", "equivalent" );

  fmt::print( "[i] processing technology library\n" );

  std::vector<gate> gates;

  std::istringstream in( mcnc_library );
  lorina::read_genlib( in, genlib_reader( gates ) );

  tech_library_params tps;
  tech_library<5> lib( gates, tps );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );
    aig_network aig;
    lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) );

    const uint32_t size_before = aig.num_gates();
    const uint32_t depth_before = depth_view( aig ).depth();

    map_params ps;
    ps.cut_enumeration_ps.cut_size = 5;
    map_stats st;

    auto res = tech_map( aig, lib, ps, &st );

    const auto cec = benchmark == "hyp" ? true : abc_cec( res, benchmark );

    exp( benchmark, size_before, st.area, depth_before, st.delay, to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}
