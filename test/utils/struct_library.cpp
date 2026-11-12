#include <catch.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include <lorina/genlib.hpp>
#include <lorina/super.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/io/super_reader.hpp>
#include <mockturtle/utils/struct_library.hpp>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/npn.hpp>
#include <kitty/static_truth_table.hpp>

using namespace mockturtle;

std::string const test_library = "GATE   inv1    3 O=!a;               PIN * INV 3 999 1.1 0.09 1.1 0.09\n"
                                 "GATE   inv2    2 O=!a;               PIN * INV 2 999 1.0 0.1 1.0 0.1\n"
                                 "GATE   inv3    1 O=!a;               PIN * INV 1 999 0.9 0.3 0.9 0.3\n"
                                 "GATE   inv4    4 O=!a;               PIN * INV 4 999 1.2 0.07 1.2 0.07\n"
                                 "GATE   nand2   2 O=!(a*b);           PIN * INV 1 999 1.0 0.2 1.0 0.2\n"
                                 "GATE   nand3   3 O=!(a*b*c);         PIN * INV 1 999 1.1 0.3 1.1 0.3\n"
                                 "GATE   nand4   4 O=!(a*b*c*d);       PIN * INV 1 999 1.4 0.4 1.4 0.4\n"
                                 "GATE   nor2    2 O=!(a+b);           PIN * INV 1 999 1.4 0.5 1.4 0.5\n"
                                 "GATE   nor3    3 O=!(a+b+c);         PIN * INV 1 999 2.4 0.7 2.4 0.7\n"
                                 "GATE   nor4    4 O=!(a+b+c+d);       PIN * INV 1 999 3.8 1.0 3.8 1.0\n"
                                 "GATE   zero    0 O=CONST0;\n"
                                 "GATE   one     0 O=CONST1;";

std::string const sizes_library = "GATE   inv1    3 O=!a;               PIN * INV 3 999 1.1 0.09 1.1 0.09\n"
                                  "GATE   inv2    2 O=!a;               PIN * INV 2 999 1.0 0.1 1.0 0.1\n"
                                  "GATE   inv3    1 O=!a;               PIN * INV 1 999 0.9 0.3 0.9 0.3\n"
                                  "GATE   inv4    4 O=!a;               PIN * INV 4 999 1.2 0.07 1.2 0.07\n"
                                  "GATE   nand2a  2 O=!(a*b);           PIN * INV 1 999 1.0 0.2 1.0 0.2\n"
                                  "GATE   nand2b  3 O=!(a*b);           PIN a INV 1 999 0.9 0.2 0.9 0.2 PIN b INV 1 999 1.2 0.2 1.2 0.2\n"
                                  "GATE   nand2c  3 O=!(a*b);           PIN a INV 1 999 0.9 0.2 0.9 0.2 PIN b INV 1 999 1.1 0.2 1.1 0.2\n"
                                  "GATE   zero    0 O=CONST0;\n"
                                  "GATE   one     0 O=CONST1;";

std::string const reconv_library = "GATE   inv1    3 O=!a;               PIN * INV 3 999 1.1 0.09 1.1 0.09\n"
                                   "GATE   nand2   2 O=!(a*b);           PIN * INV 1 999 1.0 0.2 1.0 0.2\n"
                                   "GATE   xor2    5 O=a*!b+!a*b;        PIN * UNKNOWN 2 999 1.9 0.5 1.9 0.5\n"
                                   "GATE   maj     6 O=a*b+a*c+b*c;      PIN * INV 1 999 2.1 0.4 2.1 0.4\n"
                                   "GATE   zero    0 O=CONST0;\n"
                                   "GATE   one     0 O=CONST1;";

std::string const large_library = "GATE   inv1    3 O=!a;                      PIN * INV 3 999 1.1 0.09 1.1 0.09\n"
                                  "GATE   oai322  8 O=!((a+b+c)*(d+e)*(f+g));  PIN * INV 1 999 3.0 0.4 3.0 0.4\n"
                                  "GATE   zero    0 O=CONST0;\n"
                                  "GATE   one     0 O=CONST1;";

