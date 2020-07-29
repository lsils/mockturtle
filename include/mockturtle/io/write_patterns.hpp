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

/*!
  \file write_patterns.hpp
  \brief Write simulation patterns

  \author Siang-Yun Lee
*/

#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <kitty/print.hpp>

#include "../algorithms/simulation.hpp"

namespace mockturtle
{

/*! \brief Writes simulation patterns
 *
 * The output contains `num_pis()` lines, each line contains a stream of
 * simulation values of a primary input, represented in hexadecimal.
 *
 * \param sim The `partial_simulator` object containing simulation patterns
 * \param out Output stream
 */
inline void write_patterns( partial_simulator const& sim, std::ostream& out = std::cout )
{
  auto const& patterns = sim.get_patterns();
  for ( auto i = 0u; i < patterns.size(); ++i )
  {
    out << kitty::to_hex( patterns.at( i ) ) << "\n";
  }
}

/*! \brief Writes simulation patterns
 *
 * The output contains `num_pis()` lines, each line contains a stream of
 * simulation values of a primary input, represented in hexadecimal.
 *
 * \param sim The `partial_simulator` object containing simulation patterns
 * \param filename Filename
 */
inline void write_patterns( partial_simulator const& sim, std::string const& filename )
{
  std::ofstream os( filename.c_str(), std::ofstream::out );
  write_patterns( sim, os );
  os.close();
}

} /* namespace mockturtle */
