#include <catch.hpp>
#include <vector>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>
#include <kitty/static_truth_table.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/views/names_view.hpp>

using namespace mockturtle;

template<class Ntk>
void test_cleanup_network()
{
  Ntk ntk;

  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  const auto f1 = ntk.create_nand( a, b );
  const auto f2 = ntk.create_nand( a, f1 );
  const auto f3 = ntk.create_nand( b, f1 );
  ntk.create_nand( f2, f3 );

  CHECK( ntk.size() == 7 );

  const auto ntk2 = cleanup_dangling( ntk );

  CHECK( ntk2.size() == 3 );
}

template<class NtkSource, class NtkDest>
void test_cleanup_into_network()
{
  NtkSource ntk;

  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  const auto f1 = ntk.create_xor( a, b );
  const auto f2 = ntk.create_nand( a, f1 );
  const auto f3 = ntk.create_nand( b, f1 );
  ntk.create_po( ntk.create_maj( f1, f2, f3 ) );

  NtkDest dest;
  const auto x1 = dest.create_pi();
  const auto x2 = dest.create_pi();
  std::vector<typename NtkDest::signal> pis = {x1, x2};
  dest.create_po( cleanup_dangling( ntk, dest, pis.begin(), pis.end() )[0] );

  CHECK( simulate<kitty::static_truth_table<2u>>( ntk )[0] == simulate<kitty::static_truth_table<2u>>( dest )[0] );
}

TEST_CASE( "cleanup networks without PO", "[cleanup]" )
{
  test_cleanup_network<aig_network>();
  test_cleanup_network<xag_network>();
  test_cleanup_network<mig_network>();
  test_cleanup_network<xmg_network>();
}

TEST_CASE( "cleanup network with registers", "[cleanup]" )
{
  aig_network ntk;
  const auto pi = ntk.create_pi();
  const auto ro0 = ntk.create_ro();
  const auto ro1 = ntk.create_ro();

  ntk.create_ri( ntk.create_and( pi, ro0 ) );
  ntk.create_ri( ntk.create_and( pi, ro1 ) );

  CHECK( 1u == ntk.num_pis() );
  CHECK( 2u == ntk.num_cos() );
  CHECK( 0u == ntk.num_pos() );
  CHECK( 3u == ntk.num_cis() );
  ntk._storage->latch_information[ntk.get_node(ro0)].control = "s";
  ntk._storage->latch_information[ntk.get_node(ro0)].init = 1;
  ntk._storage->latch_information[ntk.get_node(ro0)].type = "t";

  ntk._storage->latch_information[ntk.get_node(ro1)].control = "u";
  ntk._storage->latch_information[ntk.get_node(ro1)].init = 0;
  ntk._storage->latch_information[ntk.get_node(ro1)].type = "v";

  ntk = cleanup_dangling( ntk );

  CHECK( 1u == ntk.num_pis() );
  CHECK( 2u == ntk.num_cos() );
  CHECK( 0u == ntk.num_pos() );
  CHECK( 3u == ntk.num_cis() );
  CHECK( ntk._storage->latch_information[ntk.get_node(ro0)].control == "s");
  CHECK( ntk._storage->latch_information[ntk.get_node(ro0)].init == 1);
  CHECK( ntk._storage->latch_information[ntk.get_node(ro0)].type == "t");

  CHECK( ntk._storage->latch_information[ntk.get_node(ro1)].control == "u");
  CHECK( ntk._storage->latch_information[ntk.get_node(ro1)].init == 0);
  CHECK( ntk._storage->latch_information[ntk.get_node(ro1)].type == "v");
}

