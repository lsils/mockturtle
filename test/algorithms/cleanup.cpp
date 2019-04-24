#include <catch.hpp>
#include <vector>

#include <kitty/operators.hpp>
#include <kitty/static_truth_table.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>

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
  ntk.create_nand( f2, f3 );

  CHECK( ntk.size() == 7 );

  const auto ntk2 = cleanup_dangling( ntk );

  CHECK( ntk2.size() == 3 );
}

template<class NtkSource, class NtkDest>
void test_cleanup_into_network()
{
  NtkSource ntk;

  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  const auto f1 = ntk.create_xor( a, b );
  const auto f2 = ntk.create_nand( a, f1 );
  const auto f3 = ntk.create_nand( b, f1 );
  ntk.create_po( ntk.create_maj( f1, f2, f3 ) );

  NtkDest dest;
  const auto x1 = dest.create_pi();
  const auto x2 = dest.create_pi();
  std::vector<typename NtkDest::signal> pis = {x1, x2};
  dest.create_po( cleanup_dangling( ntk, dest, pis.begin(), pis.end() )[0] );

  CHECK( simulate<kitty::static_truth_table<2>>( ntk )[0] == simulate<kitty::static_truth_table<2>>( dest )[0] );
}

TEST_CASE( "cleanup networks without PO", "[cleanup]" )
{
  test_cleanup_network<aig_network>();
  test_cleanup_network<xag_network>();
  test_cleanup_network<mig_network>();
  test_cleanup_network<xmg_network>();
}

TEST_CASE( "cleanup networks with different types", "[cleanup]" )
{
  test_cleanup_into_network<aig_network, xag_network>();
  test_cleanup_into_network<xag_network, aig_network>();
  test_cleanup_into_network<aig_network, mig_network>();
  test_cleanup_into_network<mig_network, aig_network>();

  test_cleanup_into_network<aig_network, klut_network>();
  test_cleanup_into_network<xag_network, klut_network>();
  test_cleanup_into_network<mig_network, klut_network>();
}
