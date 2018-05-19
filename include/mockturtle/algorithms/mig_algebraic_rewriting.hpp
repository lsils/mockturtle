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
  \file mig_algebraic_rewriting.hpp
  \brief MIG algebraric rewriting

  \author Mathias Soeken
*/

#pragma once

#include <iostream>
#include <optional>

#include "../views/topo_view.hpp"

namespace mockturtle
{

namespace detail
{

template<class Ntk>
class mig_algebraic_dfs_depth_rewriting_impl
{
public:
  mig_algebraic_dfs_depth_rewriting_impl( Ntk& ntk )
      : ntk( ntk )
  {
  }

  void run()
  {
    ntk.foreach_po( [this]( auto po ) {
      const auto driver = ntk.get_node( po );
      if ( ntk.level( driver ) < ntk.depth() )
        return;
      topo_view topo{ntk, driver};
      topo.foreach_node( [this]( auto n ) {
        if ( !ntk.is_maj( n ) )
          return true;

        if ( ntk.level( n ) == 0 )
          return true;

        /* get children of top node, ordered by node level (ascending) */
        const auto ocs = ordered_children( n );

        if ( !ntk.is_maj( ntk.get_node( ocs[2] ) ) )
          return true;

        /* depth of last child must be (significantly) higher than depth of second child */
        if ( ntk.level( ntk.get_node( ocs[2] ) ) <= ntk.level( ntk.get_node( ocs[1] ) ) + 1 )
          return true;

        /* get children of last child */
        auto ocs2 = ordered_children( ntk.get_node( ocs[2] ) );

        /* depth of last grand-child must be higher than depth of second grand-child */
        if ( ntk.level( ntk.get_node( ocs2[2] ) ) == ntk.level( ntk.get_node( ocs2[1] ) ) )
          return true;

        /* propagate inverter if necessary */
        if ( ntk.is_complemented( ocs[2] ) )
        {
          ocs2[0] = !ocs2[0];
          ocs2[1] = !ocs2[1];
          ocs2[2] = !ocs2[2];
        }

        if ( auto cand = associativity_candidate( ocs[0], ocs[1], ocs2[0], ocs2[1], ocs2[2] ); cand )
        {
          const auto& [x, y, z, u, assoc] = *cand;
          auto opt = ntk.create_maj( z, assoc ? u : x, ntk.create_maj( x, y, u ) );
          ntk.substitute_node( n, opt );
          ntk.update();

          return true;
        }

        /* distributivity */
        auto opt = ntk.create_maj( ocs2[2],
                                   ntk.create_maj( ocs[0], ocs[1], ocs2[0] ),
                                   ntk.create_maj( ocs[0], ocs[1], ocs2[1] ) );
        ntk.substitute_node( n, opt );
        ntk.update();
        return true;
      } );
    } );
  }

private:
  using candidate_t = std::tuple<signal<Ntk>, signal<Ntk>, signal<Ntk>, signal<Ntk>, bool>;
  std::optional<candidate_t> associativity_candidate( signal<Ntk> const& v, signal<Ntk> const& w, signal<Ntk> const& x, signal<Ntk> const& y, signal<Ntk> const& z ) const
  {
    if ( v.index == x.index )
    {
      return candidate_t{w, y, z, v, v.complement == x.complement};
    }
    if ( v.index == y.index )
    {
      return candidate_t{w, x, z, v, v.complement == y.complement};
    }
    if ( w.index == x.index )
    {
      return candidate_t{v, y, z, w, w.complement == x.complement};
    }
    if ( w.index == y.index )
    {
      return candidate_t{v, x, z, w, w.complement == y.complement};
    }

    return std::nullopt;
  }

  std::array<signal<Ntk>, 3> ordered_children( node<Ntk> const& n ) const
  {
    std::array<signal<Ntk>, 3> children;
    ntk.foreach_fanin( n, [&children]( auto const& f, auto i ) { children[i] = f; } );
    std::sort( children.begin(), children.end(), [this]( auto const& c1, auto const& c2 ) {
      return ntk.level( ntk.get_node( c1 ) ) < ntk.level( ntk.get_node( c2 ) );
    } );
    return children;
  }

private:
  Ntk& ntk;
};

} // namespace detail

/*! \brief Majority algebraic rewriting (DFS depth optimization).
 *
 * This algorithm tries to rewrite a network with majority gates for depth
 * optimization using the associativity and distributivity rule in
 * majority-of-3 logic.  It can be applied to networks other than MIGs, but
 * only considers pairs of nodes which both implement the majority-of-3
 * function.
 *
 * **Required network functions:**
 * - `get_node`
 * - `level`
 * - `create_maj`
 * - `substitute_node`
 * - `update`
 * - `foreach_node`
 * - `foreach_fanin`
 * - `is_maj`
 *
   \verbatim embed:rst

  .. note::

      The implementation of this algorithm was heavily inspired by an
      implementation from Luca Amarù.
   \endverbatim
 */
template<class Ntk>
void mig_algebraic_dfs_depth_rewriting( Ntk& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_level_v<Ntk>, "Ntk does not implement the level method" );
  static_assert( has_create_maj_v<Ntk>, "Ntk does not implement the create_maj method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the substitute_node method" );
  static_assert( has_update_v<Ntk>, "Ntk does not implement the update method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_is_maj_v<Ntk>, "Ntk does not implement the is_maj method" );

  detail::mig_algebraic_dfs_depth_rewriting_impl<Ntk> p( ntk );
  p.run();
}

} /* namespace mockturtle */
