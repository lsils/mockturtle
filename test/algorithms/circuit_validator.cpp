#include <catch.hpp>

#include <mockturtle/algorithms/circuit_validator.hpp>
#include <mockturtle/networks/aig.hpp>

using namespace mockturtle;

TEST_CASE( "Validator on AIGs", "[validator]" )
{
  /* original circuit */
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const f1 = aig.create_and( !a, b );
  auto const f2 = aig.create_and( a, !b );
  auto const f3 = aig.create_or( f1, f2 );
  aig.create_po( f3 );

  circuit_validator v( aig );
  
  auto const f4 = aig.create_xor( a, b );
  CHECK( v.validate( aig.get_node( f3 ), !f4 ) == true );

  CHECK( v.validate( aig.get_node( f1 ), f2 ) == false );
  CHECK( unsigned( v.cex[0] ) + unsigned( v.cex[1] ) == 1u ); /* either 01 or 10 */

  circuit_validator<aig_network>::gate::fanin fi1;
  fi1.idx = 0; fi1.inv = true;
  circuit_validator<aig_network>::gate::fanin fi2;
  fi2.idx = 1; fi2.inv = true;
  circuit_validator<aig_network>::gate g;
  g.fanins = {fi1, fi2};
  CHECK( v.validate( aig.get_node( f3 ), {aig.get_node(f1), aig.get_node(f2)}, {g}, false ) == true );
}

