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
#include <memory>
#include <unordered_map>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>
#include <kitty/hash.hpp>
#include <kitty/print.hpp>
#include <percy/percy.hpp>

#include "../../networks/klut.hpp"

namespace mockturtle
{

/*! \brief Resynthesis function based on Akers synthesis.
 *
 * This resynthesis function can be passed to ``node_resynthesis``,
 * ``cut_rewriting``, and ``refactoring``.  The given truth table will be
 * resynthized in terms of an optimum size `k`-LUT network, where `k` is
 * specified as input to the constructor.  In order to guarantee a reasonable
 * runtime, `k` should be 3 or 4.
 *
   \verbatim embed:rst
  
   Example
   
   .. code-block:: c++
   
      const klut_network klut = ...;

      exact_resynthesis resyn( 3 );
      cut_rewriting( klut, resyn );
      klut = cleanup_dangling( klut );
   \endverbatim
 *
 * A cache can be passed as second parameter to the constructor, which will
 * store optimum networks for all functions for which resynthesis is invoked
 * for.  The cache can be used to retrieve the computed network, which reduces
 * runtime.
 *
   \verbatim embed:rst
  
   Example
   
   .. code-block:: c++
   
      const klut_network klut = ...;

      auto cache = std::make_shared<exact_resynthesis::cache_map_t>();
      exact_resynthesis resyn( 3, cache );
      cut_rewriting( klut, resyn );
      klut = cleanup_dangling( klut );

   The underlying engine for this resynthesis function is percy_.

   .. _percy: https://github.com/whaaswijk/percy
   \endverbatim
 *
 */
class exact_resynthesis
{
public:
  using cache_map_t = std::unordered_map<kitty::dynamic_truth_table, percy::chain, kitty::hash<kitty::dynamic_truth_table>>;
  using cache_t = std::shared_ptr<cache_map_t>;

  explicit exact_resynthesis( uint32_t fanin_size = 3u, cache_t cache = {} )
    : _fanin_size( fanin_size ),
      _cache( cache )
  {
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( klut_network& ntk, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn )
  {
    if ( static_cast<uint32_t>( function.num_vars() ) <= _fanin_size )
    {
      fn( ntk.create_node( std::vector<klut_network::signal>( begin, end ), function ) );
      return;
    }

    percy::spec spec;
    spec.fanin = _fanin_size;
    spec.verbosity = 0;
    spec[0] = function;

    percy::chain c = [&]() {
      if ( _cache )
      {
        const auto it = _cache->find( function );
        if ( it != _cache->end() )
        {
          return it->second;
        }
      }

      percy::chain c;
      const auto result = percy::synthesize( spec, c );
      assert( result == percy::success );
      c.denormalize();
      if ( _cache )
      {
        (*_cache)[function] = c;
      }
      return c;
    }();

    std::vector<klut_network::signal> signals( begin, end );

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

    fn( signals.back() );
  }

private:
  uint32_t _fanin_size{3u};
  cache_t _cache;
};

} /* namespace mockturtle */
