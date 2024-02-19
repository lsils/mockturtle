#include <catch.hpp>

#include <vector>

#include <mockturtle/networks/block.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <kitty/operators.hpp>

using namespace mockturtle;

TEST_CASE( "create and use constants in a block network", "[block_net]" )
{
  block_network block_net;

  CHECK( has_size_v<block_network> );
  CHECK( has_get_constant_v<block_network> );
  CHECK( has_is_constant_v<block_network> );
  CHECK( has_is_pi_v<block_network> );
  CHECK( has_is_constant_v<block_network> );
  CHECK( has_get_node_v<block_network> );
  CHECK( has_is_complemented_v<block_network> );

  CHECK( block_net.size() == 2 );

  auto c0 = block_net.get_constant( false );
  auto c1 = block_net.get_constant( true );

  CHECK( block_net.size() == 2 );
  CHECK( c0 != c1 );
  CHECK( block_net.get_node( c0 ) == 0 );
  CHECK( block_net.get_node( c1 ) == 1 );
  CHECK( !block_net.is_complemented( c0 ) );
  CHECK( !block_net.is_complemented( c1 ) );
  CHECK( block_net.is_constant( block_net.get_node( c0 ) ) );
  CHECK( block_net.is_constant( block_net.get_node( c1 ) ) );
  CHECK( !block_net.is_pi( block_net.get_node( c1 ) ) );
  CHECK( !block_net.is_pi( block_net.get_node( c1 ) ) );
}

TEST_CASE( "create and use primary inputs in a block network", "[block_net]" )
{
  block_network block_net;

  CHECK( has_create_pi_v<block_network> );
  CHECK( has_is_constant_v<block_network> );
  CHECK( has_is_pi_v<block_network> );
  CHECK( has_num_pis_v<block_network> );

  CHECK( block_net.num_pis() == 0 );

  auto x1 = block_net.create_pi();
  auto x2 = block_net.create_pi();

  CHECK( block_net.size() == 4 );
  CHECK( block_net.num_pis() == 2 );
  CHECK( x1 != x2 );
}

TEST_CASE( "create and use primary outputs in a block network", "[block_net]" )
{
  block_network block_net;

  CHECK( has_create_po_v<block_network> );
  CHECK( has_num_pos_v<block_network> );

  auto c0 = block_net.get_constant( false );
  auto c1 = block_net.get_constant( true );
  auto x = block_net.create_pi();

  block_net.create_po( c0 );
  block_net.create_po( c1 );
  block_net.create_po( x );

  CHECK( block_net.size() == 3 );
  CHECK( block_net.num_pis() == 1 );
  CHECK( block_net.num_pos() == 3 );
}

TEST_CASE( "create unary operations in a block network", "[block_net]" )
{
  block_network block_net;

  CHECK( has_create_buf_v<block_network> );
  CHECK( has_create_not_v<block_network> );

  auto x1 = block_net.create_pi();

  CHECK( block_net.size() == 3 );

  auto f1 = block_net.create_buf( x1 );
  auto f2 = block_net.create_not( x1 );

  CHECK( block_net.size() == 5 );
  CHECK( f1 != x1 );
  CHECK( f2 != x1 );
}

TEST_CASE( "create binary operations in a block network", "[block_net]" )
{
  block_network block_net;

  CHECK( has_create_and_v<block_network> );

  const auto x1 = block_net.create_pi();
  const auto x2 = block_net.create_pi();

  CHECK( block_net.size() == 4 );

  block_net.create_and( x1, x2 );
  CHECK( block_net.size() == 5 );

  block_net.create_and( x1, x2 );
  CHECK( block_net.size() == 6 );

  block_net.create_and( x2, x1 );
  CHECK( block_net.size() == 7 );
}

TEST_CASE( "create multi-output operations in a block network", "[block_net]" )
{
  block_network block_net;

  const auto x1 = block_net.create_pi();
  const auto x2 = block_net.create_pi();
  const auto x3 = block_net.create_pi();

  CHECK( block_net.size() == 5 );

  block_net.create_ha( x1, x2 );
  CHECK( block_net.size() == 6 );

  block_net.create_fa( x1, x2, x3 );
  CHECK( block_net.size() == 7 );
}

