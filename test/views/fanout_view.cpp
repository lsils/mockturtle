#include <catch.hpp>

#include <set>

#include <mockturtle/traits.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/views/fanout_view.hpp>

using namespace mockturtle;

template<typename Ntk>
void test_fanout_view()
{
  CHECK( is_network_type_v<Ntk> );
  CHECK( !has_foreach_fanout_v<Ntk> );

  using fanout_ntk = fanout_view<Ntk>;

  CHECK( is_network_type_v<fanout_ntk> );
  CHECK( has_foreach_fanout_v<fanout_ntk> );

  using fanout_fanout_ntk = fanout_view<fanout_ntk>;

  CHECK( is_network_type_v<fanout_fanout_ntk> );
  CHECK( has_foreach_fanout_v<fanout_fanout_ntk> );
};

TEST_CASE( "create different fanout views", "[fanout_view]" )
{
  test_fanout_view<aig_network>();
  test_fanout_view<mig_network>();
  test_fanout_view<xag_network>();
  test_fanout_view<xmg_network>();
  test_fanout_view<klut_network>();
}

template<typename Ntk>
void test_fanout_computation()
{
  using node = node<Ntk>;
  using nodes_t = std::set<node>;

  Ntk ntk;
  auto const a = ntk.create_pi();
  auto const b = ntk.create_pi();
  auto const f1 = ntk.create_and( a, b );
  auto const f2 = ntk.create_and( a, f1 );
  auto const f3 = ntk.create_and( b, f1 );
  auto const f4 = ntk.create_and( f2, f3 );
  ntk.create_po( f4 );

  fanout_view fanout_ntk{ntk};
  {
    nodes_t nodes;
    fanout_ntk.foreach_fanout( ntk.get_node( a ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == nodes_t{ntk.get_node( f1 ), ntk.get_node( f2 )} );
  }

  {
    nodes_t nodes;
    fanout_ntk.foreach_fanout( ntk.get_node( b ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == nodes_t{ntk.get_node( f1 ), ntk.get_node( f3 )} );
  }

  {
    nodes_t nodes;
    fanout_ntk.foreach_fanout( ntk.get_node( f1 ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == nodes_t{ntk.get_node( f2 ), ntk.get_node( f3 )} );
  }

  {
    nodes_t nodes;
    fanout_ntk.foreach_fanout( ntk.get_node( f2 ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == nodes_t{ntk.get_node( f4 )} );
  }

  {
    nodes_t nodes;
    fanout_ntk.foreach_fanout( ntk.get_node( f3 ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == nodes_t{ntk.get_node( f4 )} );
  }
}

TEST_CASE( "compute fanouts for network", "[fanout_view]" )
{
  test_fanout_computation<aig_network>();
  test_fanout_computation<xag_network>();
  test_fanout_computation<mig_network>();
  test_fanout_computation<xmg_network>();
  test_fanout_computation<klut_network>();
}

TEST_CASE( "compute fanouts during node construction after move ctor", "[fanout_view]" )
{
  xag_network xag{};
  auto tmp = new fanout_view<xag_network>{xag};
  fanout_view<xag_network> fxag{ std::move( *tmp ) }; // move ctor
  delete tmp;

  auto const a = fxag.create_pi();
  auto const b = fxag.create_pi();
  auto const c = fxag.create_pi();
  auto const f = fxag.create_xor( b, fxag.create_and( fxag.create_xor( a, b ), fxag.create_xor( b, c ) ) );
  fxag.create_po( f );

  using node = node<xag_network>;
  xag.foreach_node( [&]( node const& n ){
    std::set<node> fanouts;
    fxag.foreach_fanout( n, [&]( node const& fo ){
      fanouts.insert( fo );
    } );

    /* `fanout_size` counts internal and external fanouts (POs), but `fanouts` only contains internal fanouts */
    CHECK( fanouts.size() + ( xag.get_node( f ) == n ) == xag.fanout_size( n ) );
  } );
}

TEST_CASE( "compute fanouts during node construction after copy ctor", "[fanout_view]" )
{
  xag_network xag{};
  auto tmp = new fanout_view<xag_network>{xag};
  fanout_view<xag_network> fxag{*tmp}; // copy ctor
  delete tmp;

  auto const a = fxag.create_pi();
  auto const b = fxag.create_pi();
  auto const c = fxag.create_pi();
  auto const f = fxag.create_xor( b, fxag.create_and( fxag.create_xor( a, b ), fxag.create_xor( b, c ) ) );
  fxag.create_po( f );

  using node = node<xag_network>;
  xag.foreach_node( [&]( node const& n ){
    std::set<node> fanouts;
    fxag.foreach_fanout( n, [&]( node const& fo ){
      fanouts.insert( fo );
    } );

    /* `fanout_size` counts internal and external fanouts (POs), but `fanouts` only contains internal fanouts */
    CHECK( fanouts.size() + ( xag.get_node( f ) == n ) == xag.fanout_size( n ) );
  } );
}

TEST_CASE( "compute fanouts during node construction after copy assignment", "[fanout_view]" )
{
  xag_network xag{};
  fanout_view<xag_network> fxag;
  {
    auto tmp = new fanout_view<xag_network>{xag};
    fxag = *tmp; /* copy assignment */
    delete tmp;
  }

  auto const a = fxag.create_pi();
  auto const b = fxag.create_pi();
  auto const c = fxag.create_pi();
  auto const f = fxag.create_xor( b, fxag.create_and( fxag.create_xor( a, b ), fxag.create_xor( b, c ) ) );
  fxag.create_po( f );

  using node = node<xag_network>;
  xag.foreach_node( [&]( node const& n ){
    std::set<node> fanouts;
    fxag.foreach_fanout( n, [&]( node const& fo ){
      fanouts.insert( fo );
    } );

    /* `fanout_size` counts internal and external fanouts (POs), but `fanouts` only contains internal fanouts */
    CHECK( fanouts.size() + ( xag.get_node( f ) == n ) == xag.fanout_size( n ) );
  } );
}

TEST_CASE( "compute fanouts during node construction after move assignment", "[fanout_view]" )
{
  xag_network xag{};
  fanout_view<xag_network> fxag;
  {
    auto tmp = new fanout_view<xag_network>{xag};
    fxag = std::move( *tmp ); /* move assignment */
    delete tmp;
  }

  auto const a = fxag.create_pi();
  auto const b = fxag.create_pi();
  auto const c = fxag.create_pi();
  auto const f = fxag.create_xor( b, fxag.create_and( fxag.create_xor( a, b ), fxag.create_xor( b, c ) ) );
  fxag.create_po( f );

  using node = node<xag_network>;
  xag.foreach_node( [&]( node const& n ){
    std::set<node> fanouts;
    fxag.foreach_fanout( n, [&]( node const& fo ){
      fanouts.insert( fo );
    } );

    /* `fanout_size` counts internal and external fanouts (POs), but `fanouts` only contains internal fanouts */
    CHECK( fanouts.size() + ( xag.get_node( f ) == n ) == xag.fanout_size( n ) );
  } );
}
