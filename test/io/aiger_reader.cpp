#include <catch.hpp>

#include <sstream>
#include <string>

#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>

#include <lorina/aiger.hpp>

using namespace mockturtle;

TEST_CASE( "read an ASCII Aiger file into an AIG network and store input-output names", "[aiger_reader]" )
{
  aig_network aig;

  std::string file{"aag 6 2 0 1 4\n"
  "2\n"
  "4\n"
  "13\n"
  "6 2 4\n"
  "8 2 7\n"
  "10 4 7\n"
  "12 9 11\n"
  "i0 foo\n"
  "i1 bar\n"
  "o0 foobar\n"};

  std::unordered_map<aig_network::signal,std::string> names;
  std::istringstream in( file );
  lorina::read_ascii_aiger( in, aiger_reader( aig, &names ) );

  CHECK( aig.size() == 7 );
  CHECK( aig.num_pis() == 2 );
  CHECK( aig.num_pos() == 1 );
  CHECK( aig.num_gates() == 4 );

  aig.foreach_pi( [&]( auto const& n, auto index ) {
      auto const s = aig.make_signal( n );
      if ( index == 0 )
        CHECK( names[s] == "foo" );
      else if ( index == 1 )
        CHECK( names[s] == "bar" );
    } );

  aig.foreach_po( [&]( auto const& f ) {
      CHECK( aig.is_complemented( f ) );
      CHECK( names[f] == "foobar" );
      return false;
    } );
}
