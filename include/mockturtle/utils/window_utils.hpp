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
  \brief Utils to structurally collect node sets

  \author Heinz Riener
*/

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
 * \param inputs Inputs of a window
 * \param nodes Inner nodes of a window (i.e., the intersection of
 *              inputs and nodes is assumed to be empty)
 * \param refs Reference counters (in the size of the network and
 *             initialized to 0)
 * \return Output signals of the window
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

    assert( !ntk.is_constant( n ) && !ntk.is_pi( n ) );
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

    assert( !ntk.is_constant( n ) && !ntk.is_pi( n ) );
    ntk.foreach_fanin( n, [&]( signal const& fi ){
      refs[ntk.get_node( fi )] -= 1;
    });
  }

  return outputs;
}

} /* namespace mockturtle */
