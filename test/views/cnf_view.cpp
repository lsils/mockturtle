#include <catch.hpp>

#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/cnf_view.hpp>

using namespace mockturtle;

TEST_CASE( "create a simple miter of equivalent functions with cnf_view", "[cnf_view]" )
{
  cnf_view<xag_network> xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();

  const auto f = xag.create_xor( a, b );
  const auto g = xag.create_or( xag.create_and( !a, b ), xag.create_and( a, !b ) );
  xag.create_po( xag.create_xor( f, g ) );

  const auto result = xag.solve();
  CHECK( result );   /* has result */
  CHECK( !*result ); /* result is UNSAT */
}

TEST_CASE( "create a simple miter of non-equivalent functions with cnf_view", "[cnf_view]" )
{
  cnf_view<xag_network> xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();

  const auto f = xag.create_or( a, b );
  const auto g = xag.create_xor( a, b );
  xag.create_po( xag.create_xor( f, g ) );

  const auto result = xag.solve();
  CHECK( result );  /* has result */
  CHECK( *result ); /* result is SAT */
  CHECK( xag.pi_values() == std::vector<bool>{{true, true}} );
}
