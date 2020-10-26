#include <catch.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>

#include <mockturtle/networks/mig.hpp>
#include <mockturtle/algorithms/resub_engines.hpp>

using namespace mockturtle;

TEST_CASE( "MIG resub engine (bottom-up) -- 0-resub", "[resub_engines]" )
{
  std::vector<kitty::dynamic_truth_table> tts( 4, kitty::dynamic_truth_table( 3 ) );

  kitty::create_from_binary_string( tts[0], "00110110" );
  kitty::create_from_binary_string( tts[1], "11111100" );
  kitty::create_from_binary_string( tts[2], "10000001" );
  kitty::create_from_binary_string( tts[3], "11001001" );

  mig_resub_engine_bottom_up<kitty::dynamic_truth_table> engine( 3 );
  engine.add_root( 0, tts );
  engine.add_divisor( 1, tts );
  engine.add_divisor( 2, tts );
  engine.add_divisor( 3, tts );

  const auto res = engine.compute_function( 0u );
  CHECK( res );
  CHECK( (*res).size() == 1u );
  CHECK( (*res)[0] == 7u );
}

TEST_CASE( "MIG resub engine (bottom-up) -- 1-resub", "[resub_engines]" )
{
  std::vector<kitty::dynamic_truth_table> tts( 4, kitty::dynamic_truth_table( 3 ) );

  kitty::create_from_binary_string( tts[0], "01110110" );
  kitty::create_from_binary_string( tts[1], "11110100" );
  kitty::create_from_binary_string( tts[2], "11001001" );
  kitty::create_from_binary_string( tts[3], "01000111" );

  mig_resub_engine_bottom_up<kitty::dynamic_truth_table> engine( tts.size() - 1u );
  engine.add_root( 0, tts );
  for ( auto i = 1u; i < tts.size(); ++i )
  {
    engine.add_divisor( i, tts );
  }
  // target = <1,~2,3>

  const auto res = engine.compute_function( 1u );
  CHECK( res );
  CHECK( (*res).size() == 4u );

  /* when translating index list, 0 and 1 are for constants */
  auto target = tts[0];
  kitty::create_from_binary_string( tts[0], "00000000" );
  
  auto f1 = (*res)[0] % 2 ? ~tts[(*res)[0] / 2] : tts[(*res)[0] / 2];
  auto f2 = (*res)[1] % 2 ? ~tts[(*res)[1] / 2] : tts[(*res)[1] / 2];
  auto f3 = (*res)[2] % 2 ? ~tts[(*res)[2] / 2] : tts[(*res)[2] / 2];
  tts.emplace_back( kitty::ternary_majority( f1, f2, f3 ) );

  auto ans = (*res)[3] % 2 ? ~tts[(*res)[3] / 2] : tts[(*res)[3] / 2];
  CHECK( target == ans );
}

TEST_CASE( "MIG resub engine (bottom-up) -- 2-resub", "[resub_engines]" )
{
  std::vector<kitty::dynamic_truth_table> tts( 5, kitty::dynamic_truth_table( 3 ) );

  kitty::create_from_binary_string( tts[0], "00101110" );
  kitty::create_from_binary_string( tts[1], "11101111" );
  kitty::create_from_binary_string( tts[2], "00100000" );
  kitty::create_from_binary_string( tts[3], "10011110" );
  kitty::create_from_binary_string( tts[4], "01011111" );

  mig_resub_engine_bottom_up<kitty::dynamic_truth_table> engine( tts.size() - 1u );
  engine.add_root( 0, tts );
  for ( auto i = 1u; i < tts.size(); ++i )
  {
    engine.add_divisor( i, tts );
  }
  // target = <<1,2,3>,2,4>

  const auto res = engine.compute_function( 2u );
  CHECK( res );
  CHECK( (*res).size() == 7u );

  /* when translating index list, 0 and 1 are for constants */
  auto target = tts[0];
  kitty::create_from_binary_string( tts[0], "00000000" );
  
  auto f1 = (*res)[0] % 2 ? ~tts[(*res)[0] / 2] : tts[(*res)[0] / 2];
  auto f2 = (*res)[1] % 2 ? ~tts[(*res)[1] / 2] : tts[(*res)[1] / 2];
  auto f3 = (*res)[2] % 2 ? ~tts[(*res)[2] / 2] : tts[(*res)[2] / 2];
  tts.emplace_back( kitty::ternary_majority( f1, f2, f3 ) );
  
  f1 = (*res)[3] % 2 ? ~tts[(*res)[3] / 2] : tts[(*res)[3] / 2];
  f2 = (*res)[4] % 2 ? ~tts[(*res)[4] / 2] : tts[(*res)[4] / 2];
  f3 = (*res)[5] % 2 ? ~tts[(*res)[5] / 2] : tts[(*res)[5] / 2];
  tts.emplace_back( kitty::ternary_majority( f1, f2, f3 ) );

  auto ans = (*res)[6] % 2 ? ~tts[(*res)[6] / 2] : tts[(*res)[6] / 2];
  CHECK( target == ans );
}