TEST_CASE( "Struct library creation", "[struct_library]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );

  CHECK( result == lorina::return_code::success );

  struct_library<4> lib( gates );
  lib.construct( 2 );

  auto const& library_map = lib.get_struct_library();

  /* translate to sorted vector */
  std::vector<uint32_t> entry_ids;
  std::for_each( library_map.begin(), library_map.end(), [&]( auto const& pair ) { entry_ids.push_back( pair.first ); return; } );
  std::stable_sort( entry_ids.begin(), entry_ids.end() );

  CHECK( entry_ids.size() == 8 );

  CHECK( entry_ids[0] % 2 == 0 );
  CHECK( library_map.find( entry_ids[0] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[0] )->second[0].root->root->name == "nor2" );
  CHECK( library_map.find( entry_ids[0] )->second[0].area == 2 );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[0] == 1.4f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[1] == 1.4f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[2] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[3] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[0].polarity == 3 );

  CHECK( entry_ids[1] % 2 == 1 );
  CHECK( library_map.find( entry_ids[1] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[1] )->second[0].root->root->name == "nand2" );
  CHECK( library_map.find( entry_ids[1] )->second[0].area == 2 );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[0] == 1.0f );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[1] == 1.0f );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[2] == 0 );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[3] == 0 );
  CHECK( library_map.find( entry_ids[1] )->second[0].polarity == 0 );

  CHECK( entry_ids[2] % 2 == 0 );
  CHECK( library_map.find( entry_ids[2] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[2] )->second[0].root->root->name == "nor3" );
  CHECK( library_map.find( entry_ids[2] )->second[0].area == 3 );
  CHECK( library_map.find( entry_ids[2] )->second[0].tdelay[0] == 2.4f );
  CHECK( library_map.find( entry_ids[2] )->second[0].tdelay[1] == 2.4f );
  CHECK( library_map.find( entry_ids[2] )->second[0].tdelay[2] == 2.4f );
  CHECK( library_map.find( entry_ids[2] )->second[0].tdelay[3] == 0 );
  CHECK( library_map.find( entry_ids[2] )->second[0].polarity == 7 );

  CHECK( entry_ids[3] % 2 == 1 );
  CHECK( library_map.find( entry_ids[3] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[3] )->second[0].root->root->name == "nand3" );
  CHECK( library_map.find( entry_ids[3] )->second[0].area == 3 );
  CHECK( library_map.find( entry_ids[3] )->second[0].tdelay[0] == 1.1f );
  CHECK( library_map.find( entry_ids[3] )->second[0].tdelay[1] == 1.1f );
  CHECK( library_map.find( entry_ids[3] )->second[0].tdelay[2] == 1.1f );
  CHECK( library_map.find( entry_ids[3] )->second[0].tdelay[3] == 0 );
  CHECK( library_map.find( entry_ids[3] )->second[0].polarity == 0 );

  CHECK( entry_ids[4] % 2 == 0 );
  CHECK( library_map.find( entry_ids[4] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[4] )->second[0].root->root->name == "nor4" );
  CHECK( library_map.find( entry_ids[4] )->second[0].area == 4 );
  CHECK( library_map.find( entry_ids[4] )->second[0].tdelay[0] == 3.8f );
  CHECK( library_map.find( entry_ids[4] )->second[0].tdelay[1] == 3.8f );
  CHECK( library_map.find( entry_ids[4] )->second[0].tdelay[2] == 3.8f );
  CHECK( library_map.find( entry_ids[4] )->second[0].tdelay[3] == 3.8f );
  CHECK( library_map.find( entry_ids[4] )->second[0].polarity == 15 );

  CHECK( entry_ids[5] % 2 == 1 );
  CHECK( library_map.find( entry_ids[5] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[5] )->second[0].root->root->name == "nand4" );
  CHECK( library_map.find( entry_ids[5] )->second[0].area == 4 );
  CHECK( library_map.find( entry_ids[5] )->second[0].tdelay[0] == 1.4f );
  CHECK( library_map.find( entry_ids[5] )->second[0].tdelay[1] == 1.4f );
  CHECK( library_map.find( entry_ids[5] )->second[0].tdelay[2] == 1.4f );
  CHECK( library_map.find( entry_ids[5] )->second[0].tdelay[3] == 1.4f );
  CHECK( library_map.find( entry_ids[5] )->second[0].polarity == 0 );

  CHECK( entry_ids[6] % 2 == 0 );
  CHECK( library_map.find( entry_ids[6] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[6] )->second[0].root->root->name == "nor4" );
  CHECK( library_map.find( entry_ids[6] )->second[0].area == 4 );
  CHECK( library_map.find( entry_ids[6] )->second[0].tdelay[0] == 3.8f );
  CHECK( library_map.find( entry_ids[6] )->second[0].tdelay[1] == 3.8f );
  CHECK( library_map.find( entry_ids[6] )->second[0].tdelay[2] == 3.8f );
  CHECK( library_map.find( entry_ids[6] )->second[0].tdelay[3] == 3.8f );
  CHECK( library_map.find( entry_ids[6] )->second[0].polarity == 15 );

  CHECK( entry_ids[7] % 2 == 1 );
  CHECK( library_map.find( entry_ids[7] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[7] )->second[0].root->root->name == "nand4" );
  CHECK( library_map.find( entry_ids[7] )->second[0].area == 4 );
  CHECK( library_map.find( entry_ids[7] )->second[0].tdelay[0] == 1.4f );
  CHECK( library_map.find( entry_ids[7] )->second[0].tdelay[1] == 1.4f );
  CHECK( library_map.find( entry_ids[7] )->second[0].tdelay[2] == 1.4f );
  CHECK( library_map.find( entry_ids[7] )->second[0].tdelay[3] == 1.4f );
  CHECK( library_map.find( entry_ids[7] )->second[0].polarity == 0 );
}

