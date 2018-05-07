#include <catch.hpp>

#include <mockturtle/algorithms/lut_mapping.hpp>
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
  lut_mapping( mapped_aig );

  CHECK( has_has_mapping_v<mapping_view<aig_network>> );
  CHECK( mapped_aig.has_mapping() );

  CHECK( !mapped_aig.is_mapped( aig.get_node( a ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( b ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f1 ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f2 ) ) );
  CHECK( !mapped_aig.is_mapped( aig.get_node( f3 ) ) );
  CHECK( mapped_aig.is_mapped( aig.get_node( f4 ) ) );
}
