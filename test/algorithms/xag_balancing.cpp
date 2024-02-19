#include <catch.hpp>

#include <algorithm>
#include <vector>

#include <mockturtle/algorithms/xag_balancing.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/depth_view.hpp>

using namespace mockturtle;

TEST_CASE( "Balancing AND chain in AIG (XAG)", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();
  const auto d = xag.create_pi();

  xag.create_po( xag.create_and( a, xag.create_and( b, xag.create_and( c, d ) ) ) );

  CHECK( depth_view{ xag }.depth() == 3u );

  xag_balance( xag );

  CHECK( depth_view{ xag }.depth() == 2u );
}

TEST_CASE( "Balance AND finding structural hashing (XAG)", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();
  const auto d = xag.create_pi();

  const auto f1 = xag.create_and( a, b );
  const auto f2 = xag.create_and( f1, c );
  const auto f3 = xag.create_and( b, c );
  const auto f4 = xag.create_and( f3, d );

  xag.create_po( f2 );
  xag.create_po( f4 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 4u );

  xag_balancing_params ps;
  ps.minimize_levels = false;
  xag_balance( xag, ps );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );
}

TEST_CASE( "Balance AND finding structural hashing (XAG) slow", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();
  const auto d = xag.create_pi();

  const auto f1 = xag.create_and( a, b );
  const auto f2 = xag.create_and( f1, c );
  const auto f3 = xag.create_and( b, c );
  const auto f4 = xag.create_and( f3, d );

  xag.create_po( f2 );
  xag.create_po( f4 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 4u );

  xag_balancing_params ps;
  ps.minimize_levels = false;
  ps.fast_mode = false;

  xag_balance( xag, ps );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );
}

TEST_CASE( "Balance AND tree that is constant 0 (XAG)", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();

  const auto f1 = xag.create_and( a, b );
  const auto f2 = xag.create_and( !a, c );
  const auto f3 = xag.create_and( f1, f2 );

  xag.create_po( f3 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );

  xag_balance( xag );

  CHECK( depth_view{ xag }.depth() == 0u );
  CHECK( xag.num_gates() == 0u );
}

TEST_CASE( "Balance AND tree that has redundant leaves (XAG)", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();

  const auto f1 = xag.create_and( a, b );
  const auto f2 = xag.create_and( a, c );
  const auto f3 = xag.create_and( f1, f2 );

  xag.create_po( f3 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );

  xag_balance( xag );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 2u );
}

TEST_CASE( "Balance AND tree that has redundant leaves (XAG) slow", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();

  const auto f1 = xag.create_and( a, b );
  const auto f2 = xag.create_and( a, c );
  const auto f3 = xag.create_and( f1, f2 );

  xag.create_po( f3 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );

  xag_balancing_params ps;
  ps.fast_mode = false;
  xag_balance( xag, ps );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 2u );
}

TEST_CASE( "Balancing XOR chain", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();
  const auto d = xag.create_pi();

  xag.create_po( xag.create_xor( a, xag.create_xor( b, xag.create_xor( c, d ) ) ) );

  CHECK( depth_view{ xag }.depth() == 3u );

  xag_balance( xag );

  CHECK( depth_view{ xag }.depth() == 2u );
}

TEST_CASE( "Balance XOR finding structural hashing", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();
  const auto d = xag.create_pi();

  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_xor( f1, c );
  const auto f3 = xag.create_xor( b, c );
  const auto f4 = xag.create_xor( f3, d );

  xag.create_po( f2 );
  xag.create_po( f4 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 4u );

  xag_balancing_params ps;
  ps.minimize_levels = false;
  xag_balance( xag, ps );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );
}

TEST_CASE( "Balance XOR finding structural hashing slow", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();
  const auto d = xag.create_pi();

  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_xor( f1, c );
  const auto f3 = xag.create_xor( b, c );
  const auto f4 = xag.create_xor( f3, d );

  xag.create_po( f2 );
  xag.create_po( f4 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 4u );

  xag_balancing_params ps;
  ps.minimize_levels = false;
  ps.fast_mode = false;
  xag_balance( xag, ps );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );
}

TEST_CASE( "Balance XOR tree that has redundant leaves", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();

  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_xor( a, c );
  const auto f3 = xag.create_xor( f1, f2 );

  xag.create_po( f3 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );

  xag_balance( xag );

  CHECK( depth_view{ xag }.depth() == 1u );
  CHECK( xag.num_gates() == 1u );
}

TEST_CASE( "Balance XOR tree that has redundant leaves negated", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();

  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_xnor( a, c );
  const auto f3 = xag.create_xor( f1, f2 );

  xag.create_po( f3 );

  CHECK( depth_view{ xag }.depth() == 2u );
  CHECK( xag.num_gates() == 3u );

  xag_balance( xag );

  CHECK( depth_view{ xag }.depth() == 1u );
  CHECK( xag.num_gates() == 1u );
}

TEST_CASE( "Balance XOR tree that is constant 1", "[xag_balancing]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();

  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_xor( a, c );
  const auto f3 = xag.create_xor( b, c );
  const auto f4 = xag.create_xnor( f1, f2 );
  const auto f5 = xag.create_xor( f3, f4 );

  xag.create_po( f5 );

  CHECK( depth_view{ xag }.depth() == 3u );
  CHECK( xag.num_gates() == 5u );

  xag_balance( xag );

  CHECK( depth_view{ xag }.depth() == 0u );
  CHECK( xag.num_gates() == 0u );
}