TEST_CASE( "Struct library creation min sizes", "[struct_library]" )
{
  std::vector<gate> gates;

  std::istringstream in( sizes_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );

  CHECK( result == lorina::return_code::success );

  struct_library_params ps;
  ps.load_minimum_size_only = true;
  struct_library<4> lib( gates, ps );
  lib.construct( 2 );

  auto const& library_map = lib.get_struct_library();

  /* translate to sorted vector */
  std::vector<uint32_t> entry_ids;
  std::for_each( library_map.begin(), library_map.end(), [&]( auto const& pair ) { entry_ids.push_back( pair.first ); return; } );
  std::stable_sort( entry_ids.begin(), entry_ids.end() );

  CHECK( entry_ids.size() == 1 );

  CHECK( entry_ids[0] % 2 == 1 );
  CHECK( library_map.find( entry_ids[0] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[0] )->second[0].root->root->name == "nand2a" );
  CHECK( library_map.find( entry_ids[0] )->second[0].area == 2 );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[0] == 1.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[1] == 1.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[2] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[3] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[0].polarity == 0 );
}

TEST_CASE( "Struct library creation dominated sizes", "[struct_library]" )
{
  std::vector<gate> gates;

  std::istringstream in( sizes_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );

  CHECK( result == lorina::return_code::success );

  struct_library_params ps;
  ps.load_minimum_size_only = false;
  struct_library<4> lib( gates, ps );
  lib.construct( 2 );

  auto const& library_map = lib.get_struct_library();

  /* translate to sorted vector */
  std::vector<uint32_t> entry_ids;
  std::for_each( library_map.begin(), library_map.end(), [&]( auto const& pair ) { entry_ids.push_back( pair.first ); return; } );
  std::stable_sort( entry_ids.begin(), entry_ids.end() );

  CHECK( entry_ids.size() == 1 );

  CHECK( entry_ids[0] % 2 == 1 );
  CHECK( library_map.find( entry_ids[0] )->second.size() == 2 );
  CHECK( library_map.find( entry_ids[0] )->second[0].root->root->name == "nand2a" );
  CHECK( library_map.find( entry_ids[0] )->second[0].area == 2 );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[0] == 1.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[1] == 1.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[2] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[3] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[0].polarity == 0 );

  CHECK( library_map.find( entry_ids[0] )->second[1].root->root->name == "nand2c" );
  CHECK( library_map.find( entry_ids[0] )->second[1].area == 3 );
  CHECK( library_map.find( entry_ids[0] )->second[1].tdelay[0] == 0.9f );
  CHECK( library_map.find( entry_ids[0] )->second[1].tdelay[1] == 1.1f );
  CHECK( library_map.find( entry_ids[0] )->second[1].tdelay[2] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[1].tdelay[3] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[1].polarity == 0 );
}

