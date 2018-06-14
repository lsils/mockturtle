#include <catch.hpp>

#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>

#include <kitty/static_truth_table.hpp>

using namespace mockturtle;

TEST_CASE( "Simulate XOR AIG circuit with Booleans", "[simulation]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  CHECK( !simulate<bool>( aig, default_simulator<bool>( {false, false} ) )[0] );
  CHECK( simulate<bool>( aig, default_simulator<bool>( {false, true} ) )[0] );
  CHECK( simulate<bool>( aig, default_simulator<bool>( {true, false} ) )[0] );
  CHECK( !simulate<bool>( aig, default_simulator<bool>( {false, false} ) )[0] );
}

TEST_CASE( "Simulate XOR AIG circuit with static truth table", "[simulation]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  const auto tt = simulate<kitty::static_truth_table<2>>( aig )[0];
  CHECK( tt._bits == 0x6 );
}


TEST_CASE( "Simulate XOR AIG circuit with dynamic truth table", "[simulation]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  default_simulator<kitty::dynamic_truth_table> sim( 2 );
  const auto tt = simulate<kitty::dynamic_truth_table>( aig, sim )[0];
  CHECK( tt._bits[0] == 0x6 );
}
