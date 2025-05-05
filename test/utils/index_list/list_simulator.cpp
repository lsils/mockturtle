#include <catch.hpp>

#include <kitty/print.hpp>
#include <kitty/static_truth_table.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/index_list/index_list.hpp>
#include <mockturtle/utils/index_list/list_simulator.hpp>

using namespace mockturtle;

TEST_CASE( "simulation of xag_index_list with static truth tables", "[list_simulator]" )
{
  xag_network xag;
  auto const a = xag.create_pi();
  auto const b = xag.create_pi();
  auto const c = xag.create_pi();
  auto const d = xag.create_pi();
  auto const t0 = xag.create_and( a, b );
  auto const t1 = xag.create_and( c, d );
  auto const t2 = xag.create_xor( t0, t1 );
  xag.create_po( t2 );

  std::vector<kitty::static_truth_table<4u>> xs;
  for ( auto i = 0u; i < 4u; ++i )
  {
    xs.emplace_back();
    kitty::create_nth_var( xs[i], i );
  }
  xs.emplace_back( xs[0] & xs[1] ); // 4
  xs.emplace_back( xs[2] & xs[3] ); // 5
  xs.emplace_back( xs[4] ^ xs[5] ); // 6

  std::vector<kitty::static_truth_table<4u> const*> xs_r;
  for ( auto i = 0u; i < 4u; ++i )
  {
    xs_r.emplace_back( &xs[i] );
  }
  xag_index_list<true> list_separate;
  encode( list_separate, xag );

  xag_list_simulator<kitty::static_truth_table<4u>> sim_separate;
  sim_separate( list_separate, xs_r );
  kitty::static_truth_table<4u> tt4_separate, tt5_separate, tt6_separate;
  sim_separate.get_simulation_inline( tt4_separate, list_separate, xs_r, 10u );
  sim_separate.get_simulation_inline( tt5_separate, list_separate, xs_r, 12u );
  sim_separate.get_simulation_inline( tt6_separate, list_separate, xs_r, 14u );
  CHECK( kitty::equal( xs[4], tt4_separate ) );
  CHECK( kitty::equal( xs[5], tt5_separate ) );
  CHECK( kitty::equal( xs[6], tt6_separate ) );

  xag_index_list<false> list_unified;
  encode( list_unified, xag );
  xag_list_simulator<kitty::static_truth_table<4u>> sim_unified;
  sim_unified( list_unified, xs_r );
  kitty::static_truth_table<4u> tt4_unified, tt5_unified, tt6_unified;
  sim_unified.get_simulation_inline( tt4_unified, list_unified, xs_r, 10u );
  sim_unified.get_simulation_inline( tt5_unified, list_unified, xs_r, 12u );
  sim_unified.get_simulation_inline( tt6_unified, list_unified, xs_r, 14u );
  CHECK( kitty::equal( xs[4], tt4_unified ) );
  CHECK( kitty::equal( xs[5], tt5_unified ) );
  CHECK( kitty::equal( xs[6], tt6_unified ) );
}

TEST_CASE( "simulation of xag_index_list with dynamic truth tables", "[list_simulator]" )
{
  xag_network xag;
  auto const a = xag.create_pi();
  auto const b = xag.create_pi();
  auto const c = xag.create_pi();
  auto const d = xag.create_pi();
  auto const t0 = xag.create_and( a, b );
  auto const t1 = xag.create_and( c, d );
  auto const t2 = xag.create_xor( t0, t1 );
  xag.create_po( t2 );

  std::vector<kitty::dynamic_truth_table> xs;
  for ( auto i = 0u; i < 4u; ++i )
  {
    xs.emplace_back( 4u );
    kitty::create_nth_var( xs[i], i );
  }
  xs.emplace_back( xs[0] & xs[1] ); // 4
  xs.emplace_back( xs[2] & xs[3] ); // 5
  xs.emplace_back( xs[4] ^ xs[5] ); // 6

  std::vector<kitty::dynamic_truth_table const*> xs_r;
  for ( auto i = 0u; i < 4u; ++i )
  {
    xs_r.emplace_back( &xs[i] );
  }
  xag_index_list<true> list_separate;
  encode( list_separate, xag );
  xag_list_simulator<kitty::dynamic_truth_table> sim_separate;
  sim_separate( list_separate, xs_r );
  kitty::dynamic_truth_table tt4_separate( 4u ), tt5_separate( 4u ), tt6_separate( 4u );
  sim_separate.get_simulation_inline( tt4_separate, list_separate, xs_r, 10u );
  sim_separate.get_simulation_inline( tt5_separate, list_separate, xs_r, 12u );
  sim_separate.get_simulation_inline( tt6_separate, list_separate, xs_r, 14u );
  CHECK( kitty::equal( xs[4], tt4_separate ) );
  CHECK( kitty::equal( xs[5], tt5_separate ) );
  CHECK( kitty::equal( xs[6], tt6_separate ) );

  xag_index_list<false> list_unified;
  encode( list_unified, xag );
  xag_list_simulator<kitty::dynamic_truth_table> sim_unified;
  sim_unified( list_unified, xs_r );
  kitty::dynamic_truth_table tt4_unified( 4u ), tt5_unified( 4u ), tt6_unified( 4u );
  sim_unified.get_simulation_inline( tt4_unified, list_unified, xs_r, 10u );
  sim_unified.get_simulation_inline( tt5_unified, list_unified, xs_r, 12u );
  sim_unified.get_simulation_inline( tt6_unified, list_unified, xs_r, 14u );
  CHECK( kitty::equal( xs[4], tt4_unified ) );
  CHECK( kitty::equal( xs[5], tt5_unified ) );
  CHECK( kitty::equal( xs[6], tt6_unified ) );
}
