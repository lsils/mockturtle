/* This example code demonstrates how to use mockturtle's
 * fuzz tester to test ABC commands.
 */

#include <mockturtle/algorithms/network_fuzz_tester.hpp>
#include <mockturtle/generators/random_network.hpp>
#include <mockturtle/networks/aig.hpp>

using namespace mockturtle;

int main( int argc, char* argv[] )
{
#ifndef _MSC_VER
  if ( argc != 2 )
  {
    std::cout << "Please give exactly one argument, which is the optimization command(s) to test in ABC\n";
    std::cout << "(excluding read, write and cec)\n";
    std::cout << "If there are spaces, use double quotes\n";
    std::cout << "For example: ./fuzz_abc \"drw -C 10; resub\"\n";
    return -1;
  }

  std::string commands( argv[1] );
  auto fn = [&]( std::string const& filename ) -> std::string {
    return "abc -c \"read " + filename + "; " + commands + "; write fuzz_opt.aig\"";
  };

#ifdef ENABLE_NAUTY
  random_network_generator_params_composed ps_gen;
  std::cout << "[i] fuzzer: using the \"composed topologies\" generator\n";
#else
  random_network_generator_params_size ps_gen;
  std::cout << "[i] fuzzer: using the default (random) generator\n";
#endif
  auto gen = random_aig_generator( ps_gen );

  fuzz_tester_params ps_fuzz;
  ps_fuzz.file_format = fuzz_tester_params::aiger;
  ps_fuzz.filename = "fuzz.aig";
  ps_fuzz.outputfile = "fuzz_opt.aig";

  network_fuzz_tester<aig_network, decltype( gen )> fuzzer( gen, ps_fuzz );
  fuzzer.run( fn );
#endif

  return 0;
}
