#include <catch.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/traits.hpp>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <kitty/operators.hpp>

using namespace mockturtle;

TEST_CASE( "create and use constants in an AIG", "[aig]" )
{
  aig_network aig;

  CHECK( aig.size() == 1 );
  CHECK( has_get_constant_v<aig_network> );
  CHECK( has_is_constant_v<aig_network> );
  CHECK( has_get_node_v<aig_network> );
  CHECK( has_is_complemented_v<aig_network> );

  const auto c0 = aig.get_constant( false );
  CHECK( aig.is_constant( aig.get_node( c0 ) ) );
  CHECK( !aig.is_pi( aig.get_node( c0 ) ) );

  CHECK( aig.size() == 1 );
  CHECK( std::is_same_v<std::decay_t<decltype( c0 )>, aig_network::signal> );
  CHECK( aig.get_node( c0 ) == 0 );
  CHECK( !aig.is_complemented( c0 ) );

  const auto c1 = aig.get_constant( true );

  CHECK( aig.get_node( c1 ) == 0 );
  CHECK( aig.is_complemented( c1 ) );

  CHECK( c0 != c1 );
  CHECK( c0 == !c1 );
  CHECK( !c0 == c1 );
  CHECK( !c0 != !c1 );
  CHECK( -c0 == c1 );
  CHECK( -c1 == c1 );
  CHECK( c0 == +c1 );
  CHECK( c0 == +c0 );
}

TEST_CASE( "create and use primary inputs in an AIG", "[aig]" )
{
  aig_network aig;

  CHECK( has_create_pi_v<aig_network> );

  auto a = aig.create_pi();

  CHECK( aig.size() == 2 );
  CHECK( aig.num_pis() == 1 );

  CHECK( std::is_same_v<std::decay_t<decltype( a )>, aig_network::signal> );

  CHECK( a.index == 1 );
  CHECK( a.complement == 0 );

  a = !a;

  CHECK( a.index == 1 );
  CHECK( a.complement == 1 );

  a = +a;

  CHECK( a.index == 1 );
  CHECK( a.complement == 0 );

  a = +a;

  CHECK( a.index == 1 );
  CHECK( a.complement == 0 );

  a = -a;

  CHECK( a.index == 1 );
  CHECK( a.complement == 1 );

  a = -a;

  CHECK( a.index == 1 );
  CHECK( a.complement == 1 );

  a = a ^ true;

  CHECK( a.index == 1 );
  CHECK( a.complement == 0 );

  a = a ^ true;

  CHECK( a.index == 1 );
  CHECK( a.complement == 1 );
}

TEST_CASE( "create and use primary outputs in an AIG", "[aig]" )
{
  aig_network aig;

  CHECK( has_create_po_v<aig_network> );

  const auto c0 = aig.get_constant( false );
  const auto x1 = aig.create_pi();

  CHECK( aig.size() == 2 );
  CHECK( aig.num_pis() == 1 );
  CHECK( aig.num_pos() == 0 );

  aig.create_po( c0 );
  aig.create_po( x1 );
  aig.create_po( !x1 );

  CHECK( aig.size() == 2 );
  CHECK( aig.num_pos() == 3 );

  aig.foreach_po( [&]( auto s, auto i ) {
    switch ( i )
    {
    case 0:
      CHECK( s == c0 );
      break;
    case 1:
      CHECK( s == x1 );
      break;
    case 2:
      CHECK( s == !x1 );
      break;
    }
  } );
}

TEST_CASE( "create unary operations in an AIG", "[aig]" )
{
  aig_network aig;

  CHECK( has_create_buf_v<aig_network> );
  CHECK( has_create_not_v<aig_network> );

  auto x1 = aig.create_pi();

  CHECK( aig.size() == 2 );

  auto f1 = aig.create_buf( x1 );
  auto f2 = aig.create_not( x1 );

  CHECK( aig.size() == 2 );
  CHECK( f1 == x1 );
  CHECK( f2 == !x1 );
}

