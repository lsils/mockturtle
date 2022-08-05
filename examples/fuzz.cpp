/* This example code demonstrates how to fuzz test mockturtle's algorithms.
 */

// common
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/utils/debugging_utils.hpp>
#include <mockturtle/views/color_view.hpp>

// algorithm under test
#include <mockturtle/algorithms/aig_resub.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>

// fuzzer
#include <mockturtle/algorithms/network_fuzz_tester.hpp>
#include <mockturtle/generators/random_network.hpp>

// cec
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>

using namespace mockturtle;

int main()
{
  auto opt = [&]( aig_network aig ) -> bool {
    aig_network const aig_copy = aig.clone();

    resubstitution_params ps;
    ps.max_pis = 8u;
    ps.max_inserts = 5u;
    aig_resubstitution( aig, ps );
    aig = cleanup_dangling( aig );

    bool const cec = *equivalence_checking( *miter<aig_network>( aig_copy, aig ) );
    if ( !cec )
      std::cout << "Optimized network is not equivalent to the original one!\n";
    return cec;
  };

#ifdef ENABLE_NAUTY
  random_network_generator_params_composed ps_gen;
  std::cout << "[i] fuzzer: using the \"composed topologies\" generator\n";
#else
  random_network_generator_params_size ps_gen;
  ps_gen.num_gates = 30;
  std::cout << "[i] fuzzer: using the default (random) generator\n";
#endif
  auto gen = random_aig_generator( ps_gen );

  fuzz_tester_params ps_fuzz;
  ps_fuzz.file_format = fuzz_tester_params::aiger;
  ps_fuzz.filename = "fuzz.aig";

  network_fuzz_tester<aig_network, decltype( gen )> fuzzer( gen, ps_fuzz );
  fuzzer.run( opt );

  return 0;
}