TEST_CASE( "clone a block network", "[block_net]" )
{
  CHECK( has_clone_v<block_network> );

  block_network ntk1;
  auto a = ntk1.create_pi();
  auto b = ntk1.create_pi();
  auto f1 = ntk1.create_and( a, b );
  auto f2 = ntk1.create_ha( a, b );
  ntk1.create_po( f1 );
  ntk1.create_po( f2 );
  ntk1.create_po( ntk1.next_output_pin( f2 ) );
  CHECK( ntk1.size() == 6 );
  CHECK( ntk1.num_gates() == 2 );
  CHECK( ntk1.num_pos() == 3 );

  auto ntk2 = ntk1;
  auto ntk3 = ntk1.clone();

  auto c = ntk2.create_pi();
  auto f3 = ntk2.create_or( f2, c );
  ntk2.create_po( f3 );
  CHECK( ntk1.size() == 8 );
  CHECK( ntk1.num_gates() == 3 );
  CHECK( ntk1.num_pos() == 4 );

  CHECK( ntk3.size() == 7 );
  CHECK( ntk3.num_gates() == 3 );
  CHECK( ntk3.num_pos() == 3 );
}

TEST_CASE( "clone a node in a block network", "[block_net]" )
{
  block_network block_net1, block_net2;

  CHECK( has_clone_node_v<block_network> );

  auto a1 = block_net1.create_pi();
  auto b1 = block_net1.create_pi();
  auto f1 = block_net1.create_and( a1, b1 );
  auto f2 = block_net1.create_ha( a1, b1 );
  CHECK( block_net1.size() == 6 );

  auto a2 = block_net2.create_pi();
  auto b2 = block_net2.create_pi();
  CHECK( block_net2.size() == 4 );

  auto f3 = block_net2.clone_node( block_net1, block_net1.get_node( f1 ), { a2, b2 } );
  CHECK( block_net2.size() == 5 );
  CHECK( block_net2.num_outputs( block_net2.get_node( f3 ) ) == 1 );

  auto f4 = block_net2.clone_node( block_net1, block_net1.get_node( f2 ), { a2, b2 } );
  CHECK( block_net2.size() == 6 );
  CHECK( block_net2.num_outputs( block_net2.get_node( f4 ) ) == 2 );

  block_net2.foreach_fanin( block_net2.get_node( f3 ), [&]( auto const& s ) {
    CHECK( !block_net2.is_complemented( s ) );
  } );

  block_net2.foreach_fanin( block_net2.get_node( f4 ), [&]( auto const& s ) {
    CHECK( !block_net2.is_complemented( s ) );
  } );
}

TEST_CASE( "No hash nodes in block network", "[block_net]" )
{
  block_network block_net;

  const auto a = block_net.create_pi();
  const auto b = block_net.create_pi();
  const auto c = block_net.create_pi();

  kitty::dynamic_truth_table tt_maj( 3u ), tt_xor( 3u );
  kitty::create_from_hex_string( tt_maj, "e8" );
  kitty::create_from_hex_string( tt_xor, "96" );

  block_net.create_node( { a, b, c }, tt_maj );
  block_net.create_node( { a, b, c }, tt_xor );

  CHECK( block_net.size() == 7 );

  block_net.create_node( { a, b, c }, tt_maj );

  CHECK( block_net.size() == 8 );
}

