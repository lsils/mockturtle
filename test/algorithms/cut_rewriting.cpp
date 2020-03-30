#include <catch.hpp>

#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/exact.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xmg3_npn.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/traits.hpp>

using namespace mockturtle;

TEST_CASE( "Cut rewriting of bad MAJ", "[cut_rewriting]" )
{
  mig_network mig;
  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();

  const auto f = mig.create_maj( a, mig.create_maj( a, b, c ), c );
  mig.create_po( f );

  mig_npn_resynthesis resyn;
  cut_rewriting( mig, resyn );

  mig = cleanup_dangling( mig );

  CHECK( mig.size() == 5 );
  CHECK( mig.num_pis() == 3 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 1 );
}

TEST_CASE( "Cut rewriting with XMG3 4-input npn database", "[cut_rewriting]" )
{

  xmg_network xmg;
  const auto a = xmg.create_pi();
  const auto b = xmg.create_pi();
  const auto c = xmg.create_pi();

  const auto h = xmg.create_xor3( a, xmg.create_maj( a, b, c ), c );

  xmg.create_po( h );
  xmg3_npn_resynthesis<xmg_network> resyn;
  cut_rewriting( xmg, resyn );

  xmg = cleanup_dangling( xmg );
  CHECK( xmg.size() == 5 );
  CHECK( xmg.num_pis() == 3 );
  CHECK( xmg.num_pos() == 1 );
  CHECK( xmg.num_gates() == 1 );
}

TEST_CASE( "Cut rewriting from constant", "[cut_rewriting]" )
{
  mig_network mig;
  mig.create_po( mig.get_constant( false ) );

  mig_npn_resynthesis resyn;
  cut_rewriting( mig, resyn );

  mig = cleanup_dangling( mig );

  CHECK( mig.size() == 1 );
  CHECK( mig.num_pis() == 0 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 0 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( f == mig.get_constant( false ) );
  } );
}

TEST_CASE( "Cut rewriting from inverted constant", "[cut_rewriting]" )
{
  mig_network mig;
  mig.create_po( mig.get_constant( true ) );

  mig_npn_resynthesis resyn;
  cut_rewriting( mig, resyn );

  mig = cleanup_dangling( mig );

  CHECK( mig.size() == 1 );
  CHECK( mig.num_pis() == 0 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 0 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( f == mig.get_constant( true ) );
  } );
}

TEST_CASE( "Cut rewriting from projection", "[cut_rewriting]" )
{
  mig_network mig;
  mig.create_po( mig.create_pi() );

  mig_npn_resynthesis resyn;
  cut_rewriting( mig, resyn );

  mig = cleanup_dangling( mig );

  CHECK( mig.size() == 2 );
  CHECK( mig.num_pis() == 1 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 0 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( mig.get_node( f ) == 1 );
    CHECK( !mig.is_complemented( f ) );
  } );
}

TEST_CASE( "Cut rewriting from inverted projection", "[cut_rewriting]" )
{
  mig_network mig;
  mig.create_po( !mig.create_pi() );

  mig_npn_resynthesis resyn;
  cut_rewriting( mig, resyn );

  mig = cleanup_dangling( mig );

  CHECK( mig.size() == 2 );
  CHECK( mig.num_pis() == 1 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 0 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( mig.get_node( f ) == 1 );
    CHECK( mig.is_complemented( f ) );
  } );
}

TEST_CASE( "Cut rewriting with exact LUT synthesis", "[cut_rewriting]" )
{
  klut_network klut;
  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto d = klut.create_pi();

  klut.create_po( klut.create_and( a, klut.create_and( b, klut.create_and( c, d ) ) ) );

  CHECK( klut.num_pis() == 4u );
  CHECK( klut.num_pos() == 1u );
  CHECK( klut.num_gates() == 3u );

  exact_resynthesis resyn( 3u );
  cut_rewriting( klut, resyn );

  klut = cleanup_dangling( klut );

  CHECK( klut.num_pis() == 4u );
  CHECK( klut.num_pos() == 1u );
  CHECK( klut.num_gates() == 2u );
}

namespace detail
{
template<class Ntk>
struct free_xor_cost
{
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n ) const
  {
    return ntk.is_xor( n ) ? 0 : 1;
  }
};
} // namespace detail

TEST_CASE( "Cut rewriting with alternative costs", "[cut_rewriting]" )
{
  mig_network mig;
  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();

  const auto f = mig.create_maj( a, mig.create_maj( a, b, c ), c );
  mig.create_po( f );

  mig_npn_resynthesis resyn;
  cut_rewriting( mig, resyn, {}, nullptr, ::detail::free_xor_cost<mig_network>() );

  mig = cleanup_dangling( mig );

  CHECK( mig.size() == 5 );
  CHECK( mig.num_pis() == 3 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 1 );
}
