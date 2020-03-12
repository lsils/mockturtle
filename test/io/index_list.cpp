#include <catch.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include <mockturtle/algorithms/node_resynthesis/xag_minmc.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/algorithms/xag_optimization.hpp>
#include <mockturtle/io/index_list.hpp>
#include <mockturtle/io/write_dot.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/properties/mccost.hpp>
#include <mockturtle/traits.hpp>

#include <kitty/constructors.hpp>
#include <kitty/operators.hpp>
#include <kitty/spectral.hpp>
#include <lorina/verilog.hpp>

using namespace mockturtle;

namespace mockturtle::detail
{
// clang-format off
std::vector<std::vector<std::tuple<uint32_t, uint64_t, std::vector<uint32_t>, std::string>>> minmc_xags = {
  {{0, 0x0, {1 << 8 | 0, 0}, "0"}},
  {{0, 0x0, {1 << 8 | 0, 0}, "0"}},
  {{0, 0x0, {1 << 8 | 0, 0},                 "0"},
   {1, 0x8, {1 << 16 | 1 << 8 | 2, 2, 4, 6}, "(ab)"}},
  {{0, 0x00, {1 << 8 | 0, 0},                        "0"},
   {2, 0x88, {1 << 16 | 1 << 8 | 2, 2, 4, 6},        "(ab)"},
   {1, 0x80, {2 << 16 | 1 << 8 | 3, 2, 4, 6, 8, 10}, "(abc)"}},
  {{0, 0x0000, {1 << 8 | 0, 0},                                                "0"},
   {1, 0x8000, {3 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 10, 12, 14},                 "(abcd)"},
   {2, 0x8080, {2 << 16 | 1 << 8 | 3, 2, 4, 6, 8, 10},                         "(abc)"},
   {3, 0x0888, {4 << 16 | 1 << 8 | 4, 2, 4, 6, 10, 8, 12, 14, 10, 16},         "[(abcd)(ab)]"},
   {4, 0x8888, {1 << 16 | 1 << 8 | 2, 2, 4, 6},                                "(ab)"},
   {5, 0x2a80, {3 << 16 | 1 << 8 | 4, 4, 6, 10, 8, 2, 12, 14},                 "[(abc)(ad)]"},
   {6, 0xf888, {5 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 10, 12, 14, 10, 16, 12, 18}, "[(abcd)(ab)(cd)]"},
   {7, 0x7888, {3 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 12, 10, 14},                 "[(ab)(cd)]"}},
  {{ 0, 0x00000000, {1 << 8 | 0, 0},                                                                                                                           "0"},
   { 1, 0x80000000, {4 << 16 | 1 << 8 | 5, 2, 4, 6, 8, 12, 14, 10, 16, 18},                                                                                    "(abcde)"},
   { 2, 0x80008000, {3 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 10, 12, 14},                                                                                            "(abcd)"},
   { 3, 0x00808080, {5 << 16 | 1 << 8 | 5, 2, 4, 6, 12, 8, 14, 10, 16, 18, 14, 20},                                                                            "[(abcde)(abc)]"},
   { 4, 0x80808080, {2 << 16 | 1 << 8 | 3, 2, 4, 6, 8, 10},                                                                                                    "(abc)"},
   { 5, 0x40808080, {4 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 14, 2, 12, 16, 18},                                                                                    "[(bcde)(abc)]"},
   { 6, 0x22080808, {7 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 12, 14, 14, 12, 18, 4, 20, 16, 2, 22, 24},                                                             "[(abcde)(abc)(ade)(ab)]"},
   { 7, 0x88080808, {6 << 16 | 1 << 8 | 5, 2, 4, 6, 12, 8, 14, 10, 16, 18, 12, 20, 14, 22},                                                                    "[(abcde)(abc)(ab)]"},
   { 8, 0x2a808080, {4 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 14, 12, 2, 16, 18},                                                                                    "[(abc)(ade)]"},
   { 9, 0x15808080, {6 << 16 | 1 << 8 | 5, 4, 6, 12, 2, 8, 10, 16, 2, 14, 18, 20, 18, 22},                                                                     "[(bcde)(abc)(ade)(de)]"},
   {10, 0xc8080808, {5 << 16 | 1 << 8 | 5, 8, 10, 12, 2, 6, 14, 16, 2, 4, 18, 20},                                                                             "[(bcde)(ab)(abc)]"},
   {11, 0x08880888, {4 << 16 | 1 << 8 | 4, 2, 4, 6, 10, 8, 12, 14, 10, 16},                                                                                    "[(abcd)(ab)]"},
   {12, 0x95404040, {8 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 12, 14, 16, 12, 18, 14, 2, 20, 22, 12, 24, 14, 26},                                                    "[(abcde)(abc)(ade)(de)(bc)]"},
   {13, 0xaa808080, {6 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 12, 14, 14, 12, 18, 16, 2, 20, 22},                                                                    "[(abcde)(abc)(ade)]"},
   {14, 0x6a404040, {7 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 12, 14, 16, 12, 18, 14, 2, 20, 22, 12, 24},                                                            "[(abcde)(abc)(ade)(bc)]"},
   {15, 0xaa802a80, {6 << 16 | 1 << 8 | 5, 4, 6, 2, 8, 10, 14, 16, 2, 12, 18, 20, 14, 22},                                                                     "[(abcde)(abc)(ad)]"},
   {16, 0x08888888, {5 << 16 | 1 << 8 | 5, 2, 4, 6, 12, 8, 14, 10, 16, 18, 12, 20},                                                                            "[(abcde)(ab)]"},
   {17, 0x88888888, {1 << 16 | 1 << 8 | 2, 2, 4, 6},                                                                                                           "(ab)"},
   {18, 0xea404040, {5 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 14, 12, 2, 16, 18, 12, 20},                                                                            "[(abc)(ade)(bc)]"},
   {19, 0x2a802a80, {3 << 16 | 1 << 8 | 4, 4, 6, 10, 8, 2, 12, 14},                                                                                            "[(abc)(ad)]"},
   {20, 0x37080808, {6 << 16 | 1 << 8 | 5, 4, 6, 2, 4, 8, 10, 16, 14, 12, 18, 20, 18, 22},                                                                     "[(bcde)(abc)(ab)(de)]"},
   {21, 0xea808080, {6 << 16 | 1 << 8 | 5, 4, 6, 12, 2, 8, 10, 16, 2, 14, 18, 20, 2, 22},                                                                      "[(bcde)(abc)(ade)]"},
   {22, 0x8c804c80, {5 << 16 | 1 << 8 | 5, 8, 10, 12, 2, 6, 14, 16, 8, 4, 18, 20},                                                                             "[(bcde)(bd)(abc)]"},
   {23, 0xea802a80, {8 << 16 | 1 << 8 | 5, 6, 4, 6, 12, 14, 6, 16, 8, 10, 16, 20, 2, 19, 22, 24, 2, 26},                                                       "[(bcde)(abc)(ad)]"},
   {24, 0x7f008000, {4 << 16 | 1 << 8 | 5, 2, 4, 6, 12, 14, 10, 8, 16, 18},                                                                                    "[(abcd)(de)]"},
   {25, 0x96704c80, {12 << 16 | 1 << 8 | 5, 10, 8, 12, 2, 10, 14, 16, 6, 6, 4, 8, 20, 22, 2, 4, 24, 26, 12, 18, 28, 22, 16, 32, 30, 34},                       "[(abcde)(abc)(ade)(ce)(bd)]"},
   {26, 0x77080808, {7 << 16 | 1 << 8 | 5, 2, 4, 6, 12, 8, 10, 14, 16, 18, 12, 20, 14, 22, 16, 24},                                                            "[(abcde)(abc)(ab)(de)]"},
   {27, 0x66804c80, {8 << 16 | 1 << 8 | 5, 6, 4, 12, 2, 6, 14, 16, 8, 2, 8, 10, 20, 22, 4, 18, 24, 26},                                                        "[(abcde)(abc)(ade)(bd)]"},
   {28, 0xa6408c40, {9 << 16 | 1 << 8 | 5, 4, 6, 8, 6, 14, 2, 2, 10, 18, 4, 8, 20, 22, 16, 12, 24, 26, 22, 28},                                                "[(abcde)(abc)(ade)(bd)(bc)]"},
   {29, 0xff808080, {6 << 16 | 1 << 8 | 5, 2, 4, 6, 12, 8, 10, 14, 16, 18, 14, 20, 16, 22},                                                                    "[(abcde)(abc)(de)]"},
   {30, 0x7808f808, {7 << 16 | 1 << 8 | 5, 2, 4, 8, 10, 12, 14, 16, 12, 18, 8, 6, 20, 22, 12, 24},                                                             "[(abcde)(abc)(ab)(cd)]"},
   {31, 0xe6804c80, {6 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 14, 12, 2, 16, 4, 8, 20, 18, 22},                                                                      "[(abc)(ade)(bd)]"},
   {32, 0x7f808080, {4 << 16 | 1 << 8 | 5, 2, 4, 6, 12, 8, 10, 16, 14, 18},                                                                                    "[(abc)(de)]"},
   {33, 0xd6704c80, {10 << 16 | 1 << 8 | 5, 4, 6, 12, 2, 8, 10, 16, 12, 14, 18, 10, 4, 8, 6, 22, 24, 26, 16, 28, 20, 30},                                      "[(bcde)(abc)(ade)(ce)(bd)]"},
   {34, 0x1a702a80, {7 << 16 | 1 << 8 | 5, 8, 10, 12, 2, 4, 14, 16, 10, 6, 18, 2, 8, 22, 20, 24},                                                              "[(bcde)(abc)(ad)(ce)]"},
   {35, 0xb8887888, {5 << 16 | 1 << 8 | 5, 6, 8, 10, 12, 14, 2, 4, 16, 18, 12, 20},                                                                            "[(bcde)(ab)(cd)]"},
   {36, 0xd9804c80, {7 << 16 | 1 << 8 | 5, 8, 10, 12, 2, 4, 6, 16, 12, 14, 18, 4, 8, 22, 20, 24},                                                              "[(bcde)(abc)(ade)(bd)(de)]"},
   {37, 0xbf808080, {5 << 16 | 1 << 8 | 5, 4, 6, 8, 10, 14, 2, 12, 16, 18, 14, 20},                                                                            "[(bcde)(abc)(de)]"},
   {38, 0x3808f808, {7 << 16 | 1 << 8 | 5, 4, 8, 10, 12, 2, 4, 16, 14, 18, 8, 6, 20, 22, 16, 24},                                                              "[(bcde)(abc)(ab)(cd)]"},
   {39, 0xf888f888, {5 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 10, 12, 14, 10, 16, 12, 18},                                                                            "[(abcd)(ab)(cd)]"},
   {40, 0xa9b08c40, {15 << 16 | 1 << 8 | 5, 8, 4, 6, 4, 10, 4, 14, 16, 18, 12, 16, 2, 10, 8, 24, 14, 26, 2, 2, 8, 30, 14, 28, 32, 34, 22, 20, 36, 38, 30, 40}, "[(abcde)(abc)(ade)(de)(ce)(bd)(bc)]"},
   {41, 0x56b08c40, {13 << 16 | 1 << 8 | 5, 8, 2, 2, 12, 14, 4, 10, 8, 6, 2, 6, 4, 22, 2, 10, 24, 26, 20, 6, 28, 30, 18, 16, 32, 34, 26, 36},                  "[(abcde)(abc)(ade)(ce)(bd)(bc)]"},
   {42, 0x664c2a80, {11 << 16 | 1 << 8 | 5, 8, 2, 6, 12, 14, 8, 8, 6, 18, 4, 4, 10, 22, 6, 20, 24, 26, 2, 16, 28, 30, 22, 32},                                 "[(abcde)(abc)(ad)(be)]"},
   {43, 0xf8887888, {6 << 16 | 1 << 8 | 5, 2, 4, 6, 8, 12, 14, 10, 16, 18, 12, 20, 14, 22},                                                                    "[(abcde)(ab)(cd)]"},
   {44, 0x78887888, {3 << 16 | 1 << 8 | 4, 2, 4, 6, 8, 12, 10, 14},                                                                                            "[(ab)(cd)]"},
   {45, 0xd6b08c40, {8 << 16 | 1 << 8 | 5, 2, 10, 12, 4, 6, 2, 6, 16, 18, 8, 14, 20, 6, 10, 24, 22, 26},                                                       "[(abc)(ade)(bc)(bd)(ce)]"},
   {46, 0xe64c2a80, {5 << 16 | 1 << 8 | 5, 4, 6, 12, 8, 2, 14, 4, 10, 18, 16, 20},                                                                             "[(abc)(ad)(be)]"},
   {47, 0x7c704c80, {10 << 16 | 1 << 8 | 5, 8, 10, 12, 2, 6, 4, 16, 2, 18, 12, 14, 20, 22, 8, 4, 24, 6, 10, 28, 26, 30},                                       "[(bcde)(abc)(bd)(ce)]"}}
};
// clang-format on
}

