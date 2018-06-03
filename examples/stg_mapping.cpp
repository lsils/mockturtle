#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <mockturtle/algorithms/lut_mapping.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/mapping_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <kitty/print.hpp>

#include <lorina/aiger.hpp>

#include <fmt/format.h>

using namespace mockturtle;

int main( int argc, char** argv )
{
  if ( argc != 4 )
  {
    std::cerr << "usage: stg_mapping file.aig ancillae file.real\n";
    return 1;
  }

  unsigned long free_ancillae{std::stoul( argv[2] )};

  aig_network aig;
  lorina::read_aiger( argv[1], aiger_reader( aig ) );

  using mapped_network = mapping_view<aig_network, true>;
  mapped_network mapped_aig{aig};

  lut_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 3;

  do
  {
    ps.cut_enumeration_ps.cut_size++;
    lut_mapping<mapped_network, true>( mapped_aig, ps );

    std::cout << fmt::format( "[i] mapping with cut size {} needs {} LUTs\n", ps.cut_enumeration_ps.cut_size, mapped_aig.num_luts() );
  } while ( mapped_aig.num_luts() > free_ancillae + 1 );

  std::vector<uint32_t> node_to_line( aig.size(), 0 );
  aig.foreach_pi( [&]( auto n, auto i ) {
    node_to_line[aig.node_to_index( n )] = i;
  } );
  auto next_line = aig.num_pis();
  std::vector<std::string> gates;

  topo_view topo{aig};
  topo.foreach_node( [&]( auto n ) {
    if ( mapped_aig.is_mapped( n ) )
    {
      std::string controls;
      mapped_aig.foreach_lut_fanin( n, [&]( auto fanin ) {
        controls += fmt::format( " v{}", fanin );
      } );

      gates.push_back( fmt::format( "stg[{}]{} v{}\n", kitty::to_hex( mapped_aig.lut_function( n ) ), controls, next_line ) );
      node_to_line[aig.node_to_index( n )] = next_line++;
    }
  } );

  std::ofstream os( argv[3], std::ofstream::out );
  os << fmt::format( ".version 2.0\n.numvars {}\n.variables", next_line );
  for ( auto i = 0u; i < next_line; ++i )
  {
    os << fmt::format( " v{}", i );
  }
  os << "\n";

  os << ".begin\n";
  for ( auto const& gate : gates )
  {
    os << gate;
  }
  for ( auto it = gates.rbegin() + 1; it != gates.rend(); ++it )
  {
    os << *it;
  }
  os << ".end\n";

  return 0;
}