TEST_CASE( "create binary operations in an AIG", "[aig]" )
{
  aig_network aig;

  CHECK( has_create_and_v<aig_network> );
  CHECK( has_create_nand_v<aig_network> );
  CHECK( has_create_or_v<aig_network> );
  CHECK( has_create_nor_v<aig_network> );
  CHECK( has_create_xor_v<aig_network> );

  const auto x1 = aig.create_pi();
  const auto x2 = aig.create_pi();

  CHECK( aig.size() == 3 );

  const auto f1 = aig.create_and( x1, x2 );
  CHECK( aig.size() == 4 );

  const auto f2 = aig.create_nand( x1, x2 );
  CHECK( aig.size() == 4 );
  CHECK( f1 == !f2 );

  const auto f3 = aig.create_or( x1, x2 );
  CHECK( aig.size() == 5 );

  const auto f4 = aig.create_nor( x1, x2 );
  CHECK( aig.size() == 5 );
  CHECK( f3 == !f4 );

  const auto f5 = aig.create_xor( x1, x2 );
  CHECK( aig.size() == 8 );
}

TEST_CASE( "hash nodes in AIG network", "[aig]" )
{
  aig_network aig;

  auto a = aig.create_pi();
  auto b = aig.create_pi();

  auto f = aig.create_and( a, b );
  auto g = aig.create_and( a, b );

  CHECK( aig.size() == 4u );
  CHECK( aig.num_gates() == 1u );

  CHECK( aig.get_node( f ) == aig.get_node( g ) );
}

TEST_CASE( "clone a node in AIG network", "[aig]" )
{
  aig_network aig1, aig2;

  CHECK( has_clone_node_v<aig_network> );

  auto a1 = aig1.create_pi();
  auto b1 = aig1.create_pi();
  auto f1 = aig1.create_and( a1, b1 );
  CHECK( aig1.size() == 4 );

  auto a2 = aig2.create_pi();
  auto b2 = aig2.create_pi();
  CHECK( aig2.size() == 3 );

  auto f2 = aig2.clone_node( aig1, aig1.get_node( f1 ), {a2, b2} );
  CHECK( aig2.size() == 4 );

  aig2.foreach_fanin( aig2.get_node( f2 ), [&]( auto const& s, auto ) {
    CHECK( !aig2.is_complemented( s ) );
  } );
}

TEST_CASE( "structural properties of an AIG", "[aig]" )
{
  aig_network aig;

  CHECK( has_size_v<aig_network> );
  CHECK( has_num_pis_v<aig_network> );
  CHECK( has_num_pos_v<aig_network> );
  CHECK( has_num_gates_v<aig_network> );
  CHECK( has_fanin_size_v<aig_network> );
  CHECK( has_fanout_size_v<aig_network> );

  const auto x1 = aig.create_pi();
  const auto x2 = aig.create_pi();

  const auto f1 = aig.create_and( x1, x2 );
  const auto f2 = aig.create_or( x1, x2 );

  aig.create_po( f1 );
  aig.create_po( f2 );

  CHECK( aig.size() == 5 );
  CHECK( aig.num_pis() == 2 );
  CHECK( aig.num_pos() == 2 );
  CHECK( aig.num_gates() == 2 );
  CHECK( aig.fanin_size( aig.get_node( x1 ) ) == 0 );
  CHECK( aig.fanin_size( aig.get_node( x2 ) ) == 0 );
  CHECK( aig.fanin_size( aig.get_node( f1 ) ) == 2 );
  CHECK( aig.fanin_size( aig.get_node( f2 ) ) == 2 );
  CHECK( aig.fanout_size( aig.get_node( x1 ) ) == 2 );
  CHECK( aig.fanout_size( aig.get_node( x2 ) ) == 2 );
  CHECK( aig.fanout_size( aig.get_node( f1 ) ) == 1 );
  CHECK( aig.fanout_size( aig.get_node( f2 ) ) == 1 );
}

