#include <catch.hpp>

#include <algorithm>

#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/akers.hpp>
#include <mockturtle/algorithms/node_resynthesis/direct.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>

using namespace mockturtle;

TEST_CASE( "Node resynthesis with optimum networks", "[node_resynthesis]" )
{
  kitty::dynamic_truth_table maj( 3 );
  kitty::create_majority( maj );

  klut_network klut;
  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto f = klut.create_node( {a, b, c}, maj );
  klut.create_po( f );

  mig_npn_resynthesis resyn;
  const auto mig = node_resynthesis<mig_network>( klut, resyn );

  CHECK( mig.size() == 5 );
  CHECK( mig.num_pis() == 3 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 1 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( !mig.is_complemented( f ) );
  } );

  mig.foreach_node( [&]( auto n ) {
    mig.foreach_fanin( n, [&]( auto const& f ) {
      CHECK( !mig.is_complemented( f ) );
    } );
  } );
}

TEST_CASE( "Node resynthesis with Akers resynthesis", "[node_resynthesis]" )
{
  kitty::dynamic_truth_table maj( 3 );
  kitty::create_majority( maj );

  klut_network klut;
  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto f = klut.create_node( {a, b, c}, maj );
  klut.create_po( f );

  akers_resynthesis resyn;
  const auto mig = node_resynthesis<mig_network>( klut, resyn );

  CHECK( mig.size() == 5 );
  CHECK( mig.num_pis() == 3 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 1 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( !mig.is_complemented( f ) );
  } );

  mig.foreach_node( [&]( auto n ) {
    mig.foreach_fanin( n, [&]( auto const& f ) {
      CHECK( !mig.is_complemented( f ) );
    } );
  } );
}

TEST_CASE( "Node resynthesis from constant", "[node_resynthesis]" )
{
  klut_network klut;
  klut.create_po( klut.get_constant( false ) );

  mig_npn_resynthesis resyn;
  const auto mig = node_resynthesis<mig_network>( klut, resyn );

  CHECK( mig.size() == 1 );
  CHECK( mig.num_pis() == 0 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 0 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( f == mig.get_constant( false ) );
  } );
}

TEST_CASE( "Node resynthesis from inverted constant", "[node_resynthesis]" )
{
  klut_network klut;
  klut.create_po( klut.get_constant( true ) );

  mig_npn_resynthesis resyn;
  const auto mig = node_resynthesis<mig_network>( klut, resyn );

  CHECK( mig.size() == 1 );
  CHECK( mig.num_pis() == 0 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 0 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( f == mig.get_constant( true ) );
  } );
}

TEST_CASE( "Node resynthesis from projection", "[node_resynthesis]" )
{
  klut_network klut;
  klut.create_po( klut.create_pi() );

  mig_npn_resynthesis resyn;
  const auto mig = node_resynthesis<mig_network>( klut, resyn );

  CHECK( mig.size() == 2 );
  CHECK( mig.num_pis() == 1 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 0 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( !mig.is_complemented( f ) );
    CHECK( mig.get_node( f ) == 1 );
  } );
}

TEST_CASE( "Node resynthesis from negated projection", "[node_resynthesis]" )
{
  klut_network klut;
  klut.create_po( klut.create_not( klut.create_pi() ) );

  mig_npn_resynthesis resyn;
  const auto mig = node_resynthesis<mig_network>( klut, resyn );

  CHECK( mig.size() == 2 );
  CHECK( mig.num_pis() == 1 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 0 );

  mig.foreach_po( [&]( auto const& f ) {
    CHECK( mig.is_complemented( f ) );
    CHECK( mig.get_node( f ) == 1 );
  } );
}

TEST_CASE( "Node resynthesis with direct synthesis", "[selected]" )
{
  direct_resynthesis<aig_network> aig_resyn;
  direct_resynthesis<xag_network> xag_resyn;
  direct_resynthesis<mig_network> mig_resyn;
  direct_resynthesis<xmg_network> xmg_resyn;

  for ( auto v = 0u; v <= 2u; ++v )
  {
    for ( auto f = 0u; f < ( 1u << v ); ++f )
    {
      kitty::dynamic_truth_table tt( v );
      kitty::create_from_words( tt, &f, &f + 1 );

      klut_network klut;
      std::vector<klut_network::signal> pis( v );
      std::generate( pis.begin(), pis.end(), [&]() { return klut.create_pi(); } );
      klut.create_po( klut.create_node( pis, tt ) );

      CHECK( klut.num_pis() == v );
      CHECK( klut.num_pos() == 1 );
      CHECK( klut.num_gates() == 1 );

      const auto aig = node_resynthesis<aig_network>( klut, aig_resyn );
      CHECK( simulate<kitty::dynamic_truth_table>( aig, {v} )[0] == tt );

      const auto xag = node_resynthesis<xag_network>( klut, xag_resyn );
      CHECK( simulate<kitty::dynamic_truth_table>( xag, {v} )[0] == tt );

      const auto mig = node_resynthesis<mig_network>( klut, mig_resyn );
      CHECK( simulate<kitty::dynamic_truth_table>( mig, {v} )[0] == tt );

      const auto xmg = node_resynthesis<xmg_network>( klut, xmg_resyn );
      CHECK( simulate<kitty::dynamic_truth_table>( xmg, {v} )[0] == tt );
    }
  }

  for ( auto f : std::vector<uint64_t>{{0x00, 0xff, 0xe8, 0xd4, 0xb2, 0x8e, 0x71, 0x4d, 0x2b, 0x17, 0x69, 0x96}} )
  {
    kitty::dynamic_truth_table tt( 3u );
    kitty::create_from_words( tt, &f, &f + 1 );

    klut_network klut;
    std::vector<klut_network::signal> pis( 3u );
    std::generate( pis.begin(), pis.end(), [&]() { return klut.create_pi(); } );
    klut.create_po( klut.create_node( pis, tt ) );

    CHECK( klut.num_pis() == 3u );
    CHECK( klut.num_pos() == 1 );
    CHECK( klut.num_gates() == 1 );

    if ( f != 0x69 && f != 0x96 )
    {
      const auto mig = node_resynthesis<mig_network>( klut, mig_resyn );
      CHECK( simulate<kitty::dynamic_truth_table>( mig, {3u} )[0] == tt );
    }

    const auto xmg = node_resynthesis<xmg_network>( klut, xmg_resyn );
    CHECK( simulate<kitty::dynamic_truth_table>( xmg, {3u} )[0] == tt );
  }
}
