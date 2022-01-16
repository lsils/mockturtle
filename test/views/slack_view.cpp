#include <catch.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/slack_view.hpp>
#include <mockturtle/views/fanout_view.hpp>

using namespace mockturtle;

template<typename Ntk>
void test_slack_view()
{
  CHECK( is_network_type_v<Ntk> );
  CHECK( !has_slack_v<Ntk> );

  using slack_ntk = slack_view<Ntk>;

  CHECK( is_network_type_v<slack_ntk> );
  CHECK( has_slack_v<slack_ntk> );

  using slack_slack_ntk = slack_view<slack_ntk>;

  CHECK( is_network_type_v<slack_slack_ntk> );
  CHECK( has_slack_v<slack_slack_ntk> );
};

TEST_CASE( "create different slack views", "[slack_view]" )
{
  test_slack_view<aig_network>();
  test_slack_view<mig_network>();
  test_slack_view<klut_network>();
}

TEST_CASE( "compute require time for AIG", "[slack_view]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  slack_view slack_aig{fanout_view{aig}};
  CHECK( slack_aig.required( aig.get_node( a ) ) == 3 );
  CHECK( slack_aig.required( aig.get_node( b ) ) == 3 );
  CHECK( slack_aig.required( aig.get_node( f1 ) ) == 2 );
  CHECK( slack_aig.required( aig.get_node( f2 ) ) == 1 );
  CHECK( slack_aig.required( aig.get_node( f3 ) ) == 1 );
  CHECK( slack_aig.required( aig.get_node( f4 ) ) == 0 );
}
