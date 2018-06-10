#include <catch.hpp>

#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>

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
