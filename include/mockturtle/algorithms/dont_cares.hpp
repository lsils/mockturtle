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
  \file dont_cares.hpp
  \brief Compute don't cares

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <vector>

#include "../traits.hpp"
#include "../algorithms/reconv_cut.hpp"
#include "../algorithms/simulation.hpp"
#include "../views/fanout_view.hpp"
#include "../views/window_view.hpp"

#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>

namespace mockturtle
{

template<class Ntk>
kitty::dynamic_truth_table satisfiability_dont_cares( Ntk const& ntk, std::vector<node<Ntk>> const& leaves )
{
  auto extended_leaves = reconv_cut( reconv_cut_params{10} )( ntk, leaves );

  fanout_view<Ntk> fanout_ntk{ntk};
  fanout_ntk.clear_visited();

  window_view<fanout_view<Ntk>> window_ntk{fanout_ntk, extended_leaves, leaves, false};

  default_simulator<kitty::dynamic_truth_table> sim( window_ntk.num_pis() );
  const auto tts = simulate_nodes<kitty::dynamic_truth_table>( window_ntk, sim );

  /* first create care and then invert */
  kitty::dynamic_truth_table care( leaves.size() );
  for ( auto i = 0u; i < ( 1u << window_ntk.num_pis() ); ++i )
  {
    uint32_t entry{0u};
    for ( auto j = 0u; j < leaves.size(); ++j )
    {
      entry |= kitty::get_bit( tts[leaves[j]], i ) << j;
    }
    kitty::set_bit( care, entry );
  }
  return ~care;
}

} /* namespace mockturtle */
