#include <catch.hpp>
#include <iostream>
#include <mockturtle/algorithms/node_resynthesis/klut_to_graph.hpp>
using namespace mockturtle;

/* AIG fully dsd decomposable
*  dsd does all of the work needed
*/

TEST_CASE( "aig fully dsd decomposable", "[dsd_shannon_npn]" )
{
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "{(((ab)(cd))(ef))}" );

  klut_network kLUT_ntk;
  aig_network aig;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  aig = klut_to_graph_converter<aig_network>( kLUT_ntk );

  CHECK( aig.num_gates() == 5u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( aig, sim )[0] == table );
}

/* MIG fully dsd decomposable
 * dsd does all of the work needed
*/
TEST_CASE( "mig fully dsd decomposable", "[dsd_shannon_npn]" )
{
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "{<ab((cd)(ef))>}" );

  klut_network kLUT_ntk;
  mig_network mig;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  mig = klut_to_graph_converter<mig_network>( kLUT_ntk );

  CHECK( mig.num_gates() == 4u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( mig, sim )[0] == table );
}

/* XAG fully dsd decomposable
 * dsd does all of the work needed
*/

TEST_CASE( "xag fully dsd decomposable", "[dsd_shannon_npn]" )
{
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "{([ab][(cd)(ef)])}" );

  klut_network kLUT_ntk;
  xag_network xag;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  xag = klut_to_graph_converter<xag_network>( kLUT_ntk );

  CHECK( xag.num_gates() == 5u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( xag, sim )[0] == table );
}

/* AIG only partly dsd decomposable
 * a AND function separates the variables in two sets {ef} {abcd} => handled by DSD
 * then no more dsd is possible for {abcd} and the fallback is taken by NPN since 
 * the number of variables is <= 4
*/

TEST_CASE( "aig partly dsd decomposable", "[dsd_shannon_npn]" )
{
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "((!((ab)c)d)(ef))" );

  klut_network kLUT_ntk;
  aig_network aig;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  aig = klut_to_graph_converter<aig_network>( kLUT_ntk );

  CHECK( aig.num_gates() == 5u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( aig, sim )[0] == table );
}


/* XAG only partly dsd decomposable
 * a AND function separates the variables in two sets {ef} {abcd} => handled by DSD
 * then no more dsd is possible for {abcd} and the fallback is taken by NPN since 
 * the number of variables is <= 4
*/
TEST_CASE( "xag partly dsd decomposable", "[dsd_shannon_npn]" )
{
  
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "([([ac]b)d][ef])" );

  klut_network kLUT_ntk;
  xag_network xag;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  xag = klut_to_graph_converter<xag_network>( kLUT_ntk );

  CHECK( xag.num_gates() == 5u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( xag, sim )[0] == table );
}

/* MIG only partly dsd decomposable
 * a MAJ function separates the variables in three sets {abc} {de} {f} => handled by DSD
 * then d AND e is a simple gate <de1> 
 * no more dsd is possible for {abc} and the fallback is taken by NPN since 
 * the number of variables is <= 4
*/
TEST_CASE( "mig partly dsd decomposable", "[dsd_shannon_npn]" )
{
  
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "<((ab)c)(de)f>" );

  klut_network kLUT_ntk;
  mig_network mig;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  mig = klut_to_graph_converter<mig_network>( kLUT_ntk );

  CHECK( mig.num_gates() == 4u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( mig, sim )[0] == table );
}


/* AIG shannon + npn
*  a single step of shannon decomposition is needed on a not DS-decomposable function to create two subfunction with support of size 4.
*  after this step the NPN function is taken from the database
*/
TEST_CASE( "aig shannon + npn decomposable", "[dsd_shannon_npn]" )
{
  
  kitty::dynamic_truth_table table( 5u );
  kitty::create_from_expression( table, "{(a((bc)(de)))(!a((!b!c)(!d!e)))}" );

  klut_network kLUT_ntk;
  aig_network aig;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5}, fn ) );

  aig = klut_to_graph_converter<aig_network>( kLUT_ntk );

  CHECK( aig.num_gates() == 9u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( aig, sim )[0] == table );
}

