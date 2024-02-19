#include <catch.hpp>

#include <algorithm>
#include <cstdint>
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
  std::sort( entry_ids.begin(), entry_ids.end() );

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
  std::sort( entry_ids.begin(), entry_ids.end() );

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
  std::sort( entry_ids.begin(), entry_ids.end() );

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
  std::sort( entry_ids.begin(), entry_ids.end() );

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
  std::sort( entry_ids.begin(), entry_ids.end() );

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
