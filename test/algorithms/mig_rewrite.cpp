#include <catch.hpp>

#include <algorithm>
#include <array>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/mig_rewrite.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/generators/majority_n.hpp>
#include <mockturtle/networks/mig.hpp>

using namespace mockturtle;

template<class Ntk, int Count>
inline std::pair<Ntk, std::array<typename Ntk::signal, Count>> init_network()
{
  Ntk ntk;
  std::array<typename Ntk::signal, Count> pis;
  std::generate( pis.begin(), pis.end(), [&]() { return ntk.create_pi(); } );
  return {ntk, pis};
}

template<class Ntk>
inline bool implements_majority( Ntk const& ntk )
{
  CHECK( ntk.num_pos() == 1u );

  default_simulator<kitty::dynamic_truth_table> sim( ntk.num_pis() );
  kitty::dynamic_truth_table maj( ntk.num_pis() );
  kitty::create_majority( maj );
  return maj == simulate<kitty::dynamic_truth_table>( ntk, sim )[0];
}

template<class Ntk1, class Ntk2>
inline void check_outputs( Ntk1 const& ntk1, Ntk2 const& ntk2 )
{
  CHECK( ntk1.num_pos() == ntk2.num_pos() );
  default_simulator<kitty::dynamic_truth_table> sim1( ntk1.num_pis() );
  default_simulator<kitty::dynamic_truth_table> sim2( ntk2.num_pis() );
  for ( auto i = 0u; i < ntk1.num_pos(); i++ )
  {
    CHECK( simulate<kitty::dynamic_truth_table>( ntk1, sim1 )[i] == simulate<kitty::dynamic_truth_table>( ntk2, sim2 )[i] );
  }
}

mig_network create_distributivity_fwd_test_network()
{
  // src = < 1 2 < 3 4 5 > >
  // tgt = < 3 < 1 2 4 > < 1 2 5 > > or < 4 < 1 2 3 > < 1 2 5 > > or < 5 < 1 2 3 > < 1 2 4 > >
  auto [mig, pis] = init_network<mig_network, 5>();
  mig.create_po( mig.create_maj( pis[0], pis[1], mig.create_maj( pis[2], pis[3], pis[4] ) ) );
  return mig;
}

TEST_CASE( "distributivity (forward direction)", "[mig_rewrite]" )
{
  auto mig = create_distributivity_fwd_test_network();
  std::vector<distributivity<mig_network>> dists = get_fwd_distributivities( mig, 7u );
  CHECK( dists.size() == 3u );
  for ( auto& dist : dists )
  {
    CHECK( dist.n == 7u );
    CHECK( dist.dir == distributivity<mig_network>::direction::FWD );
    auto temp = create_distributivity_fwd_test_network();
    temp.substitute_node( 7u, dist.apply_to( temp ) );
    temp = cleanup_dangling( temp );
    CHECK( temp.num_gates() == 3u );
    check_outputs( mig, temp );
  }
}

mig_network create_distributivity_bwd_test_network()
{
  // src = < < 1 2 3 > 4 < 1 2 5 > >
  // tgt = < 1 2 < 3 4 5 > >
  auto [mig, pis] = init_network<mig_network, 5>();
  mig.create_po( mig.create_maj(
      mig.create_maj( pis[0], pis[1], pis[2] ),
      pis[3],
      mig.create_maj( pis[0], pis[1], pis[4] ) ) );
  return mig;
}

TEST_CASE( "distributivity (backward direction)", "[mig_rewrite]" )
{
  auto mig = create_distributivity_bwd_test_network();
  std::vector<distributivity<mig_network>> dists = get_bwd_distributivities( mig, 8u );

  CHECK( dists.size() == 1u );
  for ( auto& dist : dists )
  {
    CHECK( dist.n == 8u );
    CHECK( dist.dir == distributivity<mig_network>::direction::BWD );
    auto temp = create_distributivity_bwd_test_network();
    temp.substitute_node( 8u, dist.apply_to( temp ) );
    temp = cleanup_dangling( temp );
    CHECK( temp.num_gates() == 2u );
    check_outputs( mig, temp );
  }
}

mig_network create_associativity_test_network()
{
  // src = [< 1 2 < 2 3 4 > >, < 1 2 3 >, < 1 2 4 >]
  // tgt = [< 2 3 < 1 2 4 > >, < 1 2 3 >, < 1 2 4 >] or [< 2 4 < 1 2 3 > >, < 1 2 3 >, < 1 2 4 >]
  auto [mig, pis] = init_network<mig_network, 4>();
  mig.create_po( mig.create_maj( pis[0], pis[1], mig.create_maj( pis[1], pis[2], pis[3] ) ) );
  mig.create_po( mig.create_maj( pis[0], pis[1], pis[2] ) );
  mig.create_po( mig.create_maj( pis[0], pis[1], pis[3] ) );
  return mig;
}

