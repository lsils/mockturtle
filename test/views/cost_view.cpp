#include <catch.hpp>

#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/cost_view.hpp>

using namespace mockturtle;

TEST_CASE( "compute depth cost for xag network", "[cost_view]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto f1 = xag.create_and( a, b );
  const auto f2 = xag.create_and( a, f1 );
  const auto f3 = xag.create_and( b, f1 );
  const auto f4 = xag.create_and( f2, f3 );
  xag.create_po( f4 );

  cost_view cost_xag( xag, level_cost<xag_network>() );
  CHECK( cost_xag.get_cost() == 3 );
  CHECK( cost_xag.get_cost( xag.get_node( a  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( b  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f1 ) ) == 1 );
  CHECK( cost_xag.get_cost( xag.get_node( f2 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f3 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f4 ) ) == 3 );

}

TEST_CASE( "compute AND costs for xag network", "[cost_view]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_and( a, f1 );
  const auto f3 = xag.create_xor( b, f1 );
  const auto f4 = xag.create_and( f2, f3 );
  xag.create_po( f4 );

  cost_view cost_xag( xag, and_cost<xag_network>() );
  CHECK( cost_xag.get_cost() == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( a  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( b  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f1 ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f2 ) ) == 1 );
  CHECK( cost_xag.get_cost( xag.get_node( f3 ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f4 ) ) == 2 );

}

TEST_CASE( "compute gate costs for xag network", "[cost_view]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_and( a, f1 );
  const auto f3 = xag.create_xor( b, f1 );
  const auto f4 = xag.create_and( f2, f3 );
  xag.create_po( f4 );

  cost_view cost_xag( xag, gate_cost<xag_network>() );
  CHECK( cost_xag.get_cost() == 4 );
  CHECK( cost_xag.get_cost( xag.get_node( a  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( b  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f1 ) ) == 1 );
  CHECK( cost_xag.get_cost( xag.get_node( f2 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f3 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f4 ) ) == 4 );

}

TEST_CASE( "compute support numer of xag network", "[cost_view]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();
  const auto d = xag.create_pi();
  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_and( b, c );
  const auto f3 = xag.create_xor( f1, f2 );
  const auto f4 = xag.create_and( f3, d );
  xag.create_po( f4 );

  cost_view cost_xag( xag, supp_cost<xag_network>() );
  CHECK( cost_xag.get_cost() == 11 ); // 4 + 3 + 2 + 2
  CHECK( cost_xag.get_cost( xag.get_node( a  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( b  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( c  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( d  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f1 ) ) == 2 ); // 2
  CHECK( cost_xag.get_cost( xag.get_node( f2 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f3 ) ) == 7 ); // 3 + 2 + 2
  CHECK( cost_xag.get_cost( xag.get_node( f4 ) ) == 11 ); // 4 + 3 + 2 + 2

}
