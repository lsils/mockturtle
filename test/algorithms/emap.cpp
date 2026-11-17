#include <catch.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <lorina/genlib.hpp>
#include <lorina/super.hpp>
#include <mockturtle/algorithms/emap.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/io/super_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/block.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/binding_view.hpp>
#include <mockturtle/views/cell_view.hpp>
#include <mockturtle/views/dont_touch_view.hpp>

using namespace mockturtle;

std::string const test_library = "GATE   inv1    1 O=!a;            PIN * INV 1 999 0.9 0.3 0.9 0.3\n"
                                 "GATE   inv2    2 O=!a;            PIN * INV 2 999 1.0 0.1 1.0 0.1\n"
                                 "GATE   nand2   2 O=!(a*b);        PIN * INV 1 999 1.0 0.2 1.0 0.2\n"
                                 "GATE   and2    3 O=a*b;           PIN * INV 1 999 1.7 0.2 1.7 0.2\n"
                                 "GATE   xor2    4 O=a^b;           PIN * UNKNOWN 2 999 1.9 0.5 1.9 0.5\n"
                                 "GATE   mig3    3 O=a*b+a*c+b*c;   PIN * INV 1 999 2.0 0.2 2.0 0.2\n"
                                 "GATE   xor3    5 O=a^b^c;         PIN * UNKNOWN 2 999 3.0 0.5 3.0 0.5\n"
                                 "GATE   buf     2 O=a;             PIN * NONINV 1 999 1.0 0.0 1.0 0.0\n"
                                 "GATE   zero    0 O=CONST0;\n"
                                 "GATE   one     0 O=CONST1;\n"
                                 "GATE   ha      5 C=a*b;           PIN * INV 1 999 1.7 0.4 1.7 0.4\n"
                                 "GATE   ha      5 S=!a*b+a*!b;     PIN * INV 1 999 2.1 0.4 2.1 0.4\n"
                                 "GATE   fa      6 C=a*b+a*c+b*c;   PIN * INV 1 999 2.1 0.4 2.1 0.4\n"
                                 "GATE   fa      6 S=a^b^c;         PIN * INV 1 999 3.0 0.4 3.0 0.4";

std::string const large_library = "GATE   inv1    1 O=!a;            PIN * INV 1 999 0.9 0.3 0.9 0.3\n"
                                  "GATE   inv2    2 O=!a;            PIN * INV 2 999 1.0 0.1 1.0 0.1\n"
                                  "GATE   nand2   2 O=!(a*b);        PIN * INV 1 999 1.0 0.2 1.0 0.2\n"
                                  "GATE   xor2    5 O=a^b;           PIN * UNKNOWN 2 999 1.9 0.5 1.9 0.5\n"
                                  "GATE   mig3    3 O=a*b+a*c+b*c;   PIN * INV 1 999 2.0 0.2 2.0 0.2\n"
                                  "GATE   buf     2 O=a;             PIN * NONINV 1 999 1.0 0.0 1.0 0.0\n"
                                  "GATE   zero    0 O=CONST0;\n"
                                  "GATE   one     0 O=CONST1;\n"
                                  "GATE   nand8   8 O=!(a*b*c*d*e*f*g*h);   PIN * INV 1 999 4.0 0.2 4.0 0.2\n";

std::string const super_library = "test.genlib\n"
                                  "3\n"
                                  "2\n"
                                  "6\n"
                                  "* nand2 1 0\n"
                                  "inv1 3\n"
                                  "* nand2 2 4\n"
                                  "\0";

TEST_CASE( "Emap on MAJ3", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f = aig.create_maj( a, b, c );
  aig.create_po( f );

  emap_params ps;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  CHECK( luts.size() == 6u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 1u );
  CHECK( st.area == 3.0f );
  CHECK( st.delay == 2.0f );
}

TEST_CASE( "Emap on bad MAJ3 and constant output", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f = aig.create_maj( a, aig.create_maj( a, b, c ), c );
  aig.create_po( f );
  aig.create_po( aig.get_constant( true ) );

  emap_params ps;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  CHECK( luts.size() == 6u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 1u );
  CHECK( st.area == 3.0f );
  CHECK( st.delay == 2.0f );
}

TEST_CASE( "Emap on full adder 1", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto [sum, carry] = full_adder( aig, a, b, c );
  aig.create_po( sum );
  aig.create_po( carry );

  emap_params ps;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 7u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 2u );
  CHECK( st.area > 8.0f - eps );
  CHECK( st.area < 8.0f + eps );
  CHECK( st.delay > 3.0f - eps );
  CHECK( st.delay < 3.0f + eps );
}

