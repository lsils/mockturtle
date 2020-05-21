#include <catch.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/aqfp_view.hpp>

using namespace mockturtle;

TEST_CASE( "aqfp_view tests", "[aqfp_view]" )
{
  mig_network mig;
  auto const a = mig.create_pi();
  auto const b = mig.create_pi();
  auto const c = mig.create_pi();
  auto const d = mig.create_pi();
  auto const e = mig.create_pi();

  auto const f1 = mig.create_maj( a, b, c );
  auto const f2 = mig.create_maj( d, e, f1 );
  auto const f3 = mig.create_maj( a, d, f1 );
  auto const f4 = mig.create_maj( f1, f2, f3 );
  mig.create_po( f4 );

  aqfp_view view{mig};
  CHECK( view.level( view.get_node( f1 ) ) == 1u );
  CHECK( view.level( view.get_node( f2 ) ) == 3u );
  CHECK( view.level( view.get_node( f3 ) ) == 3u );
  CHECK( view.level( view.get_node( f4 ) ) == 4u );
  CHECK( view.depth() == 4u );

  CHECK( view.num_buffers( view.get_node( f1 ) ) == 2u );
  CHECK( view.num_buffers( view.get_node( f2 ) ) == 0u );
  CHECK( view.num_buffers( view.get_node( f3 ) ) == 0u );
  CHECK( view.num_buffers( view.get_node( f4 ) ) == 0u );
  CHECK( view.num_buffers() == 2u );

  CHECK( view.num_splitters( view.get_node( f1 ) ) == 1u );
  CHECK( view.num_splitters( view.get_node( f2 ) ) == 0u );
  CHECK( view.num_splitters( view.get_node( f3 ) ) == 0u );
  CHECK( view.num_splitters( view.get_node( f4 ) ) == 0u );
}
