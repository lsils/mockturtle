#include <catch.hpp>

#include <algorithm>
#include <vector>

#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/algorithms/xag_optimization.hpp>
#include <mockturtle/networks/xag.hpp>

using namespace mockturtle;

TEST_CASE( "Edge cases for linear resynthesis", "[xag_optimization]" )
{
  {
    xag_network xag;
    std::vector<xag_network::signal> pis( 4u );
    std::generate( pis.begin(), pis.end(), [&]() { return xag.create_pi(); } );
    xag.create_po( xag.create_nary_and( pis ) );

    const auto opt = exact_linear_resynthesis_optimization( xag );
    CHECK( simulate<kitty::static_truth_table<4>>( xag ) == simulate<kitty::static_truth_table<4>>( opt ) );
  }

  {
    xag_network xag;
    std::vector<xag_network::signal> pis( 4u );
    std::generate( pis.begin(), pis.end(), [&]() { return xag.create_pi(); } );
    xag.create_po( xag.create_xor( xag.create_and( pis[0u], pis[1u] ), xag.create_and( pis[2u], pis[3u] ) ) );

    const auto opt = exact_linear_resynthesis_optimization( xag );
    CHECK( simulate<kitty::static_truth_table<4>>( xag ) == simulate<kitty::static_truth_table<4>>( opt ) );
  }
}
