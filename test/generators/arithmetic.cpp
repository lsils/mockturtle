#include <catch.hpp>

#include <vector>

#include <mockturtle/traits.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>

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

template<typename Ntk>
void simulate_carry_ripple_adder( uint32_t op1, uint32_t op2 )
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );
  auto carry = ntk.get_constant( false );

  carry_ripple_adder_inplace( ntk, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );

  CHECK( ntk.num_pis() == 16 );
  CHECK( ntk.num_pos() == 8 );

  const auto simm = simulate<bool>( ntk, input_word_simulator( ( op1 << 8 ) + op2 ) );
  CHECK( simm.size() == 8 );
  for ( auto i = 0; i < 8; ++i )
  {
    CHECK( ( ( ( op1 + op2 ) >> i ) & 1 ) == simm[i] );
  }
}

TEST_CASE( "build an 8-bit adder with an AIG", "[arithmetic]" )
{
  simulate_carry_ripple_adder<aig_network>( 37, 73 );
  simulate_carry_ripple_adder<aig_network>( 0, 255 );
  simulate_carry_ripple_adder<aig_network>( 0, 255 );
  simulate_carry_ripple_adder<aig_network>( 200, 200 );
  simulate_carry_ripple_adder<aig_network>( 12, 10 );

  simulate_carry_ripple_adder<mig_network>( 37, 73 );
  simulate_carry_ripple_adder<mig_network>( 0, 255 );
  simulate_carry_ripple_adder<mig_network>( 0, 255 );
  simulate_carry_ripple_adder<mig_network>( 200, 200 );
  simulate_carry_ripple_adder<mig_network>( 12, 10 );

  simulate_carry_ripple_adder<klut_network>( 37, 73 );
  simulate_carry_ripple_adder<klut_network>( 0, 255 );
  simulate_carry_ripple_adder<klut_network>( 0, 255 );
  simulate_carry_ripple_adder<klut_network>( 200, 200 );
  simulate_carry_ripple_adder<klut_network>( 12, 10 );
}

template<typename Ntk>
void simulate_carry_ripple_subtractor( uint32_t op1, uint32_t op2 )
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );
  auto carry = ntk.get_constant( true );

  carry_ripple_subtractor_inplace( ntk, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );

  CHECK( ntk.num_pis() == 16 );
  CHECK( ntk.num_pos() == 8 );

  const auto simm = simulate<bool>( ntk, input_word_simulator( ( op2 << 8 ) + op1 ) );
  CHECK( simm.size() == 8 );
  for ( auto i = 0; i < 8; ++i )
  {
    CHECK( ( ( ( op1 - op2 ) >> i ) & 1 ) == simm[i] );
  }
}

TEST_CASE( "build an 8-bit subtractor with an AIG", "[arithmetic]" )
{
  simulate_carry_ripple_subtractor<aig_network>( 73, 37 );
  simulate_carry_ripple_subtractor<aig_network>( 0, 255 );
  simulate_carry_ripple_subtractor<aig_network>( 0, 255 );
  simulate_carry_ripple_subtractor<aig_network>( 200, 200 );
  simulate_carry_ripple_subtractor<aig_network>( 12, 10 );

  simulate_carry_ripple_subtractor<mig_network>( 37, 73 );
  simulate_carry_ripple_subtractor<mig_network>( 0, 255 );
  simulate_carry_ripple_subtractor<mig_network>( 0, 255 );
  simulate_carry_ripple_subtractor<mig_network>( 200, 200 );
  simulate_carry_ripple_subtractor<mig_network>( 12, 10 );

  simulate_carry_ripple_subtractor<klut_network>( 37, 73 );
  simulate_carry_ripple_subtractor<klut_network>( 0, 255 );
  simulate_carry_ripple_subtractor<klut_network>( 0, 255 );
  simulate_carry_ripple_subtractor<klut_network>( 200, 200 );
  simulate_carry_ripple_subtractor<klut_network>( 12, 10 );
}
