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

struct mig_algebraric_rewriting_params
{
};

namespace detail
{

template<class Ntk>
class mig_algebraic_rewriting_impl
{
public:
  mig_algebraic_rewriting_impl( Ntk& ntk, mig_algebraric_rewriting_params const& ps )
      : ntk( ntk ),
        ps( ps )
  {
  }

  void run()
  {
    topo_view topo{ntk};
    topo.foreach_node( [this]( auto n ) {
      if ( ntk.level( n ) == 0 )
        return true;

      /* get children of top node, ordered by node level (ascending) */
      const auto ocs = ordered_children( n );

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
  mig_algebraric_rewriting_params const& ps;
};

} // namespace detail

template<class Ntk>
void mig_algebraic_rewriting( Ntk& ntk, mig_algebraric_rewriting_params const& ps = {} )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  detail::mig_algebraic_rewriting_impl<Ntk> p( ntk, ps );
  p.run();
}

} /* namespace mockturtle */
