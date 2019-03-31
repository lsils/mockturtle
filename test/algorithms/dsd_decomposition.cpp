#include <catch.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/algorithms/dsd_decomposition.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>

using namespace mockturtle;

TEST_CASE( "Full DSD decomposition on some 4-input functions into AIGs", "[dsd_decomposition]" )
{
  std::vector<std::string> functions = {"b0bb", "00b0", "0804", "090f"};

  for ( auto const& func : functions )
  {
    kitty::dynamic_truth_table table( 4u );
    kitty::create_from_hex_string( table, func );

    aig_network aig;
    const auto x1 = aig.create_pi();
    const auto x2 = aig.create_pi();
    const auto x3 = aig.create_pi();
    const auto x4 = aig.create_pi();

    auto fn = [&]( kitty::dynamic_truth_table const&, std::vector<aig_network::signal> const& ) {
      CHECK( false );
      return aig.get_constant( false );
    };
    aig.create_po( dsd_decomposition( aig, table, {x1, x2, x3, x4}, fn ) );

    default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
    CHECK( simulate<kitty::dynamic_truth_table>( aig, sim )[0] == table );
  }
}

TEST_CASE( "Full DSD decomposition on some 10-input functions into XAGs", "[dsd_decomposition]" )
{
  std::vector<std::string> functions = {"0080004000080004ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
                                        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003333bbbbf3f3fbfbff33ffbbfff3fffb", 
                                        "000000000000000000000000000000003333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb"};

  for ( auto const& func : functions )
  {
    kitty::dynamic_truth_table table( 10u );
    kitty::create_from_hex_string( table, func );

    xag_network xag;
    std::vector<xag_network::signal> pis( 10u );
    std::generate( pis.begin(), pis.end(), [&]() { return xag.create_pi(); } );

    auto fn = [&]( kitty::dynamic_truth_table const&, std::vector<xag_network::signal> const& ) {
      CHECK( false );
      return xag.get_constant( false );
    };
    xag.create_po( dsd_decomposition( xag, table, pis, fn ) );

    default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
    CHECK( simulate<kitty::dynamic_truth_table>( xag, sim )[0] == table );
  }
}