TEST_CASE( "Struct library creation ignore reconvergence", "[struct_library]" )
{
  std::vector<gate> gates;

  std::istringstream in( reconv_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );

  CHECK( result == lorina::return_code::success );

  struct_library<3> lib( gates );
  lib.construct( 2 );

  auto const& library_map = lib.get_struct_library();

  /* translate to sorted vector */
  std::vector<uint32_t> entry_ids;
  std::for_each( library_map.begin(), library_map.end(), [&]( auto const& pair ) { entry_ids.push_back( pair.first ); return; } );
  std::stable_sort( entry_ids.begin(), entry_ids.end() );

  CHECK( entry_ids.size() == 1 );
  CHECK( entry_ids[0] % 2 == 1 );
  CHECK( library_map.find( entry_ids[0] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[0] )->second[0].root->root->name == "nand2" );
  CHECK( library_map.find( entry_ids[0] )->second[0].area == 2 );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[0] == 1.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[1] == 1.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[2] == 0 );
  CHECK( library_map.find( entry_ids[0] )->second[0].polarity == 0 );
}

TEST_CASE( "Struct library creation large rules", "[struct_library]" )
{
  std::vector<gate> gates;

  std::istringstream in( large_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );

  CHECK( result == lorina::return_code::success );

  struct_library<7> lib( gates );
  lib.construct( 2 );

  auto const& library_map = lib.get_struct_library();

  /* translate to sorted vector */
  std::vector<uint32_t> entry_ids;
  std::for_each( library_map.begin(), library_map.end(), [&]( auto const& pair ) { entry_ids.push_back( pair.first ); return; } );
  std::stable_sort( entry_ids.begin(), entry_ids.end() );

  CHECK( entry_ids.size() == 2 );

  CHECK( entry_ids[0] % 2 == 1 );
  CHECK( library_map.find( entry_ids[0] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[0] )->second[0].root->root->name == "oai322" );
  CHECK( library_map.find( entry_ids[0] )->second[0].area == 8 );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[0] == 3.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[1] == 3.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[2] == 3.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[3] == 3.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[4] == 3.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].tdelay[5] == 3.0f );
  CHECK( library_map.find( entry_ids[0] )->second[0].polarity == 127 );

  CHECK( entry_ids[1] % 2 == 1 );
  CHECK( library_map.find( entry_ids[1] )->second.size() == 1 );
  CHECK( library_map.find( entry_ids[1] )->second[0].root->root->name == "oai322" );
  CHECK( library_map.find( entry_ids[1] )->second[0].area == 8 );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[0] == 3.0f );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[1] == 3.0f );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[2] == 3.0f );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[3] == 3.0f );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[4] == 3.0f );
  CHECK( library_map.find( entry_ids[1] )->second[0].tdelay[5] == 3.0f );
  CHECK( library_map.find( entry_ids[1] )->second[0].polarity == 127 );
}

