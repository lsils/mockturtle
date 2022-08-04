#include <lorina/aiger.hpp>
#include <mockturtle/mockturtle.hpp>

int main()
{
  mockturtle::aig_network aig;
  auto const result = lorina::read_aiger( "../experiments/benchmarks/adder.aig", mockturtle::aiger_reader( aig ) );
  if ( result != lorina::return_code::success )
  {
    std::cout << "Read benchmark failed\n";
    return -1;
  }

  auto const cuts = cut_enumeration( aig );
  aig.foreach_node( [&]( auto node ) {
    std::cout << cuts.cuts( aig.node_to_index( node ) ) << "\n";
  } );

  return 0;
}