TEST_CASE( "structural properties of a block network", "[block_net]" )
{
  block_network block_net;

  CHECK( has_size_v<block_network> );
  CHECK( has_num_pis_v<block_network> );
  CHECK( has_num_pos_v<block_network> );
  CHECK( has_num_gates_v<block_network> );
  CHECK( has_fanin_size_v<block_network> );
  CHECK( has_fanout_size_v<block_network> );

  const auto x1 = block_net.create_pi();
  const auto x2 = block_net.create_pi();

  const auto f1 = block_net.create_and( x1, x2 );
  const auto f2 = block_net.create_and( x2, x1 );

  block_net.create_po( f1 );
  block_net.create_po( f2 );

  CHECK( block_net.size() == 6 );
  CHECK( block_net.num_pis() == 2 );
  CHECK( block_net.num_pos() == 2 );
  CHECK( block_net.num_gates() == 2 );
  CHECK( block_net.fanin_size( block_net.get_node( x1 ) ) == 0 );
  CHECK( block_net.fanin_size( block_net.get_node( x2 ) ) == 0 );
  CHECK( block_net.fanin_size( block_net.get_node( f1 ) ) == 2 );
  CHECK( block_net.fanin_size( block_net.get_node( f2 ) ) == 2 );
  CHECK( block_net.fanout_size( block_net.get_node( x1 ) ) == 2 );
  CHECK( block_net.fanout_size( block_net.get_node( x2 ) ) == 2 );
  CHECK( block_net.fanout_size( block_net.get_node( f1 ) ) == 1 );
  CHECK( block_net.fanout_size( block_net.get_node( f2 ) ) == 1 );
}

TEST_CASE( "node and signal iteration in a block network", "[block_net]" )
{
  block_network block_net;

  CHECK( has_foreach_node_v<block_network> );
  CHECK( has_foreach_pi_v<block_network> );
  CHECK( has_foreach_po_v<block_network> );
  CHECK( has_foreach_fanin_v<block_network> );

  const auto x1 = block_net.create_pi();
  const auto x2 = block_net.create_pi();
  const auto f1 = block_net.create_ha( x1, x2 );
  const auto f2 = block_net.create_and( x2, x1 );
  block_net.create_po( f1 );
  block_net.create_po( f2 );

  CHECK( block_net.size() == 6 );

  /* iterate over nodes */
  uint32_t mask{ 0 }, counter{ 0 };
  block_net.foreach_node( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; } );
  CHECK( mask == 63 );
  CHECK( counter == 15 );

  mask = 0;
  block_net.foreach_node( [&]( auto n ) { mask |= ( 1 << n ); } );
  CHECK( mask == 63 );

  mask = counter = 0;
  block_net.foreach_node( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; return false; } );
  CHECK( mask == 1 );
  CHECK( counter == 0 );

  mask = 0;
  block_net.foreach_node( [&]( auto n ) { mask |= ( 1 << n ); return false; } );
  CHECK( mask == 1 );

  /* iterate over PIs */
  mask = counter = 0;
  block_net.foreach_pi( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; } );
  CHECK( mask == 12 );
  CHECK( counter == 1 );

  mask = 0;
  block_net.foreach_pi( [&]( auto n ) { mask |= ( 1 << n ); } );
  CHECK( mask == 12 );

  mask = counter = 0;
  block_net.foreach_pi( [&]( auto n, auto i ) { mask |= ( 1 << n ); counter += i; return false; } );
  CHECK( mask == 4 );
  CHECK( counter == 0 );

  mask = 0;
  block_net.foreach_pi( [&]( auto n ) { mask |= ( 1 << n ); return false; } );
  CHECK( mask == 4 );

  /* iterate over POs */
  mask = counter = 0;
  block_net.foreach_po( [&]( auto s, auto i ) { mask |= ( 1 << block_net.get_node( s ) ); counter += i; } );
  CHECK( mask == 48 );
  CHECK( counter == 1 );

  mask = 0;
  block_net.foreach_po( [&]( auto s ) { mask |= ( 1 << block_net.get_node( s ) ); } );
  CHECK( mask == 48 );

  mask = counter = 0;
  block_net.foreach_po( [&]( auto s, auto i ) { mask |= ( 1 << block_net.get_node( s ) ); counter += i; return false; } );
  CHECK( mask == 16 );
  CHECK( counter == 0 );

  mask = 0;
  block_net.foreach_po( [&]( auto s ) { mask |= ( 1 << block_net.get_node( s ) ); return false; } );
  CHECK( mask == 16 );
}

