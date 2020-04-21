#include <catch.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/utils/frontier.hpp>

using namespace mockturtle;

TEST_CASE( "explore nodes", "[frontier]" )
{
  aig_network aig;
  const auto a = aig.create_pi(); // 1
  const auto b = aig.create_pi(); // 2
  const auto f1 = aig.create_nand( a, b ); // 3
  const auto f2 = aig.create_nand( f1, a ); // 4
  const auto f3 = aig.create_nand( f1, b ); // 5
  const auto f4 = aig.create_nand( f2, f3 ); // 6
  const auto f5 = aig.create_xor( f3, f1 ); // 7, 8
  aig.create_po( f4 );
  aig.create_po( f5 );

  fanout_view fanout_aig{aig};
  depth_view depth_aig{fanout_aig};

  aig.incr_trav_id();

  using node = node<aig_network>;
  std::vector<node> nodes;
  frontier<decltype( depth_aig )> f( depth_aig, aig.get_node( f5 ), { f2, f3 } );
  while ( f.grow( [&]( auto const& n ){
        nodes.push_back( n );
        return true;
      }) );

  CHECK( nodes == std::vector<node>{ 5, 8, 7, 6, 4, 3, 2, 1 } );
}
