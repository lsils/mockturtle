#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/views/dont_care_view.hpp>
#include <mockturtle/views/names_view.hpp>
#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/io/write_blif.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/exact.hpp>
#include <mockturtle/algorithms/node_resynthesis/dsd.hpp>
#include <mockturtle/algorithms/node_resynthesis/shannon.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>

#include <lorina/lorina.hpp>

#include <iostream>
#include <vector>

int main()
{
  using namespace mockturtle;

  /* read BLIF files as k-LUTs */
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

  /* transform k-LUTs into AIGs */
  using ntk_t = aig_network;
  xag_npn_resynthesis<ntk_t> fallback;
  dsd_resynthesis<ntk_t, decltype( fallback )> resyn( fallback );
  shannon_resynthesis<ntk_t> resyn2;
  ntk_t ntk = node_resynthesis<ntk_t>( klut_ntk, resyn );
  ntk_t dc = node_resynthesis<ntk_t>( klut_dc, resyn2 );
  ntk_t ntk_ori = cleanup_dangling( ntk );

  std::cout << "main network has " << ntk.num_gates() << " gates\n";
  std::cout << "DC network has " << dc.num_gates() << " gates\n";
  
  /* wrap with don't care view and simple tests on CDC patterns */
  dont_care_view dc_view( ntk, dc );
  std::vector<bool> pat0{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  std::vector<bool> pat1{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

  std::cout << "all-zero pattern " << ( dc_view.pattern_is_EXCDC( pat0 ) ? "is" : "is not" ) << " EXCDC\n";
  std::cout << "all-one pattern " << ( dc_view.pattern_is_EXCDC( pat1 ) ? "is" : "is not" ) << " EXCDC\n";

  /* simulation-guided resubstitution */
  resubstitution_params ps;
  ps.max_inserts = 20;
  ps.odc_levels = 10;
  ps.save_patterns = "exdc.pat";

  for ( auto i = 0u; i < 1; ++i )
  {
    sim_resubstitution( dc_view, ps );
    ntk = cleanup_dangling( ntk );
  }

  std::cout << "original network has " << ntk_ori.num_gates() << " gates\n";
  std::cout << "optimized network has " << dc_view.num_gates() << " gates\n";

  /* check if POs connected to PIs can be substituted with constant */
  partial_simulator sim = partial_simulator( *ps.save_patterns );
  sim.remove_CDC_patterns( dc_view );
  circuit_validator<dont_care_view<ntk_t>, bill::solvers::bsat2> validator( dc_view );
  ntk.foreach_po( [&]( auto const& f ) {
    auto const n = ntk.get_node( f );
    if ( ntk.is_pi( n ) && !ntk.is_constant( n ) )
    {
      if ( kitty::is_const0( sim.compute_pi( ntk.node_to_index( n ) - 1 ) ) )
      {
        auto valid = validator.validate( n, 0 );
        if ( valid && *valid )
        {
          ntk.substitute_node( n, ntk.get_constant( false ) );
        }
      }
      else if ( kitty::is_const0( ~sim.compute_pi( ntk.node_to_index( n ) - 1 ) ) )
      {
        auto valid = validator.validate( n, 1 );
        if ( valid && *valid )
        {
          ntk.substitute_node( n, ntk.get_constant( true ) );
        }
      }
    }
  } );

  /* equivalence checking */
  ntk_t miter_aig = *miter<ntk_t>( ntk, ntk_ori );
  dont_care_view miter_with_DC( miter_aig, dc );
  auto cec = equivalence_checking_bill( miter_with_DC );
  std::cout << "optimized network " << ( *cec ? "is" : "is NOT" ) << " equivalent to the original network\n";

  /* match I/O names and write out optimized network */
  names_view<ntk_t> named_ntk( ntk );
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
  write_verilog( named_ntk, "testOPT.v" );

  return 0;
}