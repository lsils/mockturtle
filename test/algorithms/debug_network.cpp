#include <catch.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/algorithms/debug_network.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <fstream>

using namespace mockturtle;

TEST_CASE( "Minimize logic network with respect to an erroneous optimization", "[debug]" )
{
  auto status = system( "abc --help &> /dev/null" );
  if ( WEXITSTATUS( status ) == 127 )
  {
    /* do not run the test if cec is not in the path */
    return;
  }

  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto f = aig.create_or( aig.create_and( a, b ), c );
  const auto g = aig.create_xor( a, !c );
  aig.create_po( f );

  auto simplified = debug_network<aig_network>( aig,
    [&]( aig_network const& ntk ) -> aig_network {
      auto optimized_ntk = ntk;

      /*** introduce a bug into output #1 ***/
      if ( ntk.num_pis() >= 1u )
      {
        std::unordered_map<aig_network::node, bool> values;
        values.emplace( ntk.get_node( ntk.po_at( 0 ) ), true );      
        optimized_ntk = constant_propagation( optimized_ntk, values );
      }
      
      optimized_ntk = cleanup_dangling( optimized_ntk ); 
      return optimized_ntk;
    },
    [&]( aig_network const& ref_ntk, aig_network const& mod_ntk ) -> bool {
      std::ofstream ref_file( "/tmp/ref.v" );
      write_verilog( ref_ntk, ref_file );
      ref_file.close();
      
      std::ofstream mod_file( "/tmp/mod.v" );
      write_verilog( mod_ntk, mod_file );
      mod_file.close();

      system( "abc -c \"cec -n /tmp/ref.v /tmp/mod.v\" > /tmp/out.log" );
      std::ifstream ifs( "/tmp/out.log" );
      std::string line;
      while ( std::getline( ifs, line ) )
      {
        if ( line.substr( 0, 23 ) == "Networks are equivalent" )
          return true;
      }
      ifs.close();
      return false;
    }
  );

  CHECK( simplified.po_at( 0 ) == simplified.get_constant( false ) );
}