TEST_CASE( "cleanup network with names", "[cleanup]" )
{
  names_view<aig_network> ntk_orig;
  ntk_orig.set_network_name( "network" );
  const auto ro0 = ntk_orig.create_ro();
  ntk_orig.set_name(ro0, "ro0");
  const auto pi = ntk_orig.create_pi();
  ntk_orig.set_name(pi, "pi");
  const auto ro1 = ntk_orig.create_ro();
  ntk_orig.set_name(ro1, "ro1");
  const auto nand2 = ntk_orig.create_nand( pi, ro0 );
  ntk_orig.set_name(nand2, "nand2");
  const auto and2 = ntk_orig.create_and( pi, ro1 );
  ntk_orig.set_name(and2, "and2");
  const auto inv = ntk_orig.create_not( pi );
  ntk_orig.set_name(inv, "inv");
  const uint32_t ri0 = ntk_orig.create_ri( nand2, 1 );
  ntk_orig.set_output_name(ri0, "ri0");
  const uint32_t po0 = ntk_orig.create_po( inv );
  ntk_orig.set_output_name(po0, "po0");
  const uint32_t ri1 = ntk_orig.create_ri( and2, 1 );
  ntk_orig.set_output_name(ri1, "ri1");
  const uint32_t po1 = ntk_orig.create_po( ro0 );
  ntk_orig.set_output_name(po1, "po1");

  names_view<aig_network> ntk = cleanup_dangling( ntk_orig );

  CHECK( ntk.get_network_name() == "network" );
  CHECK( ntk.has_name( pi ) );
  CHECK( ntk.get_name( pi ) == "pi" );
  CHECK( ntk.has_name( ro0 ) );
  CHECK( ntk.get_name( ro0 ) == "ro0" );
  CHECK( ntk.has_name( ro1 ) );
  CHECK( ntk.get_name( ro1 ) == "ro1" );
  CHECK( ntk.has_name( and2 ) );
  CHECK( ntk.get_name( and2 ) == "and2" );
  CHECK( ntk.has_name( nand2 ) );
  CHECK( ntk.get_name( nand2 ) == "nand2" );
  CHECK( ntk.has_name( inv ) );
  CHECK( ntk.get_name( inv ) == "inv" );
  CHECK( ntk.has_output_name( ri0 ) );
  CHECK( ntk.get_output_name( ri0 ) == "ri0" );
  CHECK( ntk.has_output_name( po0 ) );
  CHECK( ntk.get_output_name( po0 ) == "po0" );
  CHECK( ntk.has_output_name( ri1 ) );
  CHECK( ntk.get_output_name( ri1 ) == "ri1" );
  CHECK( ntk.has_output_name( po1 ) );
  CHECK( ntk.get_output_name( po1 ) == "po1" );
}

TEST_CASE( "cleanup networks with different types", "[cleanup]" )
{
  test_cleanup_into_network<aig_network, xag_network>();
  test_cleanup_into_network<xag_network, aig_network>();
  test_cleanup_into_network<aig_network, mig_network>();
  test_cleanup_into_network<mig_network, aig_network>();

  test_cleanup_into_network<aig_network, klut_network>();
  test_cleanup_into_network<xag_network, klut_network>();
  test_cleanup_into_network<mig_network, klut_network>();
}

TEST_CASE( "cleanup LUT network with too large AND gate", "[cleanup]" )
{
  klut_network ntk;
  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();
  const auto c = ntk.create_pi();

  kitty::dynamic_truth_table func( 3u );
  kitty::create_from_binary_string( func, "10100000" ); /* a AND c */
  ntk.create_po( ntk.create_node( {a, b, c}, func ) );

  CHECK( 3u == ntk.num_pis() );
  CHECK( 1u == ntk.num_pos() );
  CHECK( 1u == ntk.num_gates() );
  CHECK( 6u == ntk.size() );
  ntk.foreach_gate( [&]( auto const& n ) {
    CHECK( 3u == ntk.fanin_size( n ) );
  } );

  ntk = cleanup_luts( ntk );

  CHECK( 3u == ntk.num_pis() );
  CHECK( 1u == ntk.num_pos() );
  CHECK( 1u == ntk.num_gates() );
  CHECK( 6u == ntk.size() );
  ntk.foreach_gate( [&]( auto const& n ) {
    CHECK( 2u == ntk.fanin_size( n ) );
    CHECK( 0b1000 == *( ntk.node_function( n ).begin() ) );
  } );
}

