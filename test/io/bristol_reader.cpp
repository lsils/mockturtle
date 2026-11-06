#include <catch.hpp>

#include <mockturtle/io/bristol_reader.hpp>
#include <mockturtle/networks/xag.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace mockturtle;

TEST_CASE( "reject Bristol wire bounds and malformed gates", "[bristol_reader]" )
{
  for ( auto const& file : std::vector<std::string>{
            "1 2\n1 1\n1 1\n2 1 0 0 2 XOR\n",
            "1 2\n1 1\n1 1\n2 1 0 2 1 AND\n",
            "1 2\n1 3\n1 1\n",
            "1 2\n1 1\n1 3\n",
            "1 2\n1 1\n1 1\n1 1 0 1 XOR\n",
            "1 2\n1 1\n1 1\n2\n" } )
  {
    CAPTURE( file );
    xag_network ntk;
    std::istringstream in( file );
    CHECK( lorina::read_bristol( in, bristol_reader( ntk ) ) == lorina::return_code::parse_error );
  }
}

TEST_CASE( "read Bristol wires at valid boundaries", "[bristol_reader]" )
{
  xag_network ntk;
  std::istringstream in( "1 3\n1 2\n1 1\n2 1 0 1 2 XOR\n" );
  CHECK( lorina::read_bristol( in, bristol_reader( ntk ) ) == lorina::return_code::success );
  CHECK( ntk.num_pis() == 2u );
  CHECK( ntk.num_pos() == 1u );
}

TEST_CASE( "reject Bristol gates with incorrect input counts", "[bristol_reader]" )
{
  for ( auto const& gate : { "INV", "EQW", "AND", "XOR" } )
  {
    for ( auto const& inputs : { "0 1 1 ", "1 1 0 1 ", "2 1 0 0 1 ", "3 1 0 0 0 1 " } )
    {
      const std::string type( gate );
      const std::string declaration( inputs );
      const auto expected_inputs = type == "INV" || type == "EQW" ? '1' : '2';
      if ( declaration[0] == expected_inputs )
        continue;

      CAPTURE( gate, inputs );
      xag_network ntk;
      std::istringstream in( "1 2\n1 1\n1 1\n" + declaration + type + "\n" );
      CHECK( lorina::read_bristol( in, bristol_reader( ntk ) ) == lorina::return_code::parse_error );
    }
  }
}
