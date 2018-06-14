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
  \file node_resynthesis.hpp
  \brief Node resynthesis

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <vector>

#include "../traits.hpp"
#include "../utils/node_map.hpp"

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/static_truth_table.hpp>

namespace mockturtle
{

template<class SimulationType>
class default_simulator
{
public:
  default_simulator() = delete;
};

template<>
class default_simulator<bool>
{
public:
  default_simulator() = delete;
  default_simulator( std::vector<bool> const& assignments ) : assignments( assignments ) {}

  bool compute_constant( bool value ) const { return value; }
  bool compute_pi( uint32_t index ) const { return assignments[index]; }
  bool compute_not( bool value ) const { return !value; }

private:
  std::vector<bool> assignments;
};

template<>
class default_simulator<kitty::dynamic_truth_table>
{
public:
  default_simulator() = delete;
  default_simulator( unsigned num_vars ) : num_vars( num_vars ) {}

  kitty::dynamic_truth_table compute_constant( bool value ) const
  {
    kitty::dynamic_truth_table tt( num_vars );
    return value ? ~tt : tt;
  }

  kitty::dynamic_truth_table compute_pi( uint32_t index ) const
  {
    kitty::dynamic_truth_table tt( num_vars );
    kitty::create_nth_var( tt, index );
    return tt;
  }

  kitty::dynamic_truth_table compute_not( kitty::dynamic_truth_table const& value ) const
  {
    return ~value;
  }

private:
  unsigned num_vars;
};

template<int NumVars>
class default_simulator<kitty::static_truth_table<NumVars>>
{
public:
  kitty::static_truth_table<NumVars> compute_constant( bool value ) const
  {
    kitty::static_truth_table<NumVars> tt;
    return value ? ~tt : tt;
  }

  kitty::static_truth_table<NumVars> compute_pi( uint32_t index ) const
  {
    kitty::static_truth_table<NumVars> tt;
    kitty::create_nth_var( tt, index );
    return tt;
  }

  kitty::static_truth_table<NumVars> compute_not( kitty::static_truth_table<NumVars> const& value ) const
  {
    return ~value;
  }
};

template<class SimulationType, class Ntk, class Simulator = default_simulator<SimulationType>>
std::vector<SimulationType> simulate( Ntk const& ntk, Simulator const& sim = Simulator() )
{
  // TODO traits
  node_map<SimulationType, Ntk> node_to_value( ntk );

  // TODO get constant value, assume false
  node_to_value[ntk.get_constant( false )] = sim.compute_constant( false );
  ntk.foreach_pi( [&]( auto const& n, auto i ) {
    node_to_value[n] = sim.compute_pi( i );
  } );

  ntk.foreach_gate( [&]( auto const& n ) {
    std::vector<SimulationType> fanin_values( ntk.fanin_size( n ) );
    ntk.foreach_fanin( n, [&]( auto const& f, auto i ) {
      fanin_values[i] = node_to_value[f];
    } );
    node_to_value[n] = ntk.template compute( n, fanin_values.begin(), fanin_values.end() );
  } );

  std::vector<SimulationType> po_values( ntk.num_pos() );
  ntk.foreach_po( [&]( auto const& f, auto i ) {
    if ( ntk.is_complemented( f ) )
    {
      po_values[i] = sim.compute_not( node_to_value[f] );
    }
    else
    {
      po_values[i] = node_to_value[f];
    }
  } );
  return po_values;
}

} // namespace mockturtle
