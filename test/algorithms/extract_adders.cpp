#include <catch.hpp>

#include <cstdint>
#include <vector>

#include <mockturtle/algorithms/extract_adders.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/block.hpp>

using namespace mockturtle;

TEST_CASE( "Map Adders on AIG with no adders", "[extract_adders]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f = aig.create_maj( a, b, c );
  aig.create_po( f );

  extract_adders_params ps;
  extract_adders_stats st;
  block_network luts = extract_adders( aig, ps, &st );

  CHECK( luts.size() == 9u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 4u );
  CHECK( st.maj3 == 1u );
  CHECK( st.mapped_fa + st.mapped_ha == 0u );
}

TEST_CASE( "Map Adders on full adder 1", "[extract_adders]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto [sum, carry] = full_adder( aig, a, b, c );
  aig.create_po( sum );
  aig.create_po( carry );

  extract_adders_params ps;
  extract_adders_stats st;
  block_network luts = extract_adders( aig, ps, &st );

  CHECK( luts.size() == 6u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 1u );
  CHECK( st.maj3 == 1u );
  CHECK( st.xor3 == 1u );
  CHECK( st.mapped_ha == 0u );
  CHECK( st.mapped_fa == 1u );
}

TEST_CASE( "Map Adders on full adder 2", "[extract_adders]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto [sum, carry] = full_adder( aig, a, b, c );
  aig.create_po( sum );
  aig.create_po( carry );

  extract_adders_params ps;
  ps.map_inverted = true;
  extract_adders_stats st;
  block_network luts = extract_adders( aig, ps, &st );

  CHECK( luts.size() == 6u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 1u );
  CHECK( st.maj3 == 1u );
  CHECK( st.xor3 == 1u );
  CHECK( st.mapped_ha == 0u );
  CHECK( st.mapped_fa == 1u );
}

TEST_CASE( "Map adders on ripple carry adder", "[extract_adders]" )
{
  aig_network aig;
  
  std::vector<aig_network::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  extract_adders_params ps;
  extract_adders_stats st;
  block_network luts = extract_adders( aig, ps, &st );

  CHECK( luts.size() == 26u );
  CHECK( luts.num_pis() == 16u );
  CHECK( luts.num_pos() == 9u );
  CHECK( luts.num_gates() == 8u );
  CHECK( st.and2 == 52u );
  CHECK( st.xor2 == 15u );
  CHECK( st.maj3 == 7u );
  CHECK( st.xor3 == 7u );
  CHECK( st.mapped_ha == 1u );
  CHECK( st.mapped_fa == 7u );
}

TEST_CASE( "Map adders on multiplier", "[extract_adders]" )
{
  aig_network aig;

  std::vector<typename aig_network::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );

  for ( auto const& o : carry_ripple_multiplier( aig, a, b ) )
  {
    aig.create_po( o );
  }

  CHECK( aig.num_pis() == 16 );
  CHECK( aig.num_pos() == 16 );

  extract_adders_params ps;
  extract_adders_stats st;
  block_network luts = extract_adders( aig, ps, &st );

  CHECK( luts.size() == 138u );
  CHECK( luts.num_pis() == 16u );
  CHECK( luts.num_pos() == 16u );
  CHECK( luts.num_gates() == 120u );
  CHECK( st.and2 == 424u );
  CHECK( st.xor2 == 104u );
  CHECK( st.maj3 == 48u );
  CHECK( st.xor3 == 90u );
  CHECK( st.mapped_ha == 8u );
  CHECK( st.mapped_fa == 48u );
}
