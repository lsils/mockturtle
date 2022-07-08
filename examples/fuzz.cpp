/* This example code demonstrates how to fuzz test mockturtle's algorithms.
 */

// common
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/color_view.hpp>
#include <mockturtle/utils/debugging_utils.hpp>

// algorithm under test
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>

// fuzzer
#include <mockturtle/algorithms/network_fuzz_tester.hpp>
#include <mockturtle/generators/random_network.hpp>

// cec
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>

using namespace mockturtle;

int main()
{
  xag_npn_resynthesis<aig_network> resyn;
  auto opt = [&]( aig_network aig ) -> bool {
    aig_network const aig_copy = cleanup_dangling( aig );

    cut_rewriting_params ps;
    ps.cut_enumeration_ps.cut_size = 4;
    cut_rewriting_with_compatibility_graph( aig, resyn, ps );

    if ( !network_is_acyclic( color_view{aig} ) )
    {
      return false;
    }
    aig = cleanup_dangling( aig );

    return *equivalence_checking( *miter<aig_network>( aig_copy, aig ) );
  };

#ifdef ENABLE_NAUTY
  random_network_generator_params_composed ps_gen;
  ps_gen.num_networks_per_configuration = 100000;
  ps_gen.min_num_gates_component = 4u;
  ps_gen.max_num_gates_component = 4u;
  ps_gen.num_pis = 3u;
  std::cout << "[i] fuzzer: using the \"composed topologies\" generator\n";
#else
  random_network_generator_params_size ps_gen;
  std::cout << "[i] fuzzer: using the default (random) generator\n";
#endif
  auto gen = random_aig_generator( ps_gen );

  fuzz_tester_params ps_fuzz;
  ps_fuzz.file_format = fuzz_tester_params::aiger;
  ps_fuzz.filename = "fuzz_crw_cg.aig";

  network_fuzz_tester<aig_network, decltype(gen)> fuzzer( gen, ps_fuzz );
  fuzzer.run( opt );

  return 0;
}