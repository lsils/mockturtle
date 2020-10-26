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
  kitty::create_from_hex_string( tts[0], "17ac" ); // target

  mig_resub_engine<kitty::dynamic_truth_table> engine( n );
  engine.add_root( 0, tts );
  for ( auto i = 0u; i < n; ++i )
  {
    engine.add_divisor( i+1, tts );
  }
  const auto res = engine.compute_function( 10u );
  if ( !res )
  {
    std::cout << "did not find solution within 15 nodes.\n";
  }
  else
  {
    std::cout << "found solution of size " << ( (*res).size() - 1 ) / 3 << "\n";
  }

#else
  std::unordered_set<kitty::dynamic_truth_table, kitty::hash<kitty::dynamic_truth_table>> classes;
  kitty::dynamic_truth_table tt( n );

  do
  {
    /* apply NPN canonization and add resulting representative to set */
    const auto res = kitty::exact_npn_canonization( tt );
    classes.insert( std::get<0>( res ) );

    /* increment truth table */
    kitty::next_inplace( tt );
  } while ( !kitty::is_const0( tt ) );

  uint64_t total_size = 0u, failed = 0u;
  for ( auto const& func : classes )
  {
    tts[0] = func;
    std::cout << "function: "; kitty::print_hex( tts[0] );
    mig_resub_engine<kitty::dynamic_truth_table> engine( n );
    engine.add_root( 0, tts );
    for ( auto i = 0u; i < n; ++i )
    {
      engine.add_divisor( i+1, tts );
    }
    const auto res = engine.compute_function( 15u );
    if ( !res )
    {
      std::cout << " did not find solution within 15 nodes.\n";
      ++failed;
    }
    else
    {
      assert( ( (*res).size() - 1 ) % 3 == 0 );
      std::cout << " found solution of size " << ( (*res).size() - 1 ) / 3 << "\n";
      total_size += ( (*res).size() - 1 ) / 3;
    }
  }

  std::cout << "total size: " << total_size << "\n";
  std::cout << "failed: " << failed << "\n";
#endif
  return 0;
}
