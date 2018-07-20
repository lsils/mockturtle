#include <catch.hpp>

#include <numeric>
#include <random>
#include <vector>

#include <mockturtle/traits.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/generators/modular_arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>

using namespace mockturtle;

template<class IntType = uint64_t>
inline IntType to_int( std::vector<bool> const& sim )
{
  return std::accumulate( sim.rbegin(), sim.rend(), IntType( 0 ), []( auto x, auto y ) { return ( x << 1 ) + y; } );
}

template<typename Ntk>
void simulate_modular_adder( uint32_t op1, uint32_t op2 )
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );

  modular_adder_inplace( ntk, a, b );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );

  CHECK( ntk.num_pis() == 16 );
  CHECK( ntk.num_pos() == 8 );

  const auto simm = simulate<bool>( ntk, input_word_simulator( ( op1 << 8 ) + op2 ) );
  CHECK( simm.size() == 8 );
  const auto result = ( op1 + op2 ) % ( 1 << 8 );
  for ( auto i = 0; i < 8; ++i )
  {
    CHECK( ( ( result >> i ) & 1 ) == simm[i] );
  }
}

TEST_CASE( "build an 8-bit modular adder with different networks", "[modular_arithmetic]" )
{
  simulate_modular_adder<aig_network>( 37, 73 );
  simulate_modular_adder<aig_network>( 0, 255 );
  simulate_modular_adder<aig_network>( 0, 255 );
  simulate_modular_adder<aig_network>( 200, 200 );
  simulate_modular_adder<aig_network>( 120, 250 );

  simulate_modular_adder<mig_network>( 37, 73 );
  simulate_modular_adder<mig_network>( 0, 255 );
  simulate_modular_adder<mig_network>( 0, 255 );
  simulate_modular_adder<mig_network>( 200, 200 );
  simulate_modular_adder<mig_network>( 120, 250 );

  simulate_modular_adder<klut_network>( 37, 73 );
  simulate_modular_adder<klut_network>( 0, 255 );
  simulate_modular_adder<klut_network>( 0, 255 );
  simulate_modular_adder<klut_network>( 200, 200 );
  simulate_modular_adder<klut_network>( 120, 250 );
}

template<typename Ntk>
void simulate_modular_adder2( uint32_t op1, uint32_t op2, uint32_t k, uint64_t c )
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( k ), b( k );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );

  modular_adder_inplace( ntk, a, b, c );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );

  CHECK( ntk.num_pis() == 2u * k );
  CHECK( ntk.num_pos() == k );

  const auto simm = simulate<bool>( ntk, input_word_simulator( ( op1 << k ) + op2 ) );
  CHECK( simm.size() == k );
  const auto result = ( op1 + op2 ) % ( ( 1 << k ) - c );
  CHECK( to_int( simm ) == result );
}

TEST_CASE( "build an k-bit modular adder with constants", "[modular_arithmetic]" )
{
  for ( uint32_t i = 0u; i < 29u; ++i )
  {
    for ( uint32_t j = 0u; j < 29u; ++j )
    {
      simulate_modular_adder2<aig_network>( i, j, 5, 3 );
      simulate_modular_adder2<mig_network>( i, j, 5, 3 );
    }
  }

  std::default_random_engine gen( 655321 );

  for ( auto i = 0; i < 1000; ++i )
  {
    auto k = std::uniform_int_distribution<uint32_t>( 5, 16 )( gen );
    auto c = std::uniform_int_distribution<uint64_t>( 1, 20 )( gen );
    auto a = std::uniform_int_distribution<uint32_t>( 0, ( 1 << k ) - c - 1 )( gen );
    auto b = std::uniform_int_distribution<uint32_t>( 0, ( 1 << k ) - c - 1 )( gen );

    simulate_modular_adder2<aig_network>( a, b, k, c );
    simulate_modular_adder2<mig_network>( a, b, k, c );
  }
}

template<typename Ntk>
void simulate_modular_subtractor( uint32_t op1, uint32_t op2 )
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );

  modular_subtractor_inplace( ntk, a, b );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );

  CHECK( ntk.num_pis() == 16 );
  CHECK( ntk.num_pos() == 8 );

  const auto simm = simulate<bool>( ntk, input_word_simulator( ( op1 << 8 ) + op2 ) );
  CHECK( simm.size() == 8 );
  const auto result = ( op2 - op1 ) % ( 1 << 8 );
  for ( auto i = 0; i < 8; ++i )
  {
    CHECK( ( ( result >> i ) & 1 ) == simm[i] );
  }
}

TEST_CASE( "build an 8-bit modular subtractor with different networks", "[modular_arithmetic]" )
{
  simulate_modular_subtractor<aig_network>( 37, 73 );
  simulate_modular_subtractor<aig_network>( 0, 255 );
  simulate_modular_subtractor<aig_network>( 0, 255 );
  simulate_modular_subtractor<aig_network>( 200, 200 );
  simulate_modular_subtractor<aig_network>( 120, 250 );

  simulate_modular_subtractor<mig_network>( 37, 73 );
  simulate_modular_subtractor<mig_network>( 0, 255 );
  simulate_modular_subtractor<mig_network>( 0, 255 );
  simulate_modular_subtractor<mig_network>( 200, 200 );
  simulate_modular_subtractor<mig_network>( 120, 250 );

  simulate_modular_subtractor<klut_network>( 37, 73 );
  simulate_modular_subtractor<klut_network>( 0, 255 );
  simulate_modular_subtractor<klut_network>( 0, 255 );
  simulate_modular_subtractor<klut_network>( 200, 200 );
  simulate_modular_subtractor<klut_network>( 120, 250 );
}

template<typename Ntk>
void simulate_modular_subtractor2( uint32_t op1, uint32_t op2, uint32_t k, uint64_t c )
{
  Ntk ntk;

  std::vector<typename Ntk::signal> a( k ), b( k );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );

  modular_subtractor_inplace( ntk, a, b, c );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );

  CHECK( ntk.num_pis() == 2u * k );
  CHECK( ntk.num_pos() == k );

  const auto simm = simulate<bool>( ntk, input_word_simulator( ( op1 << k ) + op2 ) );
  CHECK( simm.size() == k );

  // C++ behaves different in computing mod with negative numbers (a % b) => (a + (a % b)) % b
  const int32_t ma = static_cast<int32_t>( op2 ) - static_cast<int32_t>( op1 );
  const int32_t mb = ( 1 << k ) - c;
  const auto result = ( mb + ( ma % mb ) ) % mb;
  CHECK( to_int( simm ) == result );
}

TEST_CASE( "build an k-bit modular subtractor with constants", "[modular_arithmetic]" )
{
  for ( uint32_t i = 0u; i < 29u; ++i )
  {
    for ( uint32_t j = 0u; j < 29u; ++j )
    {
      simulate_modular_subtractor2<aig_network>( i, j, 5, 3 );
      simulate_modular_subtractor2<mig_network>( i, j, 5, 3 );
    }
  }

  std::default_random_engine gen( 655321 );

  for ( auto i = 0; i < 1000; ++i )
  {
    auto k = std::uniform_int_distribution<uint32_t>( 5, 16 )( gen );
    auto c = std::uniform_int_distribution<uint64_t>( 1, 20 )( gen );
    auto a = std::uniform_int_distribution<uint32_t>( 0, ( 1 << k ) - c - 1 )( gen );
    auto b = std::uniform_int_distribution<uint32_t>( 0, ( 1 << k ) - c - 1 )( gen );

    simulate_modular_subtractor2<aig_network>( a, b, k, c );
    simulate_modular_subtractor2<mig_network>( a, b, k, c );
  }
}