TEST_CASE( "associativity", "[mig_rewrite]" )
{
  auto mig = create_associativity_test_network();
  std::vector<associativity<mig_network>> assocs = get_associativities( mig, 6u );
  CHECK( assocs.size() == 2u );
  for ( auto& assoc : assocs )
  {
    CHECK( assoc.n == 6u );
    auto temp = create_associativity_test_network();
    temp.substitute_node( 6u, assoc.apply_to( temp ) );
    temp = cleanup_dangling( temp );
    CHECK( temp.num_gates() == 3u );
    check_outputs( mig, temp );
  }
}

mig_network create_relevance_test_network()
{
  // src = < 1 2 < 3 < 1 2 ~3 > < 1 ~2 3 > > >
  // tgt = same or < 1 >
  auto [mig, pis] = init_network<mig_network, 3>();
  mig.create_po( mig.create_maj( pis[0], pis[1], mig.create_maj( pis[2], mig.create_maj( pis[0], pis[1], !pis[2] ), mig.create_maj( pis[0], !pis[1], pis[2] ) ) ) );
  return mig;
}

TEST_CASE( "relevance", "[mig_rewrite]" )
{
  auto mig = create_relevance_test_network();
  std::vector<relevance<mig_network>> relevs = get_relevances( mig, 7u );
  CHECK( relevs.size() == 2u );
  for ( auto& relev : relevs )
  {
    auto temp = create_relevance_test_network();
    temp.substitute_node( 7u, relev.apply_to( temp ) );
    temp = cleanup_dangling( temp );
    CHECK( ( ( temp.num_gates() == 7u ) || ( temp.num_gates() == 0u ) ) );
    check_outputs( mig, temp );
  }
}

mig_network create_comp_assoc_test_network()
{
  // src = < 1 2 < 1 ~2 3 > >
  // tgt = same or < 1 >
  auto [mig, pis] = init_network<mig_network, 3>();
  mig.create_po( mig.create_maj( pis[0], pis[1], mig.create_maj( pis[0], !pis[1], pis[2] ) ) );
  return mig;
}

TEST_CASE( "complement associativity forward", "[mig_rewrite]" )
{
  auto mig = create_comp_assoc_test_network();
  std::vector<relevance<mig_network>> cassocs = get_fwd_comp_assocs( mig, 5u );
  CHECK( cassocs.size() == 1u );
  for ( auto& cassoc : cassocs )
  {
    auto temp = create_comp_assoc_test_network();
    temp.substitute_node( 5u, cassoc.apply_to( temp ) );
    temp = cleanup_dangling( temp );
    CHECK( temp.num_gates() == 0u );
    check_outputs( mig, temp );
  }
}

TEST_CASE( "complement associativity backward", "[mig_rewrite]" )
{
  auto mig = create_comp_assoc_test_network();
  std::vector<relevance<mig_network>> cassocs = get_bwd_comp_assocs( mig, 5u );
  CHECK( cassocs.size() == 1u );
  for ( auto& cassoc : cassocs )
  {
    auto temp = create_comp_assoc_test_network();
    temp.substitute_node( 5u, cassoc.apply_to( temp ) );
    temp = cleanup_dangling( temp );
    CHECK( temp.num_gates() == 0u );
    check_outputs( mig, temp );
  }
}

mig_network create_swapping_test_network()
{
  // src = [< 1 < 2 3 4 > < 2 (3&4) (4&5) > >, < 2 3 (4|5) >]
  // tgt = [< 1 < 2 3 (4&5) > < 2 (3&4) 4 > >, < 2 3 (4&5) >]
  auto [mig, pis] = init_network<mig_network, 5>();
  mig.create_po( mig.create_maj( pis[0], mig.create_maj( pis[1], pis[2], pis[3] ), mig.create_maj( pis[1], mig.create_and( pis[2], pis[3] ), mig.create_and( pis[3], pis[4] ) ) ) );
  mig.create_po( mig.create_maj( pis[1], pis[2], mig.create_and( pis[3], pis[4] ) ) );
  return mig;
}

TEST_CASE( "swapping", "[mig_rewrite]" )
{
  auto mig = create_swapping_test_network();
  std::vector<swapping<mig_network>> swappings = get_swappings( mig, 10u );
  CHECK( mig.num_gates() == 6u );
  CHECK( swappings.size() == 2u );
  for ( auto& swp : swappings )
  {
    auto temp = create_swapping_test_network();
    temp.substitute_node( 10u, swp.apply_to( temp ) );
    temp = cleanup_dangling( temp );
    CHECK( temp.num_gates() == 5u );
    check_outputs( mig, temp );
  }
}

TEST_CASE( "symmetry", "[mig_rewrite]" )
{
  auto [mig, pis] = init_network<mig_network, 7>();
  mig.create_po( majority_n_bdd( mig, pis ) );
  std::vector<symmetry<mig_network>> symmetries = get_symmetries( mig, 22u );
  CHECK( symmetries.size() == 3u );
  for ( auto& sym : symmetries )
  {
    auto [temp, tpis] = init_network<mig_network, 7>();
    temp.create_po( majority_n_bdd( temp, pis ) );
    temp.substitute_node( 22u, sym.apply_to( temp ) );
    temp = cleanup_dangling( temp );
    check_outputs( mig, temp );
  }
}

