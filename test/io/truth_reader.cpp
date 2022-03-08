#include <catch.hpp>

#include <sstream>
#include <string>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/io/truth_reader.hpp>
#include <mockturtle/io/write_blif.hpp>
#include <mockturtle/networks/cover.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>

#include <kitty/kitty.hpp>
#include <lorina/truth.hpp>

using namespace mockturtle;

TEST_CASE( "read a combinational TRUTH file into KLUT network: 1 node", "[truth_reader]" )
{
  klut_network klut;

  std::string file{
      "10001001\n" };

  std::istringstream in( file );
  auto result = lorina::read_truth( in, truth_reader( klut ) );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( klut.size() == 6 );
  CHECK( klut.num_pis() == 3 );
  CHECK( klut.num_pos() == 1 );
  CHECK( klut.num_gates() == 1 );

  /* functional checks */
  default_simulator<kitty::dynamic_truth_table> sim( klut.num_pis() );
  const auto tts = simulate<kitty::dynamic_truth_table>( klut, sim );
  CHECK( kitty::to_hex( tts[0] ) == "89" );
}

TEST_CASE( "read a combinational TRUTH file into KLUT network: 2 nodes", "[truth_reader]" )
{
  klut_network klut;

  std::string file{
      "1000\n"
      "0110\n" };

  std::istringstream in( file );
  auto result = lorina::read_truth( in, truth_reader( klut ) );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( klut.size() == 6 );
  CHECK( klut.num_pis() == 2 );
  CHECK( klut.num_pos() == 2 );
  CHECK( klut.num_gates() == 2 );

  /* functional checks */
  default_simulator<kitty::dynamic_truth_table> sim( klut.num_pis() );
  const auto tts = simulate<kitty::dynamic_truth_table>( klut, sim );
  klut.foreach_po( [&]( auto const&, auto i ) {
    switch ( i )
    {
    case 0:
      CHECK( kitty::to_hex( tts[i] ) == "8" );
      break;
    case 1:
      CHECK( kitty::to_hex( tts[i] ) == "6" );
      break;
    }
  } );
}

TEST_CASE( "read a combinational TRUTH file into KLUT network: 3 nodes", "[truth_reader]" )
{
  klut_network klut;

  std::string file{
      "10000001\n"
      "01101001\n"
      "01111001\n" };

  std::istringstream in( file );
  auto result = lorina::read_truth( in, truth_reader( klut ) );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( klut.size() == 8 );
  CHECK( klut.num_pis() == 3 );
  CHECK( klut.num_pos() == 3 );
  CHECK( klut.num_gates() == 3 );

  /* functional checks */
  default_simulator<kitty::dynamic_truth_table> sim( klut.num_pis() );
  const auto tts = simulate<kitty::dynamic_truth_table>( klut, sim );
  klut.foreach_po( [&]( auto const&, auto i ) {
    switch ( i )
    {
    case 0:
      CHECK( kitty::to_hex( tts[i] ) == "81" );
      break;
    case 1:
      CHECK( kitty::to_hex( tts[i] ) == "69" );
      break;
    case 3:
      CHECK( kitty::to_hex( tts[i] ) == "79" );
      break;
    }
  } );
}

TEST_CASE( "read a combinational TRUTH file into KLUT network: wrong dimension", "[truth_reader]" )
{
  klut_network klut;

  std::string file{
      "0110100\n" };

  std::istringstream in( file );
  auto result = lorina::read_truth( in, truth_reader( klut ) );

  /* structural checks */
  CHECK( result == lorina::return_code::parse_error );
}

TEST_CASE( "read a combinational TRUTH file into KLUT network: wrong dimensions", "[truth_reader]" )
{
  klut_network klut;

  std::string file{
      "10000001\n"
      "0110100\n" };

  std::istringstream in( file );
  auto result = lorina::read_truth( in, truth_reader( klut ) );

  /* structural checks */
  CHECK( result == lorina::return_code::parse_error );
}