/* XAG shannon + npn
*  a single step of shannon decomposition is needed on a not DS-decomposable function to create two subfunction with support of size 4.
*  after this step the NPN function is taken from the database
*/
TEST_CASE( "xag shannon + npn decomposable", "[dsd_shannon_npn]" )
{
  
  kitty::dynamic_truth_table table( 5u );
  kitty::create_from_expression( table, "{(a((bc)(de)))(!a((!b!c)(!d!e)))}" );

  klut_network kLUT_ntk;
  xag_network xag;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5}, fn ) );

  xag = klut_to_graph_converter<xag_network>( kLUT_ntk );

  CHECK( xag.num_gates() == 9u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( xag, sim )[0] == table );
}


/* MIG shannon + npn
*  a single step of shannon decomposition is needed on a not DS-decomposable function to create two subfunction with support of size 4.
*  after this step the NPN function is taken from the database
*/
TEST_CASE( "mig shannon + npn decomposable", "[dsd_shannon_npn]" )
{
  
  kitty::dynamic_truth_table table( 5u );
  kitty::create_from_expression( table, "{(a((bc)(de)))(!a((!b!c)(!d!e)))}" );

  klut_network kLUT_ntk;
  mig_network mig;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  
  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5}, fn ) );

  mig = klut_to_graph_converter<mig_network>( kLUT_ntk );

  CHECK( mig.num_gates() == 9u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( mig, sim )[0] == table );
}

/* AIG dsd + shannon + npn
*  first a DSD is applicable on the subsets {abcde} and {f}.
*  then a single step of shannon decomposition is needed on a not DS-decomposable function to create two subfunction with support of size 4.
*  after this step the NPN function is taken from the database
*/
TEST_CASE( "aig dsd + shannon + npn decomposable", "[dsd_shannon_npn]" )
{
  
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "({(a((bc)(de)))(!a((!b!c)(!d!e)))}f)" );

  klut_network kLUT_ntk;
  aig_network aig;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  aig = klut_to_graph_converter<aig_network>( kLUT_ntk );

  CHECK( aig.num_gates() == 10u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( aig, sim )[0] == table );
}


/* MIG dsd + shannon + npn
*  first a DSD is applicable on the subsets {abcde} and {f}.
*  then a single step of shannon decomposition is needed on a not DS-decomposable function to create two subfunction with support of size 4.
*  after this step the NPN function is taken from the database
*/
TEST_CASE( "mig dsd + shannon + npn decomposable", "[dsd_shannon_npn]" )
{
  
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "({(a((bc)(de)))(!a((!b!c)(!d!e)))}f)" );

  klut_network kLUT_ntk;
  mig_network mig;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  mig = klut_to_graph_converter<mig_network>( kLUT_ntk );

  CHECK( mig.num_gates() == 10u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( mig, sim )[0] == table );
}


/* XAG dsd + shannon + npn
*  first a DSD is applicable on the subsets {abcde} and {f}.
*  then a single step of shannon decomposition is needed on a not DS-decomposable function to create two subfunction with support of size 4.
*  after this step the NPN function is taken from the database
*/
TEST_CASE( "xag dsd + shannon + npn decomposable", "[dsd_shannon_npn]" )
{
  
  kitty::dynamic_truth_table table( 6u );
  kitty::create_from_expression( table, "({(a((bc)(de)))(!a((!b!c)(!d!e)))}f)" );

  klut_network kLUT_ntk;
  xag_network xag;

  const auto x1 = kLUT_ntk.create_pi();
  const auto x2 = kLUT_ntk.create_pi();
  const auto x3 = kLUT_ntk.create_pi();
  const auto x4 = kLUT_ntk.create_pi();
  const auto x5 = kLUT_ntk.create_pi();
  const auto x6 = kLUT_ntk.create_pi();

  auto fn = [&]( kitty::dynamic_truth_table const& remainder, std::vector<klut_network::signal> const& children ) {
    return kLUT_ntk.create_node( children, remainder );
  };

  kLUT_ntk.create_po( dsd_decomposition( kLUT_ntk, table, {x1, x2, x3, x4, x5, x6}, fn ) );

  xag = klut_to_graph_converter<xag_network>( kLUT_ntk );

  CHECK( xag.num_gates() == 10u );

  default_simulator<kitty::dynamic_truth_table> sim( table.num_vars() );
  CHECK( simulate<kitty::dynamic_truth_table>( xag, sim )[0] == table );
}