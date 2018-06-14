#include <catch.hpp>

#include <vector>

#include <mockturtle/traits.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>

#include <kitty/static_truth_table.hpp>

using namespace mockturtle;

TEST_CASE( "build a full adder with an AIG", "[arithmetic]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  auto [sum, carry] = full_adder( aig, a, b, c );

  aig.create_po( sum );
  aig.create_po( carry );

  const auto simm = simulate<kitty::static_truth_table<3>>( aig );
  CHECK( simm.size() == 2 );
  CHECK( simm[0]._bits == 0x96 );
  CHECK( simm[1]._bits == 0xe8 );
}

TEST_CASE( "build a 2-bit adder with an AIG", "[arithmetic]" )
{
  aig_network aig;

  std::vector<aig_network::signal> a( 2 ), b( 2 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.create_pi();

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  CHECK( aig.num_pis() == 5 );
  CHECK( aig.num_pos() == 3 );
  CHECK( aig.num_gates() == 14 );

  const auto simm = simulate<kitty::static_truth_table<5>>( aig );
  CHECK( simm.size() == 3 );
  CHECK( simm[0]._bits == 0xa5a55a5a );
  CHECK( simm[1]._bits == 0xc936936c );
  CHECK( simm[2]._bits == 0xfec8ec80 );
}
