#include <catch.hpp>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>

using namespace mockturtle;

template<class Ntk>
void test_cleanup_network()
{
  Ntk ntk;

  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  const auto f1 = ntk.create_nand( a, b );
  const auto f2 = ntk.create_nand( a, f1 );
  const auto f3 = ntk.create_nand( b, f1 );
  const auto f4 = ntk.create_nand( f2, f3 );

  CHECK( ntk.size() == 7 );

  const auto ntk2 = cleanup_dangling( ntk );

  CHECK( ntk2.size() == 3 );
}

TEST_CASE( "cleanup networks without PO", "[cleanup]" )
{
  test_cleanup_network<aig_network>();
  test_cleanup_network<mig_network>();
}
