#include <catch.hpp>

#include <iostream>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <kitty/print.hpp>

#include <mockturtle/algorithms/akers_synthesis.hpp>
#include <mockturtle/networks/mig.hpp>

using namespace mockturtle;

TEST_CASE( "Check Akers for MAJ-3", "[maj_3_akers]" )
{
  mig_network mig;

  std::vector<kitty::dynamic_truth_table> xs{5, kitty::dynamic_truth_table( 3 )};

  create_majority( xs[0] );
  for ( auto i = 0u; i < unsigned( xs[0].num_bits() ); i++ )
  {
    set_bit( xs[1], i );
  }

  mig = akers_synthesis( mig, xs[0], xs[1] );

  kitty::create_nth_var( xs[2], 0 );
  kitty::create_nth_var( xs[3], 1 );
  kitty::create_nth_var( xs[4], 2 );

  CHECK( mig.compute( mig.index_to_node( mig.size() ), xs.begin() + 2, xs.end() ) == xs[0] );
  CHECK( mig.size() == 5 );
}

TEST_CASE( "Check Akers for MAJ-5", "[maj_5_akers]" )
{
  mig_network mig;

  std::vector<kitty::dynamic_truth_table> xs{7, kitty::dynamic_truth_table( 5 )};

  create_majority( xs[0] );
  for ( auto i = 0u; i < unsigned( xs[0].num_bits() ); i++ )
  {
    set_bit( xs[1], i );
  }

  mig = akers_synthesis( mig, xs[0], xs[1] );

  kitty::create_nth_var( xs[2], 0 );
  kitty::create_nth_var( xs[3], 1 );
  kitty::create_nth_var( xs[4], 2 );
  kitty::create_nth_var( xs[5], 3 );
  kitty::create_nth_var( xs[6], 4 );

  mig.foreach_gate( [&]( auto n ) {
    std::vector<kitty::dynamic_truth_table> fanin{3, kitty::dynamic_truth_table( 5 )};
    mig.foreach_fanin( n, [&]( auto s, auto j ) {
      fanin[j] = xs[mig.node_to_index( mig.get_node( s ) ) + 1];
    } );
    xs.push_back( mig.compute( n, fanin.begin(), fanin.end() ) );
  } );
  CHECK( xs[xs.size() - 1] == xs[0] );
}

TEST_CASE( "Check Akers for random - 4 inputs", "[maj_random4_akers]" )
{
  for ( auto y = 0; y < 5; y++ )
  {
    mig_network mig;

    std::vector<kitty::dynamic_truth_table> xs{6, kitty::dynamic_truth_table( 4 )};
    kitty::create_nth_var( xs[2], 0 );
    kitty::create_nth_var( xs[3], 1 );
    kitty::create_nth_var( xs[4], 2 );
    kitty::create_nth_var( xs[5], 3 );

    create_random( xs[0] );
    //kitty::print_binary(xs[0], std::cout); std::cout << std::endl; 

    for ( auto i = 0u; i < unsigned( xs[0].num_bits() ); i++ )
    {
      set_bit( xs[1], i );
    }

    mig = akers_synthesis( mig, xs[0], xs[1] );
    if ( mig.size() > 4 )
    {
      mig.foreach_gate( [&]( auto n ) {
        std::vector<kitty::dynamic_truth_table> fanin{3, kitty::dynamic_truth_table( 4 )};
        mig.foreach_fanin( n, [&]( auto s, auto j ) {
          if ( mig.node_to_index( mig.get_node( s ) ) == 0 )
          {
            fanin[j] = ~xs[1];
          }
          else
          {
            fanin[j] = xs[mig.node_to_index( mig.get_node( s ) ) + 1];
          }
        } );
        xs.push_back( mig.compute( n, fanin.begin(), fanin.end() ) );
      } );

      mig.foreach_po( [&]( auto n ) {
        if ( mig.is_complemented( n ) )
          CHECK( ~xs[xs.size() - 1] == xs[0] );
        else
          CHECK( xs[xs.size() - 1] == xs[0] );
      } );
    }
  }
}

