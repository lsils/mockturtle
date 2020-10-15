#include <catch.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/window_view.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/algorithms/reconv_cut.hpp>
#include <mockturtle/views/color_view.hpp>

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

template<typename Ntk>
class create_window_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  struct window_info
  {
    std::vector<node> inputs;
    std::vector<node> nodes;
    std::vector<signal> outputs;
  };

protected:
  /* constant node used to denotes invalid window element */
  static constexpr node INVALID_NODE{0};

public:
  create_window_impl( Ntk const& ntk )
    : ntk( ntk )
    , path( ntk.size() )
    , refs( ntk.size() )
  {
  }

  std::optional<window_info> run( node const& pivot )
  {
    /* find a reconvergence from the pivot and collect the nodes */
    std::optional<std::vector<node>> nodes;
    if ( !( nodes = identify_reconvergence( pivot, 1u ) ) )
    {
      /* if there is no reconvergence, then optimization is not possible */
      return std::nullopt;
    }

    /* collect the fanins for these nodes */
    ntk.new_color();
    std::vector<node> inputs = collect_inputs( ntk, *nodes );

    /* expand the nodes towards the TFI */
    ntk.new_color();
    expand_towards_tfi( ntk, inputs, 6u );

    /* expand the nodes towards the TFO */
    ntk.new_color();
    levelized_expand_towards_tfo( ntk, inputs, *nodes );

    /* collect the nodes with fanout outside of nodes */
    ntk.new_color();
    std::vector<signal> outputs = collect_outputs( ntk, inputs, *nodes, refs );

    /* top. sort nodes */
    std::sort( std::begin( inputs ), std::end( inputs ) );
    std::sort( std::begin( *nodes ), std::end( *nodes ) );

    return window_info{inputs, *nodes, outputs};
  }

protected:
  std::optional<std::vector<node>> identify_reconvergence( node const& pivot, uint64_t num_iterations )
  {
    visited.clear();
    ntk.foreach_fanin( pivot, [&]( signal const& fi ){
      uint32_t const color = ntk.new_color();
      node const& n = ntk.get_node( fi );
      path[n] = INVALID_NODE;
      visited.push_back( n );
      ntk.paint( n, color );
    });

    uint64_t start{0};
    uint64_t stop;
    for ( uint32_t iteration = 0u; iteration < num_iterations; ++iteration )
    {
      stop = visited.size();
      for ( uint32_t i = start; i < stop; ++i )
      {
        node const n = visited.at( i );
        std::optional<node> meet = explore_frontier_of_node( n );
        if ( meet )
        {
          visited.clear();
          gather_nodes_recursively( *meet );
          gather_nodes_recursively( n );
          visited.push_back( pivot );
          return visited;
        }
      }
      start = stop;
    }

    return std::nullopt;
  }

  std::optional<node> explore_frontier_of_node( node const& n )
  {
    std::optional<node> meet;
    ntk.foreach_fanin( n, [&]( signal const& fi ){
      node const& fi_node = ntk.get_node( fi );
      if ( ntk.is_constant( fi_node ) || ntk.is_ci( fi_node ) )
      {
        return true; /* next */
      }

      if ( ntk.eval_color( n, [this]( auto c ){ return c > ntk.current_color() - ntk.max_fanin_size; } )&&
           ntk.eval_color( fi_node, [this]( auto c ){ return c > ntk.current_color() - ntk.max_fanin_size; } ) &&
           ntk.eval_color( n, fi_node, []( auto c0, auto c1 ){ return c0 != c1; } ) )
      {
        meet = fi_node;
        return false;
      }

      if ( ntk.eval_color( fi_node, [this]( auto c ){ return c > ntk.current_color() - ntk.max_fanin_size; } ) )
      {
        return true; /* next */
      }

      ntk.paint( fi_node, n );
      path[fi_node] = n;
      visited.push_back( fi_node );
      return true; /* next */
    });

    return meet;
  }

  /* collect nodes recursively following along the `path` until INVALID_NODE is reached */
  void gather_nodes_recursively( node const& n )
  {
    if ( n == INVALID_NODE )
    {
      return;
    }

    visited.push_back( n );
    node const pred = path[n];
    if ( pred == INVALID_NODE )
    {
      return;
    }

    assert( ntk.eval_color( n, pred, []( auto c0, auto c1 ){ return c0 == c1; } ) );
    gather_nodes_recursively( pred );
  }

protected:
  Ntk const& ntk;
  std::vector<node> visited;
  std::vector<node> path;
  std::vector<uint32_t> refs;
}; /* create_window_impl */