TEST_CASE( "Emap on full adder 2", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3, classification_type::p_configurations> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto [sum, carry] = full_adder( aig, a, b, c );
  aig.create_po( sum );
  aig.create_po( carry );

  emap_params ps;
  ps.cut_enumeration_ps.minimize_truth_table = false;
  ps.ela_rounds = 1;
  ps.eswp_rounds = 2;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 7u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 2u );
  CHECK( st.area > 8.0f - eps );
  CHECK( st.area < 8.0f + eps );
  CHECK( st.delay > 3.0f - eps );
  CHECK( st.delay < 3.0f + eps );
}

TEST_CASE( "Emap on full adder 1 with cells", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto [sum, carry] = full_adder( aig, a, b, c );
  aig.create_po( sum );
  aig.create_po( carry );

  emap_params ps;
  emap_stats st;
  cell_view<block_network> luts = emap( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 7u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 2u );
  CHECK( st.area > 8.0f - eps );
  CHECK( st.area < 8.0f + eps );
  CHECK( st.delay > 3.0f - eps );
  CHECK( st.delay < 3.0f + eps );
}

TEST_CASE( "Emap on full adder 2 with cells", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3, classification_type::p_configurations> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto [sum, carry] = full_adder( aig, a, b, c );
  aig.create_po( sum );
  aig.create_po( carry );

  emap_params ps;
  ps.cut_enumeration_ps.minimize_truth_table = false;
  ps.ela_rounds = 1;
  ps.eswp_rounds = 2;
  emap_stats st;
  cell_view<block_network> luts = emap( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 7u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 2u );
  CHECK( st.area > 8.0f - eps );
  CHECK( st.area < 8.0f + eps );
  CHECK( st.delay > 3.0f - eps );
  CHECK( st.delay < 3.0f + eps );
}

TEST_CASE( "Emap on ripple carry adder with multi-output gates", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library_params tps;
  tps.load_multioutput_gates_single = false;
  tech_library<3, classification_type::p_configurations> lib( gates, tps );

  aig_network aig;
  
  std::vector<aig_network::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  emap_params ps;
  ps.map_multioutput = true;
  ps.area_oriented_mapping = true;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 34u );
  CHECK( luts.num_pis() == 16u );
  CHECK( luts.num_pos() == 9u );
  CHECK( luts.num_gates() == 16u );
  CHECK( st.area > 47.0f - eps );
  CHECK( st.area < 47.0f + eps );
  CHECK( st.delay > 17.3f - eps );
  CHECK( st.delay < 17.3f + eps );
  CHECK( st.multioutput_gates == 8 );
}

TEST_CASE( "Emap on ripple carry adder with multi-output cells", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library_params tps;
  tps.load_multioutput_gates_single = false;
  tech_library<3, classification_type::p_configurations> lib( gates, tps );

  aig_network aig;
  
  std::vector<aig_network::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  emap_params ps;
  ps.map_multioutput = true;
  ps.area_oriented_mapping = true;
  emap_stats st;
  cell_view<block_network> luts = emap( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 26u );
  CHECK( luts.num_pis() == 16u );
  CHECK( luts.num_pos() == 9u );
  CHECK( luts.num_gates() == 8u );
  CHECK( st.area > 47.0f - eps );
  CHECK( st.area < 47.0f + eps );
  CHECK( st.delay > 17.3f - eps );
  CHECK( st.delay < 17.3f + eps );
  CHECK( st.multioutput_gates == 8 );
}

TEST_CASE( "Emap on multiplier with multi-output gates", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library_params tps;
  tps.load_minimum_size_only = false;
  tps.load_multioutput_gates_single = true;
  tech_library<3> lib( gates, tps );

  aig_network aig;

  std::vector<typename aig_network::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );

  for ( auto const& o : carry_ripple_multiplier( aig, a, b ) )
  {
    aig.create_po( o );
  }

  CHECK( aig.num_pis() == 16 );
  CHECK( aig.num_pos() == 16 );

  emap_params ps;
  ps.map_multioutput = true;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 235u );
  CHECK( luts.num_pis() == 16u );
  CHECK( luts.num_pos() == 16u );
  CHECK( luts.num_gates() == 217u );
  CHECK( st.area > 612.0f - eps );
  CHECK( st.area < 612.0f + eps );
  CHECK( st.delay > 33.60f - eps );
  CHECK( st.delay < 33.60f + eps );
  CHECK( st.multioutput_gates == 40 );
}

TEST_CASE( "Emap with inverters", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f1 = aig.create_and( !a, b );
  const auto f2 = aig.create_and( f1, !c );

  aig.create_po( f2 );

  emap_params ps;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 9u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 4u );
  CHECK( st.area > 8.0f - eps );
  CHECK( st.area < 8.0f + eps );
  CHECK( st.delay > 4.3f - eps );
  CHECK( st.delay < 4.3f + eps );
}

TEST_CASE( "Emap with inverters minimization", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f = aig.create_maj( !a, !b, !c );
  aig.create_po( f );

  emap_params ps;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 7u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 2u );
  CHECK( st.area > 4.0f - eps );
  CHECK( st.area < 4.0f + eps );
  CHECK( st.delay > 2.9f - eps );
  CHECK( st.delay < 2.9f + eps );
}

