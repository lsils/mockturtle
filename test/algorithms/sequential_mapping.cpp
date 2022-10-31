#include <catch.hpp>

#include <mockturtle/algorithms/experimental/sequential_mapping.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/sequential.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/mapping_view.hpp>
#include <mockturtle/algorithms/collapse_mapped_sequential.hpp>
#include <mockturtle/algorithms/retiming_network.hpp>
#include <mockturtle/io/write_blif.hpp>

using namespace mockturtle;
using namespace mockturtle::experimental;

TEST_CASE( "initialize a sequential k-LUT", "[sequential_mapping]" )
{
  sequential<klut_network> klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto d = klut.create_pi();
  const auto e = klut.create_pi();


  const auto f_ = klut.create_and( a, b );
  klut.create_po( f_ ); // place-holder, POs should be defined first

  const auto f1 = klut.create_or( a, b );
  const auto f2 = klut.create_or( f1, c );
  const auto f3 = klut.create_or( f2, d );
  const auto f4 = klut.create_or( f3, e ); // f4 = a+b+c+d+e

  klut.create_ri( f4 );
  const auto f5 = klut.create_ro(); // f5 <- f4
  const auto fo = klut.create_or( f5, d );
  klut.substitute_node( klut.get_node( f_ ), fo );

  /* check sequential network interfaces */
  using Ntk = decltype( klut );
  CHECK( has_foreach_po_v<Ntk> );
  CHECK( has_create_po_v<Ntk> );
  CHECK( has_create_pi_v<Ntk> );
  CHECK( has_create_ro_v<Ntk> );
  CHECK( has_create_ri_v<Ntk> );

  /* cleanup dangling */
  klut = cleanup_dangling( klut );
  CHECK( klut.num_gates() == 5 );
  CHECK( klut.num_registers() == 1 );

  /* sequential mapping */
  mapping_view<Ntk, true> viewed{ klut };
  sequential_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 3;
  sequential_mapping<decltype(viewed), true>( viewed, ps );
  klut = *collapse_mapped_sequential_network<Ntk>( viewed );

  CHECK( klut.num_gates() == 4 );
  CHECK( klut.num_registers() == 1 );

}

TEST_CASE( "retming a simple sequential k-LUT", "[retiming]" )
{
  sequential<klut_network> klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto d = klut.create_pi();
  const auto e = klut.create_pi();

  const auto f1 = klut.create_or( a, b );
  const auto f2 = klut.create_or( f1, c );
  const auto f3 = klut.create_or( f2, d );
  const auto f4 = klut.create_or( f3, e ); // f4 = a+b+c+d+e

  klut.create_po( f4 );

  /* check sequential network interfaces */
  using Ntk = decltype( klut );
  CHECK( has_foreach_po_v<Ntk> );
  CHECK( has_create_po_v<Ntk> );
  CHECK( has_create_pi_v<Ntk> );
  CHECK( has_create_ro_v<Ntk> );
  CHECK( has_create_ri_v<Ntk> );

  /* cleanup dangling */
  klut = cleanup_dangling( klut );
  CHECK( klut.num_gates() == 5 );
  CHECK( klut.num_registers() == 1 );

  /* sequential mapping */
  mapping_view<Ntk, true> viewed{ klut };
  sequential_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 3;
  sequential_mapping<decltype(viewed), true>( viewed, ps );
  klut = *collapse_mapped_sequential_network<Ntk>( viewed );

  retiming_network_params rps;
  rps.clock_period = 1;
  retiming_network( klut, rps );

  write_blif( klut, "output.blif" );

  CHECK( klut.num_gates() == 4 );
  CHECK( klut.num_registers() == 1 );

}


TEST_CASE( "initialize a simple sequential k-LUT with registers", "[sequential_mapping]" )
{
  sequential<klut_network> klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto d = klut.create_pi();
  const auto e = klut.create_pi();

  const auto f_ = klut.create_and( a, b );
  klut.create_po( f_ ); // place-holder, POs should be defined first

  const auto f1 = klut.create_or( a, b );
  const auto f2 = klut.create_or( f1, c );
  const auto f3 = klut.create_or( f2, d );
  const auto f4 = klut.create_or( f3, e ); // f4 = a+b+c+d+e

  klut.create_ri( f4 );
  const auto fo = klut.create_ro(); // fo <- f4
  klut.substitute_node( klut.get_node( f_ ), fo );

  /* check sequential network interfaces */
  using Ntk = decltype( klut );
  CHECK( has_foreach_po_v<Ntk> );
  CHECK( has_create_po_v<Ntk> );
  CHECK( has_create_pi_v<Ntk> );
  CHECK( has_create_ro_v<Ntk> );
  CHECK( has_create_ri_v<Ntk> );

  /* cleanup dangling */
  klut = cleanup_dangling( klut );
  CHECK( klut.num_gates() == 4 );
  CHECK( klut.num_registers() == 1 );

  /* sequential mapping */
  mapping_view<Ntk, true> viewed{ klut };
  sequential_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 3;
  sequential_mapping<decltype(viewed), true>( viewed, ps );

  CHECK( viewed.num_cells() == 3 );

  /* collapse to network */
  klut = *collapse_mapped_sequential_network<Ntk>( viewed );

  CHECK( klut.num_gates() == 3 );
  CHECK( klut.num_registers() == 1 );
}

