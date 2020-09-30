#include <catch.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/window_view.hpp>
#include <mockturtle/algorithms/reconv_cut.hpp>

using namespace mockturtle;

template<typename Ntk>
std::vector<typename Ntk::node> collect_fanin_nodes( Ntk const& ntk, typename Ntk::node const& n )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  std::vector<node> fanin_nodes;
  ntk.foreach_fanin( n, [&]( signal const& fi ){
    fanin_nodes.emplace_back( ntk.get_node( fi ) );
  });
  return fanin_nodes;
}

/*! \brief Ensure that all outputs and fanins belong to the window */
template<typename Ntk>
bool window_is_well_formed( Ntk const& ntk )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  bool all_fanins_belong_to_window = true;
  ntk.foreach_node( [&]( node const& n ){
    ntk.foreach_fanin( n, [&]( signal const& fi ){
      if ( !ntk.belongs_to_window( ntk.get_node( fi ) ) )
      {
        all_fanins_belong_to_window = false;
        return false; /* terminate */
      }
      return true; /* next */
    });

    /* check if property is already violated */
    if ( !all_fanins_belong_to_window )
    {
      return false; /* terminate */
    }
    return true; /* next */
  });

  bool all_outputs_belong_to_window = true;
  ntk.foreach_po( [&]( signal const& o ){
    if ( !ntk.belongs_to_window( ntk.get_node( o ) ) )
    {
      all_outputs_belong_to_window = false;
      return false; /* terminate */
    }
    return true; /* next */
  });

  return all_fanins_belong_to_window &&
         all_outputs_belong_to_window;
}

TEST_CASE( "create new window view on AIG", "[window_view]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  CHECK( aig.size() == 7 );
  CHECK( aig.num_pis() == 2 );
  CHECK( aig.num_pos() == 1 );
  CHECK( aig.num_gates() == 4 );

  /* make a window */
  {
    new_window_view view( aig,
                          /* inputs = */ { aig.get_node( a ), aig.get_node( b ) },
                          /* outputs = */ { f3 },
                          /* nodes = */ { aig.get_node( f1 ), aig.get_node( f3 ) } );
    CHECK( view.size() == 5 );
    CHECK( view.num_gates() == 2 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 1 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 1 );

    CHECK(  view.belongs_to_window( view.get_node( f1 ) ) );
    CHECK( !view.belongs_to_window( view.get_node( f2 ) ) );
    CHECK(  view.belongs_to_window( view.get_node( f3 ) ) );
    CHECK( !view.belongs_to_window( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f3 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }

  {
    new_window_view view( aig,
                          /* inputs = */ { aig.get_node( f1 ), aig.get_node( b ) },
                          /* outputs = */ { f3 },
                          /* nodes = */ { aig.get_node( f3 ) } );
    CHECK( view.size() == 4 );
    CHECK( view.num_gates() == 1 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 1 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 1 );

    CHECK(  view.belongs_to_window( view.get_node( f1 ) ) );
    CHECK( !view.belongs_to_window( view.get_node( f2 ) ) );
    CHECK(  view.belongs_to_window( view.get_node( f3 ) ) );
    CHECK( !view.belongs_to_window( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 0 );
    CHECK( collect_fanin_nodes( view, view.get_node( f3 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }

  {
    new_window_view view( aig,
                          /* inputs = */ { aig.get_node( a ), aig.get_node( b ) },
                          /* outputs = */ { f2 },
                          /* nodes = */ { aig.get_node( f1 ), aig.get_node( f2 ) } );

    CHECK( view.size() == 5 );
    CHECK( view.num_gates() == 2 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 1 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 1 );

    CHECK(  view.belongs_to_window( view.get_node( f1 ) ) );
    CHECK(  view.belongs_to_window( view.get_node( f2 ) ) );
    CHECK( !view.belongs_to_window( view.get_node( f3 ) ) );
    CHECK( !view.belongs_to_window( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f2 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }

  {
    new_window_view view( aig,
                          /* inputs = */ { aig.get_node( a ), aig.get_node( b ) },
                          /* outputs = */ { f4 },
                          /* nodes = */ { aig.get_node( f1 ), aig.get_node( f2 ), aig.get_node( f3 ), aig.get_node( f4 ) } );

    CHECK( view.size() == 7 );
    CHECK( view.num_gates() == 4 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 1 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 1 );

    CHECK( view.belongs_to_window( view.get_node( f1 ) ) );
    CHECK( view.belongs_to_window( view.get_node( f2 ) ) );
    CHECK( view.belongs_to_window( view.get_node( f3 ) ) );
    CHECK( view.belongs_to_window( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f2 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f3 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f4 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }

  {
    new_window_view view( aig,
                          /* inputs = */ { aig.get_node( a ), aig.get_node( b ) },
                          /* outputs = */ { f2, f3 },
                          /* nodes = */ { aig.get_node( f1 ), aig.get_node( f2 ), aig.get_node( f3 ) } );

    CHECK( view.size() == 6 );
    CHECK( view.num_gates() == 3 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 2 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 2 );

    CHECK(  view.belongs_to_window( view.get_node( f1 ) ) );
    CHECK(  view.belongs_to_window( view.get_node( f2 ) ) );
    CHECK(  view.belongs_to_window( view.get_node( f3 ) ) );
    CHECK( !view.belongs_to_window( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f2 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f3 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }
}

TEST_CASE( "create window view on AIG", "[window_view]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  CHECK( aig.size() == 7 );
  CHECK( aig.num_pis() == 2 );
  CHECK( aig.num_pos() == 1 );
  CHECK( aig.num_gates() == 4 );

  fanout_view<aig_network> fanout_ntk( aig );
  fanout_ntk.clear_visited();

  const auto pivot1 = aig.get_node( f3 );

  const auto leaves1 = reconvergence_driven_cut<aig_network, false, false>( aig, pivot1 ).first;
  window_view<fanout_view<aig_network>> win1( fanout_ntk, leaves1, { pivot1 }, false );

  CHECK( is_network_type_v<decltype( win1 )> );
  CHECK( win1.size() == 5 );
  CHECK( win1.num_pis() == 2 );
  CHECK( win1.num_pos() == 3 ); // b, f1, f2

  const auto pivot2 = aig.get_node( f2 );
  const auto leaves2 = reconvergence_driven_cut<aig_network, false, false>( aig, pivot2 ).first;
  window_view<fanout_view<aig_network>> win2( fanout_ntk, leaves2, { pivot2 }, false );
  CHECK( win2.size() == 5 );
  CHECK( win2.num_pis() == 2 );
  CHECK( win2.num_pos() == 3 ); // a, f1, f3

  const auto pivot3 = aig.get_node( f1 );
  const auto leaves3 = reconvergence_driven_cut<aig_network, false, false>( aig, pivot3 ).first;
  window_view<fanout_view<aig_network>> win3( fanout_ntk, leaves3, { pivot3 }, true );
  CHECK( win3.size() == 7 );
  CHECK( win3.num_pis() == 2 );
  CHECK( win3.num_pos() == 1 ); // f4
}