TEST_CASE( "custom node values in block networks", "[block_net]" )
{
  block_network block_net;

  CHECK( has_clear_values_v<block_network> );
  CHECK( has_value_v<block_network> );
  CHECK( has_set_value_v<block_network> );
  CHECK( has_incr_value_v<block_network> );
  CHECK( has_decr_value_v<block_network> );

  const auto x1 = block_net.create_pi();
  const auto x2 = block_net.create_pi();
  const auto f1 = block_net.create_and( x1, x2 );
  const auto f2 = block_net.create_and( x2, x1 );
  block_net.create_po( f1 );
  block_net.create_po( f2 );

  CHECK( block_net.size() == 6 );

  block_net.clear_values();
  block_net.foreach_node( [&]( auto n ) {
    CHECK( block_net.value( n ) == 0 );
    block_net.set_value( n, static_cast<uint32_t>( n ) );
    CHECK( block_net.value( n ) == n );
    CHECK( block_net.incr_value( n ) == n );
    CHECK( block_net.value( n ) == n + 1 );
    CHECK( block_net.decr_value( n ) == n );
    CHECK( block_net.value( n ) == n );
  } );
  block_net.clear_values();
  block_net.foreach_node( [&]( auto n ) {
    CHECK( block_net.value( n ) == 0 );
  } );
}

TEST_CASE( "visited values in block networks", "[block_net]" )
{
  block_network block_net;

  CHECK( has_clear_visited_v<block_network> );
  CHECK( has_visited_v<block_network> );
  CHECK( has_set_visited_v<block_network> );

  const auto x1 = block_net.create_pi();
  const auto x2 = block_net.create_pi();
  const auto f1 = block_net.create_and( x1, x2 );
  const auto f2 = block_net.create_and( x2, x1 );
  block_net.create_po( f1 );
  block_net.create_po( f2 );

  CHECK( block_net.size() == 6 );

  block_net.clear_visited();
  block_net.foreach_node( [&]( auto n ) {
    CHECK( block_net.visited( n ) == 0 );
    block_net.set_visited( n, static_cast<uint32_t>( n ) );
    CHECK( block_net.visited( n ) == n );
  } );
  block_net.clear_visited();
  block_net.foreach_node( [&]( auto n ) {
    CHECK( block_net.visited( n ) == 0 );
  } );
}

TEST_CASE( "Multi-output functions in block networks", "[block_net]" )
{
  block_network block_net;

  CHECK( has_clear_visited_v<block_network> );
  CHECK( has_visited_v<block_network> );
  CHECK( has_set_visited_v<block_network> );

  const auto x1 = block_net.create_pi();
  const auto x2 = block_net.create_pi();
  const auto x3 = block_net.create_pi();
  const auto f1 = block_net.create_ha( x1, x2 );
  const auto f2 = block_net.create_fa( x1, x2, x3 );
  block_net.create_po( f1 );
  block_net.create_po( block_net.next_output_pin( f1 ) );
  block_net.create_po( f2 );
  block_net.create_po( block_net.next_output_pin( f2 ) );

  CHECK( block_net.size() == 7 );

  CHECK( block_net.get_node( f1 ) == 5 );
  CHECK( block_net.get_node( block_net.next_output_pin( f1 ) ) == 5 );
  CHECK( block_net.get_node( f2 ) == 6 );
  CHECK( block_net.get_node( block_net.next_output_pin( f2 ) ) == 6 );
  CHECK( block_net.num_outputs( 5 ) == 2 );
  CHECK( block_net.num_outputs( 6 ) == 2 );
  CHECK( block_net.is_multioutput( 5 ) == true );
  CHECK( block_net.is_multioutput( 6 ) == true );
  CHECK( block_net.node_function_pin( 5, 0 )._bits[0] == 0x8 );
  CHECK( block_net.node_function_pin( 5, 1 )._bits[0] == 0x6 );
  CHECK( block_net.node_function_pin( 6, 0 )._bits[0] == 0xe8 );
  CHECK( block_net.node_function_pin( 6, 1 )._bits[0] == 0x96 );

  const auto f3 = block_net.create_and( x1, x2 );
  CHECK( block_net.get_node( f3 ) == 7 );
  CHECK( block_net.num_outputs( 7 ) == 1 );
  CHECK( block_net.is_multioutput( 7 ) == false );
  CHECK( block_net.node_function_pin( 7, 0 )._bits[0] == 0x8 );
}

