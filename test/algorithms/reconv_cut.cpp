#include <catch.hpp>

#include <mockturtle/algorithms/reconv_cut.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/networks/aig.hpp>
#include <set>

using namespace mockturtle;

TEST_CASE( "generate fanin-cuts for an AIG", "[reconv_cut]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( f1, a );
  const auto f3 = aig.create_nand( f1, b );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  /* some unordered container */
  using set_t = std::set<node<aig_network>>;

  /* helper functions for extracting leave sets */
  auto leaves = [&]( auto const& p, uint64_t size = 10u ){
    const auto leaves = reconvergence_driven_cut<aig_network, false, false>( aig, p, reconvergence_driven_cut_parameters{ size } ).first;
    return set_t( std::begin( leaves ), std::end( leaves ) );
  };

  /* for all CIs i : rrcut( i ) == { i } */
  CHECK( leaves( a ) == set_t{ aig.get_node( a ) } );
  CHECK( leaves( b ) == set_t{ aig.get_node( b ) } );

  CHECK( leaves( f1 ) == set_t{ aig.get_node( a ), aig.get_node( b ) } );
  CHECK( leaves( f2 ) == set_t{ aig.get_node( a ), aig.get_node( b ) } );
  CHECK( leaves( f3 ) == set_t{ aig.get_node( a ), aig.get_node( b ) } );
  CHECK( leaves( f4 ) == set_t{ aig.get_node( a ), aig.get_node( b ) } );

  CHECK( leaves( f4, 1u ) == set_t{ aig.get_node( f4 ) } );
  CHECK( leaves( f4, 2u ) == set_t{ aig.get_node( f2 ), aig.get_node( f3 ) } );
  CHECK( leaves( f4, 3u ) == set_t{ aig.get_node( a ), aig.get_node( b ) } );
  CHECK( leaves( f4, 2u ) == set_t{ aig.get_node( f2 ), aig.get_node( f3 ) } );
  CHECK( leaves( f4, 3u ) == set_t{ aig.get_node( a ), aig.get_node( b ) } );
}
