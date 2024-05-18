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
  \file cell_library.hpp
  \brief Store cell library

  \author Alessandro tempia Calvino
*/

#pragma once
#include <string>

#include <alice/alice.hpp>
#include <fmt/format.h>
#include <mockturtle/utils/standard_cell.hpp>
#include <mockturtle/utils/tech_library.hpp>

namespace alice
{

using tech_library_store = std::shared_ptr<mockturtle::tech_library<9u, mockturtle::classification_type::np_configurations>>;

ALICE_ADD_STORE( tech_library_store, "cell", "c", "Cell library", "Cell libraries" )

ALICE_DESCRIBE_STORE( tech_library_store, lib )
{
  return lib->get_library_name();
}

ALICE_PRINT_STORE_STATISTICS( tech_library_store, os, lib )
{
  os << fmt::format( "{} containing {} cells",
                     lib->get_library_name(),
                     lib->get_cells().size() )
     << std::endl;
}

ALICE_LOG_STORE_STATISTICS( tech_library_store, lib )
{
  return { { "name", lib->get_library_name() },
           { "cells", lib->get_cells().size() } };
}

} // namespace alice