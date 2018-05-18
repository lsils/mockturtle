#include <catch.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/depth_view.hpp>

using namespace mockturtle;

TEST_CASE( "MIG depth optimization", "[mig_algebraic_rewriting]" )
{
  mig_network mig;

  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();
  const auto d = mig.create_pi();

  const auto f1 = mig.create_and( a, b );
  const auto f2 = mig.create_and( f1, c );
  const auto f3 = mig.create_and( f2, d );

  mig.create_po( f3 );

  depth_view depth_mig{mig};

  CHECK( depth_mig.depth() == 3 );

  mig_algebraic_dfs_depth_rewriting( depth_mig );

  CHECK( depth_mig.depth() == 2 );
}