TEST_CASE( "substitute node by another in a block network", "[block_net]" )
{
  block_network block_net;

  const auto c0 = block_net.get_node( block_net.get_constant( false ) );
  const auto c1 = block_net.get_node( block_net.get_constant( true ) );
  const auto a = block_net.create_pi();
  const auto b = block_net.create_pi();

  // XOR with NAND
  const auto n1 = block_net.create_nand( a, b );
  const auto n2 = block_net.create_le( a, n1 );
  const auto n3 = block_net.create_lt( b, n1 );
  const auto n4 = block_net.create_ha( n2, n3 );
  const auto n5 = block_net.create_or( n3, n4 );
  const auto n6 = block_net.create_xor( a, b );
  const auto po = block_net.po_at( block_net.create_po( block_net.next_output_pin( n4 ) ) );

  CHECK( po.index == 7 );
  CHECK( po.complement == 0 );
  CHECK( po.output == 1 );

  std::vector<node<block_network>> nodes;
  block_net.foreach_node( [&]( auto node ) { nodes.push_back( node ); CHECK( !block_net.is_dead( node ) ); } );

  std::vector<block_network::node> node_ref = { block_net.get_node( c0 ),
                                                block_net.get_node( c1 ),
                                                block_net.get_node( a ),
                                                block_net.get_node( b ),
                                                block_net.get_node( n1 ),
                                                block_net.get_node( n2 ),
                                                block_net.get_node( n3 ),
                                                block_net.get_node( n4 ),
                                                block_net.get_node( n5 ),
                                                block_net.get_node( n6 )
                                              };

  CHECK( nodes == node_ref );
  CHECK( block_net.fanout_size( block_net.get_node( n4 ) ) == 2 );
  CHECK( block_net.fanout_size_pin( block_net.get_node( n4 ), 0 ) == 1 );
  CHECK( block_net.fanout_size_pin( block_net.get_node( n4 ), 1 ) == 1 );
  CHECK( block_net.fanout_size( block_net.get_node( n2 ) ) == 1 );
  CHECK( block_net.fanout_size_pin( block_net.get_node( n2 ), 0 ) == 1 );

  block_net.foreach_po( [&]( auto f ) {
    CHECK( block_net.get_node( f ) == block_net.get_node( n4 ) );
    CHECK( block_net.get_output_pin( f ) == 1 );
  } );  

  // substitute nodes
  block_net.substitute_node( block_net.get_node( n4 ), n6 );

  block_net.is_dead( block_net.get_node( n4 ) == false );
  block_net.is_dead( block_net.get_node( n2 ) == false );
  CHECK( block_net.size() == 10 );
  CHECK( block_net.fanout_size( block_net.get_node( n4 ) ) == 0 );
  CHECK( block_net.fanout_size_pin( block_net.get_node( n4 ), 0 ) == 0 );
  CHECK( block_net.fanout_size_pin( block_net.get_node( n4 ), 1 ) == 0 );
  CHECK( block_net.fanout_size( block_net.get_node( n6 ) ) == 2 );
  CHECK( block_net.fanout_size_pin( block_net.get_node( n6 ), 0 ) == 2 );

  CHECK( block_net.is_dead( block_net.get_node( n4 ) ) );
  CHECK( block_net.is_dead( block_net.get_node( n2 ) ) );
  CHECK( block_net.fanout_size( block_net.get_node( n2 ) ) == 0 );
  CHECK( block_net.fanout_size_pin( block_net.get_node( n2 ), 0 ) == 0 );
  block_net.foreach_po( [&]( auto f ) {
    CHECK( block_net.get_node( f ) == block_net.get_node( n6 ) );
    CHECK( block_net.get_output_pin( f ) == 0 );
  } );
}
