#include <catch.hpp>

#include <kitty/kitty.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/index_list.hpp>
#include <mockturtle/algorithms/xag_resyn_engines.hpp>
#include <mockturtle/algorithms/simulation.hpp>

using namespace mockturtle;

void test_aig_kresub( std::vector<kitty::partial_truth_table> const& tts, uint32_t num_inserts )
{
  xag_resyn_engine<kitty::partial_truth_table> engine( tts[0], ~tts[0].construct() );
  for ( auto i = 1u; i < tts.size(); ++i )
  {
    engine.add_divisor( i, tts );
  }
  const auto res = engine.compute_function( num_inserts );
  CHECK( res );
  CHECK( (*res).num_gates() == num_inserts );

  aig_network aig;
  decode( aig, *res );
  partial_simulator sim( tts ); // tts[0] is the redundant input that should never be used
  const auto ans = simulate<kitty::partial_truth_table, aig_network, partial_simulator>( aig, sim )[0];
  CHECK( tts[0] == ans );
}

void test_xag_kresub( std::vector<kitty::partial_truth_table> const& tts, uint32_t num_inserts )
{
  xag_resyn_engine<kitty::partial_truth_table, true> engine( tts[0], ~tts[0].construct() );
  for ( auto i = 1u; i < tts.size(); ++i )
  {
    engine.add_divisor( i, tts );
  }
  const auto res = engine.compute_function( num_inserts );
  CHECK( res );
  CHECK( (*res).num_gates() == num_inserts );

  xag_network xag;
  decode( xag, *res );
  partial_simulator sim( tts ); // tts[0] is the redundant input that should never be used
  const auto ans = simulate<kitty::partial_truth_table, xag_network, partial_simulator>( xag, sim )[0];
  CHECK( tts[0] == ans );
}

TEST_CASE( "AIG/XAG resynthesis -- 0-resub with don't care", "[xag_resyn]" )
{
  std::vector<kitty::partial_truth_table> tts( 1, kitty::partial_truth_table( 8 ) );
  kitty::partial_truth_table target( 8 );
  kitty::partial_truth_table care( 8 );

  /* const */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "11001100" );
  xag_resyn_engine<kitty::partial_truth_table> engine1( target, care );
  const auto res1 = engine1.compute_function( 0 );
  CHECK( res1 );
  CHECK( to_index_list_string( *res1 ) == "{1 | 1 << 8 | 0 << 16, 0}" );

  /* buffer */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "00111100" );
  kitty::create_from_binary_string( tts[0], "11110000" );
  xag_resyn_engine<kitty::partial_truth_table> engine2( target, care );
  engine2.add_divisor( 0, tts );
  const auto res2 = engine2.compute_function( 0 );
  CHECK( res2 );
  CHECK( to_index_list_string( *res2 ) == "{2 | 1 << 8 | 0 << 16, 4}" );

  /* inverter */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "00110110" );
  kitty::create_from_binary_string( tts[0], "00000101" );
  xag_resyn_engine<kitty::partial_truth_table> engine3( target, care );
  engine3.add_divisor( 0, tts );
  const auto res3 = engine3.compute_function( 0 );
  CHECK( res3 );
  CHECK( to_index_list_string( *res3 ) == "{2 | 1 << 8 | 0 << 16, 5}" );
}

TEST_CASE( "AIG resynthesis -- 1 <= k <= 3", "[xag_resyn]" )
{
  std::vector<kitty::partial_truth_table> tts( 4, kitty::partial_truth_table( 8 ) );

  kitty::create_from_binary_string( tts[0], "11110000" ); // target
  kitty::create_from_binary_string( tts[1], "11000000" );
  kitty::create_from_binary_string( tts[2], "00110000" );
  kitty::create_from_binary_string( tts[3], "01011111" ); // binate
  test_aig_kresub( tts, 1 ); // 1 | 2

  kitty::create_from_binary_string( tts[0], "11110000" ); // target
  kitty::create_from_binary_string( tts[1], "11001100" ); // binate
  kitty::create_from_binary_string( tts[2], "11111100" );
  kitty::create_from_binary_string( tts[3], "00001100" );
  test_aig_kresub( tts, 1 ); // 2 & ~3

  kitty::create_from_binary_string( tts[0], "11110000" ); // target
  kitty::create_from_binary_string( tts[1], "01110010" ); // binate
  kitty::create_from_binary_string( tts[2], "11111100" ); 
  kitty::create_from_binary_string( tts[3], "10000011" ); // binate
  test_aig_kresub( tts, 2 ); // 2 & (1 | 3)

  tts.emplace_back( 8 );
  kitty::create_from_binary_string( tts[0], "11110000" ); // target
  kitty::create_from_binary_string( tts[1], "01110010" ); // binate
  kitty::create_from_binary_string( tts[2], "00110011" ); // binate
  kitty::create_from_binary_string( tts[3], "10000011" ); // binate
  kitty::create_from_binary_string( tts[4], "11001011" ); // binate
  test_aig_kresub( tts, 3 ); // ~(2 & 4) & (1 | 3)
}

TEST_CASE( "AIG resynthesis -- recursive", "[xag_resyn]" )
{

}
