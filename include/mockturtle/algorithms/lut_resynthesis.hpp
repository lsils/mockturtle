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
  \file lut_resynthesis.hpp
  \brief LUT resynthesis

  \author Mathias Soeken
*/

#pragma once

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

namespace detail
{

template<class NtkDest, class NtkSrc, class ResynthesisFn>
class lut_resynthesis_impl
{
public:
  lut_resynthesis_impl( NtkSrc const& ntk, ResynthesisFn&& resynthesis_fn )
      : ntk( ntk ),
        resynthesis_fn( resynthesis_fn )
  {
  }

  NtkDest run()
  {
    NtkDest ntk_dest;
    node_map<signal<NtkDest>, NtkSrc> node2new( ntk );

    /* map constants */
    node2new[ntk.get_node( ntk.get_constant( false ) )] = ntk_dest.get_constant( false );
    if ( ntk.get_node( ntk.get_constant( true ) ) != ntk.get_node( ntk.get_constant( false ) ) )
    {
      node2new[ntk.get_node( ntk.get_constant( true ) )] = ntk_dest.get_constant( true );
    }

    /* map primary inputs */
    ntk.foreach_pi( [&]( auto n ) {
      node2new[n] = ntk_dest.create_pi();
    } );

    /* map nodes */
    topo_view ntk_topo{ntk};
    ntk_topo.foreach_node( [&]( auto n ) {
      if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
        return;

      std::vector<signal<NtkDest>> children;
      ntk.foreach_fanin( n, [&]( auto const& f ) {
        children.push_back( ntk.is_complemented( f ) ? ntk_dest.create_not( node2new[f] ) : node2new[f] );
      } );

      node2new[n] = resynthesis_fn( ntk_dest, ntk.node_function( n ), children.begin(), children.end() );
    } );

    /* map primary outputs */
    ntk.foreach_po( [&]( auto const& f ) {
      ntk_dest.create_po( ntk.is_complemented( f ) ? ntk_dest.create_not( node2new[f] ) : node2new[f] );
    } );

    return ntk_dest;
  }

private:
  NtkSrc const& ntk;
  ResynthesisFn&& resynthesis_fn;
};

} /* namespace detail */

template<class NtkDest, class NtkSrc, class ResynthesisFn>
NtkDest lut_resynthesis( NtkSrc const& ntk, ResynthesisFn&& resynthesis_fn )
{
  detail::lut_resynthesis_impl<NtkDest, NtkSrc, ResynthesisFn> p( ntk, resynthesis_fn );
  return p.run();
}

} // namespace mockturtle
