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
  \file window_utils.hpp
  \brief Utilities to collect small-scale sets of nodes

  \author Heinz Riener
*/

#include <algorithm>
#include <set>
#include <type_traits>
#include <vector>

namespace mockturtle
{

namespace detail
{

template<typename Ntk>
inline void collect_nodes_recur( Ntk const& ntk, typename Ntk::node const& n, std::vector<typename Ntk::node>& nodes )
{
  using signal = typename Ntk::signal;

  if ( ntk.visited( n ) == ntk.trav_id() )
  {
    return;
  }
  ntk.set_visited( n, ntk.trav_id() );

  ntk.foreach_fanin( n, [&]( signal const& fi ){
    collect_nodes_recur( ntk, ntk.get_node( fi ), nodes );
  });
  nodes.push_back( n );
}

} /* namespace detail */

/*! \brief Collect nodes in between of two node sets
  *
  * \param ntk A network
  * \param inputs A node set
  * \param outputs A signal set
  * \return Nodes enclosed by inputs and outputs
  *
  * The output set has to be chosen in a way such that every path from
  * PIs to outputs passes through at least one input.
  *
  * **Required network functions:**
  * - `foreach_fanin`
  * - `get_node`
  * - `incr_trav_id`
  * - `set_visited`
  * - `trav_id`
  * - `visited`
  */
template<typename Ntk, typename = std::enable_if_t<!std::is_same_v<typename Ntk::signal, typename Ntk::node>>>
inline std::vector<typename Ntk::node> collect_nodes( Ntk const& ntk,
                                                      std::vector<typename Ntk::node> const& inputs,
                                                      std::vector<typename Ntk::signal> const& outputs )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  /* convert output signals to nodes */
  std::vector<node> _outputs;
  std::transform( std::begin( outputs ), std::end( outputs ), std::back_inserter( _outputs ),
                  [&ntk]( signal const& s ){
                    return ntk.get_node( s );
                  });
  return collect_nodes( ntk, inputs, _outputs );
}

/*! \brief Collect nodes in between of two node sets
  *
  * \param ntk A network
  * \param inputs A node set
  * \param outputs A node set
  * \return Nodes enclosed by inputs and outputs
  *
  * The output set has to be chosen in a way such that every path from
  * PIs to outputs passes through at least one input.
  *
  * **Required network functions:**
  * - `foreach_fanin`
  * - `get_node`
  * - `incr_trav_id`
  * - `set_visited`
  * - `trav_id`
  * - `visited`
  */
template<typename Ntk>
inline std::vector<typename Ntk::node> collect_nodes( Ntk const& ntk,
                                                      std::vector<typename Ntk::node> const& inputs,
                                                      std::vector<typename Ntk::node> const& outputs )
{
  using node = typename Ntk::node;

  /* create a new traversal ID */
  ntk.incr_trav_id();

  /* mark inputs visited */
  for ( auto const& i : inputs )
  {
    if ( ntk.visited( i ) == ntk.trav_id() )
    {
      continue;
    }
    ntk.set_visited( i, ntk.trav_id() );
  }

  /* recursively collect all nodes in between inputs and outputs */
  std::vector<node> nodes;
  for ( auto const& o : outputs )
  {
    detail::collect_nodes_recur( ntk, o, nodes );
  }
  return nodes;
}

/*! \brief Identify outputs using reference counting
 *
 * Identify outputs using a reference counting approach.  The
 * algorithm counts the references of the fanins of all nodes and
 * compares them with the fanout_sizes of the respective nodes.  If
 * reference count and fanout_size do not match, then the node is
 * references outside of the node set and the respective is identified
 * as an output.
 *
 * \param ntk A network
 * \param inputs Inputs of a window
 * \param nodes Inner nodes of a window (i.e., the intersection of
 *              inputs and nodes is assumed to be empty)
 * \param refs Reference counters (in the size of the network and
 *             initialized to 0)
 * \return Output signals of the window
  *
  * **Required network functions:**
  * - `fanout_size`
  * - `foreach_fanin`
  * - `get_node`
  * - `incr_trav_id`
  * - `is_ci`
  * - `is_constant`
  * - `make_signal`
  * - `set_visited`
  * - `trav_id`
  * - `visited`
 */
template<typename Ntk>
inline std::vector<typename Ntk::signal> find_outputs( Ntk const& ntk,
                                                       std::vector<typename Ntk::node> const& inputs,
                                                       std::vector<typename Ntk::node> const& nodes,
                                                       std::vector<uint32_t>& refs )
{
  using signal = typename Ntk::signal;
  std::vector<signal> outputs;

  /* create a new traversal ID */
  ntk.incr_trav_id();

  /* mark the inputs visited */
  for ( auto const& i : inputs )
  {
    ntk.set_visited( i, ntk.trav_id() );
  }

  /* reference fanins of nodes */
  for ( auto const& n : nodes )
  {
    if ( ntk.visited( n ) == ntk.trav_id() )
    {
      continue;
    }

    assert( !ntk.is_constant( n ) && !ntk.is_ci( n ) );
    ntk.foreach_fanin( n, [&]( signal const& fi ){
      refs[ntk.get_node( fi )] += 1;
    });
  }

  /* if the fanout_size of a node does not match the reference count,
     the node has fanout outside of the window is an output */
  for ( const auto& n : nodes )
  {
    if ( ntk.visited( n ) == ntk.trav_id() )
    {
      continue;
    }

    if ( ntk.fanout_size( n ) != refs[n] )
    {
      outputs.emplace_back( ntk.make_signal( n ) );
    }
  }

  /* dereference fanins of nodes */
  for ( auto const& n : nodes )
  {
    if ( ntk.visited( n ) == ntk.trav_id() )
    {
      continue;
    }

    assert( !ntk.is_constant( n ) && !ntk.is_ci( n ) );
    ntk.foreach_fanin( n, [&]( signal const& fi ){
      refs[ntk.get_node( fi )] -= 1;
    });
  }

  return outputs;
}

