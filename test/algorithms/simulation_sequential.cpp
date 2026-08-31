#include <catch.hpp>

#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/algorithms/simulation_sequential.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/sequential.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace mockturtle;

namespace
{

/*! \brief Builds a Fibonacci LFSR with taps on the top two bits.
 *
 * It has no primary inputs at all, so it runs off its reset state alone -- which
 * makes it a direct test of whether the reset values are honoured.
 */
sequential<aig_network> lfsr( uint32_t width, uint32_t seed )
{
  sequential<aig_network> aig;

  std::vector<aig_network::signal> state( width );
  for ( auto i = 0u; i < width; ++i )
  {
    state[i] = aig.create_ro();
  }

  auto const feedback = aig.create_xor( state[width - 1], state[width - 2] );

  /* primary outputs are created before register inputs: both are combinational
     outputs of the same network, sliced by position */
  aig.create_po( state[width - 1] );

  aig.create_ri( feedback );
  for ( auto i = 0u; i + 1 < width; ++i )
  {
    aig.create_ri( state[i] );
  }

  for ( auto i = 0u; i < width; ++i )
  {
    mockturtle::register_t reg;
    reg.init = ( ( seed >> i ) & 1 ) ? register_init::one : register_init::zero;
    aig.set_register( i, reg );
  }

  return aig;
}

/*! \brief Collects the single primary output of every cycle into a bit string. */
std::string trace_of( simulate_sequential_result<bool> const& result )
{
  std::string bits;
  for ( auto const& outputs : result.outputs )
  {
    bits += outputs[0] ? '1' : '0';
  }
  return bits;
}

/*! \brief Reads a register state as an integer, register 0 being the low bit. */
uint32_t state_of( std::vector<bool> const& state )
{
  uint32_t value{ 0 };
  for ( auto i = 0u; i < state.size(); ++i )
  {
    value |= static_cast<uint32_t>( state[i] ) << i;
  }
  return value;
}

} /* namespace */

TEST_CASE( "simulate an LFSR from its reset state", "[simulation_sequential]" )
{
  auto const aig = lfsr( 4, 1 );

  auto const result = simulate_sequential<bool>( aig, 15, default_simulator<bool>( std::vector<bool>{} ) );

  CHECK( result.num_cycles() == 15 );

  /* a maximal-length sequence: 15 states before it comes back around */
  CHECK( trace_of( result ) == "000100110101111" );

  /* and it does come back around -- cycle 15 repeats cycle 0 */
  auto const two_periods = simulate_sequential<bool>( aig, 30, default_simulator<bool>( std::vector<bool>{} ) );
  CHECK( trace_of( two_periods ).substr( 0, 15 ) == trace_of( two_periods ).substr( 15 ) );
}

TEST_CASE( "a different seed shifts the same sequence", "[simulation_sequential]" )
{
  /* seeding with the second state of the first LFSR must produce the same
     sequence one step ahead, which is only true if the reset values are used */
  auto const from_one = simulate_sequential<bool>( lfsr( 4, 1 ), 15, default_simulator<bool>( std::vector<bool>{} ) );
  auto const from_two = simulate_sequential<bool>( lfsr( 4, 2 ), 15, default_simulator<bool>( std::vector<bool>{} ) );

  CHECK( trace_of( from_one ).substr( 1 ) == trace_of( from_two ).substr( 0, 14 ) );
}

TEST_CASE( "a register with no reset value follows the parameter", "[simulation_sequential]" )
{
  /* a single register that simply holds whatever it was reset to */
  sequential<aig_network> aig;
  auto const state = aig.create_ro();
  aig.create_po( state );
  aig.create_ri( state );

  mockturtle::register_t reg;
  reg.init = register_init::unknown;
  aig.set_register( 0, reg );

  simulate_sequential_params ps;

  ps.undefined_reset_value = false;
  CHECK( trace_of( simulate_sequential<bool>( aig, 3, default_simulator<bool>( std::vector<bool>{} ), ps ) ) == "000" );

  ps.undefined_reset_value = true;
  CHECK( trace_of( simulate_sequential<bool>( aig, 3, default_simulator<bool>( std::vector<bool>{} ), ps ) ) == "111" );
}

TEST_CASE( "simulate a shift register with a per-cycle stimulus", "[simulation_sequential]" )
{
  /* three registers in a chain: whatever is put in appears at the output three
     cycles later */
  sequential<aig_network> aig;

  auto const in = aig.create_pi();
  auto const a = aig.create_ro();
  auto const b = aig.create_ro();
  auto const c = aig.create_ro();

  aig.create_po( c );

  aig.create_ri( in );
  aig.create_ri( a );
  aig.create_ri( b );

  for ( auto i = 0u; i < 3u; ++i )
  {
    mockturtle::register_t reg;
    reg.init = register_init::zero;
    aig.set_register( i, reg );
  }

  /* a single 1 on the input, then silence */
  stimulus_simulator sim( { { true }, { false } } );

  CHECK( trace_of( simulate_sequential<bool>( aig, 6, sim ) ) == "000100" );
}

