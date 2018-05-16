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
      if ( auto cand = associativity_candidate( n ); cand )
      {
        const auto& [x, y, z, u] = *cand;
        auto opt = ntk.create_maj( z, u, ntk.create_maj( x, y, u ) );
        ntk.substitute_node( n, opt );
        ntk.update();
      }
    } );
  }

private:
  std::optional<std::array<signal<Ntk>, 4>> associativity_candidate( node<Ntk> const& n ) const
  {
    if ( ntk.level( n ) == 0 )
      return std::nullopt;

    /* get children of top node, ordered by node level (ascending) */
    const auto ocs = ordered_children( n );

    /* depth of last child must be (significantly) higher than depth of second child */
    if ( ntk.level( ntk.get_node( ocs[2] ) ) <= ntk.level( ntk.get_node( ocs[1] ) ) + 1 )
      return std::nullopt;

    /* get children of last child and propagate inverter if necessary */
    auto ocs2 = ordered_children( ntk.get_node( ocs[2] ) );
    if ( ntk.is_complemented( ocs[2] ) )
    {
      ocs2[0] = !ocs2[0];
      ocs2[1] = !ocs2[1];
      ocs2[2] = !ocs2[2];
    }

    /* depth of last grand-child must be higher than depth of second grand-child */
    if ( ntk.level( ntk.get_node( ocs2[2] ) ) == ntk.level( ntk.get_node( ocs2[1] ) ) )
      return std::nullopt;

    if ( ocs[0].index == ocs2[0].index )
    {
      if ( ocs[0].complement == ocs2[0].complement )
      {
        return std::array<signal<Ntk>, 4>{ocs[1], ocs2[1], ocs2[2], ocs[0]};
      }
      else
      {
        std::cout << "NOT GOOD\n";
      }
    }
    if ( ocs[0].index == ocs2[1].index )
    {
      if ( ocs[0].complement == ocs2[1].complement )
      {
        return std::array<signal<Ntk>, 4>{ocs[1], ocs2[0], ocs2[2], ocs[0]};
      }
      else
      {
        std::cout << "NOT GOOD\n";
      }
    }
    if ( ocs[1].index == ocs2[0].index )
    {
      if ( ocs[1].complement == ocs2[0].complement )
      {
        return std::array<signal<Ntk>, 4>{ocs[0], ocs2[1], ocs2[2], ocs[1]};
      }
      else
      {
        std::cout << "NOT GOOD\n";
      }
    }
    if ( ocs[1].index == ocs2[1].index )
    {
      if ( ocs[1].complement == ocs2[1].complement )
      {
        return std::array<signal<Ntk>, 4>{ocs[0], ocs2[0], ocs2[2], ocs[1]};
      }
      else
      {
        std::cout << "NOT GOOD\n";
      }
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