TEST_CASE( "Struct library creation with unused inputs", "[struct_library]" )
{
  /* Same compact function, different physical supports and area ordering. */
  std::string const library =
      "GATE unused_first 3 O=!b*c; PIN a UNKNOWN 1 999 1 0 1 0 PIN b INV 1 999 2 0 2 0 PIN c NONINV 1 999 3 0 3 0\n"
      "GATE unused_middle 1 O=!a*c; PIN a INV 1 999 1 0 1 0 PIN b UNKNOWN 1 999 2 0 2 0 PIN c NONINV 1 999 3 0 3 0\n"
      "GATE unused_last 2 O=!a*b; PIN a INV 1 999 1 0 1 0 PIN b NONINV 1 999 2 0 2 0 PIN c UNKNOWN 1 999 3 0 3 0\n"
      "GATE full_support 1 O=!a*b; PIN a INV 1 999 1 0 1 0 PIN b NONINV 1 999 2 0 2 0\n";
  std::vector<gate> gates;
  std::istringstream in( library );
  REQUIRE( lorina::read_genlib( in, genlib_reader( gates ) ) == lorina::return_code::success );
  REQUIRE( gates.size() == 4 );

  struct_library<3> lib( gates );
  lib.construct();

  auto const pattern = lib.get_pattern_id( 2, 2 );
  REQUIRE( pattern != UINT32_MAX );
  auto const* matches = lib.get_supergates_pattern( pattern, false );
  REQUIRE( matches != nullptr );
  REQUIRE( matches->size() == 4 );
  /* Equal-area matches prefer fewer physical pins. */
  CHECK( matches->front().root->root == &gates[3] );
  std::vector<std::vector<uint8_t>> const supports{ { 1, 2 }, { 0, 2 }, { 0, 1 }, { 0, 1 } };
  for ( auto i = 0u; i < gates.size(); ++i )
  {
    CAPTURE( gates[i].name );
    auto const it = std::find_if( matches->begin(), matches->end(), [&]( auto const& match ) {
      return match.root->root == &gates[i];
    } );
    REQUIRE( it != matches->end() );
    auto const& match = *it;
    CHECK( match.root->function == gates[i].function );
    CHECK( match.root->num_vars == gates[i].num_vars );
    auto support = match.permutation;
    std::sort( support.begin(), support.end() );
    REQUIRE( support == supports[i] );
    for ( auto j = 0u; j < match.permutation.size(); ++j )
    {
      CHECK( match.tdelay[j] == static_cast<float>( match.permutation[j] + 1 ) );
    }

    /* Evaluate the matched AND pattern in the original pin order. */
    for ( auto assignment = 0u; assignment < ( 1u << gates[i].num_vars ); ++assignment )
    {
      bool value = true;
      for ( auto j = 0u; j < match.permutation.size(); ++j )
      {
        value &= ( ( assignment >> match.permutation[j] ) ^ ( match.polarity >> j ) ) & 1;
      }
      CHECK( kitty::get_bit( gates[i].function, assignment ) == value );
    }
  }

  struct_library<3> larger_lib( gates );
  larger_lib.construct( 3 );
  auto const larger_pattern = larger_lib.get_pattern_id( 2, 2 );
  REQUIRE( larger_pattern != UINT32_MAX );
  auto const* larger_matches = larger_lib.get_supergates_pattern( larger_pattern, false );
  REQUIRE( larger_matches != nullptr );
  REQUIRE( larger_matches->size() == 3 );
  for ( auto const& match : *larger_matches )
  {
    CHECK( match.root->num_vars == 3 );
  }
}

TEST_CASE( "Struct library matches a multi-level pattern with unused inputs", "[struct_library]" )
{
  /* The second cell reuses the first cell's rule after bottom decomposition. */
  std::istringstream in(
      "GATE seed 1 O=!a+(c*d);\n"
      "PIN a INV 1 999 1 0 1 0\n"
      "PIN b UNKNOWN 1 999 2 0 2 0\n"
      "PIN c NONINV 1 999 3 0 3 0\n"
      "PIN d NONINV 1 999 4 0 4 0\n"
      "GATE unused 2 O=(b*!c)+(e*f);\n"
      "PIN a UNKNOWN 1 999 1 0 1 0\n"
      "PIN b NONINV 1 999 2 0 2 0\n"
      "PIN c INV 1 999 3 0 3 0\n"
      "PIN d UNKNOWN 1 999 4 0 4 0\n"
      "PIN e NONINV 1 999 5 0 5 0\n"
      "PIN f NONINV 1 999 6 0 6 0\n" );
  std::vector<gate> gates;
  REQUIRE( lorina::read_genlib( in, genlib_reader( gates ) ) == lorina::return_code::success );
  REQUIRE( gates.size() == 2 );

  struct_library<6> lib( gates );
  lib.construct();

  /* (x0 * x1) + (x2 * x3) = !(!(x0 * x1) * !(x2 * x3)). */
  auto const and_pattern = lib.get_pattern_id( 2, 2 );
  REQUIRE( and_pattern != UINT32_MAX );
  auto const pattern = lib.get_pattern_id( ( and_pattern << 1 ) | 1, ( and_pattern << 1 ) | 1 );
  REQUIRE( pattern != UINT32_MAX );
  auto const* matches = lib.get_supergates_pattern( pattern, true );
  REQUIRE( matches != nullptr );
  REQUIRE( matches->size() == 1 );
  auto const& match = matches->front();
  CHECK( match.root->root == &gates[1] );
  auto support = match.permutation;
  std::sort( support.begin(), support.end() );
  REQUIRE( support == std::vector<uint8_t>{ 1, 2, 4, 5 } );
  for ( auto j = 0u; j < match.permutation.size(); ++j )
  {
    CHECK( match.tdelay[j] == static_cast<float>( match.permutation[j] + 1 ) );
  }

  for ( auto assignment = 0u; assignment < ( 1u << gates[1].num_vars ); ++assignment )
  {
    auto input = [&]( auto j ) {
      return ( ( assignment >> match.permutation[j] ) ^ ( match.polarity >> j ) ) & 1;
    };
    bool const value = ( input( 0 ) && input( 1 ) ) || ( input( 2 ) && input( 3 ) );
    CHECK( kitty::get_bit( gates[1].function, assignment ) == value );
  }
}

