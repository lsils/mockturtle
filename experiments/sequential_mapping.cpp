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

#include <string>
#include <vector>

#include <fmt/format.h>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/experimental/sequential_mapping.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/sequential.hpp>
#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/io/write_blif.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/views/mapping_view.hpp>
#include <mockturtle/algorithms/collapse_mapped.hpp>

int main( int argc, char ** argv )
{

  (void)argc;

  using namespace mockturtle;
  using namespace mockturtle::experimental;

  sequential<klut_network> sequential_klut;
  (void)lorina::read_blif( argv[1], blif_reader( sequential_klut ) );

  fmt::print( "Read initial network (blif_reader)\n" );
  fmt::print( "num LUTs = {}\t", sequential_klut.num_gates() );
  fmt::print( "num FFs = {}\n", sequential_klut.num_registers() );

  sequential_klut = cleanup_dangling( sequential_klut );

  fmt::print( "Cleanup network (cleanup_dangling)\n" );
  fmt::print( "num LUTs = {}\t", sequential_klut.num_gates() );
  fmt::print( "num FFs = {}\n", sequential_klut.num_registers() );
  write_blif( sequential_klut, argv[2] );
  
  mapping_view<decltype(sequential_klut), true> viewed{sequential_klut};
  sequential_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 6;
  sequential_mapping( viewed, ps );
  sequential_klut = *collapse_mapped_network<decltype(sequential_klut)>( viewed );

  fmt::print( "Re-Mapped network (sequential_mapping, cut_size = 6)\n" );
  fmt::print( "num LUTs = {}\t", sequential_klut.num_gates() );
  fmt::print( "num FFs = {}\n", sequential_klut.num_registers() );
  // write_blif( sequential_klut, argv[2] );

  static_assert( has_ri_to_ro_v<sequential<klut_network>>, "sequential interface with no ri-to-ro" );

  return 0;
}