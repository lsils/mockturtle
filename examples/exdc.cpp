#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/cover.hpp>

#include <mockturtle/views/dont_care_view.hpp>
#include <mockturtle/views/names_view.hpp>

#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/io/write_blif.hpp>

#include <mockturtle/algorithms/cover_to_graph.hpp>
#include <mockturtle/algorithms/klut_to_graph.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>

#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/utils/name_utils.hpp>

#include <lorina/lorina.hpp>

#include <iostream>
#include <vector>

namespace mockturtle
{
struct stats
{
  stopwatch<>::duration time_total{0};
  stopwatch<>::duration time_resyn_main{0};
  stopwatch<>::duration time_resyn_dc{0};
  stopwatch<>::duration time_sim_resub{0};
  stopwatch<>::duration time_cec{0};
};
}

int main( int argc, char* argv[] )
{
  using namespace mockturtle;

  stats st;
  stopwatch t( st.time_total );

  std::string testcase, path = "exdc_tests/";
  if ( argc == 2 )
    testcase = argv[1];
  else
    testcase = "test";

  /* main network: read BLIF as k-LUT */
  names_view<klut_network> klut_ntk;
  if ( lorina::read_blif( path + testcase + ".blif", blif_reader( klut_ntk ) ) != lorina::return_code::success )
  {
    std::cout << "read <testcase>.blif failed!\n";
    return -1;
  }

  /* DC network: read BLIF as COVER */
  cover_network cover_dc;
  if ( lorina::read_blif( path + testcase + "DC.blif", blif_reader( cover_dc ) ) != lorina::return_code::success )
  {
    std::cout << "read <testcase>DC.blif failed!\n";
    return -1;
  }

  /* convert networks into AIG */
  using ntk_t = aig_network;
  ntk_t ntk = call_with_stopwatch( st.time_resyn_main, [&]() { return convert_klut_to_graph<ntk_t>( klut_ntk ); });
  ntk_t dc = call_with_stopwatch( st.time_resyn_dc, [&]() { return convert_cover_to_graph<ntk_t>( cover_dc ); });

  /* save a copy for CEC */
  ntk_t ntk_ori = cleanup_dangling( ntk );

  /* create dont_care_view */
  dont_care_view dc_view( ntk, dc );

  /* simulation-guided resubstitution */
  resubstitution_params ps;
  ps.max_pis = ntk.num_pis();
  ps.max_divisors = 1000;
  ps.max_inserts = 20;
  ps.odc_levels = 10;
  ps.save_patterns = path + testcase + ".pat";
  {
    stopwatch tcec( st.time_sim_resub );
    for ( auto i = 0u; i < 2; ++i )
    {
      sim_resubstitution( dc_view, ps );
      ntk = cleanup_dangling( ntk );
    }
  }

  std::cout << "original network has " << klut_ntk.num_gates() << " LUTs => " << ntk_ori.num_gates() << " AND gates\n";
  std::cout << "optimized network has " << dc_view.num_gates() << " AND gates\n";

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
  {
    stopwatch tcec( st.time_cec );
    ntk_t miter_aig = *miter<ntk_t>( ntk, ntk_ori );
    dont_care_view miter_with_DC( miter_aig, dc );
    auto cec = equivalence_checking_bill( miter_with_DC );
    std::cout << "optimized network " << ( *cec ? "is" : "is NOT" ) << " equivalent to the original network\n";
  }

  /* match I/O names and write out optimized network */
  names_view<ntk_t> named_ntk( ntk );
  restore_pio_names_by_order( klut_ntk, named_ntk );
  write_blif( named_ntk, path + testcase + "OPT.blif" );

  t.~stopwatch();
  std::cout << "total time = " << to_seconds( st.time_total )
            << ", resyn main = " << to_seconds( st.time_resyn_main )
            << ", resyn dc = " << to_seconds( st.time_resyn_dc )
            << ", sim_resub = " << to_seconds( st.time_sim_resub )
            << ", cec = " << to_seconds( st.time_cec ) << "\n";

  return 0;
}