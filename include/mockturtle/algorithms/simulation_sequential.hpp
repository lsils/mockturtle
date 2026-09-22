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
  \file simulation_sequential.hpp
  \brief Cycle-accurate simulation of sequential networks

  \author Marcel Walter
*/

#pragma once

#include "../networks/sequential.hpp"
#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace mockturtle
{

/*! \brief Simulates Boolean assignments that change from cycle to cycle.
 *
 * A simulator for `simulate_sequential` holding one assignment vector per clock
 * cycle.  Cycles past the end of the stimulus repeat its last assignment, so a
 * stimulus shorter than the run holds its final value for the remainder of it.
 */
class stimulus_simulator
{
public:
  stimulus_simulator() = delete;

  explicit stimulus_simulator( std::vector<std::vector<bool>> stimulus )
      : _stimulus( std::move( stimulus ) )
  {
    assert( !_stimulus.empty() && "a stimulus needs at least one assignment" );
  }

  bool compute_constant( bool value ) const { return value; }

  bool compute_pi( uint32_t index, uint32_t cycle ) const
  {
    return _stimulus[std::min<std::size_t>( cycle, _stimulus.size() - 1 )][index];
  }

  bool compute_not( bool value ) const { return !value; }

private:
  std::vector<std::vector<bool>> _stimulus;
};

/*! \brief The result of simulating a sequential network.
 *
 * Both traces are indexed by clock cycle first.  `outputs[cycle][index]` is the
 * value primary output `index` took in that cycle, and `states[cycle][index]` the
 * value register `index` held while that cycle was evaluated.
 *
 * `states` is one entry longer than `outputs`: simulating `n` cycles crosses
 * `n + 1` state boundaries.  `states.front()` is the reset state the run started
 * from and `states.back()` the state it ended in, so a run of zero cycles still
 * reports the reset state and nothing else.
 */
template<class SimulationType>
struct simulate_sequential_result
{
  /*! \brief Primary output values, one vector per clock cycle. */
  std::vector<std::vector<SimulationType>> outputs;

  /*! \brief Register values, one vector per state boundary. */
  std::vector<std::vector<SimulationType>> states;

  /*! \brief Number of clock cycles simulated. */
  uint32_t num_cycles() const
  {
    return static_cast<uint32_t>( outputs.size() );
  }

  /*! \brief The state the registers were reset to. */
  std::vector<SimulationType> const& reset_state() const
  {
    assert( !states.empty() && "the state trace always holds the reset state" );
    return states.front();
  }

  /*! \brief The state the registers held after the last cycle. */
  std::vector<SimulationType> const& final_state() const
  {
    assert( !states.empty() && "the state trace always holds the reset state" );
    return states.back();
  }
};

/*! \brief Parameters for `simulate_sequential`. */
struct simulate_sequential_params
{
  /*! \brief Value a register starts at when its reset value is not defined.
   *
   * A register may declare no reset value at all -- `register_init::dont_care`
   * or `register_init::unknown`, which is what `register_t` defaults to and what
   * an AIGER latch with a nondeterministic reset reads back as.  Simulation
   * needs a concrete value, so this is the one it uses.
   */
  bool undefined_reset_value{ false };
};

namespace detail
{

/*! \brief Whether a simulator can produce a different value in every cycle. */
template<class Simulator, class = void>
struct has_compute_pi_at_cycle : std::false_type
{
};

template<class Simulator>
struct has_compute_pi_at_cycle<Simulator, std::void_t<decltype( std::declval<Simulator const>().compute_pi( uint32_t(), uint32_t() ) )>> : std::true_type
{
};

template<class Simulator>
inline constexpr bool has_compute_pi_at_cycle_v = has_compute_pi_at_cycle<Simulator>::value;

/*! \brief Evaluates every gate of the network from the values of its inputs. */
template<class SimulationType, class Ntk>
void simulate_gates( Ntk const& ntk, node_map<SimulationType, Ntk>& node_to_value )
{
  ntk.foreach_gate( [&]( auto const& n ) {
    std::vector<SimulationType> fanin_values( ntk.fanin_size( n ) );
    ntk.foreach_fanin( n, [&]( auto const& f, auto i ) {
      fanin_values[i] = node_to_value[f];
    } );
    node_to_value[n] = ntk.compute( n, fanin_values.begin(), fanin_values.end() );
  } );
}

} /* namespace detail */

/*! \brief Simulates a sequential network over a number of clock cycles.
 *
 * Every register starts at its reset value, the combinational logic is evaluated
 * once per cycle, the primary outputs are recorded, and the register inputs are
 * latched into the register outputs for the next cycle.
 *
 * This is what distinguishes it from `simulate`, which evaluates the
 * combinational logic exactly once and has no notion of a register: on a
 * sequential network `simulate` never assigns the register outputs at all, and
 * every value in their fanout cone is meaningless.
 *
 * The simulator follows the same concept as for `simulate`, with one addition.
 * If it provides `compute_pi( index, cycle )`, that overload is used and the
 * primary inputs may take a different value in every cycle -- see
 * `stimulus_simulator`.  A simulator offering only `compute_pi( index )` holds
 * its assignment for the whole run, which is what a design with no primary
 * inputs, such as an LFSR, wants anyway.
 *
 * A register whose reset value is undefined starts at
 * `simulate_sequential_params::undefined_reset_value`.
 *
 * **Required network functions:**
 * - `num_registers`
 * - `register_at`
 * - `foreach_ro`
 * - `foreach_ri`
 * - `foreach_pi`
 * - `foreach_po`
 * - `foreach_gate`
 * - `foreach_fanin`
 * - `fanin_size`
 * - `get_constant`
 * - `constant_value`
 * - `get_node`
 * - `is_complemented`
 * - `compute`
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      sequential<aig_network> aig = ...; // a 4-bit LFSR, say

      auto const result = simulate_sequential<bool>( aig, 15, default_simulator<bool>( std::vector<bool>{} ) );

      for ( auto const& outputs : result.outputs )
      {
        std::cout << outputs[0];
      }

      // where it ended up
      auto const& state = result.final_state();
   \endverbatim
 *
 * \param ntk The sequential network to simulate
 * \param num_cycles Number of clock cycles to run
 * \param sim The simulator
 * \param ps Parameters
 * \return The primary output values and the register values, per clock cycle
 */
template<class SimulationType, class Ntk, class Simulator = default_simulator<SimulationType>>
simulate_sequential_result<SimulationType> simulate_sequential( Ntk const& ntk, uint32_t num_cycles, Simulator const& sim = Simulator(), simulate_sequential_params const& ps = {} )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_num_registers_v<Ntk>, "Ntk does not implement the num_registers method" );
  static_assert( has_foreach_ro_v<Ntk>, "Ntk does not implement the foreach_ro method" );
  static_assert( has_foreach_ri_v<Ntk>, "Ntk does not implement the foreach_ri method" );
  static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_fanin_size_v<Ntk>, "Ntk does not implement the fanin_size method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_constant_value_v<Ntk>, "Ntk does not implement the constant_value method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  static_assert( has_compute_v<Ntk, SimulationType>, "Ntk does not implement the compute method for SimulationType" );

  /* the register state, one entry per register, seeded from the reset values */
  std::vector<SimulationType> state( ntk.num_registers() );
  for ( auto i = 0u; i < ntk.num_registers(); ++i )
  {
    auto const init = ntk.register_at( i ).init;
    bool const value = register_init::is_defined( init ) ? init == register_init::one
                                                         : ps.undefined_reset_value;
    state[i] = sim.compute_constant( value );
  }

  simulate_sequential_result<SimulationType> result;
  result.outputs.reserve( num_cycles );
  result.states.reserve( num_cycles + 1u );
  result.states.push_back( state );

  node_map<SimulationType, Ntk> node_to_value( ntk );

  auto const evaluate = [&]( auto const& f ) {
    return ntk.is_complemented( f ) ? sim.compute_not( node_to_value[f] ) : node_to_value[f];
  };

  for ( auto cycle = 0u; cycle < num_cycles; ++cycle )
  {
    /* constants */
    node_to_value[ntk.get_node( ntk.get_constant( false ) )] = sim.compute_constant( ntk.constant_value( ntk.get_node( ntk.get_constant( false ) ) ) );
    if ( ntk.get_node( ntk.get_constant( false ) ) != ntk.get_node( ntk.get_constant( true ) ) )
    {
      node_to_value[ntk.get_node( ntk.get_constant( true ) )] = sim.compute_constant( ntk.constant_value( ntk.get_node( ntk.get_constant( true ) ) ) );
    }

    /* primary inputs */
    ntk.foreach_pi( [&]( auto const& n, auto i ) {
      if constexpr ( detail::has_compute_pi_at_cycle_v<Simulator> )
      {
        node_to_value[n] = sim.compute_pi( i, cycle );
      }
      else
      {
        node_to_value[n] = sim.compute_pi( i );
      }
    } );

    /* the register outputs hold the state this cycle starts in */
    ntk.foreach_ro( [&]( auto const& n, auto i ) {
      node_to_value[n] = state[i];
    } );

    detail::simulate_gates<SimulationType, Ntk>( ntk, node_to_value );

    std::vector<SimulationType> outputs( ntk.num_pos() );
    ntk.foreach_po( [&]( auto const& f, auto i ) {
      outputs[i] = evaluate( f );
    } );
    result.outputs.push_back( std::move( outputs ) );

    /* latch the register inputs for the next cycle */
    std::vector<SimulationType> next( ntk.num_registers() );
    ntk.foreach_ri( [&]( auto const& f, auto i ) {
      next[i] = evaluate( f );
    } );
    state = std::move( next );
    result.states.push_back( state );
  }

  return result;
}

} /* namespace mockturtle */
