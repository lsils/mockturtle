/* This example code demonstrates how to fuzz test mockturtle's algorithms.
 */

// common
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/color_view.hpp>

// algorithm under test
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>

// fuzzer
#include <mockturtle/utils/debugging_utils.hpp>
#include <mockturtle/algorithms/network_fuzz_tester.hpp>
#include <mockturtle/generators/random_logic_generator.hpp>
#include <lorina/lorina.hpp>

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
    aig = cut_rewriting( aig, resyn, ps );

    color_view caig{aig};
    if ( !network_is_acyclic( caig ) )
      return false;

    return *equivalence_checking( *miter<aig_network>( aig_copy, aig ) );
  };

  fuzz_tester_params ps;
  ps.num_iterations_step = 1000;
  ps.num_pis_max = 100;
  ps.num_gates_max = 1000;
  ps.file_format = fuzz_tester_params::aiger;
  ps.filename = "fuzz.aig";

  auto gen = default_random_aig_generator();
  network_fuzz_tester<aig_network, decltype(gen)> fuzzer( gen, ps );
  fuzzer.run_incremental( opt );

  return 0;
}