TEST_CASE( "a stimulus shorter than the run holds its last assignment", "[simulation_sequential]" )
{
  sequential<aig_network> aig;

  auto const in = aig.create_pi();
  auto const state = aig.create_ro();

  aig.create_po( state );
  aig.create_ri( in );

  mockturtle::register_t reg;
  reg.init = register_init::zero;
  aig.set_register( 0, reg );

  /* One assignment for a four-cycle run: the input stays high after cycle 0.
     Spelled through a named vector rather than as `sim( { { true } } )`, which
     GCC 12 and older cannot tell apart from a copy construction -- the same
     reason `default_simulator<bool>` is spelled with an explicit `std::vector<bool>{}`
     throughout this file. */
  std::vector<std::vector<bool>> const stimulus{ { true } };
  stimulus_simulator sim( stimulus );

  CHECK( trace_of( simulate_sequential<bool>( aig, 4, sim ) ) == "0111" );
}

TEST_CASE( "simulate a sequential network with truth tables", "[simulation_sequential]" )
{
  /* a register holding the AND of the two primary inputs: the output is constant
     0 in the first cycle and the AND from the second one on */
  sequential<aig_network> aig;

  auto const x0 = aig.create_pi();
  auto const x1 = aig.create_pi();
  auto const state = aig.create_ro();

  aig.create_po( state );
  aig.create_ri( aig.create_and( x0, x1 ) );

  mockturtle::register_t reg;
  reg.init = register_init::zero;
  aig.set_register( 0, reg );

  auto const result = simulate_sequential<kitty::dynamic_truth_table>(
      aig, 3, default_simulator<kitty::dynamic_truth_table>( 2 ) );

  kitty::dynamic_truth_table expected( 2 );
  kitty::create_from_hex_string( expected, "8" );

  CHECK( kitty::is_const0( result.outputs[0][0] ) );
  CHECK( result.outputs[1][0] == expected );
  CHECK( result.outputs[2][0] == expected );

  /* the register itself carries the AND from the first cycle on */
  CHECK( kitty::is_const0( result.states[0][0] ) );
  CHECK( result.states[1][0] == expected );
  CHECK( result.final_state()[0] == expected );
}

TEST_CASE( "simulating no cycles still reports the reset state", "[simulation_sequential]" )
{
  auto const result = simulate_sequential<bool>( lfsr( 4, 1 ), 0, default_simulator<bool>( std::vector<bool>{} ) );

  CHECK( result.num_cycles() == 0 );
  CHECK( result.outputs.empty() );

  /* zero cycles still cross one state boundary: the one the run started at */
  CHECK( result.states.size() == 1 );
  CHECK( state_of( result.reset_state() ) == 1 );
  CHECK( state_of( result.final_state() ) == 1 );
}

TEST_CASE( "the state trace follows the LFSR through its cycle", "[simulation_sequential]" )
{
  auto const result = simulate_sequential<bool>( lfsr( 4, 1 ), 15, default_simulator<bool>( std::vector<bool>{} ) );

  /* n cycles cross n + 1 state boundaries */
  CHECK( result.num_cycles() == 15 );
  CHECK( result.states.size() == result.outputs.size() + 1 );

  /* it starts at its seed and, after a full period, returns to it */
  CHECK( state_of( result.reset_state() ) == 1 );
  CHECK( state_of( result.final_state() ) == 1 );

  /* every intermediate state is distinct and non-zero -- a maximal-length run */
  std::vector<uint32_t> seen;
  for ( auto i = 0u; i < result.num_cycles(); ++i )
  {
    CHECK( state_of( result.states[i] ) != 0 );
    seen.push_back( state_of( result.states[i] ) );
  }
  std::sort( seen.begin(), seen.end() );
  CHECK( std::unique( seen.begin(), seen.end() ) == seen.end() );
}

TEST_CASE( "the state trace records what a register held during its cycle", "[simulation_sequential]" )
{
  /* one register, driven straight from the primary input, and read out on the
     primary output: the output of a cycle is the state it started in */
  sequential<aig_network> aig;

  auto const in = aig.create_pi();
  auto const state = aig.create_ro();

  aig.create_po( state );
  aig.create_ri( in );

  mockturtle::register_t reg;
  reg.init = register_init::zero;
  aig.set_register( 0, reg );

  std::vector<std::vector<bool>> const stimulus{ { true }, { false }, { true } };
  stimulus_simulator sim( stimulus );

  auto const result = simulate_sequential<bool>( aig, 3, sim );

  CHECK( trace_of( result ) == "010" );
  for ( auto i = 0u; i < result.num_cycles(); ++i )
  {
    CHECK( result.states[i][0] == result.outputs[i][0] );
  }

  /* the last input latched but never read out */
  CHECK( result.final_state()[0] == true );
}
