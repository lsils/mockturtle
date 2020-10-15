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

/*! \brief Identify inputs using reference counting
 */
template<typename Ntk>
std::vector<typename Ntk::node> collect_inputs( Ntk const& ntk, std::vector<typename Ntk::node> const& nodes )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  
  /* mark all nodes with a new color */
  for ( const auto& n : nodes )
  {
    ntk.paint( n );
  }

  /* if a fanin is not colored, then it's an input */
  std::vector<node> inputs;
  for ( const auto& n : nodes )
  {
    ntk.foreach_fanin( n, [&]( signal const& fi ){
      node const n = ntk.get_node( fi );
      if ( !ntk.eval_color( n, [&ntk]( auto c ){ return c == ntk.current_color(); } ) )
      {
        if ( std::find( std::begin( inputs ), std::end( inputs ), n ) == std::end( inputs ) )
        {
          inputs.push_back( n );
        }
      }
    });
  }

  return inputs;
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
inline std::vector<typename Ntk::signal> collect_outputs( Ntk const& ntk,
                                                          std::vector<typename Ntk::node> const& inputs,
                                                          std::vector<typename Ntk::node> const& nodes,
                                                          std::vector<uint32_t>& refs )
{
  using signal = typename Ntk::signal;

  std::vector<signal> outputs;

  /* mark the inputs visited */
  for ( auto const& i : inputs )
  {
    ntk.paint( i );
  }

  /* reference fanins of nodes */
  for ( auto const& n : nodes )
  {
    if ( ntk.eval_color( n, [&ntk]( auto c ){ return c == ntk.current_color(); } ) )
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
    if ( ntk.eval_color( n, [&ntk]( auto c ){ return c == ntk.current_color(); } ) )
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
    if ( ntk.eval_color( n, [&ntk]( auto c ){ return c == ntk.current_color(); } ) )
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

/*! \brief Performs in-place zero-cost expansion of a set of nodes towards TFI
 *
 * The algorithm potentially derives a different cut of the same size
 * that is closer to the network's PIs.  This expansion towards TFI is
 * called zero-cost because it merges nodes only if the number of
 * inputs does not increase.
 *
 * \param ntk A network
 * \param inputs Input nodes
 * \return True if and only if the inputs form a trivial cut that
 *         cannot be further extended, e.g., when the cut only
 *         consists of PIs.
 *
 * On termination, `colors[i] == color` if `i \in inputs`.  However,
 *   previous inputs are also marked.
 *
 * **Required network functions:**
 * - `size`
 * - `foreach_fanin`
 * - `get_node`
 */
template<typename Ntk>
bool expand0_towards_tfi( Ntk const& ntk, std::vector<typename Ntk::node>& inputs )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  /* mark all inputs */
  for ( auto const& i : inputs )
  {
    ntk.paint( i );
  }

  /* we call a set of inputs (= a cut) trivial if all nodes are either
     constants or CIs, such that they cannot be further expanded towards
     the TFI */
  bool trivial_cut = false;

  /* repeat expansion towards TFI until a fix-point is reached */
  bool changed = true;
  std::vector<node> new_inputs;
  while ( changed )
  {
    changed = false;
    trivial_cut = true;

    for ( auto it = std::begin( inputs ); it != std::end( inputs ); )
    {
      /* count how many fanins are not in the cut */
      uint32_t count_fanin_outside{0};
      std::optional<node> ep;

      ntk.foreach_fanin( *it, [&]( signal const& fi ){
        node const n = ntk.get_node( fi );
        trivial_cut = false;

        if ( ntk.eval_color( n, [&ntk]( auto c ){ return c != ntk.current_color(); } ) )
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
        new_inputs.push_back( *ep );
        changed = true;
      }
      else
      {
        ++it;
      }
    }

    std::copy( std::begin( new_inputs ), std::end( new_inputs ),
               std::back_inserter( inputs ) );
    new_inputs.clear();
  }

  return trivial_cut;
}

namespace detail
{

template<typename Ntk>
inline void evaluate_fanin( typename Ntk::node const& n, std::vector<std::pair<typename Ntk::node, uint32_t>>& candidates )
{
  auto it = std::find_if( std::begin( candidates ), std::end( candidates ),
                          [&n]( auto const& p ){
                            return p.first == n;
                          } );
  if ( it == std::end( candidates ) )
  {
    /* new fanin: referenced for the 1st time */
    candidates.push_back( std::make_pair( n, 1u ) );
  }
  else
  {
    /* otherwise, if not new, then just increase the reference counter */
    ++it->second;
  }
}

template<typename Ntk>
inline typename Ntk::node select_next_fanin_to_expand_tfi( Ntk const& ntk, std::vector<typename Ntk::node> const& inputs )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  assert( inputs.size() > 0u && "inputs must not be empty" );
  // assert( cut_is_not_trivial( inputs ) );

  /* evaluate the fanins with respect to their costs (how often are they referenced) */
  std::vector<std::pair<node, uint32_t>> candidates;
  for ( auto const& i : inputs )
  {
    if ( ntk.is_constant( i ) || ntk.is_ci( i ) )
    {
      continue;
    }
    ntk.foreach_fanin( i, [&]( signal const& fi ){
      detail::evaluate_fanin<Ntk>( ntk.get_node( fi ), candidates );
    });
  }

  /* select the fanin with maximum reference count; if two fanins have equal reference count, select the one with more fanouts */
  std::pair<node, uint32_t> best_fanin;
  for ( auto const& candidate : candidates )
  {
    if ( candidate.second > best_fanin.second ||
         ( candidate.second == best_fanin.second && ntk.fanout_size( candidate.first ) > ntk.fanout_size( best_fanin.first ) ) )
    {
      best_fanin = candidate;
    }
  }

  /* as long as the inputs do not form a trivial cut, this procedure will always find a fanin to expand */
  assert( best_fanin.first != 0 );

  return best_fanin.first;
}

} /* namespace detail */

/*! \brief Performs in-place expansion of a set of nodes towards TFI
 *
 * Expand the inputs towards TFI by iteratively selecting the fanins
 * with the highest reference count within the cut and highest number
 * of fanouts.  Expansion continues until either `inputs` forms a
 * trivial cut or the `inputs`'s size reaches `input_limit`.  The
 * procedure allows a temporary increase of `inputs` beyond the
 * `input_limit` for at most `MAX_ITERATIONS`.
 *
 * \param ntk A network
 * \param inputs Input nodes
 * \param input_limit Size limit for the maximum number of input nodes
 */
template<typename Ntk>
void expand_towards_tfi( Ntk const& ntk, std::vector<typename Ntk::node>& inputs, uint32_t input_limit )
{
  using node = typename Ntk::node;

  static constexpr uint32_t const MAX_ITERATIONS{5u};

  if ( expand0_towards_tfi( ntk, inputs ) )
  {
    return;
  }

  std::vector<node> best_cut{inputs};
  bool trivial_cut = false;
  uint32_t iterations{0};
  while ( !trivial_cut && ( inputs.size() <= input_limit || iterations <= MAX_ITERATIONS ) )
  {
    node const n = detail::select_next_fanin_to_expand_tfi( ntk, inputs );
    inputs.push_back( n );
    ntk.paint( n );

    trivial_cut = expand0_towards_tfi( ntk, inputs );
    if ( inputs.size() <= input_limit )
    {
      best_cut = inputs;
    }

    iterations = inputs.size() > input_limit ? iterations + 1 : 0;
  }

  inputs = best_cut;
}

/*! \brief Performs in-place expansion of a set of nodes towards TFO
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

/*! \brief Performs in-place expansion of a set of nodes towards TFO
 *
 * Iteratively expands the inner nodes of the window with those
 * fanouts that are supported by the window.  Explores the fanouts
 * level by level.  Starting with those that are closest to the
 * inputs.
 *
 * \param ntk A network
 * \param inputs Input nodes of a window
 * \param nodes Inner nodes of a window
 *
 * **Required network functions:**
 * - `foreach_fanin`
 * - `foreach_fanout`
 * - `get_node`
 * - `level`
 * - `is_constant`
 * - `is_ci`
 * - `depth`
 * - `paint`
 * - `eval_color`
 * - `current_color`
 * - `eval_fanins_color`
 */
template<typename Ntk>
void levelized_expand_towards_tfo( Ntk const& ntk, std::vector<typename Ntk::node> const& inputs, std::vector<typename Ntk::node>& nodes )
{
  using node = typename Ntk::node;

  static constexpr uint32_t const MAX_FANOUTS{5u};

  /* mapping from level to nodes (which nodes are on a certain level?) */
  std::vector<std::vector<node>> levels( ntk.depth() );

  /* list of indicves of used levels (avoid iterating over all levels) */
  std::vector<uint32_t> used;

  /* remove all nodes */
  nodes.clear();

  /* mark all inputs and fill their level information into `levels` and `used` */
  for ( const auto& i : inputs )
  {
    uint32_t const node_level = ntk.level( i );
    ntk.paint( i );
    levels[node_level].push_back( i );
    if ( std::find( std::begin( used ), std::end( used ), node_level ) == std::end( used ) )
    {
      used.push_back( node_level );
    }
  }

  for ( uint32_t const& index : used )
  {
    std::vector<node>& level = levels.at( index );
    for ( auto it = std::begin( level ); it != std::end( level ); ++it )
    {
      ntk.foreach_fanout( *it, [&]( node const& fo, uint64_t index ){
        /* avoid getting stuck on nodes with many fanouts */
        if ( index == MAX_FANOUTS )
        {
          return false;
        }

        /* ignore nodes wihout fanins */
        if ( ntk.is_constant( fo ) || ntk.is_ci( fo ) )
        {
          return true;
        }

        if (  ntk.eval_color( fo, [&ntk]( auto c ){ return c != ntk.current_color(); } ) &&
              ntk.eval_fanins_color( fo, [&ntk]( auto c ){ return c == ntk.current_color(); } ) )
        {
          /* add fanout to nodes */
          nodes.push_back( fo );

          /* update data structured */
          uint32_t const node_level = ntk.level( fo );
          ntk.paint( fo );
          levels.at( node_level ).push_back( fo );
          if ( std::find( std::begin( used ), std::end( used ), node_level ) == std::end( used ) )
          {
            used.push_back( node_level );
          }
        }

        return true;
      });
    }
    level.clear();
  }
}

} /* namespace mockturtle */