TEST_CASE( "Emap on buffer and constant outputs", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3, classification_type::np_configurations> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();

  const auto n5 = aig.create_and( a, d );
  const auto n6 = aig.create_and( a, !c );
  const auto n7 = aig.create_and( !c, n5 );
  const auto n8 = aig.create_and( c, n6 );
  const auto n9 = aig.create_and( !n6, n7 );
  const auto n10 = aig.create_and( n7, n8 );
  const auto n11 = aig.create_and( a, n10 );
  const auto n12 = aig.create_and( !d, n11 );
  const auto n13 = aig.create_and( !d, !n7 );
  const auto n14 = aig.create_and( !n6, !n7 );

  aig.create_po( aig.get_constant( true ) );
  aig.create_po( b );
  aig.create_po( n9 );
  aig.create_po( n12 );
  aig.create_po( !n13 );
  aig.create_po( n14 );

  emap_params ps;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 10u );
  CHECK( luts.num_pis() == 4u );
  CHECK( luts.num_pos() == 6u );
  CHECK( luts.num_gates() == 4u );
  CHECK( st.area > 7.0f - eps );
  CHECK( st.area < 7.0f + eps );
  CHECK( st.delay > 1.9f - eps );
  CHECK( st.delay < 1.9f + eps );
}

TEST_CASE( "Emap with boolean matching", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( large_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<8> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();
  const auto e = aig.create_pi();
  const auto f = aig.create_pi();
  const auto g = aig.create_pi();
  const auto h = aig.create_pi();

  const auto f1 = aig.create_and( !a, b );
  const auto f2 = aig.create_and( f1, !c );
  const auto f3 = aig.create_and( d, e );
  const auto f4 = aig.create_and( f, !g );
  const auto f5 = aig.create_and( f4, h );
  const auto f6 = aig.create_and( f2, f3 );
  const auto f7 = aig.create_and( f5, f6 );

  aig.create_po( f7 );

  emap_params ps;
  ps.matching_mode = emap_params::boolean;
  emap_stats st;
  cell_view<block_network> ntk = emap<8>( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( ntk.size() == 27u );
  CHECK( ntk.num_pis() == 8u );
  CHECK( ntk.num_pos() == 1u );
  CHECK( ntk.num_gates() == 17u );
  CHECK( st.area > 24.0f - eps );
  CHECK( st.area < 24.0f + eps );
  CHECK( st.delay > 8.5f - eps );
  CHECK( st.delay < 8.5f + eps );
}

TEST_CASE( "Emap with structural matching", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( large_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<8> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();
  const auto e = aig.create_pi();
  const auto f = aig.create_pi();
  const auto g = aig.create_pi();
  const auto h = aig.create_pi();

  const auto f1 = aig.create_and( !a, b );
  const auto f2 = aig.create_and( f1, !c );
  const auto f3 = aig.create_and( d, e );
  const auto f4 = aig.create_and( f, !g );
  const auto f5 = aig.create_and( f4, h );
  const auto f6 = aig.create_and( f2, f3 );
  const auto f7 = aig.create_and( f5, f6 );

  aig.create_po( f7 );

  emap_params ps;
  ps.matching_mode = emap_params::structural;
  emap_stats st;
  cell_view<block_network> ntk = emap<8>( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( ntk.size() == 15u );
  CHECK( ntk.num_pis() == 8u );
  CHECK( ntk.num_pos() == 1u );
  CHECK( ntk.num_gates() == 5u );
  CHECK( st.area > 12.0f - eps );
  CHECK( st.area < 12.0f + eps );
  CHECK( st.delay > 5.8f - eps );
  CHECK( st.delay < 5.8f + eps );
}

TEST_CASE( "Emap with hybrid matching", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( large_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<8> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();
  const auto e = aig.create_pi();
  const auto f = aig.create_pi();
  const auto g = aig.create_pi();
  const auto h = aig.create_pi();

  const auto f1 = aig.create_and( !a, b );
  const auto f2 = aig.create_and( f1, !c );
  const auto f3 = aig.create_and( d, e );
  const auto f4 = aig.create_and( f, !g );
  const auto f5 = aig.create_and( f4, h );
  const auto f6 = aig.create_and( f2, f3 );
  const auto f7 = aig.create_and( f5, f6 );

  aig.create_po( f7 );

  emap_params ps;
  ps.matching_mode = emap_params::hybrid;
  emap_stats st;
  cell_view<block_network> ntk = emap<8>( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( ntk.size() == 15u );
  CHECK( ntk.num_pis() == 8u );
  CHECK( ntk.num_pos() == 1u );
  CHECK( ntk.num_gates() == 5u );
  CHECK( st.area > 12.0f - eps );
  CHECK( st.area < 12.0f + eps );
  CHECK( st.delay > 5.8f - eps );
  CHECK( st.delay < 5.8f + eps );
}

TEST_CASE( "Emap with arrival times", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( large_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<6> lib( gates );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();
  const auto e = aig.create_pi();
  const auto f = aig.create_pi();
  const auto g = aig.create_pi();
  const auto h = aig.create_pi();

  const auto f1 = aig.create_and( !a, b );
  const auto f2 = aig.create_and( f1, !c );
  const auto f3 = aig.create_and( d, e );
  const auto f4 = aig.create_and( f, !g );
  const auto f5 = aig.create_and( f4, h );
  const auto f6 = aig.create_and( f2, f3 );
  const auto f7 = aig.create_and( f5, f6 );

  aig.create_po( f7 );

  emap_params ps;
  ps.matching_mode = emap_params::boolean;
  emap_stats st;

  ps.arrival_times = std::vector<double>( 8 );
  ps.arrival_times[0] = 0.0;
  ps.arrival_times[1] = 1.0;
  ps.arrival_times[2] = 2.0;
  ps.arrival_times[3] = 3.0;
  ps.arrival_times[4] = 4.0;
  ps.arrival_times[5] = 5.0;
  ps.arrival_times[6] = 6.0;
  ps.arrival_times[7] = 7.0;

  cell_view<block_network> ntk = emap<6>( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( ntk.size() == 27u );
  CHECK( ntk.num_pis() == 8u );
  CHECK( ntk.num_pos() == 1u );
  CHECK( ntk.num_gates() == 17u );
  CHECK( st.area > 24.0f - eps );
  CHECK( st.area < 24.0f + eps );
  CHECK( st.delay > 12.6f - eps );
  CHECK( st.delay < 12.6f + eps );
}

TEST_CASE( "Emap with global required times", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<6> lib( gates );

  aig_network aig;
  
  std::vector<aig_network::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  emap_params ps;
  ps.matching_mode = emap_params::boolean;
  ps.required_time = 20.0; // real delay 15.7
  emap_stats st;

  cell_view<block_network> ntk = emap<6>( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( ntk.size() == 34 );
  CHECK( ntk.num_pis() == 16u );
  CHECK( ntk.num_pos() == 9u );
  CHECK( ntk.num_gates() == 16u );
  CHECK( st.area > 63.0f - eps );
  CHECK( st.area < 63.0f + eps );
  CHECK( st.delay < 20.0f + eps );
}

TEST_CASE( "Emap with required times", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<6> lib( gates );

  aig_network aig;
  
  std::vector<aig_network::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );

  carry_ripple_adder_inplace( aig, a, b, carry );

  emap_params ps;
  ps.matching_mode = emap_params::boolean;
  // ps.required_time = 20.0; // real delay 15.7
  emap_stats st;

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); ps.required_times.push_back( 19.0 ); } );
  aig.create_po( carry );
  ps.required_times.push_back( 20.0 );

  cell_view<block_network> ntk = emap<6>( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( ntk.size() == 34 );
  CHECK( ntk.num_pis() == 16u );
  CHECK( ntk.num_pos() == 9u );
  CHECK( ntk.num_gates() == 16u );
  CHECK( st.area > 63.0f - eps );
  CHECK( st.area < 63.0f + eps );
  CHECK( st.delay < 20.0f + eps );
}

TEST_CASE( "Emap with required time relaxation", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<6> lib( gates );

  aig_network aig;
  
  std::vector<aig_network::signal> a( 8 ), b( 8 );
  std::generate( a.begin(), a.end(), [&aig]() { return aig.create_pi(); } );
  std::generate( b.begin(), b.end(), [&aig]() { return aig.create_pi(); } );
  auto carry = aig.get_constant( false );

  carry_ripple_adder_inplace( aig, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { aig.create_po( f ); } );
  aig.create_po( carry );

  emap_params ps;
  ps.matching_mode = emap_params::boolean;
  ps.relax_required = 27.5; // real delay 15.7
  emap_stats st;

  cell_view<block_network> ntk = emap<6>( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( ntk.size() == 34 );
  CHECK( ntk.num_pis() == 16u );
  CHECK( ntk.num_pos() == 9u );
  CHECK( ntk.num_gates() == 16u );
  CHECK( st.area > 63.0f - eps );
  CHECK( st.area < 63.0f + eps );
  CHECK( st.delay < 20.0f + eps );
}

TEST_CASE( "Emap with supergates", "[emap]" )
{
  std::vector<gate> gates;
  super_lib super_data;

  std::istringstream in_lib( test_library );
  auto result = lorina::read_genlib( in_lib, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  std::istringstream in_super( super_library );
  result = lorina::read_super( in_super, super_reader( super_data ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3, classification_type::p_configurations> lib( gates, super_data );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto n4 = aig.create_and( a, b );
  const auto n5 = aig.create_and( b, c );
  const auto f = aig.create_and( n4, n5 );
  aig.create_po( f );

  emap_params ps;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 8u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 3u );
  CHECK( st.area == 9.0f );
  CHECK( st.delay > 3.4f - eps );
  CHECK( st.delay < 3.4f + eps );
}

TEST_CASE( "Emap with supergates 2", "[emap]" )
{
  std::vector<gate> gates;
  super_lib super_data;

  std::istringstream in_lib( test_library );
  auto result = lorina::read_genlib( in_lib, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  std::istringstream in_super( super_library );
  result = lorina::read_super( in_super, super_reader( super_data ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3, classification_type::p_configurations> lib( gates, super_data );

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto n4 = aig.create_and( a, b );
  const auto n5 = aig.create_and( b, c );
  const auto f = aig.create_and( n4, n5 );
  aig.create_po( f );

  emap_params ps;
  emap_stats st;
  cell_view<block_network> luts = emap( aig, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 8u );
  CHECK( luts.num_pis() == 3u );
  CHECK( luts.num_pos() == 1u );
  CHECK( luts.num_gates() == 3u );
  CHECK( st.area == 9.0f );
  CHECK( st.delay > 3.4f - eps );
  CHECK( st.delay < 3.4f + eps );
}

TEST_CASE( "Emap on circuit with don't touch gates", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3, classification_type::np_configurations> lib( gates );

  klut_network klut;
  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto d = klut.create_pi();

  const auto n5 = klut.create_xor( c, d );
  const auto n6 = klut.create_not( n5 );
  const auto n7 = klut.create_xor( a, b );
  const auto sum = klut.create_xor( n6, n7 );
  const auto carry = klut.create_maj( a, b, n5 );

  klut.create_po( sum );
  klut.create_po( carry );

  binding_view<klut_network> b_klut{ klut, gates };
  dont_touch_view<binding_view<klut_network>> db_klut{ b_klut };

  db_klut.add_binding( klut.get_node( n5 ), 3 );
  db_klut.select_dont_touch( klut.get_node( n5 ) );
  db_klut.add_binding( klut.get_node( n6 ), 0 );
  db_klut.select_dont_touch( klut.get_node( n6 ) );

  emap_params ps;
  ps.map_multioutput = true;
  ps.area_oriented_mapping = true;
  emap_stats st;
  binding_view<klut_network> luts = emap_klut( klut, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 10u );
  CHECK( luts.num_pis() == 4u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 4u );
  CHECK( st.area > 11.0f - eps );
  CHECK( st.area < 11.0f + eps );
  CHECK( st.delay > 5.8f - eps );
  CHECK( st.delay < 5.8f + eps );
}

TEST_CASE( "Emap on circuit with don't touch cells", "[emap]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( result == lorina::return_code::success );

  tech_library<3, classification_type::np_configurations> lib( gates );

  klut_network klut;
  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto d = klut.create_pi();

  const auto n5 = klut.create_xor( c, d );
  const auto n6 = klut.create_not( n5 );
  const auto n7 = klut.create_xor( a, b );
  const auto sum = klut.create_xor( n6, n7 );
  const auto carry = klut.create_maj( a, b, n5 );

  klut.create_po( sum );
  klut.create_po( carry );

  binding_view<klut_network> b_klut{ klut, gates };
  dont_touch_view<binding_view<klut_network>> db_klut{ b_klut };

  db_klut.add_binding( klut.get_node( n5 ), 3 );
  db_klut.select_dont_touch( klut.get_node( n5 ) );
  db_klut.add_binding( klut.get_node( n6 ), 0 );
  db_klut.select_dont_touch( klut.get_node( n6 ) );

  emap_params ps;
  ps.map_multioutput = true;
  ps.area_oriented_mapping = true;
  emap_stats st;
  cell_view<block_network> luts = emap( klut, lib, ps, &st );

  const float eps{ 0.005f };

  CHECK( luts.size() == 9u );
  CHECK( luts.num_pis() == 4u );
  CHECK( luts.num_pos() == 2u );
  CHECK( luts.num_gates() == 3u );
  CHECK( st.area > 11.0f - eps );
  CHECK( st.area < 11.0f + eps );
  CHECK( st.delay > 5.8f - eps );
  CHECK( st.delay < 5.8f + eps );
}

namespace
{

/* Ordinary gates are deliberately slower than the two-output FA. */
std::string const recovery_library = R"(GATE inv 1 O=!a;
PIN a UNKNOWN 1 999 0.1 0 0.1 0
GATE buf 1 O=a;
PIN a UNKNOWN 1 999 0.1 0 0.1 0
GATE zero 0 O=CONST0;
GATE one 0 O=CONST1;
GATE nand2 2 O=!(a*b);
PIN a UNKNOWN 1 999 1 0 1 0
PIN b UNKNOWN 1 999 1 0 1 0
GATE and2 3 O=a*b;
PIN a UNKNOWN 1 999 1 0 1 0
PIN b UNKNOWN 1 999 1 0 1 0
GATE nor2 2 O=!(a+b);
PIN a UNKNOWN 1 999 1 0 1 0
PIN b UNKNOWN 1 999 1 0 1 0
GATE xor2 4 O=a^b;
PIN a UNKNOWN 1 999 3 0 3 0
PIN b UNKNOWN 1 999 3 0 3 0
GATE maj3 5 O=a*b+a*c+b*c;
PIN a UNKNOWN 1 999 2 0 2 0
PIN b UNKNOWN 1 999 2 0 2 0
PIN c UNKNOWN 1 999 2 0 2 0
GATE fa 6 C=a*b+a*c+b*c;
PIN a UNKNOWN 1 999 0.2 0 0.2 0
PIN b UNKNOWN 1 999 0.3 0 0.3 0
PIN c UNKNOWN 1 999 0.4 0 0.4 0
GATE fa 6 S=a^b^c;
PIN a UNKNOWN 1 999 0.5 0 0.5 0
PIN b UNKNOWN 1 999 0.7 0 0.7 0
PIN c UNKNOWN 1 999 0.9 0 0.9 0
)";

tech_library<3> recovery_tech_library( std::string const& text )
{
  std::vector<gate> gates;
  std::istringstream in( text );
  REQUIRE( lorina::read_genlib( in, genlib_reader( gates ) ) == lorina::return_code::success );
  return tech_library<3>( gates );
}

uint32_t count_cells( cell_view<block_network> const& mapped, std::string const& name )
{
  uint32_t count = 0;
  mapped.foreach_gate( [&]( auto n ) {
    REQUIRE( mapped.has_cell( n ) );
    count += mapped.get_cell( n ).name == name;
  } );
  return count;
}

/* Evaluate the physical library functions, including unused output pins, instead
 * of using block_network's single-output simulation interface. */
void check_recovery_mapping( aig_network const& source, cell_view<block_network> const& mapped,
                             emap_params const& ps, emap_stats const& st )
{
  REQUIRE_FALSE( st.mapping_error );
  REQUIRE( mapped.num_pis() == source.num_pis() );
  REQUIRE( mapped.num_pos() == source.num_pos() );
  CHECK( mapped.compute_area() == Approx( st.area ) );

  std::vector<std::array<double, 2>> arrival( mapped.size(), { 0.0, 0.0 } );
  mapped.foreach_pi( [&]( auto n, auto i ) {
    arrival[n][0] = ps.arrival_times.empty() ? 0.0 : ps.arrival_times[i];
  } );
  mapped.foreach_gate( [&]( auto n ) {
    REQUIRE( mapped.has_cell( n ) );
    auto const& cell = mapped.get_cell( n );
    REQUIRE( cell.gates.size() == mapped.num_outputs( n ) );
    for ( auto pin = 0u; pin < cell.gates.size(); ++pin )
    {
      auto const& gate = cell.gates[pin];
      REQUIRE( gate.num_vars == mapped.fanin_size( n ) );
      REQUIRE( gate.pins.size() == mapped.fanin_size( n ) );
      CHECK( gate.function == mapped.node_function_pin( n, pin ) );
      mapped.foreach_fanin( n, [&]( auto f, auto i ) {
        REQUIRE_FALSE( mapped.is_complemented( f ) );
        arrival[n][pin] = std::max( arrival[n][pin], arrival[mapped.get_node( f )][mapped.get_output_pin( f )] +
                                                         std::max( gate.pins[i].rise_block_delay, gate.pins[i].fall_block_delay ) );
      } );
    }
  } );
  double delay = 0.0;
  mapped.foreach_po( [&]( auto f, auto i ) {
    auto const time = arrival[mapped.get_node( f )][mapped.get_output_pin( f )];
    delay = std::max( delay, time );
    if ( !ps.required_times.empty() )
      CHECK( time <= ps.required_times[i] + 0.0005 );
  } );
  CHECK( delay == Approx( st.delay ) );

  auto const expected = simulate<kitty::dynamic_truth_table>( source, default_simulator<kitty::dynamic_truth_table>( source.num_pis() ) );
  for ( auto assignment = 0u; assignment < ( 1u << source.num_pis() ); ++assignment )
  {
    std::vector<std::array<bool, 2>> values( mapped.size(), { false, false } );
    values[mapped.get_node( mapped.get_constant( true ) )][0] = true;
    mapped.foreach_pi( [&]( auto n, auto i ) { values[n][0] = ( assignment >> i ) & 1; } );
    auto value = [&]( auto f ) { return values[mapped.get_node( f )][mapped.get_output_pin( f )] != mapped.is_complemented( f ); };
    mapped.foreach_gate( [&]( auto n ) {
      uint32_t bits = 0;
      mapped.foreach_fanin( n, [&]( auto f, auto i ) { bits |= uint32_t( value( f ) ) << i; } );
      auto const& cell = mapped.get_cell( n );
      for ( auto pin = 0u; pin < cell.gates.size(); ++pin )
        values[n][pin] = kitty::get_bit( cell.gates[pin].function, bits );
    } );
    mapped.foreach_po( [&]( auto f, auto i ) { CHECK( value( f ) == bool( kitty::get_bit( expected[i], assignment ) ) ); } );
  }
}

} // namespace

TEST_CASE( "Emap retains physical full adders with unused outputs", "[emap][emap_recovery]" )
{
  auto lib = recovery_tech_library( recovery_library );
  for ( auto rounds : { 1u, 4u } )
  {
    for ( auto carry_only : { false, true } )
    {
      for ( auto inverted : { false, true } )
      {
        CAPTURE( rounds, carry_only, inverted );
        aig_network aig;
        auto a = aig.create_pi(), b = aig.create_pi(), c = aig.create_pi();
        auto const [sum, carry] = full_adder( aig, a, b, c );
        // Tuple discovery requires a source fanout on the lower-index sum.
        // Its dangling consumer disappears from the cover, leaving only carry.
        if ( carry_only )
          aig.create_and( sum, a );
        auto live = carry_only ? carry : sum;
        aig.create_po( live );
        if ( inverted )
          aig.create_po( !live );
        emap_params ps;
        ps.map_multioutput = true;
        ps.area_flow_rounds = rounds;
        ps.ela_rounds = rounds;
        emap_stats st;
        auto mapped = emap( aig, lib, ps, &st );
        check_recovery_mapping( aig, mapped, ps, st );
        if ( carry_only )
        {
          // Export the full physical FA even when only its carry is live.
          std::ostringstream os;
          write_verilog_with_cell( mapped, os );
          auto const verilog = os.str();
          CHECK_FALSE( verilog.empty() );
          // Do not assume the writer uses GENLIB output names as Verilog ports.
          // The physical binding checker above verifies both FA functions.
          CHECK( verilog.find( "module" ) != std::string::npos );
          CHECK( verilog.find( "endmodule" ) != std::string::npos );
        }
        CHECK( count_cells( mapped, "fa" ) == 1u );
        CHECK( count_cells( mapped, "inv" ) == uint32_t( inverted ) );
        CHECK( mapped.num_gates() == ( inverted ? 2u : 1u ) );
        CHECK( mapped.compute_area() == Approx( inverted ? 7.0 : 6.0 ) );
        CHECK( st.delay == Approx( ( carry_only ? 0.4 : 0.9 ) + ( inverted ? 0.1 : 0.0 ) ) );
      }
    }
  }
}

TEST_CASE( "Emap singleton replacement uses full-cell exact area", "[emap][emap_recovery]" )
{
  for ( auto price : { "6", "6.00025", "5.99975" } )
  {
    CAPTURE( price );
    auto lib = recovery_tech_library( recovery_library + "GATE xor3 " + price + " O=a^b^c;\n"
                                                                                "PIN a UNKNOWN 1 999 0.5 0 0.5 0\n"
                                                                                "PIN b UNKNOWN 1 999 0.7 0 0.7 0\n"
                                                                                "PIN c UNKNOWN 1 999 0.9 0 0.9 0\n" );
    aig_network aig;
    auto a = aig.create_pi(), b = aig.create_pi(), c = aig.create_pi();
    auto const [sum, carry] = full_adder( aig, a, b, c );
    aig.create_po( sum );
    emap_params ps;
    ps.map_multioutput = true;
    ps.area_flow_rounds = 1;
    ps.ela_rounds = 1;
    emap_stats st;
    auto mapped = emap( aig, lib, ps, &st );
    check_recovery_mapping( aig, mapped, ps, st );
    bool const retain = std::string( price ) == "6.00025";
    CHECK( count_cells( mapped, "fa" ) == uint32_t( retain ) );
    CHECK( count_cells( mapped, "xor3" ) == uint32_t( !retain ) );
    CHECK( mapped.num_gates() == 1u );
    CHECK( mapped.compute_area() == Approx( retain ? 6.0 : std::stod( price ) ).margin( 0.000001 ) );
    CHECK( st.delay == Approx( 0.9 ) );
  }
}

TEST_CASE( "Emap switching recovery accepts larger or slower singleton cells", "[emap][emap_recovery]" )
{
  for ( auto price : { "8", "5" } )
  {
    auto lib = recovery_tech_library( std::string( "GATE inv 1 O=!a; PIN * UNKNOWN 1 999 0.1 0 0.1 0\n"
                                                   "GATE nand2 2 O=!(a*b); PIN * UNKNOWN 1 999 1 0 1 0\n"
                                                   "GATE xor3 " ) +
                                      price + " O=a^b^c; PIN * UNKNOWN 1 999 2 0 2 0\n"
                                              "GATE fa 4 C=a*b+a*c+b*c; PIN * UNKNOWN 1 999 0.3 0 0.3 0\n"
                                              "GATE fa 4 S=!(a^b^c); PIN * UNKNOWN 1 999 1 0 1 0\n" );
    for ( auto switching : { 0u, 1u } )
    {
      CAPTURE( price, switching );
      aig_network aig;
      auto a = aig.create_pi(), b = aig.create_pi(), c = aig.create_pi();
      auto const [sum, carry] = full_adder( aig, a, b, c );
      aig.create_po( sum );
      emap_params ps;
      ps.map_multioutput = true;
      ps.area_oriented_mapping = true;
      ps.area_flow_rounds = 1;
      // Exact-area rounds tighten the current timing bound; this fixture tests switching acceptance.
      ps.ela_rounds = 0;
      ps.eswp_rounds = switching;
      emap_stats st;
      auto mapped = emap( aig, lib, ps, &st );
      check_recovery_mapping( aig, mapped, ps, st );
      CHECK( count_cells( mapped, "xor3" ) == switching );
      CHECK( count_cells( mapped, "fa" ) == 1u - switching );
      CHECK( count_cells( mapped, "inv" ) == 1u - switching );
      CHECK( mapped.compute_area() == Approx( switching ? std::stod( price ) : 5.0 ) );
      CHECK( st.delay == Approx( switching ? 2.0 : 1.1 ) );
      // Only the sum and (in the control) its inverter are live. Both have the
      // same activity; the unused physical carry is outside the mapper objective.
      auto const activities = detail::switching_activity( aig, ps.switching_activity_patterns );
      double const sum_activity = activities[aig.get_node( sum )];
      REQUIRE( sum_activity > 0.0 );
      if ( switching )
      {
        // The single ordinary XOR3 toggles once per source-sum transition.
        CHECK( st.power == Approx( sum_activity ) );
        // Retaining the physical FA and its sum inverter would cost twice as much.
        CHECK( st.power < 2.0 * sum_activity );
      }
    }
  }
}

TEST_CASE( "Emap rematches both full-adder output pins after recovery", "[emap][emap_recovery]" )
{
  auto lib = recovery_tech_library( recovery_library );
  for ( auto nonzero_arrivals : { false, true } )
  {
    CAPTURE( nonzero_arrivals );
    aig_network aig;
    auto a = aig.create_pi(), b = aig.create_pi(), c = aig.create_pi();
    auto const [s1, c1] = full_adder( aig, a, b, c );
    auto d = aig.create_pi(), e = aig.create_pi();
    auto const [s2, c2] = full_adder( aig, s1, d, e );
    auto f = aig.create_pi(), g = aig.create_pi();
    auto const [s3, c3] = full_adder( aig, s2, f, g );
    for ( auto output : { !c1, c2, !s3, s3, !s3 } )
      aig.create_po( output );
    emap_params ps;
    ps.map_multioutput = true;
    ps.area_flow_rounds = 1;
    ps.ela_rounds = 1;
    if ( nonzero_arrivals )
    {
      ps.arrival_times = { 0.2, 0.7, 0.3, 1.0, 0.6, 0.4, 0.8 };
      ps.required_times = { 12.0, 12.3, 12.6, 12.9, 13.2 };
    }
    emap_stats st;
    auto mapped = emap( aig, lib, ps, &st );
    check_recovery_mapping( aig, mapped, ps, st );
    if ( !nonzero_arrivals )
    {
      CHECK( count_cells( mapped, "fa" ) == 2u );
      CHECK( mapped.compute_area() == Approx( 26.0 ) );
      CHECK( st.delay == Approx( 7.4 ) );
    }
  }
}

TEST_CASE( "Emap repeatedly recovers full adders with shared fanins", "[emap][emap_recovery]" )
{
  auto lib = recovery_tech_library( recovery_library );
  aig_network aig;
  auto a = aig.create_pi(), b = aig.create_pi(), c = aig.create_pi(), d = aig.create_pi();
  auto const [s1, c1] = full_adder( aig, a, b, c );
  auto const [s2, c2] = full_adder( aig, s1, d, c1 );
  aig.create_po( s1 );
  aig.create_po( s2 );
  emap_params ps;
  ps.map_multioutput = true;
  ps.area_flow_rounds = 3;
  ps.ela_rounds = 4;
  ps.eswp_rounds = 2;
  emap_stats st;
  auto mapped = emap( aig, lib, ps, &st );
  check_recovery_mapping( aig, mapped, ps, st );
  CHECK( count_cells( mapped, "fa" ) == 2u );
  CHECK( mapped.num_gates() == 2u );
  CHECK( mapped.compute_area() == Approx( 12.0 ) );
  CHECK( st.delay == Approx( 1.4 ) );
}
