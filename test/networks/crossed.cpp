#include <catch.hpp>

#include <mockturtle/networks/crossed.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/algorithms/cleanup.hpp>

using namespace mockturtle;

TEST_CASE( "insert crossings in reversed topological order, then cleanup (topo-sort)", "[crossed]" )
{
  crossed_klut_network crossed;
  auto const x1 = crossed.create_pi();
  auto const x2 = crossed.create_pi();

  auto const n3 = crossed.create_and( x1, x2 );
  auto const n4 = crossed.create_or( x1, x2 );
  auto const n5 = crossed.create_xor( x1, x2 );

  crossed.create_po( n3 );
  crossed.create_po( n4 );
  crossed.create_po( n5 );

  auto const c6 = crossed.insert_crossing( x1, x2, crossed.get_node( n4 ), crossed.get_node( n3 ) );
  auto const c7 = crossed.insert_crossing( x1, x2, crossed.get_node( n5 ), crossed.get_node( n4 ) );
  auto const c8 = crossed.insert_crossing( x1, x2, c7, c6 );
  (void)c8;

  crossed = cleanup_dangling( crossed );

  crossed.foreach_po( [&]( auto const& po ){
    crossed.foreach_fanin_ignore_crossings( crossed.get_node( po ), [&]( auto const& f, auto i ){
      if ( i == 0 )
        CHECK( f == x1 );
      else
        CHECK( f == x2 );
    } );
  } );
}

TEST_CASE( "create crossings in topological order", "[crossed]" )
{
  crossed_klut_network crossed;
  auto const x1 = crossed.create_pi();
  auto const x2 = crossed.create_pi();

  auto const [c3x1, c3x2] = crossed.create_crossing( x1, x2 );
  auto const [c4x1, c4x2] = crossed.create_crossing( x1, c3x2 );
  auto const [c5x1, c5x2] = crossed.create_crossing( c3x1, x2 );

  auto const n6 = crossed.create_and( x1, c4x2 );
  auto const n7 = crossed.create_or( c4x1, c5x2 );
  auto const n8 = crossed.create_xor( c5x1, x2 );

  crossed.create_po( n6 );
  crossed.create_po( n7 );
  crossed.create_po( n8 );

  crossed.foreach_po( [&]( auto const& po ){
    crossed.foreach_fanin_ignore_crossings( crossed.get_node( po ), [&]( auto const& f, auto i ){
      if ( i == 0 )
        CHECK( f == x1 );
      else
        CHECK( f == x2 );
    } );
  } );
}

TEST_CASE( "transform from klut to crossed_klut", "[crossed]" )
{
  klut_network klut;

  auto const x1 = klut.create_pi();
  auto const x2 = klut.create_pi();

  auto const n3 = klut.create_and( x1, x2 );
  auto const n4 = klut.create_or( x1, x2 );
  auto const n5 = klut.create_xor( x1, x2 );

  klut.create_po( n3 );
  klut.create_po( n4 );
  klut.create_po( n5 );

  crossed_klut_network crossed = cleanup_dangling<klut_network, crossed_klut_network>( klut );
  CHECK( klut.size() == crossed.size() );
}