TEST_CASE( "Check Akers for random - 5 inputs", "[maj_random5_akers]" )
{
  for ( auto y = 0; y < 5; y++ )
  {
    mig_network mig;

    std::vector<kitty::dynamic_truth_table> xs{7, kitty::dynamic_truth_table( 5 )};
    kitty::create_nth_var( xs[2], 0 );
    kitty::create_nth_var( xs[3], 1 );
    kitty::create_nth_var( xs[4], 2 );
    kitty::create_nth_var( xs[5], 3 );
    kitty::create_nth_var( xs[6], 4 );

    create_random( xs[0] );

    for ( auto i = 0u; i < unsigned( xs[0].num_bits() ); i++ )
    {
      set_bit( xs[1], i );
    }

    mig = akers_synthesis( mig, xs[0], xs[1] );
    if ( mig.size() > 6 )
    {
      mig.foreach_gate( [&]( auto n ) {
        std::vector<kitty::dynamic_truth_table> fanin{3, kitty::dynamic_truth_table( 5 )};
        mig.foreach_fanin( n, [&]( auto s, auto j ) {
          if ( mig.node_to_index( mig.get_node( s ) ) == 0 )
          {
            fanin[j] = ~xs[1];
          }
          else
          {
            fanin[j] = xs[mig.node_to_index( mig.get_node( s ) ) + 1];
          }
        } );
        xs.push_back( mig.compute( n, fanin.begin(), fanin.end() ) );
      } );

      mig.foreach_po( [&]( auto n ) {
        if ( mig.is_complemented( n ) )
          CHECK( ~xs[xs.size() - 1] == xs[0] );
        else
          CHECK( xs[xs.size() - 1] == xs[0] );
      } );
    }
  }
}

TEST_CASE( "Check Akers for random - 6 inputs", "[maj_random6_akers]" )
{
 for ( auto y = 0; y < 100; y++ )
  {
    std::cout << y << std::endl; 
    mig_network mig;

    std::vector<kitty::dynamic_truth_table> xs{8, kitty::dynamic_truth_table( 6 )};
    kitty::create_nth_var( xs[2], 0 );
    kitty::create_nth_var( xs[3], 1 );
    kitty::create_nth_var( xs[4], 2 );
    kitty::create_nth_var( xs[5], 3 );
    kitty::create_nth_var( xs[6], 4 );
    kitty::create_nth_var( xs[7], 5 );

    //create_from_binary_string(xs[0], "1011011101100001100111111110000111110111000010111010011010011110");
    create_random( xs[0] );

    for ( auto i = 0u; i < unsigned( xs[0].num_bits() ); i++ )
    {
      set_bit( xs[1], i );
    }

    mig = akers_synthesis( mig, xs[0], xs[1] );
    if ( mig.size() > 6 )
    {
      mig.foreach_gate( [&]( auto n ) {
        std::vector<kitty::dynamic_truth_table> fanin{3, kitty::dynamic_truth_table( 6 )};
        mig.foreach_fanin( n, [&]( auto s, auto j ) {
          if ( mig.node_to_index( mig.get_node( s ) ) == 0 )
          {
            fanin[j] = ~xs[1];
          }
          else
          {
            fanin[j] = xs[mig.node_to_index( mig.get_node( s ) ) + 1];
          }
        } );
        xs.push_back( mig.compute( n, fanin.begin(), fanin.end() ) );
      } );
     // CHECK( mig.size() == 35 );
      mig.foreach_po( [&]( auto n ) {
        if ( mig.is_complemented( n ) )
          CHECK( ~xs[xs.size() - 1] == xs[0] );
        else
          CHECK( xs[xs.size() - 1] == xs[0] );
      } );
    }
  }
}
