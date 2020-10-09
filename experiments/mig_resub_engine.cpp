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

  std::vector<kitty::dynamic_truth_table> tts( 4, kitty::dynamic_truth_table( 3 ) );

  // DOT: 00011010
  // ONEHOT: 00010110
  // GAMBLE: 10000001
  kitty::create_from_binary_string( tts[0], "10000001" ); // target
  kitty::create_nth_var( tts[1], 0 );
  kitty::create_nth_var( tts[2], 1 );
  kitty::create_nth_var( tts[3], 2 );

  mig_resub_engine<kitty::dynamic_truth_table, true> engine( 3 );
  engine.add_root( 0, tts );
  engine.add_divisor( 1, tts );
  engine.add_divisor( 2, tts );
  engine.add_divisor( 3, tts );

  const auto res = engine.compute_function( 5u );

  return 0;
}
