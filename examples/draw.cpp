/* This example code makes it easy to visualize small AIG benchmarks.
 */

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/write_dot.hpp>
#include <lorina/aiger.hpp>

using namespace mockturtle;

int main( int argc, char* argv[] )
{
  if( argc != 2 )
  {
    std::cout << "Please give exactly one argument, which is the AIG file to be visualized\n";
    std::cout << "For example: ./draw test.aig\n";
    return -1;
  }

  aig_network aig;
  if ( lorina::read_aiger( argv[1], aiger_reader( aig ) ) != lorina::return_code::success )
  {
    std::cout << "[e] couldn't read input file\n";
    return -1;
  }

  write_dot( aig, std::string( argv[1] ) + ".dot" );
  std::system( std::string( "dot -Tpng -O " + std::string( argv[1] ) + ".dot" ).c_str() );
  std::system( std::string( "rm " + std::string( argv[1] ) + ".dot" ).c_str() );
  std::system( std::string( "open " + std::string( argv[1] ) + ".dot.png" ).c_str() );

  return 0;
}