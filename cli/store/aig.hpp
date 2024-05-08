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
  \file aig.hpp
  \brief AIG store

  \author Alessandro tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>
#include <fmt/format.h>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/names_view.hpp>

namespace alice
{
using aig_names = mockturtle::names_view<mockturtle::aig_network>;
using aig_ntk = std::shared_ptr<aig_names>;

ALICE_ADD_STORE( aig_ntk, "aig", "a", "AIG", "AIGs" )

ALICE_DESCRIBE_STORE( aig_ntk, aig )
{
  return fmt::format( "{} : i/0 = {:5d}/{:5d}  and = {:6d}",
                      aig->get_network_name(),
                      aig->num_pis(),
                      aig->num_pos(),
                      aig->num_gates() );
}

ALICE_PRINT_STORE_STATISTICS( aig_ntk, os, aig )
{
  mockturtle::depth_view aig_d{ *aig };
  os << fmt::format( "{} : i/0 = {:5d}/{:5d}  and = {:6d}  lev = {}",
                     aig->get_network_name(),
                     aig->num_pis(),
                     aig->num_pos(),
                     aig->num_gates(),
                     aig_d.depth() )
     << std::endl;
}

ALICE_LOG_STORE_STATISTICS( aig_ntk, aig )
{
  mockturtle::depth_view aig_d{ *aig };
  return { { "name", aig->get_network_name() },
           { "inputs", aig->num_pis() },
           { "outputs", aig->num_pos() },
           { "nodes", aig->size() },
           { "and", aig->num_gates() },
           { "lev", aig_d.depth() } };
}

} // namespace alice