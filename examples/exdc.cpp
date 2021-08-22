#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/views/dont_care_view.hpp>
#include <mockturtle/views/names_view.hpp>
#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/io/write_blif.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/exact.hpp>
#include <mockturtle/algorithms/node_resynthesis/dsd.hpp>
#include <mockturtle/algorithms/node_resynthesis/shannon.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>

#include <lorina/lorina.hpp>

#include <iostream>
#include <vector>

int main()
{
  using namespace mockturtle;

  names_view<klut_network> klut_ntk, klut_dc;
  if ( lorina::read_blif( "test.blif", blif_reader( klut_ntk ) ) != lorina::return_code::success )
  {
    std::cout << "read test.blif failed!\n";
    return -1;
  }
  if ( lorina::read_blif( "testDC.blif", blif_reader( klut_dc ) ) != lorina::return_code::success )
  {
    std::cout << "read testDC.blif failed!\n";
    return -1;
  }

  xag_npn_resynthesis<aig_network> fallback;
  dsd_resynthesis<aig_network, decltype( fallback )> resyn( fallback );
  shannon_resynthesis<aig_network> resyn2;
  aig_network ntk = node_resynthesis<aig_network>( klut_ntk, resyn );
  aig_network dc = node_resynthesis<aig_network>( klut_dc, resyn2 );

  std::cout << "main network has " << ntk.num_gates() << " gates\n";
  std::cout << "DC network has " << dc.num_gates() << " gates\n";
  
  dont_care_view dc_view( ntk, dc );
  std::vector<bool> pat0{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  std::vector<bool> pat1{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

  std::cout << "all-zero pattern " << ( dc_view.pattern_is_EXCDC( pat0 ) ? "is" : "is not" ) << " EXCDC\n";
  std::cout << "all-one pattern " << ( dc_view.pattern_is_EXCDC( pat1 ) ? "is" : "is not" ) << " EXCDC\n";

  resubstitution_params ps;
  ps.max_inserts = 20;
  ps.odc_levels = 10;
  //ps.use_external_CDC = true;

  for ( auto i = 0u; i < 1; ++i )
  {
    sim_resubstitution( dc_view, ps );
    ntk = cleanup_dangling( ntk );
  }

  std::cout << "optimized network has " << dc_view.num_gates() << " gates\n";

  names_view<aig_network> named_ntk( ntk );
  std::vector<std::string> pi_names;

  klut_ntk.foreach_pi( [&]( auto const& n ) {
    assert( klut_ntk.has_name( klut_ntk.make_signal( n ) ) );
    pi_names.push_back( klut_ntk.get_name( klut_ntk.make_signal( n ) ) );
  });

  named_ntk.foreach_pi( [&]( auto const& n, auto i ) {
    named_ntk.set_name( named_ntk.make_signal( n ), pi_names.at( i ) );
  });

  for ( auto i = 0u; i < klut_ntk.num_pos(); ++i )
    named_ntk.set_output_name( i, klut_ntk.get_output_name( i ) );

  write_blif( named_ntk, "testOPT.blif" );

  return 0;
}