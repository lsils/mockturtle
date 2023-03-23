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
#include <mockturtle/networks/mig.hpp>

#include <experiments.hpp>
#include <fmt/format.h>
#include <string>

int main( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  if ( argc != 2 )
  {
    std::cerr << "No benchmark path provided\n";
    return -1;
  }

  std::string benchmark( argv[1] );
  mig_network mig;
  if ( lorina::read_aiger( benchmark, aiger_reader( mig ) ) != lorina::return_code::success )
  {
    std::cerr << "Cannot read " << benchmark << "\n";
    return -1;
  }

  explorer_params ps;
  ps.max_steps_no_impr = 100;
  ps.compressing_scripts_per_step = 1;
  ps.verbose = true;

  mig_network opt = default_mig_synthesis( mig, ps );
  bool const cec = abc_cec_impl( opt, benchmark );

  fmt::print( "size before = {}, size after = {}, cec = {}\n", mig.size(), opt.size(), cec );

  return 0;
}