#include <catch.hpp>

#include <fstream>
#include <iostream>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <kitty/print.hpp>

#include <mockturtle/algorithms/akers_synthesis.hpp>
#include <mockturtle/networks/mig.hpp>

using namespace mockturtle;

TEST_CASE( "Check Akers for MAJ-3", "[akers_synthesis]" )
{
  std::vector<kitty::dynamic_truth_table> xs{5, kitty::dynamic_truth_table( 3 )};

  create_majority( xs[0] );
  for ( auto i = 0u; i < unsigned( xs[0].num_bits() ); i++ )
  {
    set_bit( xs[1], i );
  }

  auto mig = akers_synthesis<mig_network>( xs[0], xs[1] );

  kitty::create_nth_var( xs[2], 0 );
  kitty::create_nth_var( xs[3], 1 );
  kitty::create_nth_var( xs[4], 2 );

  CHECK( mig.compute( mig.index_to_node( mig.size() ), xs.begin() + 2, xs.end() ) == xs[0] );
  CHECK( mig.size() == 5 );
}

TEST_CASE( "Check Akers for MAJ-5", "[akers_synthesis]" )
{
  std::vector<kitty::dynamic_truth_table> xs{7, kitty::dynamic_truth_table( 5 )};

  create_majority( xs[0] );
  for ( auto i = 0u; i < unsigned( xs[0].num_bits() ); i++ )
  {
    set_bit( xs[1], i );
  }

  auto mig = akers_synthesis<mig_network>( xs[0], xs[1] );

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

TEST_CASE( "Check Akers for random - 4 inputs", "[akers_synthesis]" )
{
  for ( auto y = 0; y < 5; y++ )
  {
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

    auto mig = akers_synthesis<mig_network>( xs[0], xs[1] );
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

TEST_CASE( "Check Akers for random - 5 inputs", "[akers_synthesis]" )
{
  for ( auto y = 0; y < 5; y++ )
  {
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

    auto mig = akers_synthesis<mig_network>( xs[0], xs[1] );
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

TEST_CASE( "Check Akers for random - 6 inputs", "[akers_synthesis]" )
{
  for ( auto y = 0; y < 1; y++ )
  {
    std::vector<kitty::dynamic_truth_table> xs{8, kitty::dynamic_truth_table( 6 )};
    kitty::create_nth_var( xs[2], 0 );
    kitty::create_nth_var( xs[3], 1 );
    kitty::create_nth_var( xs[4], 2 );
    kitty::create_nth_var( xs[5], 3 );
    kitty::create_nth_var( xs[6], 4 );
    kitty::create_nth_var( xs[7], 5 );

    //create_from_binary_string(xs[0], "1100110010001100011010011010100001111010101010110010011010000010");
    create_random( xs[0] );
    //kitty::print_binary( xs[0], std::cout );
    //std::cout << std::endl;

    for ( auto i = 0u; i < unsigned( xs[0].num_bits() ); i++ )
    {
      set_bit( xs[1], i );
    }

    auto mig = akers_synthesis<mig_network>( xs[0], xs[1] );
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
      mig.foreach_po( [&]( auto n ) {
        if ( mig.is_complemented( n ) )
          CHECK( ~xs[xs.size() - 1] == xs[0] );
        else
          CHECK( xs[xs.size() - 1] == xs[0] );
      } );
    }
  }
}
/*
TEST_CASE( "Check SIZE and DEPTH for Akers for random - 6 inputs", "[maj_random6_sizedepthakers]" )
{

  std::ifstream infile( "mock_akers.txt" );
  int size;
  std::string tt;

  while ( infile >> tt >> size )
  {

    std::cout << tt << std::endl;
    std::cout << size << std::endl;
    mig_network mig;

    std::vector<kitty::dynamic_truth_table> xs{8, kitty::dynamic_truth_table( 6 )};
    kitty::create_nth_var( xs[2], 0 );
    kitty::create_nth_var( xs[3], 1 );
    kitty::create_nth_var( xs[4], 2 );
    kitty::create_nth_var( xs[5], 3 );
    kitty::create_nth_var( xs[6], 4 );
    kitty::create_nth_var( xs[7], 5 );

    create_from_binary_string( xs[0], tt );

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
      mig.foreach_po( [&]( auto n ) {
        if ( mig.is_complemented( n ) )
          CHECK( ~xs[xs.size() - 1] == xs[0] );
        else
          CHECK( xs[xs.size() - 1] == xs[0] );
      } );
    }

    CHECK( mig.num_gates() == size );
  }
}*/

TEST_CASE( "Check leaves iterator -- easy case ", "[akers_synthesis]" )
{
  mig_network mig;
  auto a = mig.create_pi();
  auto b = mig.create_pi();
  auto c = mig.create_pi();
  auto d = mig.create_pi();

  std::vector<mig_network::signal> operations;
  operations.push_back( mig.create_and( a, b ) );
  operations.push_back( mig.create_and( c, d ) );

  std::vector<kitty::dynamic_truth_table> xs_in{2, kitty::dynamic_truth_table( 2 )};
  std::vector<kitty::dynamic_truth_table> xs{5, kitty::dynamic_truth_table( 4 )};
  create_from_binary_string( xs_in[0], "0110" );
  for ( auto i = 0u; i < unsigned( xs_in[0].num_bits() ); i++ )
  {
    set_bit( xs_in[1], i );
  }
  auto t = akers_synthesis( mig, xs_in[0], xs_in[1], operations.begin(), operations.end() );
  mig.create_po(t); 

  kitty::create_nth_var( xs[1], 0 );
  kitty::create_nth_var( xs[2], 1 );
  kitty::create_nth_var( xs[3], 2 );
  kitty::create_nth_var( xs[4], 3 );

for ( auto i = 0u; i < unsigned( xs[1].num_bits() ); i++ )
  {
    set_bit( xs[0], i );
  }

  CHECK( mig.num_gates() == 5 );

  
  if ( mig.size() > 6 )
  {
    mig.foreach_gate( [&]( auto n ) {
      std::vector<kitty::dynamic_truth_table> fanin{3, kitty::dynamic_truth_table( 4 )};
      mig.foreach_fanin( n, [&]( auto s, auto j ) {
        if ( mig.node_to_index( mig.get_node( s ) ) == 0 )
        {
          fanin[j] = ~xs[0];
        }
        else
        {
          fanin[j] = xs[mig.get_node( s ) ];
        }
      } );
      xs.push_back( mig.compute( n, fanin.begin(), fanin.end() ) );
    } );
    mig.foreach_po( [&]( auto n ) {
      if ( mig.is_complemented( n ) )
        CHECK( ~xs[xs.size() - 1] == binary_xor(binary_and(xs[1], xs[2]), binary_and(xs[4],xs[3])));
        else
        CHECK( xs[xs.size() - 1] == binary_xor(binary_and(xs[1], xs[2]), binary_and(xs[4],xs[3])));
    } );
  }

  //CHECK( mig.num_gates() == 5 );
}