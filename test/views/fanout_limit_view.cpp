#include <catch.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/fanout_limit_view.hpp>

using namespace mockturtle;

TEST_CASE( "test node replication", "[fanout_limit_view]" )
{
  using node = mig_network::node;
  using signal = mig_network::signal;

  fanout_limit_view_params ps{4u};
  fanout_limit_view<mig_network> lim_mig{ps};

  signal const a = lim_mig.create_pi();
  signal const b = lim_mig.create_pi();
  signal const c = lim_mig.create_pi();

  /* create a node */
  signal const f1 = lim_mig.create_maj( a, b, c );

  /* make f1 very popular */
  lim_mig.create_and( f1, a );
  lim_mig.create_and( f1, b );
  lim_mig.create_and( f1, c );
  lim_mig.create_or( f1, a );

  CHECK( lim_mig.num_gates() == 5u );

  lim_mig.create_or( f1, b );
  lim_mig.create_or( f1, c );

  CHECK( lim_mig.num_gates() == 8u );
  lim_mig.foreach_gate( [&]( node const& n ){
      CHECK( lim_mig.fanout_size( n ) <= 4u );
    });
}

TEST_CASE( "test rippled replication", "[fanout_limit_view]" )
{
  using node = mig_network::node;
  using signal = mig_network::signal;

  fanout_limit_view_params ps{4u};
  fanout_limit_view<mig_network> lim_mig{ps};

  signal const a = lim_mig.create_pi();
  signal const b = lim_mig.create_pi();
  signal const c = lim_mig.create_pi();
  signal const d = lim_mig.create_pi();
  signal const e = lim_mig.create_pi();

  /* create two nodes */
  signal const f1 = lim_mig.create_maj( a, b, c );
  signal const f2 = lim_mig.create_maj( d, f1, e );

  /* make f1 and f2 popular */
  lim_mig.create_and( f1, a );
  lim_mig.create_and( f1, b );
  lim_mig.create_and( f1, c );

  lim_mig.create_and( f2, a );
  lim_mig.create_and( f2, b );
  lim_mig.create_and( f2, c );
  lim_mig.create_or( f2, a );

  CHECK( lim_mig.num_gates() == 9u );
  lim_mig.foreach_gate( [&]( node const& n ){
      CHECK( lim_mig.fanout_size( n ) <= 4u );
    });

  /* +3 majority gates, because first f2 has to be replicated, and then also f1 */
  lim_mig.create_or( f2, b );

  CHECK( lim_mig.num_gates() == 12u );
  lim_mig.foreach_gate( [&]( node const& n ){
      CHECK( lim_mig.fanout_size( n ) <= 4u );
    });

  CHECK( lim_mig.fanout_size( lim_mig.get_node( f1 ) ) == 4u );
  CHECK( lim_mig.fanout_size( lim_mig.get_node( f2 ) ) == 4u );
}

TEST_CASE( "test duplicate fanout node", "[fanout_limit_view]" )
{
  using node = mig_network::node;
  using signal = mig_network::signal;

  fanout_limit_view_params ps{4u};
  fanout_limit_view<mig_network> lim_mig{ps};

  signal const a = lim_mig.create_pi();
  signal const b = lim_mig.create_pi();
  signal const c = lim_mig.create_pi();

  signal const f = lim_mig.create_maj( a, b, c );

  /* only one node is needed for fanout up to 4 */
  lim_mig.create_po( f );
  lim_mig.create_po( f );
  lim_mig.create_po( f );
  lim_mig.create_po( f );

  CHECK( lim_mig.num_gates() == 1u );
  CHECK( lim_mig.fanout_size( lim_mig.get_node( f ) ) == 4u );

  /* afterward the node need to be replicated */
  lim_mig.create_po( f );
  lim_mig.create_po( f );
  lim_mig.create_po( f );
  lim_mig.create_po( f );

  CHECK( lim_mig.num_gates() == 2u );

  lim_mig.foreach_gate( [&]( node const& n ){
      CHECK( lim_mig.fanout_size( n ) <= 4u );
    });
}


TEST_CASE( "popular PI", "[fanout_limit_view]" )
{
  using node = mig_network::node;
  using signal = mig_network::signal;

  fanout_limit_view_params ps{4u, true};
  fanout_limit_view<mig_network> lim_mig{ps};

  signal const a = lim_mig.create_pi();
  signal const b = lim_mig.create_pi();
  signal const c = lim_mig.create_pi();
  signal const d = lim_mig.create_pi();

  /* make `a` very popular */
  signal const f1 = lim_mig.create_maj( a, b, c );
  lim_mig.create_maj( a, b, d );
  lim_mig.create_maj( a, c, d );
  lim_mig.create_and( f1, a );
  lim_mig.create_or( f1, a );

  CHECK( lim_mig.num_gates() == 5u );
  CHECK( lim_mig.num_pis() == 4u ); /* duplicated PI does not increase `num_pis` */
  uint32_t count{0u};
  lim_mig.foreach_pi( [&]( node const& n ){
      (void)n;
      ++count;
    });
  CHECK( count == 4u ); /* duplicated PI is iterated only once in `foreach_pi` */

  /* 5 gates + 1 constant + 4 PIs + 1 duplicated PI */
  CHECK( lim_mig.size() == 11u );

  count = 0u;
  lim_mig.foreach_node( [&]( node const& n ){
      ++count;
      CHECK( lim_mig.fanout_size( n ) <= 4u );
    });
  CHECK( count == 11u ); /* duplicated PI is iterated twice in `foreach_node` */
}
