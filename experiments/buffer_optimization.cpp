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

#include "experiments.hpp"
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/algorithms/aqfp/aqfp_buffer.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <lorina/lorina.hpp>
#include <fmt/format.h>

int main( int argc, char* argv[] )
{
  (void)argc;
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>
    exp( "buffer_optimization", "benchmark", "#gates", "depth", "ASAP", "ALAP", "opt", "depth2" );

  static const std::string benchmarks_aqfp[] = {
    /*"5xp1",*/ "c1908", "c432", "c5315", "c880", "chkn", "count", "dist", "in5", "in6", "k2",
    "m3", "max512", "misex3", "mlp4", "prom2", "sqr6", "x1dn"};

  //std::string const benchmark( argv[1] );
  for ( auto const& benchmark : benchmarks_aqfp )
  {
    uint32_t b_start, b_ALAP, b_OPT;
    fmt::print( "[i] processing {}\n", benchmark );
    mig_network mig;
    auto const read_res = lorina::read_verilog( "benchmarks_aqfp/" + benchmark + ".v", verilog_reader( mig ) );
    assert( read_res == lorina::return_code::success );

    aqfp_buffer_params ps;
    ps.branch_pis = true;
    ps.balance_pis = true;
    ps.balance_pos = true;
    ps.splitter_capacity = 3u;
    aqfp_buffer bufcnt( mig, ps );
    bufcnt.count_buffers();
    b_start = bufcnt.num_buffers();
    //bufcnt.print_graph();

    bufcnt.ALAP();
    bufcnt.count_buffers();
    b_ALAP = bufcnt.num_buffers();
    //bufcnt.print_graph();

    if ( b_ALAP > b_start )
      bufcnt.ASAP(); // UNDO ALAP

    //bufcnt.print_graph();
    while ( bufcnt.optimize() ) {}
    bufcnt.count_buffers();
    //bufcnt.adjust_depth();
    b_OPT = bufcnt.num_buffers();
    //bufcnt.print_graph();

    depth_view d{mig};
    exp( benchmark, mig.num_gates(), d.depth(), b_start, b_ALAP, b_OPT, bufcnt.depth() );
  }

  exp.save();
  exp.table();

  return 0;
}