TEST_CASE( "Struct library ignores constant and unary functions with unused inputs", "[struct_library]" )
{
  for ( auto const& expression : { "a*!a", "a+!a", "a", "!a", "b", "!b", "c", "!c" } )
  {
    CAPTURE( expression );
    std::istringstream in( std::string( "GATE unused 1 O=" ) + expression +
                           "; PIN a UNKNOWN 1 999 1 0 1 0 PIN b UNKNOWN 1 999 2 0 2 0 PIN c UNKNOWN 1 999 3 0 3 0\n" );
    std::vector<gate> gates;
    REQUIRE( lorina::read_genlib( in, genlib_reader( gates ) ) == lorina::return_code::success );
    REQUIRE( gates.size() == 1 );
    REQUIRE( gates[0].num_vars == 3 );

    struct_library<3> lib( gates );
    lib.construct();
    CHECK( lib.get_struct_library().empty() );
    CHECK( lib.get_pattern_id( 2, 2 ) == UINT32_MAX );
  }
}

TEST_CASE( "Struct library compacts large functions with unused inputs", "[struct_library]" )
{
  for ( auto const& expression : { "!i*c", "i+!h+!g+!f+!d+!c+!b" } )
  {
    CAPTURE( expression );
    bool const small = std::string( expression ) == "!i*c";
    std::string library = std::string( "GATE unused 1 O=" ) + expression + ";\n";
    /* Declare pins in reverse order, including unused pins and indices above six. */
    for ( char name = 'i'; name >= ( small ? 'c' : 'a' ); --name )
    {
      auto const delay = std::to_string( 'i' - name + 1 );
      library += std::string( "PIN " ) + name + " UNKNOWN 1 999 " + delay + " 0 " + delay + " 0\n";
    }
    std::vector<gate> gates;
    std::istringstream in( library );
    REQUIRE( lorina::read_genlib( in, genlib_reader( gates ) ) == lorina::return_code::success );
    REQUIRE( gates.size() == 1 );
    REQUIRE( gates[0].num_vars == ( small ? 7u : 9u ) );
    /* Physical pin order is i, h, g, f, e, d, c, followed by b, a for the larger cell. */
    std::vector<uint8_t> const support = small ? std::vector<uint8_t>{ 0, 6 } : std::vector<uint8_t>{ 0, 1, 2, 3, 5, 6, 7 };

    struct_library<9> lib( gates );
    lib.construct( 7 );
    REQUIRE( !lib.get_struct_library().empty() );
    CHECK( lib.get_num_large_gates() == 1 );
    for ( auto const& [label, matches] : lib.get_struct_library() )
    {
      REQUIRE( matches.size() == 1 );
      auto const& match = matches.front();
      auto permutation = match.permutation;
      std::sort( permutation.begin(), permutation.end() );
      REQUIRE( permutation == support );
      for ( auto j = 0u; j < match.permutation.size(); ++j )
      {
        CHECK( match.tdelay[j] == static_cast<float>( match.permutation[j] + 1 ) );
      }
      for ( auto assignment = 0u; assignment < ( 1u << gates[0].num_vars ); ++assignment )
      {
        bool value = true;
        for ( auto j = 0u; j < match.permutation.size(); ++j )
        {
          value &= ( ( assignment >> match.permutation[j] ) ^ ( match.polarity >> j ) ) & 1;
        }
        CHECK( kitty::get_bit( gates[0].function, assignment ) == ( value ^ ( label & 1 ) ) );
      }
    }
  }
}
