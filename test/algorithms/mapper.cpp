#include <catch.hpp>

#include <cstdint>
#include <vector>

#include <lorina/genlib.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/generators/arithmetic.hpp>



using namespace mockturtle;

std::string const test_library =  "GATE   inv1    1	O=!a;     PIN * INV 1 999 0.9 0.3 0.9 0.3\n"
                                  "GATE   inv2	  2	O=!a;		  PIN * INV 2 999 1.0 0.1 1.0 0.1\n"
                                  "GATE   nand2	  2	O=!(ab);  PIN * INV 1 999 1.0 0.2 1.0 0.2\n"
                                  "GATE   xor2	  5	O=[ab];   PIN * UNKNOWN 2 999 1.9 0.5 1.9 0.5\n"
                                  "GATE   mig3    3	O=<abc>;  PIN * INV 1 999 2.0 0.2 2.0 0.2\n"
                                  "GATE   zero	  0	O=0;\n"
                                  "GATE   one		  0	O=1;";


TEST_CASE( "Map of MAJ3", "[mapper]" )
{
  std::vector<mockturtle::gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, mockturtle::genlib_reader( gates ) );
  
  CHECK( result == lorina::return_code::success );

  mockturtle::tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f = aig.create_maj( a, b, c );
  aig.create_po( f );

  map_params ps;
  map_stats st;
  klut_network luts = tech_map( aig, lib, ps, &st );

  CHECK( luts.size() == 6u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 1u );
  CHECK( st.area == 3.0f );
  CHECK( st.delay == 2.0f );
}

TEST_CASE( "Map of bad MAJ3 and constant output", "[mapper]" )
{
  std::vector<mockturtle::gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, mockturtle::genlib_reader( gates ) );
  
  CHECK( result == lorina::return_code::success );

  mockturtle::tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f = aig.create_maj( a, aig.create_maj( a, b, c ), c );
  aig.create_po( f );
  aig.create_po( aig.get_constant( true ) );

  map_params ps;
  map_stats st;
  klut_network luts = tech_map( aig, lib, ps, &st );

  CHECK( luts.size() == 6u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 1u );
  CHECK( st.area == 3.0f );
  CHECK( st.delay == 2.0f );
}

TEST_CASE( "Map of full adder", "[mapper]" )
{
  std::vector<mockturtle::gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, mockturtle::genlib_reader( gates ) );
  
  CHECK( result == lorina::return_code::success );

  mockturtle::tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto [sum, carry] = full_adder( aig, a, b, c );
  aig.create_po( sum );
  aig.create_po( carry );

  map_params ps;
  map_stats st;
  klut_network luts = tech_map( aig, lib, ps, &st );

  const float eps{0.005f};

  CHECK( luts.size() == 8u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 3u );
  CHECK( st.area > 13.0f - eps );
  CHECK( st.area < 13.0f + eps );
  CHECK( st.delay > 3.8f - eps );
  CHECK( st.delay < 3.8f + eps );
}

TEST_CASE( "Map with inverters", "[mapper]" )
{
  std::vector<mockturtle::gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, mockturtle::genlib_reader( gates ) );
  
  CHECK( result == lorina::return_code::success );

  mockturtle::tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f1 = aig.create_and( !a, b );
  const auto f2 = aig.create_and( f1, !c );

  aig.create_po( f2 );

  map_params ps;
  map_stats st;
  klut_network luts = tech_map( aig, lib, ps, &st );

  const float eps{0.005f};

  CHECK( luts.size() == 11u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 6u );
  CHECK( st.area > 8.0f - eps );
  CHECK( st.area < 8.0f + eps );
  CHECK( st.delay > 4.7f - eps );
  CHECK( st.delay < 4.7f + eps );
}

TEST_CASE( "Map for inverters minimization", "[mapper]" )
{
  std::vector<mockturtle::gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, mockturtle::genlib_reader( gates ) );
  
  CHECK( result == lorina::return_code::success );

  mockturtle::tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f = aig.create_maj( !a, !b, !c );
  aig.create_po( f );

  map_params ps;
  map_stats st;
  klut_network luts = tech_map( aig, lib, ps, &st );

  const float eps{0.005f};

  CHECK( luts.size() == 7u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 2u );
  CHECK( st.area > 4.0f - eps );
  CHECK( st.area < 4.0f + eps );
  CHECK( st.delay > 2.9f - eps );
  CHECK( st.delay < 2.9f + eps );
}