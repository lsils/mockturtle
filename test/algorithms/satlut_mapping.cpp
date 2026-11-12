#include <catch.hpp>

#include <limits>
#include <random>

#include <mockturtle/algorithms/collapse_mapped.hpp>
#include <mockturtle/algorithms/lut_mapping.hpp>
#include <mockturtle/algorithms/satlut_mapping.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/mapping_view.hpp>

#include <kitty/constructors.hpp>

using namespace mockturtle;

namespace
{
template<class Ntk>
void check_sat_equivalence( Ntk const& mapped )
{
  const auto collapsed = collapse_mapped_network<klut_network>( mapped );
  REQUIRE( collapsed );
  CHECK( collapsed->num_pis() == mapped.num_pis() );
  const default_simulator<kitty::dynamic_truth_table> sim( mapped.num_pis() );
  CHECK( simulate<kitty::dynamic_truth_table>( *collapsed, sim ) == simulate<kitty::dynamic_truth_table>( mapped, sim ) );
}

template<class Ntk>
void check_sat_mapping( Ntk const& mapped, uint32_t num_cells )
{
  CHECK( mapped.num_cells() == num_cells );
  check_sat_equivalence( mapped );
}
} // namespace

TEST_CASE( "SAT-LUT mapping of AIG", "[satlut_mapping]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( f1, a );
  const auto f3 = aig.create_nand( f1, b );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  mapping_view mapped_aig{ aig };

  CHECK( has_has_mapping_v<mapping_view<aig_network>> );
  CHECK( has_is_cell_root_v<mapping_view<aig_network>> );
  CHECK( has_clear_mapping_v<mapping_view<aig_network>> );
  CHECK( has_num_cells_v<mapping_view<aig_network>> );
  CHECK( has_add_to_mapping_v<mapping_view<aig_network>> );
  CHECK( has_remove_from_mapping_v<mapping_view<aig_network>> );
  CHECK( has_foreach_cell_fanin_v<mapping_view<aig_network>> );

  CHECK( !mapped_aig.has_mapping() );

  satlut_mapping( mapped_aig );
  check_sat_mapping( mapped_aig, 1u );
}

TEST_CASE( "SAT-LUT mapping can use one cell per gate", "[satlut_mapping]" )
{
  for ( auto gates : { 1u, 2u } )
  {
    CAPTURE( gates );
    aig_network aig;
    const auto a = aig.create_pi();
    const auto b = aig.create_pi();
    aig.create_po( aig.create_and( a, b ) );
    if ( gates == 2u )
      aig.create_po( aig.create_and( a, !b ) );

    for ( bool seeded : { false, true } )
    {
      CAPTURE( seeded );
      mapping_view mapped{ aig };
      if ( seeded )
      {
        lut_mapping( mapped );
        REQUIRE( mapped.num_cells() == gates );
      }

      satlut_mapping_params ps;
      ps.conflict_limit = 1;
      satlut_mapping( mapped, ps );
      check_sat_mapping( mapped, gates );
    }
  }
}

TEST_CASE( "SAT-LUT mapping handles networks with no gates", "[satlut_mapping]" )
{
  for ( bool outputs : { false, true } )
  {
    CAPTURE( outputs );
    aig_network aig;
    const auto a = aig.create_pi();
    if ( outputs )
    {
      aig.create_po( !a );
      aig.create_po( aig.get_constant( false ) );
      aig.create_po( aig.get_constant( true ) );
    }

    mapping_view mapped{ aig };
    satlut_mapping_stats st;
    satlut_mapping( mapped, {}, &st );
    CHECK( st.num_vars == 0u );
    check_sat_mapping( mapped, 0u );
  }
}

TEST_CASE( "SAT-LUT mapping requires only gate-driven outputs", "[satlut_mapping]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  aig.create_and( a, !b );
  const auto f = aig.create_and( a, b );
  aig.create_po( f );
  aig.create_po( f );
  aig.create_po( !f );
  aig.create_po( a );
  aig.create_po( aig.get_constant( false ) );
  aig.create_po( aig.get_constant( true ) );

  mapping_view mapped{ aig };
  satlut_mapping( mapped );
  check_sat_mapping( mapped, 1u );
}

