/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2020  EPFL
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

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>

#include <mockturtle/algorithms/resub_engines.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/networks/mig.hpp>

//#include <experiments.hpp>

int main()
{
  //using namespace experiments;
  using namespace mockturtle;
  auto n = 3u;

  std::vector<kitty::dynamic_truth_table> tts( n + 1, kitty::dynamic_truth_table( n ) );

  for ( auto i = 0u; i < n; ++i )
  {
    kitty::create_nth_var( tts[i+1], i );
  }

#if 0
  kitty::create_from_hex_string( tts[0], "1e" ); // target

  mig_resub_engine<kitty::dynamic_truth_table> engine( n );
  engine.add_root( 0, tts );
  for ( auto i = 0u; i < n; ++i )
  {
    engine.add_divisor( i+1, tts );
  }
  const auto res = engine.compute_function( 10u );

#else
  uint64_t total_size = 0u;
  for ( uint64_t func = 0u; func < (1 << (1<<n)); ++func )
  {
    tts[0]._bits[0] = func;
    tts[0].mask_bits();
    std::cout << "function: "; kitty::print_hex( tts[0] );
    mig_resub_engine<kitty::dynamic_truth_table> engine( n );
    engine.add_root( 0, tts );
    for ( auto i = 0u; i < n; ++i )
    {
      engine.add_divisor( i+1, tts );
    }
    const auto res = engine.compute_function( 10u );
    if ( !res )
    {
      std::cout << " did not find solution within 10 nodes.\n";
    }
    else
    {
      assert( ( (*res).size() - 1 ) % 3 == 0 );
      std::cout << " found solution of size " << ( (*res).size() - 1 ) / 3 << "\n";
      total_size += ( (*res).size() - 1 ) / 3;
    }
  }

  std::cout << "total size: " << total_size << "\n";
#endif
  return 0;
}