TEST_CASE( "initialize a 2-stage sequential k-LUT with registers", "[sequential_mapping]" )
{
  sequential<klut_network> klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto d = klut.create_pi();
  const auto e = klut.create_pi();

  const auto f_ = klut.create_and( a, b );
  klut.create_po( f_ ); // place-holder, POs should be defined first

  const auto f1 = klut.create_or( a, b );
  const auto f2 = klut.create_or( f1, c );
  const auto f3 = klut.create_or( f2, d );
  const auto f4 = klut.create_or( f3, e ); // f4 = a+b+c+d+e

  klut.create_ri( f4 );
  const auto f5 = klut.create_ro(); // f5 <- f4

  const auto g1 = klut.create_or( f5, d );

  klut.substitute_node( klut.get_node( f_ ), g1 );

  /* check sequential network interfaces */
  using Ntk = decltype( klut );
  CHECK( has_foreach_po_v<Ntk> );
  CHECK( has_create_po_v<Ntk> );
  CHECK( has_create_pi_v<Ntk> );
  CHECK( has_create_ro_v<Ntk> );
  CHECK( has_create_ri_v<Ntk> );

  /* cleanup dangling */
  klut = cleanup_dangling( klut );
  CHECK( klut.num_gates() == 5 );
  CHECK( klut.num_registers() == 1 );

  /* sequential mapping */
  mapping_view<Ntk, true> viewed{ klut };
  sequential_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 3;
  sequential_mapping<decltype(viewed), true>( viewed, ps );

  CHECK( viewed.num_cells() == 4 );

  /* collapse to network */
  klut = *collapse_mapped_sequential_network<Ntk>( viewed );

  CHECK( klut.num_gates() == 4 );
  CHECK( klut.num_registers() == 1 );
}

TEST_CASE( "initialize a 2-stage sequential k-LUT with timing constraints", "[sequential_mapping]" )
{
  sequential<klut_network> klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();
  const auto d = klut.create_pi();
  const auto e = klut.create_pi();

  const auto f_ = klut.create_and( a, b );
  klut.create_po( f_ ); // place-holder, POs should be defined first

  const auto f1 = klut.create_or( a, b );
  const auto f2 = klut.create_or( f1, c );
  const auto f3 = klut.create_or( f2, d );
  const auto f4 = klut.create_or( f3, e ); // f4 = a+b+c+d+e

  klut.create_ri( f4 );
  const auto f5 = klut.create_ro(); // f5 <- f4

  const auto g1 = klut.create_or( f5, d );

  klut.substitute_node( klut.get_node( f_ ), g1 );

  /* check sequential network interfaces */
  using Ntk = decltype( klut );
  CHECK( has_foreach_po_v<Ntk> );
  CHECK( has_create_po_v<Ntk> );
  CHECK( has_create_pi_v<Ntk> );
  CHECK( has_create_ro_v<Ntk> );
  CHECK( has_create_ri_v<Ntk> );

  /* cleanup dangling */
  klut = cleanup_dangling( klut );
  CHECK( klut.num_gates() == 5 );
  CHECK( klut.num_registers() == 1 );

  /* sequential mapping */
  mapping_view<Ntk, true> viewed{ klut };
  sequential_mapping_params ps;
  ps.cut_enumeration_ps.cut_size = 3;
  sequential_mapping<decltype(viewed), true>( viewed, ps );

  CHECK( viewed.num_cells() == 4 );

  /* collapse to network */
  collapse_mapped_sequential_network_params mps;
  mps.clock_period = 1;
  klut = *collapse_mapped_sequential_network<Ntk>( viewed );

  retiming_network_params rps;
  rps.clock_period = 1;
  retiming_network( klut, rps );

  // write_blif( klut, "output.blif" );

  CHECK( klut.num_gates() == 4 );
  CHECK( klut.num_registers() == 1 );
}