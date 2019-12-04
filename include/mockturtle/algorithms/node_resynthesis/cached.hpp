/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
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
  \file cached.hpp
  \brief Generic resynthesis with a cache

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <unordered_set>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/hash.hpp>

#include "../../traits.hpp"
#include "../../algorithms/cleanup.hpp"
#include "../../utils/network_cache.hpp"
#include "../../views/cut_view.hpp"

namespace mockturtle
{

template<class Ntk, class ResynthesisFn>
class cached_resynthesis
{
public:
  explicit cached_resynthesis( ResynthesisFn& resyn_fn, uint32_t max_pis )
    : _resyn_fn( resyn_fn ),
      _cache( max_pis )
  {
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( Ntk& ntk, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn )
  {
    if ( _cache.has( function ) )
    {
      fn( cleanup_dangling( _cache.get_view( function ), ntk, begin, end ).front() );
    }
    else if ( _blacklist_cache.count( function ) )
    {
      return; /* do nothing */
    }
    else
    {
      bool found_one = false;
      auto on_signal = [&]( signal<Ntk> const& f ) {
        if ( !found_one )
        {
          _cache.insert( function, cut_view( ntk, std::vector<signal<Ntk>>( begin, end ), f ) );
          found_one = true;
        }
        fn( f );
      };

      _resyn_fn( ntk, function, begin, end, on_signal );

      if ( !found_one )
      {
        _blacklist_cache.insert( function );
      }
    }
  }

private:
  ResynthesisFn& _resyn_fn;
  network_cache<Ntk, kitty::dynamic_truth_table, kitty::hash<kitty::dynamic_truth_table>> _cache;
  std::unordered_set<kitty::dynamic_truth_table, kitty::hash<kitty::dynamic_truth_table>> _blacklist_cache;
};
} /* namespace mockturtle */
