// common
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>

// algorithm under test
#include <mockturtle/algorithms/aig_resub.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>

// cec
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>

// io
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/write_aiger.hpp>
#include <lorina/aiger.hpp>

using namespace mockturtle;

int main()
{
  aig_network aig;
  auto res = lorina::read_aiger( "fuzz_aigresub_minimized.aig", aiger_reader( aig ) );
  aig_network aig_copy = cleanup_dangling( aig );

  resubstitution_params ps;
  ps.max_pis = 8u;
  ps.max_inserts = 5u;
  aig_resubstitution( aig, ps );
  aig = cleanup_dangling( aig );

  write_aiger( aig, "fuzz_aigresub_minimized_opt.aig" );

  auto cec = *equivalence_checking( *miter<aig_network>( aig_copy, aig ) );
  if ( !cec )
    std::cout << "NEQ\n";

  return 0;
}