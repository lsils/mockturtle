#include <catch.hpp>

#include <set>

#include <mockturtle/traits.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/views/parents_view.hpp>

using namespace mockturtle;

template<typename Ntk>
void test_parents_view()
{
  CHECK( is_network_type_v<Ntk> );
  CHECK( !has_foreach_parent_v<Ntk> );

  using parent_ntk = parents_view<Ntk>;

  CHECK( is_network_type_v<parent_ntk> );
  CHECK( has_foreach_parent_v<parent_ntk> );

  using parent_parent_ntk = parents_view<parent_ntk>;

  CHECK( is_network_type_v<parent_parent_ntk> );
  CHECK( has_foreach_parent_v<parent_parent_ntk> );
};

TEST_CASE( "create different parentsw views", "[parents_view]" )
{
  test_parents_view<aig_network>();
  test_parents_view<mig_network>();
  test_parents_view<klut_network>();
}

TEST_CASE( "compute parents for AIG", "[parents_view]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  parents_view parent_aig{aig};

  {
    std::set<node<aig_network>> nodes;
    parent_aig.foreach_parent( aig.get_node( a ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == std::set<node<aig_network>>{ aig.get_node( f1 ), aig.get_node( f2 ) } );
  }

  {
    std::set<node<aig_network>> nodes;
    parent_aig.foreach_parent( aig.get_node( b ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == std::set<node<aig_network>>{ aig.get_node( f1 ), aig.get_node( f3 ) } );
  }

  {
    std::set<node<aig_network>> nodes;
    parent_aig.foreach_parent( aig.get_node( f1 ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == std::set<node<aig_network>>{ aig.get_node( f2 ), aig.get_node( f3 ) } );
  }

  {
    std::set<node<aig_network>> nodes;
    parent_aig.foreach_parent( aig.get_node( f2 ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == std::set<node<aig_network>>{ aig.get_node( f4 ) } );
  }

  {
    std::set<node<aig_network>> nodes;
    parent_aig.foreach_parent( aig.get_node( f3 ), [&]( const auto& p ){ nodes.insert( p ); } );
    CHECK( nodes == std::set<node<aig_network>>{ aig.get_node( f4 ) } );
  }
}
