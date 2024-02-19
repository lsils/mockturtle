#include <catch.hpp>
#include <vector>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>
#include <kitty/static_truth_table.hpp>
#include <mockturtle/algorithms/experimental/decompose_multioutput.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/block.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/sequential.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/dont_touch_view.hpp>
#include <mockturtle/views/names_view.hpp>

using namespace mockturtle;

template<class Ntk>
void test_decompose_multioutput_single()
{
  block_network ntk;

  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  const auto f1 = ntk.create_nand( a, b );
  const auto f2 = ntk.create_nand( a, f1 );
  const auto f3 = ntk.create_nand( b, f1 );
  ntk.create_nand( f2, f3 );

  CHECK( ntk.size() == 8 );

  const Ntk ntk2 = decompose_multioutput<block_network, Ntk>( ntk );

  CHECK( ntk2.size() == 3 );
}

template<class Ntk>
Ntk test_decompose_multioutput( bool set_dont_touch = false )
{
  block_network ntk;
  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();
  const auto c = ntk.create_pi();
  const auto d = ntk.create_pi();

  const auto ha = ntk.create_ha( a, b );
  const auto fa = ntk.create_fa( c, d, ha );

  ntk.create_po( ntk.next_output_pin( ha ) );
  ntk.create_po( ntk.next_output_pin( fa ) );

  CHECK( 4u == ntk.num_pis() );
  CHECK( 2u == ntk.num_pos() );
  CHECK( 2u == ntk.num_gates() );
  CHECK( 8u == ntk.size() );

  decompose_multioutput_params ps;
  ps.set_multioutput_as_dont_touch = set_dont_touch;
  return decompose_multioutput<block_network, Ntk>( ntk, ps );
}

TEST_CASE( "decompose multioutput without PO", "[decompose_multioutput]" )
{
  test_decompose_multioutput_single<aig_network>();
  test_decompose_multioutput_single<xag_network>();
  test_decompose_multioutput_single<mig_network>();
  test_decompose_multioutput_single<xmg_network>();
}

TEST_CASE( "decompose multioutput with adders AIG", "[decompose_multioutput]" )
{
  aig_network aig = test_decompose_multioutput<aig_network>();

  CHECK( aig.num_pis() == 4 );
  CHECK( aig.num_pos() == 2 );
  CHECK( aig.num_gates() == 14 );
  CHECK( aig.size() == 19 );
}

TEST_CASE( "decompose multioutput with adders XAG", "[decompose_multioutput]" )
{
  xag_network xag = test_decompose_multioutput<xag_network>();

  CHECK( xag.num_pis() == 4 );
  CHECK( xag.num_pos() == 2 );
  CHECK( xag.num_gates() == 7 );
  CHECK( xag.size() == 12 );
}

TEST_CASE( "decompose multioutput with adders MIG", "[decompose_multioutput]" )
{
  mig_network mig = test_decompose_multioutput<mig_network>();

  CHECK( mig.num_pis() == 4 );
  CHECK( mig.num_pos() == 2 );
  CHECK( mig.num_gates() == 8 );
  CHECK( mig.size() == 13 );
}

TEST_CASE( "decompose multioutput with adders XMG", "[decompose_multioutput]" )
{
  xmg_network xmg = test_decompose_multioutput<xmg_network>();

  CHECK( xmg.num_pis() == 4 );
  CHECK( xmg.num_pos() == 2 );
  CHECK( xmg.num_gates() == 4 );
  CHECK( xmg.size() == 9 );
}

TEST_CASE( "decompose multioutput with adders", "[decompose_multioutput]" )
{
  block_network ntk = test_decompose_multioutput<block_network>();

  CHECK( ntk.num_pis() == 4 );
  CHECK( ntk.num_pos() == 2 );
  CHECK( ntk.num_gates() == 4 );
  CHECK( ntk.size() == 10 );
}

