#include <catch.hpp>

#include <algorithm>
#include <vector>

#include <mockturtle/algorithms/balancing.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/balancing/esop_balancing.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/algorithms/balancing/sop_balancing.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/depth_view.hpp>

using namespace mockturtle;

TEST_CASE( "Rebalance AND chain in AIG", "[balancing]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();

  aig.create_po( aig.create_and( a, aig.create_and( b, aig.create_and( c, d ) ) ) );

  CHECK( depth_view{ aig }.depth() == 3u );

  aig = balancing( aig, { sop_rebalancing<aig_network>{} } );
  CHECK( depth_view{ aig }.depth() == 2u );
}

TEST_CASE( "Rebalance XAG adder using ESOP balancing", "[balancing]" )
{
  xag_network xag;
  std::vector<xag_network::signal> as( 8u ), bs( 8u );
  std::generate( as.begin(), as.end(), [&]() { return xag.create_pi(); } );
  std::generate( bs.begin(), bs.end(), [&]() { return xag.create_pi(); } );
  auto carry = xag.get_constant( false );
  carry_ripple_adder_inplace( xag, as, bs, carry );
  std::for_each( as.begin(), as.end(), [&]( auto const& f ) { xag.create_po( f ); } );

  CHECK( depth_view{ xag }.depth() == 22u );

  xag = balancing( xag, { esop_rebalancing<xag_network>{} } );
  CHECK( depth_view{ xag }.depth() == 12u );
}

TEST_CASE( "Rebalance XAG adder using ESOP balancing with SPP optimization", "[balancing]" )
{
  xag_network xag;
  std::vector<xag_network::signal> as( 8u ), bs( 8u );
  std::generate( as.begin(), as.end(), [&]() { return xag.create_pi(); } );
  std::generate( bs.begin(), bs.end(), [&]() { return xag.create_pi(); } );
  auto carry = xag.get_constant( false );
  carry_ripple_adder_inplace( xag, as, bs, carry );
  std::for_each( as.begin(), as.end(), [&]( auto const& f ) { xag.create_po( f ); } );

  CHECK( depth_view{ xag }.depth() == 22u );

  esop_rebalancing<xag_network> esop{};
  esop.spp_optimization = true;

  rebalancing_function_t<xag_network> balancing_fn{ esop };
  xag = balancing( xag, balancing_fn );
  CHECK( depth_view{ xag }.depth() == 22u );
}

TEST_CASE( "Rebalance XAG adder using ESOP balancing with MUX optimization", "[balancing]" )
{
  xag_network xag;
  std::vector<xag_network::signal> as( 8u ), bs( 8u );
  std::generate( as.begin(), as.end(), [&]() { return xag.create_pi(); } );
  std::generate( bs.begin(), bs.end(), [&]() { return xag.create_pi(); } );
  auto carry = xag.get_constant( false );
  carry_ripple_adder_inplace( xag, as, bs, carry );
  std::for_each( as.begin(), as.end(), [&]( auto const& f ) { xag.create_po( f ); } );

  CHECK( depth_view{ xag }.depth() == 22u );

  esop_rebalancing<xag_network> esop{};
  esop.mux_optimization = true;

  rebalancing_function_t<xag_network> balancing_fn{ esop };
  xag = balancing( xag, balancing_fn );
  CHECK( depth_view{ xag }.depth() == 17u );
}

TEST_CASE( "SOP balance AND chain in AIG", "[balancing]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();

  aig.create_po( aig.create_and( a, aig.create_and( b, aig.create_and( c, d ) ) ) );

  CHECK( depth_view{ aig }.depth() == 3u );

  aig = sop_balancing( aig );
  CHECK( depth_view{ aig }.depth() == 2u );
}

TEST_CASE( "SOP balance AIG adder", "[balancing]" )
{
  aig_network aig;
  std::vector<aig_network::signal> as( 8u ), bs( 8u );
  std::generate( as.begin(), as.end(), [&]() { return aig.create_pi(); } );
  std::generate( bs.begin(), bs.end(), [&]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );
  carry_ripple_adder_inplace( aig, as, bs, carry );
  std::for_each( as.begin(), as.end(), [&]( auto const& f ) { aig.create_po( f ); } );

  CHECK( depth_view{ aig }.depth() == 16u );

  aig_network res = sop_balancing( aig );
  CHECK( depth_view{ res }.depth() == 9u );

  auto const miter_ntk = *miter<aig_network>( aig, res );
  CHECK( *equivalence_checking( miter_ntk ) == true );
}

TEST_CASE( "ESOP balance XAG adder", "[balancing]" )
{
  xag_network xag;
  std::vector<xag_network::signal> as( 8u ), bs( 8u );
  std::generate( as.begin(), as.end(), [&]() { return xag.create_pi(); } );
  std::generate( bs.begin(), bs.end(), [&]() { return xag.create_pi(); } );
  auto carry = xag.get_constant( false );
  carry_ripple_adder_inplace( xag, as, bs, carry );
  std::for_each( as.begin(), as.end(), [&]( auto const& f ) { xag.create_po( f ); } );

  CHECK( depth_view{ xag }.depth() == 22u );

  xag_network res = esop_balancing( xag );
  CHECK( depth_view{ res }.depth() == 8u );

  auto const miter_ntk = *miter<xag_network>( xag, res );
  CHECK( *equivalence_checking( miter_ntk ) == true );
}

TEST_CASE( "SOP balance XAG adder", "[balancing]" )
{
  xag_network xag;
  std::vector<xag_network::signal> as( 8u ), bs( 8u );
  std::generate( as.begin(), as.end(), [&]() { return xag.create_pi(); } );
  std::generate( bs.begin(), bs.end(), [&]() { return xag.create_pi(); } );
  auto carry = xag.get_constant( false );
  carry_ripple_adder_inplace( xag, as, bs, carry );
  std::for_each( as.begin(), as.end(), [&]( auto const& f ) { xag.create_po( f ); } );

  CHECK( depth_view{ xag }.depth() == 22u );

  xag_network res = sop_balancing( xag );
  CHECK( depth_view{ res }.depth() == 11u );

  auto const miter_ntk = *miter<xag_network>( xag, res );
  CHECK( *equivalence_checking( miter_ntk ) == true );
}
