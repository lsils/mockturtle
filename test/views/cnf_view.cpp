#include <catch.hpp>

#include <mockturtle/networks/mig.hpp>
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

TEST_CASE( "cnf_view with custom clauses", "[cnf_view]" )
{
  cnf_view<mig_network> mig;
  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();
  mig.create_po( mig.create_maj( a, b, c ) );

  mig.add_clause( lit_not( mig.lit( a ) ) );
  mig.add_clause( mig.lit( b ) );
  const auto result = mig.solve();
  CHECK( result );
  CHECK( *result );
  CHECK( !mig.value( mig.get_node( a ) ) );
  CHECK( mig.value( mig.get_node( b ) ) );
  CHECK( mig.value( mig.get_node( c ) ) );
}