template<int NumVars>
static void check_minmc_xags()
{
  for ( auto const& [cls, tt, repr, expr] : detail::minmc_xags[NumVars] ) {
    xag_network xag;
    std::vector<xag_network::signal> pis( NumVars );
    std::generate( pis.begin(), pis.end(), [&]() { return xag.create_pi(); });

    for ( auto const& po : create_from_binary_index_list( xag, repr.begin(), pis.begin() ) )
    {
      xag.create_po( po );
    }

    const auto f = simulate<kitty::static_truth_table<NumVars>>( xag )[0];
    auto f_tt = f.construct(), f_expr = f.construct();
    kitty::create_from_words( f_tt, &tt, &tt + 1 );
    kitty::create_from_expression( f_expr, expr );
    CHECK( kitty::get_spectral_class( f ) == cls );
    CHECK( f == f_tt );
    CHECK( f == f_expr );
  }
}

TEST_CASE( "create MC-optumum XAGs from binary index list", "[index_list]" )
{
  check_minmc_xags<0>();
  check_minmc_xags<1>();
  check_minmc_xags<2>();
  check_minmc_xags<3>();
  check_minmc_xags<4>();
  check_minmc_xags<5>();
}

template<int NumVars>
static void check_repr_match()
{
  for ( auto const& [cls, tt, repr, expr] : detail::minmc_xags[NumVars] ) {
    (void)cls;
    (void)repr;
    (void)expr;

    kitty::dynamic_truth_table f_tt( NumVars );
    kitty::create_from_words( f_tt, &tt, &tt + 1 );
    const auto func = kitty::spectral_representative( f_tt );
    const auto func2 = kitty::hybrid_exact_spectral_canonization( f_tt );
    CHECK( func == f_tt );
    CHECK( func2 == f_tt );
  }
}

TEST_CASE( "check representatives for database functions", "[index_list]" )
{
  check_repr_match<0>();
  check_repr_match<1>();
  check_repr_match<2>();
  check_repr_match<3>();
  check_repr_match<4>();
}