/*! \brief Expand a nodes towards TFO
 *
 * Iteratively expands the inner nodes of the window with those
 * fanouts that are supported by the window until a fixed-point is
 * reached.
 *
 * \param ntk A network
 * \param inputs Input nodes of a window
 * \param nodes Inner nodes of a window
 *
 * **Required network functions:**
 * - `foreach_fanin`
 * - `foreach_fanout`
 * - `get_node`
 * - `incr_trav_id`
 * - `is_ci`
 * - `set_visited`
 * - `trav_id`
 * - `visited`
 */
template<typename Ntk>
void expand_towards_tfo( Ntk const& ntk, std::vector<typename Ntk::node> const& inputs, std::vector<typename Ntk::node>& nodes )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  auto explore_fanouts = [&]( Ntk const& ntk, node const& n, std::set<node>& result ){
    ntk.foreach_fanout( n, [&]( node const& fo, uint64_t index ){
      /* only look at the first few fanouts */
      if ( index > 5 )
      {
        return false;
      }
      /* skip all nodes that are already in nodex */
      if ( ntk.visited( fo ) == ntk.trav_id() )
      {
        return true;
      }
      result.insert( fo );
      return true;
    });
  };

  /* create a new traversal ID */
  ntk.incr_trav_id();

  /* mark the inputs visited */
  for ( auto const& i : inputs )
  {
    ntk.set_visited( i, ntk.trav_id() );
  }
  /* mark the nodes visited */
  for ( const auto& n : nodes )
  {
    ntk.set_visited( n, ntk.trav_id() );
  }

  /* collect all nodes that have fanouts not yet contained in nodes */
  std::set<node> eps;
  for ( const auto& n : inputs )
  {
    explore_fanouts( ntk, n, eps );
  }
  for ( const auto& n : nodes )
  {
    explore_fanouts( ntk, n, eps );
  }

  bool changed = true;
  std::set<node> new_eps;
  while ( changed )
  {
    new_eps.clear();
    changed = false;

    auto it = std::begin( eps );
    while ( it != std::end( eps ) )
    {
      node const ep = *it;
      if ( ntk.visited( ep ) == ntk.trav_id() )
      {
        it = eps.erase( it );
        continue;
      }

      bool all_children_belong_to_window = true;
      ntk.foreach_fanin( ep, [&]( signal const& fi ){
        node const child = ntk.get_node( fi );
        if ( ntk.visited( child ) != ntk.trav_id() )
        {
          all_children_belong_to_window = false;
          return false;
        }
        return true;
      });

      if ( all_children_belong_to_window )
      {
        assert( ep != 0 );
        assert( !ntk.is_ci( ep ) );
        nodes.emplace_back( ep );
        ntk.set_visited( ep, ntk.trav_id() );
        it = eps.erase( it );

        explore_fanouts( ntk, ep, new_eps );
      }

      if ( it != std::end( eps ) )
      {
        ++it;
      }
    }

    if ( !new_eps.empty() )
    {
      eps.insert( std::begin( new_eps ), std::end( new_eps ) );
      changed = true;
    }
  }
}

/*! \brief Performs zero-cost expansion of a set of nodes towards TFI
 *
 * \param ntk A network
 * \param inputs Input nodes
 * \param colors Auxiliar data structure with `ntk.size()` elements
 * \param color A value used to mark inputs
 * \return True if and only if the inputs form a trivial cut that
 *         cannot be further extended, e.g., when the cut only
 *         consists of PIs.
 *
 * On termination, `colors[i] == color` if `i \in inputs`.  However,
 *   previous inputs are also marked.
 */
template<typename Ntk>
bool expand0_towards_tfi( Ntk const& ntk, std::vector<typename Ntk::node>& inputs, std::vector<uint32_t>& colors, uint32_t color )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  assert( ntk.size() == colors.size() );

  /* mark all inputs */
  for ( auto const& i : inputs )
  {
    colors[i] = color;
  }

  /* we call a set of inputs (= a cut) trivial if all nodes are either
     constants or CIs, such that they cannot be further expanded towards
     the TFI */
  bool trivial_cut = false;

  /* repeat expansion towards TFI until a fix-point is reached */
  bool changed = true;
  while ( changed )
  {
    changed = false;
    trivial_cut = true;

    for ( auto it = std::begin( inputs ); it != std::end( inputs ); ++it )
    {
      /* count how many fanins are not in the cut */
      uint32_t count_fanin_outside{0};
      std::optional<node> ep;

      ntk.foreach_fanin( *it, [&]( signal const& fi ){
        node const n = ntk.get_node( fi );
        trivial_cut = false;

        if ( colors[n] != color )
        {
          ++count_fanin_outside;
          ep = n;
        }
      });

      /* if only one fanin is not in the cut, then the input expansion
         can be done wihout affecting the cut size */
      if ( count_fanin_outside == 1u )
      {
        /* expand the cut */
        assert( ep );
        it = inputs.erase( it );
        inputs.push_back( *ep );
        changed = true;
      }
    }
  }

  return trivial_cut;
}

} /* namespace mockturtle */
