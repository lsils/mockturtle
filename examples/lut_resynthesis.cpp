#include <iostream>
#include <string>

#include <mockturtle/algorithms/lut_resynthesis.hpp>
#include <mockturtle/algorithms/lut_resynthesis/mig_npn.hpp>
#include <mockturtle/io/bench_reader.hpp>
#include <mockturtle/io/write_bench.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>

#include <lorina/bench.hpp>

#include <fmt/format.h>

using namespace mockturtle;

int main( int argc, char** argv )
{
  if ( argc != 3 )
  {
    std::cerr << "usage: lut_resynthesis file.bench mig.bench\n";
    return 1;
  }

  klut_network klut;
  lorina::read_bench( argv[1], bench_reader( klut ) );

  mig_npn_resynthesis resyn;
  const auto mig = lut_resynthesis<mig_network>( klut, resyn );
  write_bench( mig, argv[2] );

  return 0;
}
