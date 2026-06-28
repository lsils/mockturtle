#include <catch.hpp>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/algorithms/window_rewriting.hpp>
#include <mockturtle/networks/aig.hpp>

#include <kitty/kitty.hpp>

using namespace mockturtle;

TEST_CASE( "window rewriting reduces circuit without don't cares", "[window_rewriting]" )
{
  aig_network aig;
  auto const x0 = aig.create_pi();
  auto const x1 = aig.create_pi();
  auto const x2 = aig.create_pi();
  auto const x3 = aig.create_pi();

  auto const n0 = aig.create_and( !x2, x3 );
  auto const n1 = aig.create_and( !x2, n0 );
  auto const n2 = aig.create_and( x3, !n1 );
  auto const n3 = aig.create_and( x0, !x1 );
  auto const n4 = aig.create_and( !n2, n3 );
  auto const n5 = aig.create_and( x1, !n2 );
  auto const n6 = aig.create_and( !n4, !n5 );
  auto const n7 = aig.create_and( n1, n3 );
  aig.create_po( n6 );
  aig.create_po( n7 );

  auto opt = aig.clone();

  window_rewriting_params ps;
  ps.cut_size = 5u;
  ps.num_levels = 5u;
  ps.use_dont_cares = false;
  window_rewriting( opt, ps );
  opt = cleanup_dangling( opt );

  default_simulator<kitty::dynamic_truth_table> sim( aig.num_pis() );
  CHECK( simulate<kitty::dynamic_truth_table>( opt, sim ) == simulate<kitty::dynamic_truth_table>( aig, sim ) );

  CHECK( aig.num_gates() == 8u );
  CHECK( opt.num_gates() == 6u );
}

TEST_CASE( "window rewriting without observability don't cares on reconvergent circuit", "[window_rewriting]" )
{
  aig_network aig;
  auto const x0 = aig.create_pi();
  auto const x1 = aig.create_pi();
  auto const x2 = aig.create_pi();
  auto const x3 = aig.create_pi();

  auto const n0 = aig.create_and( x1, x2 );
  auto const n1 = aig.create_and( x3, !x2 );
  auto const n2 = aig.create_and( !x2, n1 );
  auto const n3 = aig.create_and( !x2, x1 );
  auto const n4 = aig.create_and( !n2, !n0 );
  auto const n5 = aig.create_and( x0, n4 );
  auto const n6 = aig.create_and( !n5, !n3 );

  aig.create_po( n6 );

  auto aig_without_dcs = aig.clone();

  window_rewriting_params ps;
  ps.cut_size = 5u;
  ps.num_levels = 5u;

  ps.use_dont_cares = false;
  window_rewriting( aig_without_dcs, ps );
  aig_without_dcs = cleanup_dangling( aig_without_dcs );

  default_simulator<kitty::dynamic_truth_table> sim( aig.num_pis() );
  CHECK( simulate<kitty::dynamic_truth_table>( aig_without_dcs, sim )[0] == simulate<kitty::dynamic_truth_table>( aig, sim )[0] );

  CHECK( aig.num_gates() == 7u );
  CHECK( aig_without_dcs.num_gates() == 6u );
}

TEST_CASE( "window rewriting with observability don't cares on reconvergent circuit", "[window_rewriting]" )
{
  aig_network aig;
  auto const x0 = aig.create_pi();
  auto const x1 = aig.create_pi();
  auto const x2 = aig.create_pi();
  auto const x3 = aig.create_pi();

  auto const n0 = aig.create_and( x1, x2 );
  auto const n1 = aig.create_and( x3, !x2 );
  auto const n2 = aig.create_and( !x2, n1 );
  auto const n3 = aig.create_and( !x2, x1 );
  auto const n4 = aig.create_and( !n2, !n0 );
  auto const n5 = aig.create_and( x0, n4 );
  auto const n6 = aig.create_and( !n5, !n3 );

  aig.create_po( n6 );

  auto aig_with_dcs = aig.clone();

  window_rewriting_params ps;
  ps.cut_size = 5u;
  ps.num_levels = 5u;
  ps.use_dont_cares = true;
  window_rewriting( aig_with_dcs, ps );
  aig_with_dcs = cleanup_dangling( aig_with_dcs );

  default_simulator<kitty::dynamic_truth_table> sim( aig.num_pis() );
  CHECK( simulate<kitty::dynamic_truth_table>( aig_with_dcs, sim )[0] == simulate<kitty::dynamic_truth_table>( aig, sim )[0] );

  CHECK( aig.num_gates() == 7u );
  CHECK( aig_with_dcs.num_gates() == 5u );
}
