/* mockturtle: C++ logic network library
 * Copyright (C) 2018  EPFL
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
  \file debug_network.hpp
  \brief Isolate failure-inducing logic of a network

  \author Heinz Riener
*/

#pragma once

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "../views/topo_view.hpp"
#include "constant_propagation.hpp"

#include <unordered_map>
#include <vector>

namespace mockturtle
{

/*! \brief Isolates the failure-inducing logic of a network
 *
 * Given a network `ntk`, an optimizing transformation `optimize_fn`,
 * and an evaluation predicate `check_fn`, this function attempts to
 * simplify `ntk`, such that the simplfied network if optimized with
 * `optimize_fn`, still fails evaluation with `check_fn`.
 *
 * **Required network functions:**
 */
template<typename Ntk>
Ntk debug_network( Ntk const& ntk, std::function<Ntk(Ntk const&)> optimize_fn, std::function<bool(Ntk const&, Ntk const&)> check_fn )
{
  auto current_ntk = ntk;
  auto optimize_and_evaluate = [&]( auto const& ntk, bool expected_result ) -> bool
    {
      auto const optimized_ntk = optimize_fn( current_ntk );
      auto const result = check_fn( ntk, optimized_ntk );
      return result == expected_result;
    };

  for ( auto j = 0u; j < 2u; ++j )
  {
    auto i = 0u;
    bool constant = false;
    while ( optimize_and_evaluate( current_ntk, false ) && i < current_ntk.num_pis() )
    {
      std::unordered_map<typename Ntk::node, bool> values;
      values.emplace( ntk.pi_at( i ), constant );

      auto const new_ntk = constant_propagation( current_ntk, values );
      if ( optimize_and_evaluate( new_ntk, false ) )
      {
        current_ntk = new_ntk;
      }
      else
      {
        ++i;
      }
    }

    constant ^= true;
  }

  return current_ntk;
}

} // namespace mockturtle
