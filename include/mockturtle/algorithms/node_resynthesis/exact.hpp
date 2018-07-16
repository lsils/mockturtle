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
  \file exact.hpp
  \brief Replace with exact synthesis result

  \author Mathias Soeken
*/

#pragma once

#include <iostream>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>
#include <kitty/print.hpp>
#include <percy/percy.hpp>

#include "../../networks/klut.hpp"

namespace mockturtle
{

class exact_resynthesis
{
public:
  template<typename LeavesIterator>
  klut_network::signal operator()( klut_network& ntk, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end )
  {
    if ( function.num_vars() <= 3 )
    {
      return ntk.create_node( std::vector<klut_network::signal>( begin, end ), function );
    }

    percy::spec spec;
    spec.fanin = 3;
    spec.verbosity = 0;
    spec[0] = function;

    percy::chain c;

    const auto result = percy::synthesize( spec, c );
    assert( result == percy::success );

    c.denormalize();

    std::vector<klut_network::signal> signals( begin, end );
    const auto num_vars = function.num_vars();
    assert( signals.size() == num_vars );

    for ( auto i = 0; i < c.get_nr_steps(); ++i )
    {
      const auto& children = c.get_step( i );

      std::vector<klut_network::signal> fanin;
      for ( const auto& child : c.get_step( i ) )
      {
        fanin.emplace_back( signals[child] );
      }
      signals.emplace_back( ntk.create_node( fanin, c.get_operator( i ) ) );
    }

    return signals.back();
  }
};

} /* namespace mockturtle */
