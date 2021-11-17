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
#include <lorina/verilog.hpp>
#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/algorithms/collapse_mapped.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <experiments.hpp>

using namespace experiments;
using namespace mockturtle;

struct opt_params_t {
  uint32_t optimization_rounds{1};
  uint32_t max_remapping_rounds{3};
  uint32_t max_resynthesis_rounds{10};
};

mig_network optimization_round( mig_network const& ntk, exact_library<mig_network, mig_npn_resynthesis> const& exact_lib, opt_params_t const& opt_params )
{
  map_params psm;
  psm.skip_delay_round = false;
  map_stats stm;

  mig_network mig = cleanup_dangling( ntk );

  /* initial mig mapping, depth-oriented */
  for ( auto i = 0u; i < opt_params.max_remapping_rounds; ++i )
  {
    uint32_t old_mig_depth = depth_view( ntk ).depth();
    uint32_t old_mig_size = ntk.num_gates();

    mig_network mig_map = map( mig, exact_lib, psm, &stm );

    if ( depth_view( mig_map ).depth() > old_mig_depth ||
       ( depth_view( mig_map ).depth() == old_mig_depth && mig_map.num_gates() >= old_mig_size ) )
    {
      break;
    }
    mig = cleanup_dangling( mig_map );
  }

  /* mig resynthesis */
  /* ... */

  return mig;
}

int main( int argc, char** argv )
{
  opt_params_t opt_params;

  if ( argc > 1 )
  {
    opt_params.optimization_rounds = strtoul( argv[1], nullptr, 10 );
  }
  if ( argc > 2 )
  {
    opt_params.max_remapping_rounds = strtoul( argv[2], nullptr, 10 );
  }
  if ( argc > 3 )
  {
    opt_params.max_resynthesis_rounds = strtoul( argv[3], nullptr, 10 );
  }

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool> exp(
      "aqfp", "benchmark", "size", "depth", "size_after", "depth_after", "aqfp_size", "aqfp_depth", "equivalent" );

  /* library to map to MIGs */
  mig_npn_resynthesis resyn{ true };
  exact_library_params eps;
  exact_library<mig_network, mig_npn_resynthesis> exact_lib( resyn, eps );

  for ( auto const& benchmark : aqfp_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    mig_network mig;
    if ( lorina::read_verilog( benchmark_aqfp_path( benchmark ), verilog_reader( mig ) ) != lorina::return_code::success )
    {
      continue;
    }

    const uint32_t size_before = mig.num_gates();
    const uint32_t depth_before = depth_view( mig ).depth();

    /* main optimization loop */
    for ( auto i = 0u; i < opt_params.optimization_rounds; ++i )
    {
      // const uint32_t size_before = mig.num_gates();
      // const uint32_t depth_before = depth_view( mig ).depth();

      auto mig_opt = optimization_round( mig, exact_lib, opt_params );

      /* TODO: add on improvement only, or save current best */
      mig = cleanup_dangling( mig_opt );
    }

    const auto cec = abc_cec_aqfp( mig, benchmark );
    const uint32_t size_mig = mig.num_gates();
    const uint32_t depth_mig = depth_view( mig ).depth();

    exp( benchmark, size_before, depth_before, size_mig, depth_mig, 0, 0, cec );
  }

  exp.save();
  exp.table();

  return 0;
}