TEST_CASE( "SAT-LUT mapping improves a feasible incumbent only", "[satlut_mapping]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();
  const auto f = aig.create_and( aig.create_and( aig.create_and( a, b ), c ), d );
  aig.create_po( f );
  aig.create_po( !f );

  mapping_view mapped{ aig };
  lut_mapping_params seed_ps;
  seed_ps.cut_enumeration_ps.cut_size = 2;
  lut_mapping( mapped, seed_ps );
  REQUIRE( mapped.num_cells() == 3u );

  satlut_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 2;
  ps.conflict_limit = 1;
  satlut_mapping( mapped, ps );
  check_sat_mapping( mapped, 3u );

  ps.cut_enumeration_ps.cut_size = 4;
  ps.conflict_limit = 0;
  satlut_mapping( mapped, ps );
  check_sat_mapping( mapped, 1u );
}

TEST_CASE( "SAT-LUT mapping returns an equivalent cover with a limited conflict budget", "[satlut_mapping]" )
{
  std::mt19937 rng( 0 );
  aig_network aig;
  std::vector<aig_network::signal> signals;
  for ( auto i = 0u; i < 7u; ++i )
    signals.push_back( aig.create_pi() );
  for ( auto i = 0u; i < 25u; ++i )
  {
    const auto a = signals.back() ^ ( rng() % 2u );
    const auto index = rng() % signals.size();
    const auto b = signals[index] ^ ( rng() % 2u );
    signals.push_back( aig.create_and( a, b ) );
  }
  for ( auto offset : { 0u, 5u, 10u } )
    aig.create_po( signals[signals.size() - 1 - offset] );

  mapping_view mapped{ aig };
  lut_mapping_params seed_ps;
  seed_ps.cut_enumeration_ps.cut_size = 2;
  lut_mapping( mapped, seed_ps );
  REQUIRE( mapped.has_mapping() );
  const auto incumbent_cells = mapped.num_cells();

  satlut_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 4;
  ps.conflict_limit = 1;
  satlut_mapping( mapped, ps );

  CHECK( mapped.num_cells() <= incumbent_cells );
  check_sat_equivalence( mapped );
}

TEST_CASE( "SAT-LUT mapping stores cuts with minimized support", "[satlut_mapping]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f = aig.create_or( aig.create_and( a, b ), aig.create_and( a, !b ) );
  const auto g = aig.create_and( f, !a );
  aig.create_po( f );
  aig.create_po( g );

  mapping_view<aig_network, true> mapped{ aig };
  satlut_mapping_params ps;
  ps.cut_enumeration_ps.cut_limit = 2;
  ps.cut_enumeration_ps.minimize_truth_table = true;
  satlut_mapping<decltype( mapped ), true>( mapped, ps );
  check_sat_mapping( mapped, 2u );
  CHECK( mapped.cell_function( aig.get_node( f ) ).num_vars() == 1u );
  CHECK( mapped.cell_function( aig.get_node( g ) ).num_vars() == 0u );
}

TEST_CASE( "SAT-LUT mapping optimizes within the window boundary", "[satlut_mapping]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();
  const auto d = aig.create_pi();
  const auto e = aig.create_pi();
  const auto f = aig.create_pi();
  const auto large = aig.create_and( aig.create_and( aig.create_and( a, b ), c ), d );
  const auto small = aig.create_and( e, f );
  const auto output = aig.create_and( small, large );
  aig.create_po( large );
  aig.create_po( output );

  mapping_view<aig_network, true> mapped{ aig };
  std::vector<aig_network::node> leaves{ aig.get_node( a ), aig.get_node( b ), aig.get_node( c ), aig.get_node( d ) };
  mapped.add_to_mapping( aig.get_node( large ), leaves.begin(), leaves.end() );
  kitty::dynamic_truth_table and4( 4 );
  kitty::create_from_hex_string( and4, "8000" );
  mapped.set_cell_function( aig.get_node( large ), and4 );
  leaves = { aig.get_node( e ), aig.get_node( f ) };
  mapped.add_to_mapping( aig.get_node( small ), leaves.begin(), leaves.end() );
  kitty::dynamic_truth_table and2( 2 );
  kitty::create_from_hex_string( and2, "8" );
  mapped.set_cell_function( aig.get_node( small ), and2 );
  leaves = { aig.get_node( small ), aig.get_node( large ) };
  mapped.add_to_mapping( aig.get_node( output ), leaves.begin(), leaves.end() );
  mapped.set_cell_function( aig.get_node( output ), and2 );

  SECTION( "Skip a pivot larger than the window" )
  {
    satlut_mapping<decltype( mapped ), true>( mapped, 2u );
  }
  SECTION( "Allow a window budget larger than the network" )
  {
    satlut_mapping<decltype( mapped ), true>( mapped, std::numeric_limits<uint32_t>::max() );
  }
  check_sat_mapping( mapped, 2u );
}
