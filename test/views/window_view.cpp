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
      if ( !ntk.belongs_to( ntk.get_node( fi ) ) )
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
    if ( !ntk.belongs_to( ntk.get_node( o ) ) )
    {
      all_outputs_belong_to_window = false;
      return false; /* terminate */
    }
    return true; /* next */
  });

  return all_fanins_belong_to_window &&
         all_outputs_belong_to_window;
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

  /* make a window */
  {
    window_view view( aig,
                      /* inputs = */ { aig.get_node( a ), aig.get_node( b ) },
                      /* outputs = */ { f3 },
                      /* nodes = */ { aig.get_node( f1 ), aig.get_node( f3 ) } );
    CHECK( view.size() == 5 );
    CHECK( view.num_gates() == 2 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 1 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 1 );

    CHECK(  view.belongs_to( view.get_node( f1 ) ) );
    CHECK( !view.belongs_to( view.get_node( f2 ) ) );
    CHECK(  view.belongs_to( view.get_node( f3 ) ) );
    CHECK( !view.belongs_to( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f3 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }

  {
    window_view view( aig,
                      /* inputs = */ { aig.get_node( f1 ), aig.get_node( b ) },
                      /* outputs = */ { f3 },
                      /* nodes = */ { aig.get_node( f3 ) } );
    CHECK( view.size() == 4 );
    CHECK( view.num_gates() == 1 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 1 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 1 );

    CHECK(  view.belongs_to( view.get_node( f1 ) ) );
    CHECK( !view.belongs_to( view.get_node( f2 ) ) );
    CHECK(  view.belongs_to( view.get_node( f3 ) ) );
    CHECK( !view.belongs_to( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 0 );
    CHECK( collect_fanin_nodes( view, view.get_node( f3 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }

  {
    window_view view( aig,
                      /* inputs = */ { aig.get_node( a ), aig.get_node( b ) },
                      /* outputs = */ { f2 },
                      /* nodes = */ { aig.get_node( f1 ), aig.get_node( f2 ) } );

    CHECK( view.size() == 5 );
    CHECK( view.num_gates() == 2 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 1 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 1 );

    CHECK(  view.belongs_to( view.get_node( f1 ) ) );
    CHECK(  view.belongs_to( view.get_node( f2 ) ) );
    CHECK( !view.belongs_to( view.get_node( f3 ) ) );
    CHECK( !view.belongs_to( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f2 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }

  {
    window_view view( aig,
                      /* inputs = */ { aig.get_node( a ), aig.get_node( b ) },
                      /* outputs = */ { f4 },
                      /* nodes = */ { aig.get_node( f1 ), aig.get_node( f2 ), aig.get_node( f3 ), aig.get_node( f4 ) } );

    CHECK( view.size() == 7 );
    CHECK( view.num_gates() == 4 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 1 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 1 );

    CHECK( view.belongs_to( view.get_node( f1 ) ) );
    CHECK( view.belongs_to( view.get_node( f2 ) ) );
    CHECK( view.belongs_to( view.get_node( f3 ) ) );
    CHECK( view.belongs_to( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f2 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f3 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f4 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }

  {
    window_view view( aig,
                      /* inputs = */ { aig.get_node( a ), aig.get_node( b ) },
                      /* outputs = */ { f2, f3 },
                      /* nodes = */ { aig.get_node( f1 ), aig.get_node( f2 ), aig.get_node( f3 ) } );

    CHECK( view.size() == 6 );
    CHECK( view.num_gates() == 3 );
    CHECK( view.num_pis() == 2 );
    CHECK( view.num_pos() == 2 );
    CHECK( view.num_cis() == 2 );
    CHECK( view.num_cos() == 2 );

    CHECK(  view.belongs_to( view.get_node( f1 ) ) );
    CHECK(  view.belongs_to( view.get_node( f2 ) ) );
    CHECK(  view.belongs_to( view.get_node( f3 ) ) );
    CHECK( !view.belongs_to( view.get_node( f4 ) ) );

    CHECK( collect_fanin_nodes( view, view.get_node( f1 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f2 ) ).size() == 2 );
    CHECK( collect_fanin_nodes( view, view.get_node( f3 ) ).size() == 2 );
    CHECK( window_is_well_formed( view ) );
  }
}

TEST_CASE( "collect nodes", "[window_view]" )
{
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const c = aig.create_pi();
  auto const d = aig.create_pi();
  auto const f1 = aig.create_xor( a, b );
  auto const f2 = aig.create_xor( c, d );
  auto const f3 = aig.create_xor( f1, f2 );
  auto const f4 = aig.create_and( f1, f2 );
  aig.create_po( f3 );
  aig.create_po( f4 );

  using node = typename aig_network::node;
  using signal = typename aig_network::signal;

  std::vector<node> inputs{aig.get_node( a ), aig.get_node( b ), aig.get_node( c ), aig.get_node( d )};
  std::vector<signal> outputs{f3, f4};
  std::vector<node> const gates{collect_nodes( aig, inputs, outputs )};

  CHECK( gates.size() == aig.num_gates() );
  aig.foreach_gate( [&]( node const& n ){
    CHECK( std::find( std::begin( gates ), std::end( gates ), n ) != std::end( gates ) );
  });
}

TEST_CASE( "expand towards tfo", "[window_view]" )
{
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const c = aig.create_pi();
  auto const d = aig.create_pi();
  auto const f1 = aig.create_xor( a, b );
  auto const f2 = aig.create_xor( c, d );
  auto const f3 = aig.create_xor( f1, f2 );
  auto const f4 = aig.create_and( f1, f2 );
  aig.create_po( f3 );
  aig.create_po( f4 );

  using node = typename aig_network::node;
  using signal = typename aig_network::signal;

  fanout_view<aig_network> faig( aig );

  std::vector<node> inputs{faig.get_node( a ), faig.get_node( b ), faig.get_node( c ), faig.get_node( d )};
  std::vector<signal> outputs{f1};
  std::vector<node> gates{collect_nodes( faig, inputs, outputs )};
  expand_towards_tfo( faig, inputs, gates );

  CHECK( gates.size() == aig.num_gates() );
  aig.foreach_gate( [&]( node const& n ){
    CHECK( std::find( std::begin( gates ), std::end( gates ), n ) != std::end( gates ) );
  });
}


TEST_CASE( "expand node set towards TFI without cut-size", "[window_utils]" )
{
  using node = typename aig_network::node;

  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const c = aig.create_pi();
  auto const d = aig.create_pi();
  auto const f1 = aig.create_and( b, c );
  auto const f2 = aig.create_and( b, f1 );
  auto const f3 = aig.create_and( a, f2 );
  auto const f4 = aig.create_and( d, f2 );
  auto const f5 = aig.create_and( f3, f4 );
  aig.create_po( f5 );

  std::vector<uint32_t> colors( aig.size() );

  {
    /* a cut that can be expanded without increasing cut-size */
    std::vector<node> inputs{aig.get_node( a ), aig.get_node( b ), aig.get_node( f1 ), aig.get_node( d )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs, colors, 1u );
    CHECK( trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( b ), aig.get_node( c ), aig.get_node( d )} );
  }

  {
    /* a cut that cannot be expanded without increasing cut-size */
    std::vector<node> inputs{aig.get_node( f3 ), aig.get_node( f4 )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs, colors, 2u );
    CHECK( !trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( f3 ), aig.get_node( f4 )} );
  }

  {
    /* a cut that can be moved towards the PIs */
    std::vector<node> inputs{aig.get_node( f2 ), aig.get_node( f3 ), aig.get_node( f4 )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs, colors, 3u );
    CHECK( !trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( d ), aig.get_node( f2 )} );
  }

  {
    /* the cut { f3, f5 } can be simplified to { f3, f4 } */
    std::vector<node> inputs{aig.get_node( f3 ), aig.get_node( f5 )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs, colors, 4u );
    CHECK( !trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( f3 ), aig.get_node( f4 )} );
  }

  {
    /* the cut { f4, f5 } also can be simplified to { f3, f4 } */
    std::vector<node> inputs{aig.get_node( f4 ), aig.get_node( f5 )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs, colors, 5u );
    CHECK( !trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( f3 ), aig.get_node( f4 )} );
  }
}

TEST_CASE( "expand node set towards TFI", "[window_utils]" )
{
  using node = typename aig_network::node;

  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const c = aig.create_pi();
  auto const d = aig.create_pi();
  auto const f1 = aig.create_and( b, c );
  auto const f2 = aig.create_and( b, f1 );
  auto const f3 = aig.create_and( a, f2 );
  auto const f4 = aig.create_and( d, f2 );
  auto const f5 = aig.create_and( f3, f4 );
  aig.create_po( f5 );

  std::vector<uint32_t> colors( aig.size() );

  {
    /* expand from { f5 } to 4-cut { a, b, c, d } */
    std::vector<node> inputs{aig.get_node( f5 )};
    expand_towards_tfi( aig, inputs, 4u, colors, 1u );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( b ), aig.get_node( c ), aig.get_node( d )} );
  }

  {
    /* expand from { f3, f5 } to 3-cut { a, b, f2 } */
    std::vector<node> inputs{aig.get_node( f3 ), aig.get_node( f5 )};
    expand_towards_tfi( aig, inputs, 3u, colors, 2u );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( d ), aig.get_node( f2 )} );
  }

  {
    /* expand from { f4, f5 } to 3-cut { a, b, f2 } */
    std::vector<node> inputs{aig.get_node( f4 ), aig.get_node( f5 )};
    expand_towards_tfi( aig, inputs, 3u, colors, 3u );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( d ), aig.get_node( f2 )} );
  }
}
