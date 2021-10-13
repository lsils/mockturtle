#include "experiments.hpp"
#include <mockturtle/algorithms/experimental/boolean_optimization.hpp>
#include <mockturtle/algorithms/experimental/window_resub.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <lorina/aiger.hpp>
#include <iostream>
#include <string>

int main()
{
  using namespace mockturtle;

  aig_network aig;
  std::string benchmark = "adder";
  auto const result = lorina::read_aiger( experiments::benchmark_path( benchmark ), aiger_reader( aig ) );
  assert( result == lorina::return_code::success ); (void)result;

  using wps_t = typename experimental::complete_tt_windowing_params;
  using ps_t = typename experimental::boolean_optimization_params<wps_t>;
  ps_t ps;
  ps.verbose = true;
  ps.wps.max_pis = 6;

  window_xag_heuristic_resub( aig, ps );

  const auto cec = experiments::abc_cec( aig, benchmark );
  assert( cec ); (void)cec;

  return 0;
}