TEST_CASE( "node and signal iteration in an AIG", "[aig]" )
{
  aig_network aig;

  CHECK( has_foreach_node_v<aig_network> );
  CHECK( has_foreach_pi_v<aig_network> );
  CHECK( has_foreach_po_v<aig_network> );
  CHECK( has_foreach_gate_v<aig_network> );
  CHECK( has_foreach_fanin_v<aig_network> );

  const auto x1 = aig.create_pi();
  const auto x2 = aig.create_pi();
  const auto f1 = aig.create_and( x1, x2 );
  const auto f2 = aig.create_or( x1, x2 );
  aig.create_po( f1 );
  aig.create_po( f2 );

  CHECK( aig.size() == 5 );

  /* iterate over nodes */
  uint32_t mask{0}, counter{0};
  aig.foreach_node( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; } );
  CHECK( mask == 31 );
  CHECK( counter == 10 );

  mask = 0;
  aig.foreach_node( [&]( auto n ) { mask |= ( 1 << n ); } );
  CHECK( mask == 31 );

  mask = counter = 0;
  aig.foreach_node( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; return false; } );
  CHECK( mask == 1 );
  CHECK( counter == 0 );

  mask = 0;
  aig.foreach_node( [&]( auto n ) { mask |= ( 1 << n ); return false; } );
  CHECK( mask == 1 );

  /* iterate over PIs */
  mask = counter = 0;
  aig.foreach_pi( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; } );
  CHECK( mask == 6 );
  CHECK( counter == 1 );

  mask = 0;
  aig.foreach_pi( [&]( auto n ) { mask |= ( 1 << n ); } );
  CHECK( mask == 6 );

  mask = counter = 0;
  aig.foreach_pi( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; return false; } );
  CHECK( mask == 2 );
  CHECK( counter == 0 );

  mask = 0;
  aig.foreach_pi( [&]( auto n ) { mask |= ( 1 << n ); return false; } );
  CHECK( mask == 2 );

  /* iterate over POs */
  mask = counter = 0;
  aig.foreach_po( [&]( auto s, auto i ) { mask |= ( 1 << aig.get_node( s ) ); counter += i; } );
  CHECK( mask == 24 );
  CHECK( counter == 1 );

  mask = 0;
  aig.foreach_po( [&]( auto s ) { mask |= ( 1 << aig.get_node( s ) ); } );
  CHECK( mask == 24 );

  mask = counter = 0;
  aig.foreach_po( [&]( auto s, auto i ) { mask |= ( 1 << aig.get_node( s ) ); counter += i; return false; } );
  CHECK( mask == 8 );
  CHECK( counter == 0 );

  mask = 0;
  aig.foreach_po( [&]( auto s ) { mask |= ( 1 << aig.get_node( s ) ); return false; } );
  CHECK( mask == 8 );

  /* iterate over gates */
  mask = counter = 0;
  aig.foreach_gate( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; } );
  CHECK( mask == 24 );
  CHECK( counter == 1 );

  mask = 0;
  aig.foreach_gate( [&]( auto n ) { mask |= ( 1 << n ); } );
  CHECK( mask == 24 );

  mask = counter = 0;
  aig.foreach_gate( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; return false; } );
  CHECK( mask == 8 );
  CHECK( counter == 0 );

  mask = 0;
  aig.foreach_gate( [&]( auto n ) { mask |= ( 1 << n ); return false; } );
  CHECK( mask == 8 );

  /* iterate over fanins */
  mask = counter = 0;
  aig.foreach_fanin( aig.get_node( f1 ), [&]( auto s, auto i ) { mask |= ( 1 << aig.get_node( s ) ); counter += i; } );
  CHECK( mask == 6 );
  CHECK( counter == 1 );

  mask = 0;
  aig.foreach_fanin( aig.get_node( f1 ), [&]( auto s ) { mask |= ( 1 << aig.get_node( s ) ); } );
  CHECK( mask == 6 );

  mask = counter = 0;
  aig.foreach_fanin( aig.get_node( f1 ), [&]( auto s, auto i ) { mask |= ( 1 << aig.get_node( s ) ); counter += i; return false; } );
  CHECK( mask == 2 );
  CHECK( counter == 0 );

  mask = 0;
  aig.foreach_fanin( aig.get_node( f1 ), [&]( auto s ) { mask |= ( 1 << aig.get_node( s ) ); return false; } );
  CHECK( mask == 2 );
}