TEST_CASE( "expand node set towards TFI without cut-size", "[window_utils]" )
{
  using node = typename aig_network::node;

  aig_network _aig;
  auto const a = _aig.create_pi();
  auto const b = _aig.create_pi();
  auto const c = _aig.create_pi();
  auto const d = _aig.create_pi();
  auto const f1 = _aig.create_and( b, c );
  auto const f2 = _aig.create_and( b, f1 );
  auto const f3 = _aig.create_and( a, f2 );
  auto const f4 = _aig.create_and( d, f2 );
  auto const f5 = _aig.create_and( f3, f4 );
  _aig.create_po( f5 );

  color_view aig{_aig};
  aig.new_color();

  {
    /* a cut that can be expanded without increasing cut-size */
    std::vector<node> inputs{aig.get_node( a ), aig.get_node( b ), aig.get_node( f1 ), aig.get_node( d )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs );
    CHECK( trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( b ), aig.get_node( c ), aig.get_node( d )} );
  }

  {
    aig.new_color();

    /* a cut that cannot be expanded without increasing cut-size */
    std::vector<node> inputs{aig.get_node( f3 ), aig.get_node( f4 )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs );
    CHECK( !trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( f3 ), aig.get_node( f4 )} );
  }

  {
    aig.new_color();

    /* a cut that can be moved towards the PIs */
    std::vector<node> inputs{aig.get_node( f2 ), aig.get_node( f3 ), aig.get_node( f4 )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs );
    CHECK( !trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( d ), aig.get_node( f2 )} );
  }

  {
    aig.new_color();

    /* the cut { f3, f5 } can be simplified to { f3, f4 } */
    std::vector<node> inputs{aig.get_node( f3 ), aig.get_node( f5 )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs );
    CHECK( !trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( f3 ), aig.get_node( f4 )} );
  }

  {
    aig.new_color();

    /* the cut { f4, f5 } also can be simplified to { f3, f4 } */
    std::vector<node> inputs{aig.get_node( f4 ), aig.get_node( f5 )};

    bool const trivial_cut = expand0_towards_tfi( aig, inputs );
    CHECK( !trivial_cut );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( f3 ), aig.get_node( f4 )} );
  }
}

TEST_CASE( "expand node set towards TFI", "[window_utils]" )
{
  using node = typename aig_network::node;

  aig_network _aig;
  auto const a = _aig.create_pi();
  auto const b = _aig.create_pi();
  auto const c = _aig.create_pi();
  auto const d = _aig.create_pi();
  auto const f1 = _aig.create_and( b, c );
  auto const f2 = _aig.create_and( b, f1 );
  auto const f3 = _aig.create_and( a, f2 );
  auto const f4 = _aig.create_and( d, f2 );
  auto const f5 = _aig.create_and( f3, f4 );
  _aig.create_po( f5 );

  color_view aig{_aig};
  aig.new_color();

  {
    /* expand from { f5 } to 4-cut { a, b, c, d } */
    std::vector<node> inputs{aig.get_node( f5 )};
    expand_towards_tfi( aig, inputs, 4u );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( b ), aig.get_node( c ), aig.get_node( d )} );
  }

  {
    aig.new_color();

    /* expand from { f3, f5 } to 3-cut { a, b, f2 } */
    std::vector<node> inputs{aig.get_node( f3 ), aig.get_node( f5 )};
    expand_towards_tfi( aig, inputs, 3u );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( d ), aig.get_node( f2 )} );
  }

  {
    aig.new_color();

    /* expand from { f4, f5 } to 3-cut { a, b, f2 } */
    std::vector<node> inputs{aig.get_node( f4 ), aig.get_node( f5 )};
    expand_towards_tfi( aig, inputs, 3u );

    std::sort( std::begin( inputs ), std::end( inputs ) );
    CHECK( inputs == std::vector<node>{aig.get_node( a ), aig.get_node( d ), aig.get_node( f2 )} );
  }
}

TEST_CASE( "expand node set towards TFO", "[window_utils]" )
{
  using node = typename aig_network::node;

  aig_network _aig;
  auto const a = _aig.create_pi();
  auto const b = _aig.create_pi();
  auto const c = _aig.create_pi();
  auto const d = _aig.create_pi();
  auto const f1 = _aig.create_and( b, c );
  auto const f2 = _aig.create_and( b, f1 );
  auto const f3 = _aig.create_and( a, f2 );
  auto const f4 = _aig.create_and( d, f2 );
  auto const f5 = _aig.create_and( f3, f4 );
  _aig.create_po( f5 );

  std::vector<node> inputs{_aig.get_node( a ), _aig.get_node( b ), _aig.get_node( c ), _aig.get_node( d )};

  fanout_view fanout_aig{_aig};
  depth_view depth_aig{fanout_aig};
  color_view aig{depth_aig};
  aig.new_color();

  {
    std::vector<node> nodes;
    expand_towards_tfo( aig, inputs, nodes );

    std::sort( std::begin( nodes ), std::end( nodes ) );
    CHECK( nodes == std::vector<node>{aig.get_node( f1 ), aig.get_node( f2 ), aig.get_node( f3 ),
                                       aig.get_node( f4 ), aig.get_node( f5 )} );
  }

  {
    aig.new_color();

    std::vector<node> nodes;
    levelized_expand_towards_tfo( aig, inputs, nodes );

    std::sort( std::begin( nodes ), std::end( nodes ) );
    CHECK( nodes == std::vector<node>{aig.get_node( f1 ), aig.get_node( f2 ), aig.get_node( f4 )} );
  }
}

TEST_CASE( "make a window", "[create_window]" )
{
  aig_network _aig;
  auto const a = _aig.create_pi();
  auto const b = _aig.create_pi();
  auto const c = _aig.create_pi();
  auto const d = _aig.create_pi();
  auto const f1 = _aig.create_and( b, c );
  auto const f2 = _aig.create_and( b, f1 );
  auto const f3 = _aig.create_and( a, f2 );
  auto const f4 = _aig.create_and( d, f2 );
  auto const f5 = _aig.create_and( f3, f4 );
  _aig.create_po( f5 );

  fanout_view fanout_aig{_aig};
  depth_view depth_aig{fanout_aig};
  color_view aig{depth_aig};
  aig.new_color();

  create_window_impl windowing( aig );
  auto info = windowing.run( aig.get_node( f5 ) );
  CHECK( info );
  if ( info )
  {
    window_view win( aig, info->inputs, info->outputs, info->nodes );
    CHECK( win.num_cis() == 4u );
    CHECK( win.num_cos() == 2u );
    CHECK( win.num_gates() == 4u );
  }
}
