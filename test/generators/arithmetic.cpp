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

template<class IntType = uint64_t>
inline IntType to_int( std::vector<bool> const& sim )
{
  return std::accumulate( sim.rbegin(), sim.rend(), IntType( 0 ), []( auto x, auto y ) { return ( x << 1 ) + y; } );
}

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
void validate_network( Ntk const& ntk, uint32_t input, uint32_t output )
{
  const auto simm = simulate<bool>( ntk, input_word_simulator( input ) );
  CHECK( to_int( simm ) == output );
}

template<typename Ntk>
Ntk create_adder()
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );
  auto carry = ntk.get_constant( false );

  carry_ripple_adder_inplace( ntk, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );
  ntk.create_po( carry );

  CHECK( ntk.num_pis() == 16 );
  CHECK( ntk.num_pos() == 9 );

  return ntk;
}

TEST_CASE( "build an 8-bit adder with an AIG", "[arithmetic]" )
{
  const auto aig = create_adder<aig_network>();
  validate_network( aig, ( 37 << 8 ) + 73, 37 + 73 );
  validate_network( aig, ( 0 << 8 ) + 255, 0 + 255 );
  validate_network( aig, ( 200 << 8 ) + 100, 200 + 100 );
  validate_network( aig, ( 12 << 8 ) + 10, 12 + 10 );

  const auto mig = create_adder<mig_network>();
  validate_network( mig, ( 37 << 8 ) + 73, 37 + 73 );
  validate_network( mig, ( 0 << 8 ) + 255, 0 + 255 );
  validate_network( mig, ( 200 << 8 ) + 100, 200 + 100 );
  validate_network( mig, ( 12 << 8 ) + 10, 12 + 10 );

  const auto klut = create_adder<klut_network>();
  validate_network( klut, ( 37 << 8 ) + 73, 37 + 73 );
  validate_network( klut, ( 0 << 8 ) + 255, 0 + 255 );
  validate_network( klut, ( 200 << 8 ) + 100, 200 + 100 );
  validate_network( klut, ( 12 << 8 ) + 10, 12 + 10 );
}

template<typename Ntk>
Ntk create_subtractor()
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );
  auto carry = ntk.get_constant( true );

  carry_ripple_subtractor_inplace( ntk, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );
  ntk.create_po( ntk.create_not( carry ) );

  CHECK( ntk.num_pis() == 16 );
  CHECK( ntk.num_pos() == 9 );

  return ntk;
}

TEST_CASE( "build an 8-bit subtractor with different networks", "[arithmetic]" )
{
  const auto aig = create_subtractor<aig_network>();
  validate_network( aig, ( 37 << 8 ) + 73, 73 - 37 );
  validate_network( aig, ( 0 << 8 ) + 255, 255 );
  validate_network( aig, ( 100 << 8 ) + 200, 200 - 100 );
  validate_network( aig, ( 10 << 8 ) + 12, 12 - 10 );

  const auto mig = create_subtractor<mig_network>();
  validate_network( mig, ( 37 << 8 ) + 73, 73 - 37 );
  validate_network( mig, ( 0 << 8 ) + 255, 255 );
  validate_network( mig, ( 100 << 8 ) + 200, 200 - 100 );
  validate_network( mig, ( 10 << 8 ) + 12, 12 - 10 );

  const auto klut = create_subtractor<klut_network>();
  validate_network( klut, ( 37 << 8 ) + 73, 73 - 37 );
  validate_network( klut, ( 0 << 8 ) + 255, 255 );
  validate_network( klut, ( 100 << 8 ) + 200, 200 - 100 );
  validate_network( klut, ( 10 << 8 ) + 12, 12 - 10 );
}

template<typename Ntk>
Ntk create_multiplier()
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );

  for ( auto const& o : carry_ripple_multiplier( ntk, a, b ) )
  {
    ntk.create_po( o );
  }

  CHECK( ntk.num_pis() == 16 );
  CHECK( ntk.num_pos() == 16 );

  return ntk;
}

TEST_CASE( "build an 8-bit multiplier with different networks", "[arithmetic]" )
{
  const auto aig = create_multiplier<aig_network>();
  validate_network( aig, ( 37 << 8 ) + 73, 37 * 73 );
  validate_network( aig, ( 0 << 8 ) + 255, 0 * 255 );
  validate_network( aig, ( 100 << 8 ) + 200, 100 * 200 );
  validate_network( aig, ( 10 << 8 ) + 12, 10 * 12 );
  validate_network( aig, ( 73 << 8 ) + 37, 37 * 73 );
  validate_network( aig, ( 255 << 8 ) + 0, 0 * 255 );
  validate_network( aig, ( 200 << 8 ) + 100, 100 * 200 );
  validate_network( aig, ( 12 << 8 ) + 10, 10 * 12 );

  const auto mig = create_multiplier<mig_network>();
  validate_network( mig, ( 37 << 8 ) + 73, 37 * 73 );
  validate_network( mig, ( 0 << 8 ) + 255, 0 * 255 );
  validate_network( mig, ( 100 << 8 ) + 200, 100 * 200 );
  validate_network( mig, ( 10 << 8 ) + 12, 10 * 12 );
  validate_network( mig, ( 73 << 8 ) + 37, 37 * 73 );
  validate_network( mig, ( 255 << 8 ) + 0, 0 * 255 );
  validate_network( mig, ( 200 << 8 ) + 100, 100 * 200 );
  validate_network( mig, ( 12 << 8 ) + 10, 10 * 12 );

  const auto klut = create_multiplier<klut_network>();
  validate_network( klut, ( 37 << 8 ) + 73, 37 * 73 );
  validate_network( klut, ( 0 << 8 ) + 255, 0 * 255 );
  validate_network( klut, ( 100 << 8 ) + 200, 100 * 200 );
  validate_network( klut, ( 10 << 8 ) + 12, 10 * 12 );
  validate_network( klut, ( 73 << 8 ) + 37, 37 * 73 );
  validate_network( klut, ( 255 << 8 ) + 0, 0 * 255 );
  validate_network( klut, ( 200 << 8 ) + 100, 100 * 200 );
  validate_network( klut, ( 12 << 8 ) + 10, 10 * 12 );
}
