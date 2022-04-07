/* This example code demonstrates how to use mockturtle's
 * fuzz tester to test ABC commands.
 */
#ifndef _MSC_VER

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/algorithms/network_fuzz_tester.hpp>
#include <mockturtle/generators/random_logic_generator.hpp>

using namespace mockturtle;

int main( int argc, char* argv[] )
{
  if( argc != 2 )
  {
    std::cout << "Please give exactly one argument, which is the optimization command(s) to test in ABC\n";
    std::cout << "(excluding read, write and cec)\n";
    std::cout << "If there are spaces, use double quotes\n";
    std::cout << "For example: ./fuzz_abc \"drw -C 10; resub\"\n";
    return -1;
  }

  auto fn = [&]( std::string const& filename ) -> std::string {
    return "abc -c \"read " + filename + "; " + std::string( argv[1] ) + "; write fuzz_opt.aig\"";
  };

  fuzz_tester_params ps;
  ps.num_gates_max = 500;
  ps.file_format = fuzz_tester_params::aiger;
  ps.filename = "fuzz.aig";
  ps.outputfile = "fuzz_opt.aig";

  auto gen = default_random_aig_generator();
  network_fuzz_tester<aig_network, decltype(gen)> fuzzer( gen, ps );
  fuzzer.run_incremental( fn );

  return 0;
}
#endif