#include <catch.hpp>

#include <vector>

#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>

#include <kitty/constructors.hpp>
#include <kitty/static_truth_table.hpp>

using namespace mockturtle;

TEST_CASE( "build a full adder with an AIG", "[arithmetic]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  auto [sum, carry] = full_adder( aig, a, b, c );

  /* small truth table simulator */
  std::vector<kitty::static_truth_table<3>> sim( aig.size() );
  kitty::create_nth_var( sim[aig.get_node( a )], 0 );
  kitty::create_nth_var( sim[aig.get_node( b )], 1 );
  kitty::create_nth_var( sim[aig.get_node( c )], 2 );

  aig.foreach_gate( [&]( auto n ) {
    std::vector<kitty::static_truth_table<3>> values( aig.fanin_size( n ) );
    aig.foreach_fanin( n, [&]( auto s, auto i ) {
      values[i] = sim[aig.get_node( s )];
    } );
    sim[n] = aig.compute<3>( n, values.begin(), values.end() );
  } );

  CHECK( !aig.is_complemented( sum ) );
  CHECK( aig.is_complemented( carry ) );

  CHECK( sim[aig.get_node( sum )] == ( sim[aig.get_node( a )] ^ sim[aig.get_node( b )] ^ sim[aig.get_node( c )] ) );
  CHECK( sim[aig.get_node( carry )] == ~kitty::ternary_majority( sim[aig.get_node( a )], sim[aig.get_node( b )], sim[aig.get_node( c )] ) );
}
