#include <catch.hpp>

#include <cstdint>
#include <vector>

#include <lorina/genlib.hpp>
#include <lorina/super.hpp>
#include <mockturtle/algorithms/emap.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/io/genlib_reader.hpp>
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