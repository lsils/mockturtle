#include <iostream>
#include <string>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/write_bench.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <lorina/aiger.hpp>

#include <fmt/format.h>

using namespace mockturtle;

int main( int argc, char** argv )
{
  if ( argc != 4 )
  {
    std::cerr << "usage: stg_mapping file.aig file.bench effort\n";
    return 1;
  }

  unsigned long effort{std::stoul( argv[3] )};

  mig_network mig;
  lorina::read_aiger( argv[1], aiger_reader( mig ) );

  for ( auto i = 0u; i < effort; ++i )
  {
    depth_view depth_mig{mig};
    mig_algebraic_depth_rewriting( depth_mig );
    mig = cleanup_dangling( mig );
  }

  write_bench( mig, argv[2] );

  return 0;
}
