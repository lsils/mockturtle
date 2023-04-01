/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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

#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/explorer.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <experiments.hpp>
#include <fmt/format.h>
#include <string>

static const std::string benchmark_repo_path = "../../SCE-benchmarks";

/* AQFP benchmarks */
std::vector<std::string> aqfp_benchmarks = {
    "5xp1", "c1908", "c432", "c5315", "c880", "chkn", "count", "dist", "in5", "in6", "k2",
    "m3", "max512", "misex3", "mlp4", "prom2", "sqr6", "x1dn" };

std::string benchmark_aqfp_path( std::string const& benchmark_name )
{
  return fmt::format( "{}/MCNC/original/{}.v", benchmark_repo_path, benchmark_name );
}

int main2( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, bool> exp( "deepsyn", "benchmark", "size_before", "size_after", "depth", "cec" );

  for ( auto const& benchmark : aqfp_benchmarks )
  {
    if ( argc == 2 && benchmark != std::string( argv[1] ) ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    mig_network ntk;
    if ( lorina::read_verilog( benchmark_aqfp_path( benchmark ), verilog_reader( ntk ) ) != lorina::return_code::success )
    {
      std::cerr << "Cannot read " << benchmark << "\n";
      return -1;
    }

    explorer_params ps;
    ps.num_restarts = 4;
    ps.max_steps_no_impr = 50;
    ps.timeout = 45;
    ps.compressing_scripts_per_step = 1;
    ps.verbose = true;
    //ps.very_verbose = true;

    mig_network opt = deepsyn_aqfp( ntk, ps );
    depth_view d( opt );

    exp( benchmark, ntk.num_gates(), opt.num_gates(), d.depth(), true );
  }

  exp.save();
  exp.table();

  return 0;
}

int main( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, bool> exp( "deepsyn", "benchmark", "size_before", "size_after", "depth", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    if ( argc == 2 && benchmark != std::string( argv[1] ) ) continue;
    if ( benchmark == "hyp" || benchmark == "adder" || benchmark == "dec" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    using Ntk = mig_network;
    Ntk ntk;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( ntk ) ) != lorina::return_code::success )
    {
      std::cerr << "Cannot read " << benchmark << "\n";
      return -1;
    }

    explorer_params ps;
    ps.num_restarts = 4;
    ps.max_steps_no_impr = 50;
    ps.timeout = 45;
    ps.compressing_scripts_per_step = 3;
    ps.verbose = true;
    //ps.very_verbose = true;

    Ntk opt = default_mig_synthesis( ntk, ps );
    //Ntk opt = deepsyn( ntk, ps );
    bool const cec = abc_cec_impl( opt, benchmark_path( benchmark ) );
    depth_view d( opt );

    exp( benchmark, ntk.num_gates(), opt.num_gates(), d.depth(), cec );
  }

  exp.save();
  exp.table();

  return 0;
}