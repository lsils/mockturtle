/* This example code makes it easy to visualize small AIG benchmarks.
 */

#include <fmt/format.h>
#include <lorina/aiger.hpp>
#include <lorina/verilog.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_dot.hpp>
#include <mockturtle/networks/aig.hpp>

using namespace mockturtle;

int main( int argc, char* argv[] )
{
  if ( argc != 2 )
  {
    std::cout << "[e] Please give exactly one argument, which is the AIGER or Verilog file to be visualized\n";
    std::cout << "    For example: ./draw test.aig\n";
    return -1;
  }

  std::string filename( argv[1] );
  std::string filename_trimed;
  aig_network ntk;

  if ( filename.size() > 2 && filename.substr( filename.size() - 2, 2 ) == ".v" )
  {
    filename_trimed = filename.substr( 0, filename.size() - 2 );
    if ( lorina::read_verilog( filename, verilog_reader( ntk ) ) != lorina::return_code::success )
    {
      fmt::print( "[e] Could not read input file `{}`\n", filename );
      return -1;
    }
  }
  else if ( filename.size() > 4 && filename.substr( filename.size() - 4, 4 ) == ".aig" )
  {
    filename_trimed = filename.substr( 0, filename.size() - 4 );
    if ( lorina::read_aiger( filename, aiger_reader( ntk ) ) != lorina::return_code::success )
    {
      fmt::print( "[e] Could not read input file `{}`\n", filename );
      return -1;
    }
  }
  else
  {
    std::cout << "[e] Argument does not end with `.aig` or `.v`\n";
    std::cout << "[i] Usage: ./draw [AIGER or Verilog filename]\n";
    return -1;
  }

  write_dot( ntk, filename_trimed + ".dot" );
  std::system( fmt::format( "dot -Tpng -o {}.png {}.dot", filename_trimed, filename_trimed ).c_str() );
  std::system( fmt::format( "rm {}.dot", filename_trimed ).c_str() );
  std::system( fmt::format( "open {}.png", filename_trimed ).c_str() );

  return 0;
}