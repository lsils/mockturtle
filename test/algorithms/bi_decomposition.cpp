#include <catch.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <mockturtle/algorithms/bi_decomposition.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/xag.hpp>

using namespace mockturtle;

TEST_CASE( "Bi-decomposition on some 4-input functions into AIGs", "[bi_decomposition_f]" )
{
  std::vector<std::string> functions = {"b0bb", "00b0", "0804", "090f", "abcd", "3ah6"};

  for ( auto const& func : functions )
  {
    kitty::dynamic_truth_table table( 4u );
    kitty::dynamic_truth_table dc( 4u );
    kitty::create_from_hex_string( table, func );
    kitty::create_from_hex_string( dc, "ffef" );

    aig_network aig;
    const auto x1 = aig.create_pi();
    const auto x2 = aig.create_pi();
    const auto x3 = aig.create_pi();
    const auto x4 = aig.create_pi();

    aig.create_po( bi_decomposition_f( aig, table, dc, {x1, x2, x3, x4} ) );

    default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
    CHECK( binary_and( simulate<kitty::dynamic_truth_table>( aig, sim )[0], dc ) == binary_and( table, dc ) );
  }
}

TEST_CASE( "Bi-decomposition on some 10-input functions into XAGs", "[bi_decomposition_f]" )
{
  std::vector<std::string> functions = {"0080004000080004ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
                                        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003333bbbbf3f3fbfbff33ffbbfff3fffb",
                                        "000000000000000000000000000000003333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb3333bbbbf3f3fbfbff33ffbbfff3fffb"};

  for ( auto const& func : functions )
  {
    kitty::dynamic_truth_table table( 10u );
    kitty::dynamic_truth_table dc( 10u );
    kitty::create_from_hex_string( table, func );
    kitty::create_from_hex_string( dc, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff" );

    xag_network xag;
    std::vector<xag_network::signal> pis( 10u );
    std::generate( pis.begin(), pis.end(), [&]() { return xag.create_pi(); } );

    xag.create_po( bi_decomposition_f( xag, table, dc, pis ) );

    std::cout << xag.num_gates() << std::endl;

    default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
    CHECK( binary_and( simulate<kitty::dynamic_truth_table>( xag, sim )[0], dc ) == binary_and( table, dc ) );
  }
}
