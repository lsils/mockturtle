#include <catch.hpp>

#include <mockturtle/algorithms/cnf.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/xag.hpp>

#include <fmt/format.h>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <percy/solvers/bsat2.hpp>

using namespace mockturtle;

TEST_CASE( "Translate XAG into CNF", "[cnf]" )
{
  xag_network xag;

  const auto a = xag.create_pi();
  const auto b = xag.create_pi();

  const auto f1 = xag.create_nand( a, b );
  const auto f2 = xag.create_nand( a, f1 );
  const auto f3 = xag.create_nand( b, f1 );
  const auto f4 = xag.create_nand( f2, f3 );

  xag.create_po( f4 );

  percy::bsat_wrapper solver;
  int output = generate_cnf( xag, [&]( auto const& clause ) {
    solver.add_clause( clause );
  } )[0];

  CHECK( output == 13 );

  const auto res = solver.solve( &output, &output + 1, 0 );
  CHECK( res == percy::synth_result::success );
  CHECK( solver.var_value( 1u ) != solver.var_value( 2u ) ); /* input values are different */
}

TEST_CASE( "Translate k-LUT network into CNF", "[cnf]" )
{
  klut_network ntk;

  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();
  const auto c = ntk.create_pi();

  kitty::dynamic_truth_table _xor3( 3u ), _maj( 3u ), _xnor( 2u );
  kitty::create_from_hex_string( _xor3, "96" );
  kitty::create_from_hex_string( _maj, "e8" );
  kitty::create_from_hex_string( _xnor, "9" );

  const auto f1 = ntk.create_node( {a, b, c}, _xor3 );
  const auto f2 = ntk.create_node( {a, b, c}, _maj );
  const auto f3 = ntk.create_node( {f1, f2}, _xnor );
  ntk.create_po( f3 );

  percy::bsat_wrapper solver;
  int output = generate_cnf( ntk, [&]( auto const& clause ) {
    solver.add_clause( clause );
  } )[0];

  CHECK( output / 2 == 6u );

  const auto res = solver.solve( &output, &output + 1, 0 );
  CHECK( res == percy::synth_result::success );
  CHECK( ( ( solver.var_value( 1u ) == solver.var_value( 2u ) ) && ( solver.var_value( 2u ) == solver.var_value( 3u ) ) ) ); /* input values are the same */
}
