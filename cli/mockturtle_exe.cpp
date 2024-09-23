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

/*!
  \file mockturtle_exe.cpp
  \brief CLI mockturtle

  \author Alessandro tempia Calvino
*/

/* include global data */
#include "mockturtle_global.hpp"

/* include stores */
#include "store/cell_library.hpp"
#include "store/network_manager.hpp"

/* include i/o commands */
#include "commands/io/read.hpp"
#include "commands/io/read_genlib.hpp"
#include "commands/io/write.hpp"

/* include data representations */
#include "commands/representation/aig.hpp"
#include "commands/representation/mig.hpp"
#include "commands/representation/xag.hpp"
#include "commands/representation/xmg.hpp"

/* include synthesis commands */
#include "commands/synthesis/rewrite.hpp"
#include "commands/synthesis/balance.hpp"

/* include mapping commands */
#include "commands/mapping/emap.hpp"
#include "commands/mapping/strash.hpp"

/* include printing commands */
#include "commands/printing/print_stats.hpp"

int main( int argc, char** argv )
{
  std::cout << "EPFL Mockturtle v0.3\n";

  _ALICE_MAIN_BODY( mockturtle )
  return cli.run( argc, argv );
}
