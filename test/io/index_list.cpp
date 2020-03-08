#include <catch.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/io/index_list.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/traits.hpp>

using namespace mockturtle;

TEST_CASE( "create 4-input XAGs from binary index list", "[index_list]" )
{
  std::vector<std::pair<uint64_t, std::vector<uint32_t>>> xags = {
    {0x0000, {1 << 8 | 0, 0}},
    {0x8888, {1 << 16 | 1 << 8 | 2, 2, 4, 6}},
    {0x7888, {3 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 12, 10, 14}},
    {0x8080, {2 << 16 | 1 << 8 | 3, 2, 4, 6, 8, 10}},
    {0x2a80, {3 << 16 | 1 << 8 | 4, 4, 6, 10, 8, 2, 12, 14}},
    {0x8000, {3 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 10, 12, 14}},
    {0x0888, {4 << 16 | 1 << 8 | 4, 2, 4, 6, 10, 8, 12, 14, 10, 16}},
    {0xf888, {5 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 10, 12, 14, 10, 16, 12, 18}}
  };

  for ( auto const& [tt, repr] : xags ) {
    xag_network xag;
    std::vector<xag_network::signal> pis( 4u );
    std::generate( pis.begin(), pis.end(), [&]() { return xag.create_pi(); });

    for ( auto const& po : create_from_binary_index_list( xag, repr.begin(), pis.begin() ) )
    {
      xag.create_po( po );
    }

    CHECK( xag.num_pos() == 1u );
    CHECK( simulate<kitty::static_truth_table<4>>( xag )[0]._bits == tt );
  }
}
