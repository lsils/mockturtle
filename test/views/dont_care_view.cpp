#include <catch.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/views/dont_care_view.hpp>

#include <vector>

using namespace mockturtle;

template<typename Ntk>
void test_create_dont_care_view()
{
  Ntk ntk;
  auto const a = ntk.create_pi();
  auto const b = ntk.create_pi();
  auto const c = ntk.create_pi();
  auto const t1 = ntk.create_and( a, b );
  auto const t2 = ntk.create_and( b, c );
  auto const f = ntk.create_and( t1, t2 );
  ntk.create_po( f );

  Ntk dc;
  dc.create_pi();
  dc.create_pi();
  dc.create_pi();
  dc.create_po( dc.create_and( dc.create_and( a, b ), c ) );

  std::vector<bool> pat1{1, 1, 1};
  std::vector<bool> pat2{1, 1, 0};

  dont_care_view<Ntk> dc_ntk( ntk, dc );

  CHECK( dc_ntk.pattern_is_EXCDC( pat1 ) );
  CHECK( !dc_ntk.pattern_is_EXCDC( pat2 ) );
}

TEST_CASE( "create dont_care_view and test API", "[dont_care_view]" )
{
  test_create_dont_care_view<aig_network>();
  test_create_dont_care_view<mig_network>();
  test_create_dont_care_view<xag_network>();
  test_create_dont_care_view<xmg_network>();
  test_create_dont_care_view<klut_network>();
}

template<typename Ntk>
void test_copy_dont_care_view()
{
  Ntk ntk;
  auto const a = ntk.create_pi();
  auto const b = ntk.create_pi();
  auto const c = ntk.create_pi();
  auto const t1 = ntk.create_and( a, b );
  auto const t2 = ntk.create_and( b, c );
  auto const f = ntk.create_and( t1, t2 );
  ntk.create_po( f );

  Ntk dc;
  dc.create_pi();
  dc.create_pi();
  dc.create_pi();
  dc.create_po( dc.create_and( dc.create_and( a, b ), c ) );

  std::vector<bool> pat1{1, 1, 1};
  std::vector<bool> pat2{1, 1, 0};

  dont_care_view<Ntk> dc_ntk( ntk, dc );
  dont_care_view<Ntk> new_dc_ntk = dc_ntk;
  CHECK( dc_ntk.pattern_is_EXCDC( pat1 ) );
  CHECK( !dc_ntk.pattern_is_EXCDC( pat2 ) );
  CHECK( new_dc_ntk.pattern_is_EXCDC( pat1 ) );
  CHECK( !new_dc_ntk.pattern_is_EXCDC( pat2 ) );
}

TEST_CASE( "copy dont_care_view", "[dont_care_view]" )
{
  test_copy_dont_care_view<aig_network>();
  test_copy_dont_care_view<mig_network>();
  test_copy_dont_care_view<xag_network>();
  test_copy_dont_care_view<xmg_network>();
  test_copy_dont_care_view<klut_network>();
}