TEST_CASE( "decompose multioutput with adders don't touch", "[decompose_multioutput]" )
{
  block_network ntk;
  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();
  const auto c = ntk.create_pi();
  const auto d = ntk.create_pi();

  const auto ha = ntk.create_ha( a, b );
  const auto fa = ntk.create_fa( c, d, ha );
  const auto f = ntk.create_and( ha, fa );

  ntk.create_po( ntk.next_output_pin( ha ) );
  ntk.create_po( ntk.next_output_pin( fa ) );
  ntk.create_po( f );

  CHECK( ntk.num_pis() == 4 );
  CHECK( ntk.num_pos() == 3 );
  CHECK( ntk.num_gates() == 3 );
  CHECK( ntk.size() == 9 );

  decompose_multioutput_params ps;
  ps.set_multioutput_as_dont_touch = true;
  dont_touch_view<block_network> res = decompose_multioutput<block_network, dont_touch_view<block_network>>( ntk, ps );

  CHECK( res.num_pis() == 4 );
  CHECK( res.num_pos() == 3 );
  CHECK( res.num_gates() == 5 );
  CHECK( res.size() == 11 );

  std::vector<block_network::node> gates;
  res.foreach_gate( [&]( auto const& g ) {
    gates.push_back( g );
  } );

  for ( uint32_t i = 0; i < gates.size() - 1; ++i )
  {
    CHECK( res.is_dont_touch( gates[i] ) == true );
  }
  CHECK( res.is_dont_touch( gates.back() ) == false );
}

TEST_CASE( "decompose multioutput network with names", "[decompose_multioutput]" )
{
  names_view<block_network> ntk_orig;
  ntk_orig.set_network_name( "network" );
  const auto pi0 = ntk_orig.create_pi();
  ntk_orig.set_name( pi0, "pi0" );
  const auto pi1 = ntk_orig.create_pi();
  ntk_orig.set_name( pi1, "pi1" );
  const auto pi2 = ntk_orig.create_pi();
  ntk_orig.set_name( pi2, "pi2" );
  const auto nand2 = ntk_orig.create_nand( pi1, pi0 );
  ntk_orig.set_name( nand2, "nand2" );
  const auto and2 = ntk_orig.create_and( pi1, pi2 );
  ntk_orig.set_name( and2, "and2" );
  const auto inv = ntk_orig.create_not( pi1 );
  ntk_orig.set_name( inv, "inv" );
  ntk_orig.create_po( nand2 );
  ntk_orig.set_output_name( 0, "po0" );
  ntk_orig.create_po( and2 );
  ntk_orig.set_output_name( 1, "po1" );
  ntk_orig.create_po( inv );
  ntk_orig.set_output_name( 2, "po2" );
  ntk_orig.create_po( pi0 );
  ntk_orig.set_output_name( 3, "po3" );

  names_view<block_network> ntk = decompose_multioutput<names_view<block_network>>( ntk_orig );

  CHECK( ntk.get_network_name() == "network" );
  CHECK( ntk.has_name( pi0 ) );
  CHECK( ntk.get_name( pi0 ) == "pi0" );
  CHECK( ntk.has_name( pi1 ) );
  CHECK( ntk.get_name( pi1 ) == "pi1" );
  CHECK( ntk.has_name( pi2 ) );
  CHECK( ntk.get_name( pi2 ) == "pi2" );
  CHECK( ntk.has_name( and2 ) );
  CHECK( ntk.get_name( and2 ) == "and2" );
  CHECK( ntk.has_name( nand2 ) );
  CHECK( ntk.get_name( nand2 ) == "nand2" );
  CHECK( ntk.has_name( inv ) );
  CHECK( ntk.get_name( inv ) == "inv" );
  CHECK( ntk.has_output_name( 0 ) );
  CHECK( ntk.get_output_name( 0 ) == "po0" );
  CHECK( ntk.has_output_name( 1 ) );
  CHECK( ntk.get_output_name( 1 ) == "po1" );
  CHECK( ntk.has_output_name( 2 ) );
  CHECK( ntk.get_output_name( 2 ) == "po2" );
  CHECK( ntk.has_output_name( 3 ) );
  CHECK( ntk.get_output_name( 3 ) == "po3" );
}
