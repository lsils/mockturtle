#include "experiments.hpp"
#include <mockturtle/algorithms/resubstitution/boolean_optimization.hpp>
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
  assert( result == lorina::return_code::success );

  experimental::boolean_optimization_params ps;
  ps.verbose = true;
  experimental::null_optimization( aig, ps );
  const auto cec = experiments::abc_cec( aig, benchmark );
  assert( cec );

  return 0;
}