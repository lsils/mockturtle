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
  \file collapse_mapped.hpp
  \brief Collapses mapped network into k-LUT network

  \author Heinz Riener
  \author Mathias Soeken
*/

#pragma once

#include <optional>
#include <unordered_map>

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

struct retiming_network_params
{
  uint32_t clock_period{ std::numeric_limits<uint32_t>::max() };
  uint32_t lut_delay{ 1u };
};

namespace detail
{

template<class Ntk>
class retiming_network_impl
{
public:
  using params_t = retiming_network_params;

public:
  retiming_network_impl( Ntk& ntk, params_t const& ps )
      : ntk( ntk ), ps( ps )
  {
  }

  void run()
  {
    node_map<uint32_t, Ntk> node_to_delay( ntk );

    topo_view topo{ ntk };
    topo.foreach_node( [&]( auto n ) {
      if ( ntk.is_constant( n ) || ntk.is_pi( n ) || ntk.is_ro( n ) )
      {
        node_to_delay[n] = 0;
        return; /* continue */
      }
      uint32_t max_fanin_delay = 0;
      ntk.foreach_fanin( n, [&]( auto fanin ) {
        uint32_t node_delay = node_to_delay[ntk.get_node( fanin )];
        max_fanin_delay = node_delay > max_fanin_delay? node_delay:max_fanin_delay;
      } );
      node_to_delay[n] = max_fanin_delay + ps.lut_delay;
      if ( node_to_delay[n] > ps.clock_period )
      {
        std::vector<signal<Ntk>> children;
        /* buffer all the fanins */
        ntk.foreach_fanin( n, [&]( auto fanin ) {
          ntk.create_ri( fanin );
          const auto f = ntk.create_ro();

          ntk.set_register( ntk.num_registers()-1, ntk.register_at( 0 ) ); // TODO: maybe different
          node_to_delay.resize();
          node_to_delay[ntk.get_node(f)] = 0;
          children.emplace_back( f );
        } );
        const auto new_node = ntk.create_node( children, ntk.node_function( n ) );
        ntk.substitute_node( n, ntk.make_signal( new_node ) );
        node_to_delay.resize();
        node_to_delay[new_node] = ps.lut_delay;
      }
    } );
  }

private:
  Ntk& ntk;
  params_t const& ps;
};

} /* namespace detail */

template<class Ntk>
void retiming_network( Ntk& ntk, retiming_network_params const& ps = {} )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  static_assert( has_is_ro_v<Ntk>, "Ntk does not implement the is_ro method" );
  detail::retiming_network_impl<Ntk> p( ntk, ps );
  p.run();
}
  
} /* namespace mockturtle */