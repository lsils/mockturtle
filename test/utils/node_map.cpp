#include <catch.hpp>

#include <cstdint>
#include <vector>

#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/utils/node_map.hpp>

using namespace mockturtle;

TEST_CASE( "create node map for full adder", "[node_map]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto [sum, carry] = full_adder( aig, a, b, c );

  aig.create_po( sum );
  aig.create_po( carry );

  node_map<uint32_t, aig_network> map( aig );

  aig.foreach_node( [&]( auto n, auto i ) {
    map[n] = i;
  } );

  uint32_t total{0};
  aig.foreach_node( [&]( auto n ) {
    total += map[n];
  } );

  CHECK( total == ( aig.size() * ( aig.size() - 1 ) ) / 2 );
}