TEST_CASE( "compute values in AIGs", "[aig]" )
{
  aig_network aig;

  CHECK( has_compute_v<aig_network, bool> );
  CHECK( has_compute_v<aig_network, kitty::dynamic_truth_table> );

  const auto x1 = aig.create_pi();
  const auto x2 = aig.create_pi();
  const auto f1 = aig.create_and( !x1, x2 );
  const auto f2 = aig.create_and( x1, !x2 );
  aig.create_po( f1 );
  aig.create_po( f2 );

  std::vector<bool> values{{true, false}};

  CHECK( aig.compute( aig.get_node( f1 ), values.begin(), values.end() ) == false );
  CHECK( aig.compute( aig.get_node( f2 ), values.begin(), values.end() ) == true );

  std::vector<kitty::dynamic_truth_table> xs{2, kitty::dynamic_truth_table( 2 )};
  kitty::create_nth_var( xs[0], 0 );
  kitty::create_nth_var( xs[1], 1 );

  CHECK( aig.compute( aig.get_node( f1 ), xs.begin(), xs.end() ) == ( ~xs[0] & xs[1] ) );
  CHECK( aig.compute( aig.get_node( f2 ), xs.begin(), xs.end() ) == ( xs[0] & ~xs[1] ) );
}

TEST_CASE( "custom node values in AIGs", "[aig]" )
{
  aig_network aig;

  CHECK( has_clear_values_v<aig_network> );
  CHECK( has_value_v<aig_network> );
  CHECK( has_set_value_v<aig_network> );
  CHECK( has_incr_value_v<aig_network> );
  CHECK( has_decr_value_v<aig_network> );

  const auto x1 = aig.create_pi();
  const auto x2 = aig.create_pi();
  const auto f1 = aig.create_and( x1, x2 );
  const auto f2 = aig.create_or( x1, x2 );
  aig.create_po( f1 );
  aig.create_po( f2 );

  CHECK( aig.size() == 5 );

  aig.clear_values();
  aig.foreach_node( [&]( auto n ) {
    CHECK( aig.value( n ) == 0 );
    aig.set_value( n, n );
    CHECK( aig.value( n ) == n );
    CHECK( aig.incr_value( n ) == n );
    CHECK( aig.value( n ) == n + 1 );
    CHECK( aig.decr_value( n ) == n );
    CHECK( aig.value( n ) == n );
  } );
  aig.clear_values();
  aig.foreach_node( [&]( auto n ) {
    CHECK( aig.value( n ) == 0 );
  } );
}

TEST_CASE( "visited values in AIGs", "[aig]" )
{
  aig_network aig;

  CHECK( has_clear_visited_v<aig_network> );
  CHECK( has_visited_v<aig_network> );
  CHECK( has_set_visited_v<aig_network> );

  const auto x1 = aig.create_pi();
  const auto x2 = aig.create_pi();
  const auto f1 = aig.create_and( x1, x2 );
  const auto f2 = aig.create_or( x1, x2 );
  aig.create_po( f1 );
  aig.create_po( f2 );

  CHECK( aig.size() == 5 );

  aig.clear_visited();
  aig.foreach_node( [&]( auto n ) {
    CHECK( aig.visited( n ) == 0 );
    aig.set_visited( n, n );
    CHECK( aig.visited( n ) == n );
  } );
  aig.clear_visited();
  aig.foreach_node( [&]( auto n ) {
    CHECK( aig.visited( n ) == 0 );
  } );
}
