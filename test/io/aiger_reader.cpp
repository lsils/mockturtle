#include <catch.hpp>

#include <sstream>
#include <string>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/io/aiger_reader.hpp>

#include <lorina/aiger.hpp>

using namespace mockturtle;

TEST_CASE( "read an ASCII Aiger file into an AIG network", "[aiger_reader]" )
{
  aig_network aig;

  std::string file{"aag 6 2 0 1 4\n"
  "2\n"
  "4\n"
  "13\n"
  "6 2 4\n"
  "8 2 7\n"
  "10 4 7\n"
  "12 9 11\n"};

  std::istringstream in( file );
  lorina::read_ascii_aiger( in, aiger_reader( aig ) );

  CHECK( aig.size() == 7 );
  CHECK( aig.num_pis() == 2 );
  CHECK( aig.num_pos() == 1 );
  CHECK( aig.num_gates() == 4 );

  aig.foreach_po( [&]( auto const& f ) {
    CHECK( aig.is_complemented( f ) );
    return false;
  } );
}