TEST_CASE( "cleanup LUT network with implicit projection", "[cleanup]" )
{
  klut_network ntk;
  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  kitty::dynamic_truth_table func( 2u );
  kitty::create_from_binary_string( func, "1100" ); /* b */
  ntk.create_po( ntk.create_node( {a, b}, func ) );

  CHECK( 2u == ntk.num_pis() );
  CHECK( 1u == ntk.num_pos() );
  CHECK( 1u == ntk.num_gates() );
  CHECK( 5u == ntk.size() );
  ntk.foreach_gate( [&]( auto const& n ) {
    CHECK( 2u == ntk.fanin_size( n ) );
  } );

  ntk = cleanup_luts( ntk );

  CHECK( 2u == ntk.num_pis() );
  CHECK( 1u == ntk.num_pos() );
  CHECK( 0u == ntk.num_gates() );
  CHECK( 4u == ntk.size() );
  ntk.foreach_po( [&]( auto const& f ) {
    CHECK( b == f );
  });
}

TEST_CASE( "cleanup LUT network with implicit constant", "[cleanup]" )
{
  klut_network ntk;
  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();
  const auto c = ntk.create_pi();
  const auto d = ntk.create_pi();
  const auto e = ntk.create_pi();

  kitty::dynamic_truth_table func( 5u );
  ntk.create_po( ntk.create_node( {a, b, c, d, e}, func ) );
  ntk.create_po( ntk.create_node( {a, b, c, d, e}, ~func ) );

  CHECK( 5u == ntk.num_pis() );
  CHECK( 2u == ntk.num_pos() );
  CHECK( 2u == ntk.num_gates() );
  CHECK( 9u == ntk.size() );
  ntk.foreach_gate( [&]( auto const& n ) {
    CHECK( 5u == ntk.fanin_size( n ) );
  } );

  ntk = cleanup_luts( ntk );

  CHECK( 5u == ntk.num_pis() );
  CHECK( 2u == ntk.num_pos() );
  CHECK( 0u == ntk.num_gates() );
  CHECK( 7u == ntk.size() );
  ntk.foreach_po( [&]( auto const& f, auto i ) {
    CHECK( ntk.get_constant( i == 1 ) == f );
  });
}

TEST_CASE( "cleanup LUT network with constant propagation", "[cleanup]" )
{
  klut_network ntk;
  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  ntk.create_po( ntk.create_maj( a, ntk.get_constant( false ), b ) );
  ntk.create_po( ntk.create_maj( a, ntk.get_constant( true ), b ) );

  CHECK( 2u == ntk.num_pis() );
  CHECK( 2u == ntk.num_pos() );
  CHECK( 2u == ntk.num_gates() );
  CHECK( 6u == ntk.size() );
  ntk.foreach_gate( [&]( auto const& n ) {
    CHECK( 3u == ntk.fanin_size( n ) );
  } );

  ntk = cleanup_luts( ntk );

  CHECK( 2u == ntk.num_pis() );
  CHECK( 2u == ntk.num_pos() );
  CHECK( 2u == ntk.num_gates() );
  CHECK( 6u == ntk.size() );
  ntk.foreach_gate( [&]( auto const& n, auto i ) {
    CHECK( 2u == ntk.fanin_size( n ) );
    CHECK( ( i == 0 ? 0b1000 : 0b1110 ) == *( ntk.node_function( n ).begin() ) );
  });
}

TEST_CASE( "cleanup LUT network with nested constant propagation", "[cleanup]" )
{
  klut_network ntk;
  const auto a = ntk.create_pi();

  kitty::dynamic_truth_table func( 5u );
  const auto f = ntk.create_not( ntk.get_constant( true ) );
  ntk.create_po( ntk.create_and( a, f ) );
  ntk.create_po( ntk.create_and( a, ntk.create_not( f ) ) );

  CHECK( 1u == ntk.num_pis() );
  CHECK( 2u == ntk.num_pos() );
  CHECK( 4u == ntk.num_gates() );
  CHECK( 7u == ntk.size() );

  ntk = cleanup_luts( ntk );

  CHECK( 1u == ntk.num_pis() );
  CHECK( 2u == ntk.num_pos() );
  CHECK( 0u == ntk.num_gates() );
  CHECK( 3u == ntk.size() );
  ntk.foreach_po( [&]( auto const& f, auto i ) {
    CHECK( ( i == 0 ? ntk.get_constant( false ) : a ) == f );
  });
}
