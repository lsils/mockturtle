#include <catch.hpp>
#include <vector>

#include <mockturtle/algorithms/functional_reduction.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>

using namespace mockturtle;

TEST_CASE( "functional reduction on AIG", "[functional_reduction]" )
{
  aig_network ntk;

  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  const auto f1 = ntk.create_and( a, !b );
  const auto f2 = ntk.create_and( !a, b );
  const auto f3 = ntk.create_and( !a, !b );
  const auto f4 = ntk.create_and( a, b );
  const auto f5 = ntk.create_or( f1, f2 ); // a ^ b
  const auto f6 = ntk.create_or( f3, f4 ); // a == b
  const auto f7 = ntk.create_and( f5, f6 ); // 0

  ntk.create_po( f5 );
  ntk.create_po( f6 );
  ntk.create_po( f7 );

  CHECK( ntk.size() == 10 );
  functional_reduction( ntk );
  ntk = cleanup_dangling( ntk );
  CHECK( ntk.size() == 6 );
}
