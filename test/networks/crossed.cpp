#include <catch.hpp>

#include <vector>

#include <mockturtle/networks/crossed.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <kitty/operators.hpp>

using namespace mockturtle;

TEST_CASE( "insert crossings in reversed topological order", "[crossed]" )
{
  crossed_klut_network crossed;
  auto const x1 = crossed.create_pi();
  auto const x2 = crossed.create_pi();

  auto const n3 = crossed.create_and( x1, x2 );
  auto const n4 = crossed.create_or( x1, x2 );
  auto const n5 = crossed.create_xor( x1, x2 );

  auto const c6 = crossed.insert_crossing( x1, x2, crossed.get_node( n4 ), crossed.get_node( n3 ) );
  auto const c7 = crossed.insert_crossing( x1, x2, crossed.get_node( n5 ), crossed.get_node( n4 ) );
  auto const c8 = crossed.insert_crossing( x1, x2, c7, c6 );

  crossed.foreach_fanin_ignore_crossings( crossed.get_node( n3 ), [&]( auto const& f, auto i ){
    if ( i == 0 )
      CHECK( f == x1 );
    else
      CHECK( f == x2 );
  } );
  crossed.foreach_fanin_ignore_crossings( crossed.get_node( n4 ), [&]( auto const& f, auto i ){
    if ( i == 0 )
      CHECK( f == x1 );
    else
      CHECK( f == x2 );
  } );
  crossed.foreach_fanin_ignore_crossings( crossed.get_node( n5 ), [&]( auto const& f, auto i ){
    if ( i == 0 )
      CHECK( f == x1 );
    else
      CHECK( f == x2 );
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

  crossed.foreach_fanin_ignore_crossings( crossed.get_node( n6 ), [&]( auto const& f, auto i ){
    if ( i == 0 )
      CHECK( f == x1 );
    else
      CHECK( f == x2 );
  } );
  crossed.foreach_fanin_ignore_crossings( crossed.get_node( n7 ), [&]( auto const& f, auto i ){
    if ( i == 0 )
      CHECK( f == x1 );
    else
      CHECK( f == x2 );
  } );
  crossed.foreach_fanin_ignore_crossings( crossed.get_node( n8 ), [&]( auto const& f, auto i ){
    if ( i == 0 )
      CHECK( f == x1 );
    else
      CHECK( f == x2 );
  } );
}
