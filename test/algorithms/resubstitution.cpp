#include <catch.hpp>

#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/fanout_view2.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/io/write_verilog.hpp>

#include <kitty/static_truth_table.hpp>

#include <mockturtle/algorithms/aig_resub.hpp>
#include <mockturtle/algorithms/mig_resub.hpp>
#include <mockturtle/algorithms/xag_resub_withDC.hpp>

using namespace mockturtle;

TEST_CASE( "Resubstitution of AIG", "[resubstitution]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();

  const auto f = aig.create_and( a, aig.create_and( b, a ) );
  aig.create_po( f );

  CHECK( aig.size() == 5 );
  CHECK( aig.num_pis() == 2 );
  CHECK( aig.num_pos() == 1 );
  CHECK( aig.num_gates() == 2 );

  const auto tt = simulate<kitty::static_truth_table<2>>( aig )[0];
  CHECK( tt._bits == 0x8 );

  using view_t = depth_view<fanout_view2<aig_network>>;
  fanout_view2<aig_network> fanout_view{aig};
  view_t resub_view{fanout_view};

  aig_resubstitution( resub_view );

  aig = cleanup_dangling( aig );

  /* check equivalence */
  const auto tt_opt = simulate<kitty::static_truth_table<2>>( aig )[0];
  CHECK( tt_opt._bits == tt._bits );

  CHECK( aig.size() == 4 );
  CHECK( aig.num_pis() == 2 );
  CHECK( aig.num_pos() == 1 );
  CHECK( aig.num_gates() == 1 );
}

TEST_CASE( "Resubstitution of MIG", "[resubstitution]" )
{
  mig_network mig;

  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();

  const auto f = mig.create_maj( a, mig.create_maj( a, b, c ), c );
  mig.create_po( f );

  CHECK( mig.size() == 6 );
  CHECK( mig.num_pis() == 3 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 2 );

  const auto tt = simulate<kitty::static_truth_table<3>>( mig )[0];
  CHECK( tt._bits == 0xe8 );

  using view_t = depth_view<fanout_view2<mig_network>>;
  fanout_view2<mig_network> fanout_view{mig};
  view_t resub_view{fanout_view};

  mig_resubstitution( resub_view );

  mig = cleanup_dangling( mig );

  /* check equivalence */
  const auto tt_opt = simulate<kitty::static_truth_table<3>>( mig )[0];
  CHECK( tt_opt._bits == tt._bits );

  CHECK( mig.size() == 5 );
  CHECK( mig.num_pis() == 3 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 1 );
}

TEST_CASE( "Resubstitution of XAG to minimize ANDs", "[resubstitution]" )
{
  xag_network xag;

  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();

  const auto f = xag.create_xor(xag.create_or( xag.create_and(a, xag.create_not(b)), xag.create_and(b, xag.create_not(a)) ), c);
  xag.create_po( f );

  CHECK( xag.size() == 8 );
  CHECK( xag.num_pis() == 3 );
  CHECK( xag.num_pos() == 1 );
  CHECK( xag.num_gates() == 4 );

  const auto tt = simulate<kitty::static_truth_table<2>>( xag )[0];

  resubstitution_params ps;

  using view_t = depth_view<fanout_view2<xag_network>>;
  fanout_view2<xag_network> fanout_view{xag};
  view_t resub_view{fanout_view};
  resubstitution_minmc_withDC( resub_view , ps);

  xag = cleanup_dangling( xag );

  /* check equivalence */
  const auto tt_opt = simulate<kitty::static_truth_table<2>>( xag )[0];
  CHECK( tt_opt._bits == tt._bits );

  CHECK( xag.size() == 6 );
  CHECK( xag.num_pis() == 3 );
  CHECK( xag.num_pos() == 1 );
  CHECK( xag.num_gates() == 2 );
}
