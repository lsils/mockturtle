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
  \file recursive_cost_functions.hpp
  \brief Various recursive cost functions for (optimization) algorithms

  \author Hanyu Wang
*/

#pragma once

#include <cstdint>
#include <queue>

#include "../traits.hpp"

namespace mockturtle
{

/*! \brief (Recursive) customizable cost function
 *
 * To define a new cost function, you need to first specify how each node
 * contributes to the total cost via the *contribution function*. Each node
 * is evaluated individually and independently.
 *
 * If additional (global) information is required to decide a node's contribution,
 * you may specify them as *context*. The content stored in the context can be
 * arbitrarily defined (`context_t`), but the derivation must be recursive. In
 * other words, the context of a node is derived using *context propagation function*
 * which takes only the context of fanins as input.
 *
 * Examples of recursive cost functions can be found at:
 * `mockturtle/utils/recursive_cost_functions.hpp`
 */
template<class Ntk>
struct recursive_cost_functions
{
  using base_type = recursive_cost_functions;

  /*! \brief Context type
   *
   * The context type is used to store additional information for each node.
   * The content stored in the context can be arbitrarily defined (`context_t`),
   * but the derivation must be recursive. In other words, the context of a node
   * is derived using *context propagation function* which takes only the context
   * of fanins as input.
   * 
   * Optionally you may define a `context_compare` function to compare two contexts.
   * This is used to sort the nodes in the priority queue, and potentially to
   * prune the search space.
  */
  using context_t = uint32_t;

  /*! \brief Context propagation function
   *
   * Return the context of a node given fanin contexts. The fanin contexts are
   * sorted in the same order as the fanins of the node.
   */
  virtual context_t operator()( Ntk const& ntk, node<Ntk> const& n, std::vector<context_t> const& fanin_contexts = {} ) const = 0;

  /*! \brief Contribution function
   *
   * Update the total cost using node n and its context. The total cost is
   * updated only if the cost of the node is larger than the current total cost.
   * 
   * The context of a node is derived using *context propagation function* which
   * takes only the context of fanins as input.
   * 
   * The contribution function is called only if the node is not a primary input.
   */
  virtual void operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& total_cost, context_t const context ) const = 0;
};

template<class Ntk>
using cotext_signal_pair = std::pair<typename Ntk::context_t, signal<Ntk>>;

template<class Ntk>
struct context_signal_compare
{
  bool operator()( cotext_signal_pair<Ntk> const& p1, cotext_signal_pair<Ntk> const& p2 ) const
  {
    return Ntk::costfn_t::context_compare( p1.first, p2.first );
  }
};

template<class Ntk>
using cotext_signal_queue = std::priority_queue<cotext_signal_pair<Ntk>, std::vector<cotext_signal_pair<Ntk>>, context_signal_compare<Ntk>>;

template<class Ntk>
struct xag_depth_cost_function : recursive_cost_functions<Ntk>
{
public:
  using context_t = uint32_t;
  static bool context_compare( const context_t& c1, const context_t& c2 )
  {
    return c1 > c2;
  }

  context_t operator()( Ntk const& ntk, node<Ntk> const& n, std::vector<context_t> const& fanin_contexts = {} ) const
  {
    uint32_t _cost = ntk.is_pi( n ) ? 0 : *std::max_element( std::begin( fanin_contexts ), std::end( fanin_contexts ) ) + 1;
    return _cost;
  }
  void operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& total_cost, context_t const context ) const
  {
    total_cost = std::max( total_cost, context );
  }
};

template<class Ntk>
struct xag_t_depth_cost_function : recursive_cost_functions<Ntk>
{
public:
  using context_t = uint32_t;
  static bool context_compare( const context_t& c1, const context_t& c2 )
  {
    return c1 > c2;
  }

  context_t operator()( Ntk const& ntk, node<Ntk> const& n, std::vector<context_t> const& fanin_contexts = {} ) const
  {
    uint32_t _cost = ntk.is_pi( n ) ? 0 : *std::max_element( std::begin( fanin_contexts ), std::end( fanin_contexts ) ) + ntk.is_and( n );
    return _cost;
  }

  void operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& total_cost, context_t const context ) const
  {
    total_cost = std::max( total_cost, context );
  }
};

template<class Ntk>
struct xag_size_cost_function : recursive_cost_functions<Ntk>
{
public:
  using context_t = uint32_t;
  static bool context_compare( const context_t& c1, const context_t& c2 )
  {
    return true;
  }

  context_t operator()( Ntk const& ntk, node<Ntk> const& n, std::vector<uint32_t> const& fanin_contexts = {} ) const
  {
    return 0;
  }
  void operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& total_cost, context_t const context ) const
  {
    total_cost += ( !ntk.is_pi( n ) && ntk.visited( n ) != ntk.trav_id() ) ? 1 : 0;
  }
};

template<class Ntk>
struct xag_multiplicative_complexity_cost_function : recursive_cost_functions<Ntk>
{
public:
  using context_t = uint32_t;
  static bool context_compare( const context_t& c1, const context_t& c2 )
  {
    return true;
  }

  context_t operator()( Ntk const& ntk, node<Ntk> const& n, std::vector<uint32_t> const& fanin_contexts = {} ) const
  {
    return 0;
  }
  void operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& total_cost, context_t const context ) const
  {
    total_cost += ( !ntk.is_pi( n ) && ntk.visited( n ) != ntk.trav_id() ) ? ntk.is_and( n ) : 0;
  }
};

} /* namespace mockturtle */