template<uint64_t N>
mig_network create_majority_substitution_test_network()
{
  auto [mig, pis] = init_network<mig_network, N>();
  mig.create_po( mig.create_maj( pis[0], pis[1], majority_n_bdd( mig, pis ) ) );
  return mig;
}

TEST_CASE( "majority-n substitutions", "[mig_rewrite]" )
{
  auto mig5 = create_majority_substitution_test_network<5>();
  auto temp5 = create_majority_substitution_test_network<5>();
  temp5.substitute_node( 14u, substitute_maj_n( temp5, 14u, 5 ) );
  temp5 = cleanup_dangling( temp5 );

  CHECK( ( mig5.num_gates() == 9u ) );
  CHECK( ( temp5.num_gates() == 5u ) );
  check_outputs( mig5, temp5 );

  auto mig7 = create_majority_substitution_test_network<7>();
  auto temp7 = create_majority_substitution_test_network<7>();
  temp7.substitute_node( 23u, substitute_maj_n( temp7, 23u, 7 ) );
  temp7 = cleanup_dangling( temp7 );

  CHECK( ( mig7.num_gates() == 16u ) );
  CHECK( ( temp7.num_gates() == 8u ) );
  check_outputs( mig7, temp7 );

  auto mig9 = create_majority_substitution_test_network<9>();
  auto temp9 = create_majority_substitution_test_network<9>();
  temp9.substitute_node( 34u, substitute_maj_n( temp9, 34u, 9 ) );
  temp9 = cleanup_dangling( temp9 );

  CHECK( ( mig9.num_gates() == 25u ) );
  CHECK( ( temp9.num_gates() == 13u ) );
  check_outputs( mig9, temp9 );
}

/**
 * Get a list of nodes in a given mig.
 */
template<typename Ntk>
std::vector<node<Ntk>> get_nodes( const Ntk& ntk )
{
  std::vector<node<Ntk>> nodes;
  ntk.foreach_node( [&nodes]( auto const& f ) {
    nodes.push_back( f );
  } );
  return nodes;
}

/**
 * Apply the first found for a given node @n of mig network @mig if it decreases gate count.
 */
template<typename Ntk, typename Fn>
bool apply_first( Ntk& ntk, node<Ntk> n, Fn&& fn )
{
  auto rules = fn( ntk, n );
  for ( auto& r : rules )
  {
    ntk.substitute_node( n, r.apply_to( ntk ) );
    ntk = cleanup_dangling( ntk );
    return true;
  }
  return false;
}

template<typename Ntk, typename FnRules>
bool apply_rule_backward( Ntk& ntk, FnRules&& get_rules )
{
  auto nodes = get_nodes( ntk );

  for ( auto it = nodes.rbegin(); it != nodes.rend(); it++ )
  {
    if ( apply_first( ntk, *it, get_rules ) )
      return true;
  }
  return false;
}

template<typename Ntk, typename FnRules>
bool apply_rule_forward( Ntk& ntk, FnRules&& get_rules )
{
  auto nodes = get_nodes( ntk );

  for ( auto it = nodes.begin(); it != nodes.end(); it++ )
  {
    if ( apply_first( ntk, *it, get_rules ) )
      return true;
  }
  return false;
}

TEST_CASE( "majority-5 optimization", "[mig_rewrite]" )
{

  auto [mig, pis] = init_network<mig_network, 5>();
  mig.create_po( majority_n_bdd( mig, pis ) );

  apply_rule_forward( mig, get_symmetries<mig_network> );
  apply_rule_forward( mig, get_symmetries<mig_network> );
  apply_rule_forward( mig, get_symmetries<mig_network> );

  CHECK( ( mig.num_gates() == 4u ) );
  CHECK( implements_majority( mig ) );
}

TEST_CASE( "majority-9 optimization", "[mig_rewrite]" )
{

  auto [mig, pis] = init_network<mig_network, 9>();
  for ( auto i = 0u; i < 9 / 2; i++ )
  {
    std::swap( pis[i], pis[8 - i] );
  }
  mig.create_po( majority_n_bdd( mig, pis ) );

  for ( auto i = 0; i < 9; i++ )
  {
    apply_rule_forward( mig, get_bwd_distributivities<mig_network> );
  }
  apply_rule_forward( mig, get_fwd_comp_assocs<mig_network> );
  apply_rule_forward( mig, get_fwd_comp_assocs<mig_network> );

  apply_rule_forward( mig, get_bwd_distributivities<mig_network> );
  apply_rule_forward( mig, get_bwd_distributivities<mig_network> );

  apply_rule_forward( mig, get_symmetries<mig_network> );
  apply_rule_forward( mig, get_symmetries<mig_network> );

  mig.substitute_node( mig.get_node( get_pos( mig )[0] ), substitute_maj_n( mig, mig.get_node( get_pos( mig )[0] ), 7 ) );
  mig = cleanup_dangling( mig );
  apply_rule_forward( mig, get_fwd_comp_assocs<mig_network> );
  CHECK( implements_majority( mig ) );
}
