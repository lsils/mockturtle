#include <catch.hpp>

#include <mockturtle/algorithms/lut_mapping.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/mapping_view.hpp>

using namespace mockturtle;

TEST_CASE( "LUT mapping of AIG", "[lut_mapping]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( f1, a );
  const auto f3 = aig.create_nand( f1, b );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  mapping_view mapped_aig{ aig };

  CHECK( has_has_mapping_v<mapping_view<aig_network>> );
  CHECK( has_is_mapped_v<mapping_view<aig_network>> );
  CHECK( has_clear_mapping_v<mapping_view<aig_network>> );
  CHECK( has_num_luts_v<mapping_view<aig_network>> );
  CHECK( has_add_to_mapping_v<mapping_view<aig_network>> );
  CHECK( has_remove_from_mapping_v<mapping_view<aig_network>> );
  CHECK( has_foreach_lut_fanin_v<mapping_view<aig_network>> );

  CHECK( !mapped_aig.has_mapping() );

  lut_mapping( mapped_aig );

  CHECK( mapped_aig.has_mapping() );
  CHECK( mapped_aig.num_luts() == 1 );

  CHECK( !mapped_aig.is_mapped( aig.get_node( a ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( b ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f1 ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f2 ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f3 ) ) );
  CHECK( mapped_aig.is_mapped( aig.get_node( f4 ) ) );

  mapped_aig.clear_mapping();

  CHECK( !mapped_aig.has_mapping() );
  CHECK( mapped_aig.num_luts() == 0 );

  CHECK( !mapped_aig.is_mapped( aig.get_node( a ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( b ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f1 ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f2 ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f3 ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f4 ) ) );
}

TEST_CASE( "LUT mapping of 2-LUT network", "[lut_mapping]" )
{
  aig_network aig;

  std::vector<signal<aig_network>> a( 2 ), b( 2 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.create_pi();

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  mapping_view mapped_aig{ aig };
  lut_mapping( mapped_aig );

  CHECK( mapped_aig.num_luts() == 3 );
}

TEST_CASE( "LUT mapping of 8-LUT network", "[lut_mapping]" )
{
  aig_network aig;

  std::vector<signal<aig_network>> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  mapping_view mapped_aig{ aig };
  lut_mapping( mapped_aig );

  CHECK( mapped_aig.num_luts() == 12 );
}

TEST_CASE( "LUT mapping of 64-LUT network", "[lut_mapping]" )
{
  aig_network aig;

  std::vector<signal<aig_network>> a( 64 ), b( 64 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  mapping_view mapped_aig{ aig };
  lut_mapping( mapped_aig );

  CHECK( mapped_aig.num_luts() == 96 );
}
