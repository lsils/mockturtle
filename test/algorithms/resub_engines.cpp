#include <catch.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>

#include <mockturtle/networks/mig.hpp>
#include <mockturtle/algorithms/resub_engines.hpp>
#include <mockturtle/algorithms/simulation.hpp>

using namespace mockturtle;

TEST_CASE( "MIG resub engine (bottom-up) -- 0-resub", "[resub_engines]" )
{
  std::vector<kitty::dynamic_truth_table> tts( 4, kitty::dynamic_truth_table( 3 ) );

  kitty::create_from_binary_string( tts[0], "00110110" );
  kitty::create_from_binary_string( tts[1], "11111100" );
  kitty::create_from_binary_string( tts[2], "10000001" );
  kitty::create_from_binary_string( tts[3], "11001001" );

  mig_resub_engine_bottom_up<kitty::dynamic_truth_table> engine( tts[0] );
  engine.add_divisor( 1, tts );
  engine.add_divisor( 2, tts );
  engine.add_divisor( 3, tts );

  const auto res = engine.compute_function( 0u );
  CHECK( res );
  CHECK( (*res).num_gates() == 0u );
  CHECK( (*res).raw()[1] == 7u );
}

TEST_CASE( "MIG resub engine (bottom-up) -- 1-resub", "[resub_engines]" )
{
  std::vector<kitty::partial_truth_table> tts( 3, kitty::partial_truth_table( 8 ) );
  kitty::partial_truth_table target( 8 );

  kitty::create_from_binary_string( target, "01110110" );
  kitty::create_from_binary_string( tts[0], "11110100" );
  kitty::create_from_binary_string( tts[1], "11001001" );
  kitty::create_from_binary_string( tts[2], "01000111" );

  mig_resub_engine_bottom_up<kitty::partial_truth_table> engine( target );
  for ( auto i = 0u; i < tts.size(); ++i )
  {
    engine.add_divisor( i, tts );
  }
  // target = <1,~2,3>

  const auto res = engine.compute_function( 1u );
  CHECK( res );
  CHECK( (*res).num_gates() == 1u );

  mig_network mig;
  decode( mig, *res );
  partial_simulator sim( tts );
  const auto ans = simulate<kitty::partial_truth_table, mig_network, partial_simulator>( mig, sim )[0];
  CHECK( target == ans );
}

TEST_CASE( "MIG resub engine (bottom-up) -- 2-resub", "[resub_engines]" )
{
  std::vector<kitty::partial_truth_table> tts( 4, kitty::partial_truth_table( 8 ) );
  kitty::partial_truth_table target( 8 );

  kitty::create_from_binary_string( target, "00101110" );
  kitty::create_from_binary_string( tts[0], "11101111" );
  kitty::create_from_binary_string( tts[1], "00100000" );
  kitty::create_from_binary_string( tts[2], "10011110" );
  kitty::create_from_binary_string( tts[3], "01011111" );

  mig_resub_engine_bottom_up<kitty::partial_truth_table> engine( target );
  for ( auto i = 0u; i < tts.size(); ++i )
  {
    engine.add_divisor( i, tts );
  }
  // target = <<1,2,3>,2,4>

  const auto res = engine.compute_function( 2u );
  CHECK( res );
  CHECK( (*res).num_gates() == 2u );

  mig_network mig;
  decode( mig, *res );
  partial_simulator sim( tts );
  const auto ans = simulate<kitty::partial_truth_table, mig_network, partial_simulator>( mig, sim )[0];
  CHECK( target == ans );
}
