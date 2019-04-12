#include <catch.hpp>

#include <mockturtle/algorithms/cnf.hpp>
#include <mockturtle/networks/xag.hpp>

#include <fmt/format.h>
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
    //auto lits = (int*)( const_cast<uint32_t*>( clause.data() ) );
    //solver.add_clause( lits, lits + clause.size() );
    solver.add_clause( clause );
  } )[0];

  CHECK( output == 11 );

  const auto res = solver.solve( &output, &output + 1, 0 );
  CHECK( res == percy::synth_result::success );
  CHECK( solver.var_value( 0u ) != solver.var_value( 1u ) ); /* input values are different */
}
