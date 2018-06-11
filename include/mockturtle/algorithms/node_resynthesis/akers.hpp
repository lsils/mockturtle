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
  \file akers.hpp
  \brief Resynthesis with Akers synthesis

  \author Mathias Soeken
  \author Eleonora Testa
*/

#pragma once

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>

#include "../../algorithms/akers_synthesis.hpp"
#include "../../networks/mig.hpp"

namespace mockturtle
{

/*! \brief Resynthesis function based on Akers synthesis.
 *
 * This resynthesis function can be passed to ``node_resynthesis``.  It will
 * call Akers synthesis on each node.
 *
   \verbatim embed:rst
  
   Example
   
   .. code-block:: c++
   
      const klut_network klut = ...;
      akers_resynthesis resyn;
      const auto mig = node_resynthesis<mig_network>( klut, resyn );
   \endverbatim
 */
class akers_resynthesis
{
public:
  template<typename LeavesIterator>
  mig_network::signal operator()( mig_network& mig, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end )
  {
    return akers_synthesis( mig, function, function, begin, end );
  }
};

} /* namespace mockturtle */
