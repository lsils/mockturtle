/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
  \file cost_resyn.hpp
  \brief Solver of cost-aware resynthesis problem.
         Given a resynthesis problem and the cost function, returns
         the solution with (1) correct functionality (2) lower cost.

         This solver is cost-generic.

  \author Hanyu Wang
*/

#pragma once

#include "../../algorithms/cleanup.hpp"
#include "../../utils/index_list.hpp"
#include "resub_functors.hpp"

#include <algorithm>
#include <optional>
#include <queue>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace mockturtle::experimental
{

struct cost_resyn_params
{
  /* maximum number of feasible solutions to collect */
  uint32_t max_solutions{ 1000u };
};

struct cost_resyn_stats
{
  /* time for cost view the solution network */
  stopwatch<>::duration time_eval{ 0 };

  /* time for searching the equivalent network */
  stopwatch<>::duration time_search{ 0 };

  /* number of solutions */
  uint32_t num_solutions{ 0 };

  /* number of problems */
  uint32_t num_problems{ 0 };

  /* number of solution with 0,1,2,3 insertions */
  uint32_t num_resub[4]{ 0 };

  /* size of the forest of feasible solutions */
  uint32_t size_forest{ 0 };

  /* number of root (feasible solutions) */
  uint32_t num_roots{ 0 };

  /* number of total gains */
  uint32_t num_gain{ 0 };

  /* data */
  void report() const
  {
    fmt::print( "[i]         <cost_resyn>\n" );
    fmt::print( "[i]             Evalutation      : {:>5.2f} secs\n", to_seconds( time_eval ) );
    fmt::print( "[i]             Searching        : {:>5.2f} secs\n", to_seconds( time_search ) );
    fmt::print( "[i]             # Problem        : {}\n", num_problems );
    fmt::print( "[i]             Avg. forest size : {:>5.2f}\n", (float)size_forest / num_problems );
    fmt::print( "[i]             Avg. num solution: {:>5.2f}\n", (float)num_roots / num_problems );
    fmt::print( "[i]             Opt. ratio       : {:>5.2f}%\n", (float)num_solutions / num_problems * 100 );
    fmt::print( "[i]             0 - resub        : {:>5.2f}\n", (float)num_resub[0] / num_problems );
    fmt::print( "[i]             1 - resub        : {:>5.2f}\n", (float)num_resub[1] / num_problems );
    fmt::print( "[i]             2 - resub        : {:>5.2f}\n", (float)num_resub[2] / num_problems );
    fmt::print( "[i]             3 - resub        : {:>5.2f}\n", (float)num_resub[3] / num_problems );
    fmt::print( "[i]             Gain             : {:>5.2f}\n", (float)num_gain / num_problems );
  }
};

template<class Ntk, class TT>
class cost_resyn
{
public:
  using params = cost_resyn_params;
  using stats = cost_resyn_stats;
  using signal = typename Ntk::signal;
  using node = typename Ntk::node;
  using context_t = typename Ntk::context_t;
  using index_list_t = large_xag_index_list;

public:
  explicit cost_resyn( Ntk const& ntk, params const& ps, stats& st ) noexcept
      : ntk( ntk ), ps( ps ), st( st )
  {
  }

  template<class iterator_type, class truth_table_storage_type>
  std::optional<index_list_t> operator()( TT const& target, TT const& care, std::vector<signal> const& divs, iterator_type begin, iterator_type end, truth_table_storage_type const& tts, uint32_t max_cost = std::numeric_limits<uint32_t>::max() )
  {
    uint32_t best_cost = max_cost;
    std::optional<signal> best_candidate;

    // prepare virtual network
    Ntk forest;
    std::for_each( std::begin(divs), std::end(divs), [&]( signal const& div ) {
      forest.set_context( forest.get_node( forest.create_pi() ), ntk.get_context( ntk.get_node( div ) ) ); } );
    std::vector<signal> leaves;
    forest.foreach_pi( [&]( node n ) { leaves.emplace_back( ntk.make_signal( n ) ); } );

    /* grow the forest */
    resub_functor<Ntk, TT, decltype(begin), decltype(tts)> engine( forest, target, care, begin, end, tts );
    engine.run( [&]( signal g ) {
      forest.incr_trav_id();
      uint32_t curr_cost = forest.get_cost( forest.get_node( g ), leaves );
      if ( curr_cost < best_cost )
      {
        best_cost = curr_cost;
        best_candidate = g;
        return true; /* stop searching */
      }
      return false; /* keep searching */
    } );

    if ( best_candidate )
    {
      index_list_t index_list;
      forest.create_po( *best_candidate );
      typename Ntk::base_type solution = cleanup_dangling( (typename Ntk::base_type)forest );
      encode( index_list, solution );
      return index_list;
    }
    return std::nullopt;
  }

private:
  Ntk const& ntk;
  params const& ps;
  stats& st;
};

} // namespace mockturtle::experimental