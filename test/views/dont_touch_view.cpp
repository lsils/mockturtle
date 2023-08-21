#include <catch.hpp>

#include <sstream>

#include <mockturtle/networks/klut.hpp>
#include <mockturtle/views/dont_touch_view.hpp>

using namespace mockturtle;

TEST_CASE( "Create dont touch view 1", "[dont_touch_view]" )
{
  dont_touch_view<klut_network> ntk{};

  auto const a = ntk.create_pi();
  auto const b = ntk.create_pi();
  auto const c = ntk.create_pi();
  auto const d = ntk.create_pi();

  auto const c0 = ntk.get_constant( false );
  auto const t1 = ntk.create_and( a, b );
  auto const t2 = ntk.create_or( c, d );
  auto const f = ntk.create_and( t1, t2 );
  auto const g = ntk.create_not( a );

  ntk.create_po( f );
  ntk.create_po( g );
  ntk.create_po( ntk.get_constant() );

  ntk.select_dont_touch( ntk.get_node( c0 ) );
  ntk.select_dont_touch( ntk.get_node( t1 ) );
  ntk.select_dont_touch( ntk.get_node( t2 ) );
  ntk.select_dont_touch( ntk.get_node( f ) );
  ntk.select_dont_touch( ntk.get_node( g ) );

  CHECK( ntk.is_dont_touch( ntk.get_node( a ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( b ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( c ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( d ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( c0 ) ) == true );
  CHECK( ntk.is_dont_touch( ntk.get_node( t1 ) ) == true );
  CHECK( ntk.is_dont_touch( ntk.get_node( t2 ) ) == true );
  CHECK( ntk.is_dont_touch( ntk.get_node( f ) ) == true );
  CHECK( ntk.is_dont_touch( ntk.get_node( g ) ) == true );

  uint32_t count = 0;
  ntk.foreach_dont_touch( [&]( auto const& n ) {
    (void)n;
    ++count;
  } );

  CHECK( count == 5 );

  ntk.remove_dont_touch( ntk.get_node( t1 ) );
  ntk.remove_dont_touch( ntk.get_node( t2 ) );

  CHECK( ntk.is_dont_touch( ntk.get_node( a ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( b ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( c ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( d ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( c0 ) ) == true );
  CHECK( ntk.is_dont_touch( ntk.get_node( t1 ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( t2 ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( f ) ) == true );
  CHECK( ntk.is_dont_touch( ntk.get_node( g ) ) == true );
}

TEST_CASE( "Create dont touch view 2", "[dont_touch_view]" )
{
  klut_network ntk;

  auto const a = ntk.create_pi();
  auto const b = ntk.create_pi();
  auto const c = ntk.create_pi();
  auto const d = ntk.create_pi();

  auto const c0 = ntk.get_constant( false );
  auto const t1 = ntk.create_and( a, b );
  auto const t2 = ntk.create_or( c, d );
  auto const f = ntk.create_and( t1, t2 );
  auto const g = ntk.create_not( a );

  ntk.create_po( f );
  ntk.create_po( g );
  ntk.create_po( ntk.get_constant() );

  dont_touch_view<klut_network> dt_ntk{ ntk };

  dt_ntk.select_dont_touch( dt_ntk.get_node( c0 ) );
  dt_ntk.select_dont_touch( dt_ntk.get_node( t1 ) );
  dt_ntk.select_dont_touch( dt_ntk.get_node( t2 ) );
  dt_ntk.select_dont_touch( dt_ntk.get_node( t2 ) );
  dt_ntk.select_dont_touch( dt_ntk.get_node( f ) );
  dt_ntk.select_dont_touch( dt_ntk.get_node( g ) );

  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( a ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( b ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( c ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( d ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( c0 ) ) == true );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( t1 ) ) == true );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( t2 ) ) == true );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( f ) ) == true );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( g ) ) == true );

  uint32_t count = 0;
  dt_ntk.foreach_dont_touch( [&]( auto const& n ) {
    (void)n;
    ++count;
  } );

  CHECK( count == 5 );

  dt_ntk.remove_dont_touch( dt_ntk.get_node( t1 ) );
  dt_ntk.remove_dont_touch( dt_ntk.get_node( t2 ) );

  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( a ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( b ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( c ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( d ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( c0 ) ) == true );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( t1 ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( t2 ) ) == false );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( f ) ) == true );
  CHECK( dt_ntk.is_dont_touch( dt_ntk.get_node( g ) ) == true );
}

TEST_CASE( "Dont touch view on copy", "[dont_touch_view]" )
{
  dont_touch_view<klut_network> ntk{};

  auto const a = ntk.create_pi();
  auto const b = ntk.create_pi();
  auto const c = ntk.create_pi();
  auto const d = ntk.create_pi();

  auto const c0 = ntk.get_constant( false );
  auto const t1 = ntk.create_and( a, b );
  auto const t2 = ntk.create_or( c, d );
  auto const f = ntk.create_and( t1, t2 );
  auto const g = ntk.create_not( a );

  ntk.create_po( f );
  ntk.create_po( g );
  ntk.create_po( ntk.get_constant() );

  ntk.select_dont_touch( ntk.get_node( c0 ) );
  ntk.select_dont_touch( ntk.get_node( t1 ) );
  ntk.select_dont_touch( ntk.get_node( t2 ) );
  ntk.select_dont_touch( ntk.get_node( f ) );
  ntk.select_dont_touch( ntk.get_node( g ) );

  dont_touch_view<klut_network> ntk_copy = ntk;

  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( a ) ) == false );
  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( b ) ) == false );
  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( c ) ) == false );
  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( d ) ) == false );
  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( c0 ) ) == true );
  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( t1 ) ) == true );
  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( t2 ) ) == true );
  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( f ) ) == true );
  CHECK( ntk_copy.is_dont_touch( ntk_copy.get_node( g ) ) == true );

  uint32_t count = 0;
  ntk.foreach_dont_touch( [&]( auto const& n ) {
    (void)n;
    ++count;
  } );

  CHECK( count == 5 );

  ntk.remove_dont_touch( ntk.get_node( t1 ) );
  ntk.remove_dont_touch( ntk.get_node( t2 ) );

  CHECK( ntk.is_dont_touch( ntk.get_node( a ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( b ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( c ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( d ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( c0 ) ) == true );
  CHECK( ntk.is_dont_touch( ntk.get_node( t1 ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( t2 ) ) == false );
  CHECK( ntk.is_dont_touch( ntk.get_node( f ) ) == true );
  CHECK( ntk.is_dont_touch( ntk.get_node( g ) ) == true );

  count = 0;
  ntk.foreach_dont_touch( [&]( auto const& n ) {
    (void)n;
    ++count;
  } );

  CHECK( count == 3 );
}
