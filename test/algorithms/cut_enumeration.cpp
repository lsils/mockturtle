#include <catch.hpp>

#include <iostream>

#include <mockturtle/algorithms/cut_enumeration.hpp>
#include <mockturtle/networks/aig.hpp>

using namespace mockturtle;

TEST_CASE( "enumerate cuts for an AIG", "[cut_enumeration]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( f1, a );
  const auto f3 = aig.create_nand( f1, b );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  const auto cuts = cut_enumeration( aig );

  const auto to_vector = []( auto const& cut ) {
    return std::vector<uint32_t>( cut.begin(), cut.end() );
  };

  /* all unit cuts are in the back */
  aig.foreach_node( [&]( auto n ) {
    if ( aig.is_constant( n ) )
      return;

    auto const& set = cuts.cut_set( aig.node_to_index( n ) );
    CHECK( to_vector( set[set.size() - 1] ) == std::vector<uint32_t>{aig.node_to_index( n )} );
  } );

  const auto i1 = aig.node_to_index( aig.get_node( f1 ) );
  const auto i2 = aig.node_to_index( aig.get_node( f2 ) );
  const auto i3 = aig.node_to_index( aig.get_node( f3 ) );
  const auto i4 = aig.node_to_index( aig.get_node( f4 ) );

  CHECK( cuts.cut_set( i1 ).size() == 2 );
  CHECK( cuts.cut_set( i2 ).size() == 3 );
  CHECK( cuts.cut_set( i3 ).size() == 3 );
  CHECK( cuts.cut_set( i4 ).size() == 5 );

  CHECK( to_vector( cuts.cut_set( i1 )[0] ) == std::vector<uint32_t>{1, 2} );

  CHECK( to_vector( cuts.cut_set( i2 )[0] ) == std::vector<uint32_t>{1, 3} );
  CHECK( to_vector( cuts.cut_set( i2 )[1] ) == std::vector<uint32_t>{1, 2} );

  CHECK( to_vector( cuts.cut_set( i3 )[0] ) == std::vector<uint32_t>{2, 3} );
  CHECK( to_vector( cuts.cut_set( i3 )[1] ) == std::vector<uint32_t>{1, 2} );

  CHECK( to_vector( cuts.cut_set( i4 )[0] ) == std::vector<uint32_t>{4, 5} );
  CHECK( to_vector( cuts.cut_set( i4 )[1] ) == std::vector<uint32_t>{1, 2} );
  CHECK( to_vector( cuts.cut_set( i4 )[2] ) == std::vector<uint32_t>{2, 3, 4} );
  CHECK( to_vector( cuts.cut_set( i4 )[3] ) == std::vector<uint32_t>{1, 3, 5} );
}

TEST_CASE( "enumerate smaller cuts for an AIG", "[cut_enumeration]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( f1, a );
  const auto f3 = aig.create_nand( f1, b );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  cut_enumeration_params ps;
  ps.cut_size = 2;
  const auto cuts = cut_enumeration( aig, ps );

  const auto to_vector = []( auto const& cut ) {
    return std::vector<uint32_t>( cut.begin(), cut.end() );
  };

  const auto i1 = aig.node_to_index( aig.get_node( f1 ) );
  const auto i2 = aig.node_to_index( aig.get_node( f2 ) );
  const auto i3 = aig.node_to_index( aig.get_node( f3 ) );
  const auto i4 = aig.node_to_index( aig.get_node( f4 ) );

  CHECK( cuts.cut_set( i1 ).size() == 2 );
  CHECK( cuts.cut_set( i2 ).size() == 3 );
  CHECK( cuts.cut_set( i3 ).size() == 3 );
  CHECK( cuts.cut_set( i4 ).size() == 3 );

  CHECK( to_vector( cuts.cut_set( i1 )[0] ) == std::vector<uint32_t>{1, 2} );

  CHECK( to_vector( cuts.cut_set( i2 )[0] ) == std::vector<uint32_t>{1, 3} );
  CHECK( to_vector( cuts.cut_set( i2 )[1] ) == std::vector<uint32_t>{1, 2} );

  CHECK( to_vector( cuts.cut_set( i3 )[0] ) == std::vector<uint32_t>{2, 3} );
  CHECK( to_vector( cuts.cut_set( i3 )[1] ) == std::vector<uint32_t>{1, 2} );

  CHECK( to_vector( cuts.cut_set( i4 )[0] ) == std::vector<uint32_t>{4, 5} );
  CHECK( to_vector( cuts.cut_set( i4 )[1] ) == std::vector<uint32_t>{1, 2} );
}

TEST_CASE( "compute truth tables of AIG cuts", "[cut_enumeration]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( f1, a );
  const auto f3 = aig.create_nand( f1, b );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  const auto cuts = cut_enumeration<aig_network, true>( aig );

  const auto i1 = aig.node_to_index( aig.get_node( f1 ) );
  const auto i2 = aig.node_to_index( aig.get_node( f2 ) );
  const auto i3 = aig.node_to_index( aig.get_node( f3 ) );
  const auto i4 = aig.node_to_index( aig.get_node( f4 ) );

  CHECK( cuts.cut_set( i1 ).size() == 2 );
  CHECK( cuts.cut_set( i2 ).size() == 3 );
  CHECK( cuts.cut_set( i3 ).size() == 3 );
  CHECK( cuts.cut_set( i4 ).size() == 5 );

  CHECK( cuts.truth_table( cuts.cut_set( i1 )[0] )._bits[0] == 0x8 );
  CHECK( cuts.truth_table( cuts.cut_set( i2 )[0] )._bits[0] == 0x2 );
  CHECK( cuts.truth_table( cuts.cut_set( i2 )[1] )._bits[0] == 0x2 );
  CHECK( cuts.truth_table( cuts.cut_set( i3 )[0] )._bits[0] == 0x2 );
  CHECK( cuts.truth_table( cuts.cut_set( i3 )[1] )._bits[0] == 0x4 );
  CHECK( cuts.truth_table( cuts.cut_set( i4 )[0] )._bits[0] == 0x1 );
  CHECK( cuts.truth_table( cuts.cut_set( i4 )[1] )._bits[0] == 0x9 );
  CHECK( cuts.truth_table( cuts.cut_set( i4 )[2] )._bits[0] == 0x0d );
  CHECK( cuts.truth_table( cuts.cut_set( i4 )[3] )._bits[0] == 0x0d );
}
