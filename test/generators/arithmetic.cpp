#include <catch.hpp>

#include <vector>

#include <mockturtle/traits.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>

#include <kitty/constructors.hpp>
#include <kitty/static_truth_table.hpp>

using namespace mockturtle;

/* simple truth table simulator */
template<typename Ntk>
inline std::vector<kitty::dynamic_truth_table> simulate( Ntk const& ntk )
{
  std::vector<kitty::dynamic_truth_table> sim( ntk.size(), kitty::dynamic_truth_table( ntk.num_pis() ) );

  ntk.foreach_pi( [&]( auto n, auto i ) {
    kitty::create_nth_var( sim[ntk.node_to_index( n )], i );
  } );

  ntk.foreach_gate( [&]( auto n ) {
    std::vector<kitty::dynamic_truth_table> values( ntk.fanin_size( n ) );
    ntk.foreach_fanin( n, [&]( auto s, auto i ) {
      values[i] = sim[ntk.get_node( s )];
    } );
    sim[n] = ntk.compute( n, values.begin(), values.end() );
  } );

  std::vector<kitty::dynamic_truth_table> pos;

  ntk.foreach_po( [&]( auto const& f ) {
    if ( ntk.is_complemented( f ) )
    {
      pos.push_back( ~sim[ntk.get_node( f )] );
    }
    else
    {
      pos.push_back( sim[ntk.get_node( f )] );
    }
  } );

  return pos;
}

TEST_CASE( "build a full adder with an AIG", "[arithmetic]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  auto [sum, carry] = full_adder( aig, a, b, c );

  aig.create_po( sum );
  aig.create_po( carry );

  const auto simm = simulate( aig );
  CHECK( simm.size() == 2 );
  CHECK( simm[0]._bits[0] == 0x96 );
  CHECK( simm[1]._bits[0] == 0xe8 );
}

TEST_CASE( "build a 2-bit adder with an AIG", "[arithmetic]" )
{
  aig_network aig;

  std::vector<aig_network::signal> a( 2 ), b( 2 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.create_pi();

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  CHECK( aig.num_pis() == 5 );
  CHECK( aig.num_pos() == 3 );
  CHECK( aig.num_gates() == 14 );

  const auto simm = simulate( aig );
  CHECK( simm.size() == 3 );
  CHECK( simm[0]._bits[0] == 0xa5a55a5a );
  CHECK( simm[1]._bits[0] == 0xc936936c );
  CHECK( simm[2]._bits[0] == 0xfec8ec80 );
}
