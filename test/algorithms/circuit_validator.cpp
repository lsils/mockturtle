#include <catch.hpp>

#include <mockturtle/algorithms/circuit_validator.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <bill/sat/interface/z3.hpp>

using namespace mockturtle;

TEST_CASE( "Validating NEQ nodes and get CEX", "[validator]" )
{
  /* original circuit */
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const f1 = aig.create_and( !a, b );
  auto const f2 = aig.create_and( a, !b );

  circuit_validator v( aig );

  CHECK( v.validate( aig.get_node( f1 ), f2 ) == false );
  CHECK( unsigned( v.cex[0] ) + unsigned( v.cex[1] ) == 1u ); /* either 01 or 10 */
}

TEST_CASE( "Validating EQ nodes in XAG", "[validator]" )
{
  /* original circuit */
  xag_network xag;
  auto const a = xag.create_pi();
  auto const b = xag.create_pi();
  auto const f1 = xag.create_and( !a, b );
  auto const f2 = xag.create_and( a, !b );
  auto const f3 = xag.create_or( f1, f2 );
  auto const g = xag.create_xor( a, b );

  circuit_validator v( xag );

  CHECK( v.validate( xag.get_node( f3 ), !g ) == true );
}

TEST_CASE( "Validating with non-existing circuit", "[validator]" )
{
  /* original circuit */
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const f1 = aig.create_and( !a, b );
  auto const f2 = aig.create_and( a, !b );
  auto const f3 = aig.create_or( f1, f2 );

  circuit_validator v( aig );

  circuit_validator<aig_network>::gate::fanin fi1;
  fi1.idx = 0; fi1.inv = true;
  circuit_validator<aig_network>::gate::fanin fi2;
  fi2.idx = 1; fi2.inv = true;
  circuit_validator<aig_network>::gate g;
  g.fanins = {fi1, fi2};
  CHECK( v.validate( aig.get_node( f3 ), {aig.get_node( f1 ), aig.get_node( f2 )}, {g}, false ) == true );
}

TEST_CASE( "Validating after circuit update", "[validator]" )
{
  /* original circuit */
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const f1 = aig.create_and( !a, b );
  auto const f2 = aig.create_and( a, !b );
  auto const f3 = aig.create_or( f1, f2 );

  circuit_validator v( aig );

  /* new nodes created after construction of `circuit_validator` have to be added to it manually with `add_node` */
  auto const g1 = aig.create_and( a, b );
  auto const g2 = aig.create_and( !a, !b );
  auto const g3 = aig.create_or( g1, g2 );
  v.add_node( aig.get_node( g1 ) );
  v.add_node( aig.get_node( g2 ) );
  v.add_node( aig.get_node( g3 ) );

  CHECK( v.validate( aig.get_node( f3 ), g3 ) == true );
}

TEST_CASE( "Validating const nodes", "[validator]" )
{
  /* original circuit */
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const f1 = aig.create_and( !a, b );
  auto const f2 = aig.create_and( a, !b );
  auto const f3 = aig.create_or( f1, f2 ); // a ^ b

  auto const g1 = aig.create_and( a, b );
  auto const g2 = aig.create_and( !a, !b );
  auto const g3 = aig.create_or( g1, g2 ); // a == b

  auto const h = aig.create_and( f3, g3 ); // const 0

  circuit_validator v( aig );

  CHECK( v.validate( aig.get_node( h ), false ) == true );
  CHECK( v.validate( aig.get_node( f1 ), false ) == false );
  CHECK( v.cex[0] == false );
  CHECK( v.cex[1] == true );
}

TEST_CASE( "Validating with ODC", "[validator]" )
{
  /* original circuit */
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const f1 = aig.create_and( !a, b );
  auto const f2 = aig.create_and( a, !b );
  auto const f3 = aig.create_or( f1, f2 ); // a ^ b

  auto const g1 = aig.create_and( a, b );
  auto const g2 = aig.create_and( !a, !b );
  auto const g3 = aig.create_or( g1, g2 ); // a == b

  auto const h = aig.create_and( f3, g3 ); // const 0
  aig.create_po( h );

  validator_params ps;
  fanout_view view{aig};
  circuit_validator<fanout_view<aig_network>, bill::solvers::z3, true, true> v( view, ps );

  /* considering only 1 level, f1 can not be substituted with const 0 */
  ps.odc_levels = 1;
  CHECK( v.validate( aig.get_node( f1 ), false ) == false );
  
  /* considering 2 levels, f1 can be substituted with const 0 */
  ps.odc_levels = 2;
  CHECK( v.validate( aig.get_node( f1 ), false ) == true );
  CHECK( v.validate( aig.get_node( f1 ), aig.get_node( aig.get_constant( false ) ) ) == true );
}
