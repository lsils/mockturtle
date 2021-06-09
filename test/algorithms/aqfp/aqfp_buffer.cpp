#include <catch.hpp>

#include <lorina/lorina.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/buffered.hpp>
#include <mockturtle/algorithms/aqfp/aqfp_buffer.hpp>

using namespace mockturtle;

TEST_CASE( "aqfp_buffer simple test", "[aqfp_buffer]" )
{
  mig_network mig;
  auto const a = mig.create_pi();
  auto const b = mig.create_pi();
  auto const c = mig.create_pi();
  auto const d = mig.create_pi();
  auto const e = mig.create_pi();

  auto const f1 = mig.create_maj( a, b, c );
  auto const f2 = mig.create_maj( d, e, f1 );
  auto const f3 = mig.create_maj( a, d, f1 );
  auto const f4 = mig.create_maj( f1, f2, f3 );
  mig.create_po( f4 );

  aqfp_buffer_params ps;
  ps.branch_pis = false;
  ps.balance_pis = false;
  ps.balance_pos = true;
  ps.splitter_capacity = 4u;
  aqfp_buffer bufcnt( mig, ps );
  bufcnt.count_buffers();

  CHECK( bufcnt.level( mig.get_node( f1 ) ) == 1u );
  CHECK( bufcnt.level( mig.get_node( f2 ) ) == 3u );
  CHECK( bufcnt.level( mig.get_node( f3 ) ) == 3u );
  CHECK( bufcnt.level( mig.get_node( f4 ) ) == 4u );
  CHECK( bufcnt.depth() == 4u );

  CHECK( bufcnt.num_buffers( mig.get_node( f1 ) ) == 2u );
  CHECK( bufcnt.num_buffers( mig.get_node( f2 ) ) == 0u );
  CHECK( bufcnt.num_buffers( mig.get_node( f3 ) ) == 0u );
  CHECK( bufcnt.num_buffers( mig.get_node( f4 ) ) == 0u );
  CHECK( bufcnt.num_buffers() == 2u );

  auto const buffered = bufcnt.dump_buffered_network<buffered_mig_network>();
  CHECK( verify_aqfp_buffer( buffered, ps ) == true );
}

TEST_CASE( "two layers of splitters", "[aqfp_buffer]" )
{
  mig_network mig;
  auto const a = mig.create_pi();
  auto const b = mig.create_pi();
  auto const c = mig.create_pi();
  auto const d = mig.create_pi();
  auto const e = mig.create_pi();
  auto const f = mig.create_pi();
  auto const g = mig.create_pi();
  auto const h = mig.create_pi();
  auto const i = mig.create_pi();
  auto const j = mig.create_pi();

  auto const f1 = mig.create_maj( a, b, c );
  auto const f2 = mig.create_maj( b, c, d );
  auto const f3 = mig.create_maj( d, e, f );
  auto const f4 = mig.create_maj( g, h, i );
  auto const f5 = mig.create_maj( h, i, j );

  auto const f6 = mig.create_maj( f3, f4, f5 );
  auto const f7 = mig.create_maj( a, f1, f2 );
  auto const f8 = mig.create_maj( f2, f3, g );
  auto const f9 = mig.create_maj( f7, f2, f8 );
  auto const f10 = mig.create_maj( f8, f2, f5 );
  auto const f11 = mig.create_maj( f2, f8, f6 );
  auto const f12 = mig.create_maj( f9, f10, f11 );
  mig.create_po( f12 );

  aqfp_buffer_params ps;
  ps.branch_pis = false;
  ps.balance_pis = false;
  ps.balance_pos = true;
  ps.splitter_capacity = 4u;
  aqfp_buffer bufcnt( mig, ps );
  bufcnt.count_buffers();

  CHECK( bufcnt.num_buffers( mig.get_node( f2 ) ) == 4u );
  CHECK( bufcnt.num_buffers( mig.get_node( f6 ) ) == 2u );
  CHECK( bufcnt.depth() == 7u );
  CHECK( bufcnt.num_buffers() == 17u );

  auto const buffered = bufcnt.dump_buffered_network<buffered_mig_network>();
  CHECK( verify_aqfp_buffer( buffered, ps ) == true );
}

TEST_CASE( "PO splitters and buffers", "[aqfp_buffer]" )
{
  mig_network mig;
  auto const a = mig.create_pi();
  auto const b = mig.create_pi();
  auto const c = mig.create_pi();
  auto const d = mig.create_pi();

  auto const f1 = mig.create_maj( a, b, c );
  auto const f2 = mig.create_maj( f1, c, d );
  mig.create_po( f1 );
  mig.create_po( !f1 );
  mig.create_po( f2 );
  mig.create_po( f2 );
  mig.create_po( !f2 );

  aqfp_buffer_params ps;
  ps.branch_pis = false;
  ps.balance_pis = false;
  ps.balance_pos = true;
  ps.splitter_capacity = 4u;
  aqfp_buffer bufcnt( mig, ps );
  bufcnt.count_buffers();

  CHECK( bufcnt.depth() == 4u );

  CHECK( bufcnt.num_buffers( mig.get_node( f1 ) ) == 3u );
  CHECK( bufcnt.num_buffers( mig.get_node( f2 ) ) == 1u );

  CHECK( bufcnt.num_buffers() == 4u );

  auto const buffered = bufcnt.dump_buffered_network<buffered_mig_network>();
  CHECK( verify_aqfp_buffer( buffered, ps ) == true );
}

TEST_CASE( "chain of fanouts", "[aqfp_buffer]" )
{
  mig_network mig;
  auto const a = mig.create_pi();
  auto const b = mig.create_pi();
  auto const c = mig.create_pi();
  auto const d = mig.create_pi();
  auto const e = mig.create_pi();
  auto const f = mig.create_pi();
  auto const g = mig.create_pi();
  auto const h = mig.create_pi();
  auto const i = mig.create_pi();

  auto const f1 = mig.create_maj( a, b, c );
  auto const f2 = mig.create_maj( f1, c, d );
  auto const f3 = mig.create_maj( f1, f2, e );
  auto const f4 = mig.create_maj( f1, f2, f );
  auto const f5 = mig.create_maj( f1, f3, f4 );
  auto const f6 = mig.create_maj( f1, f5, f );
  auto const f7 = mig.create_maj( f1, f2, g );
  auto const f8 = mig.create_maj( f1, f7, h );
  auto const f9 = mig.create_maj( f1, f7, i );
  mig.create_po( f1 );
  mig.create_po( f1 );
  mig.create_po( f1 );
  mig.create_po( f1 );
  mig.create_po( f1 );
  mig.create_po( f6 );
  mig.create_po( f8 );
  mig.create_po( f9 );

  aqfp_buffer_params ps;
  ps.branch_pis = false;
  ps.balance_pis = false;
  ps.balance_pos = true;
  ps.splitter_capacity = 4u;
  aqfp_buffer bufcnt( mig, ps );
  bufcnt.count_buffers();

  CHECK( bufcnt.num_buffers( mig.get_node( f1 ) ) == 9u );
  CHECK( bufcnt.depth() == 8u );
  CHECK( bufcnt.num_buffers() == 11u );

  auto const buffered = bufcnt.dump_buffered_network<buffered_mig_network>();
  CHECK( verify_aqfp_buffer( buffered, ps ) == true );
}

TEST_CASE( "branch but not balance PIs", "[aqfp_buffer]" )
{
  mig_network mig;
  auto const a = mig.create_pi();
  auto const b = mig.create_pi(); // shared
  auto const c = mig.create_pi(); // shared
  auto const d = mig.create_pi();
  auto const e = mig.create_pi(); // shared at higher level
  auto const f = mig.create_pi(); // connects to two POs

  auto const f1 = mig.create_maj( a, b, c );
  auto const f2 = mig.create_maj( b, c, d );
  auto const f3 = mig.create_and( f1, e );
  auto const f4 = mig.create_and( f2, e );
  mig.create_po( f3 );
  mig.create_po( f4 );
  mig.create_po( f );
  mig.create_po( !f );

  aqfp_buffer_params ps;
  ps.branch_pis = true;
  ps.balance_pis = false;
  ps.balance_pos = true;
  ps.splitter_capacity = 4u;
  aqfp_buffer bufcnt( mig, ps );
  bufcnt.ALAP();
  bufcnt.count_buffers();

  CHECK( bufcnt.level( mig.get_node( f1 ) ) == 2u );
  CHECK( bufcnt.level( mig.get_node( f2 ) ) == 2u );
  CHECK( bufcnt.level( mig.get_node( f3 ) ) == 3u );
  CHECK( bufcnt.level( mig.get_node( f4 ) ) == 3u );

  CHECK( bufcnt.level( mig.get_node( a ) ) == 1u );
  CHECK( bufcnt.level( mig.get_node( b ) ) == 0u );
  CHECK( bufcnt.level( mig.get_node( c ) ) == 0u );
  CHECK( bufcnt.level( mig.get_node( d ) ) == 1u );
  CHECK( bufcnt.level( mig.get_node( e ) ) == 1u );
  CHECK( bufcnt.level( mig.get_node( f ) ) == 2u );

  CHECK( bufcnt.depth() == 3u );
  CHECK( bufcnt.num_buffers() == 4u );

  auto const buffered = bufcnt.dump_buffered_network<buffered_mig_network>();
  CHECK( verify_aqfp_buffer( buffered, ps ) == true );
}

TEST_CASE( "various assumptions", "[aqfp_buffer]" )
{
  aig_network aig;
  auto const a = aig.create_pi();
  auto const b = aig.create_pi();
  auto const c = aig.create_pi();
  auto const d = aig.create_pi();
  auto const e = aig.create_pi();

  auto const f1 = aig.create_and( c, d );
  auto const f2 = aig.create_or( c, d );
  auto const f3 = aig.create_and( d, e );
  auto const f4 = aig.create_and( f2, f3 );

  aig.create_po( aig.get_constant( false ) ); // const -- PO
  aig.create_po( a ); // PI -- PO
  aig.create_po( b ); aig.create_po( b ); aig.create_po( b ); // PI -- buffer tree -- PO
  aig.create_po( f1 );
  aig.create_po( f3 );
  aig.create_po( f4 );

  aqfp_buffer_params ps;
  ps.splitter_capacity = 2u;

  /* branch PI, balance PI and PO */
  ps.branch_pis = true;
  ps.balance_pis = true;
  ps.balance_pos = true;
  {
    aqfp_buffer bufcnt( aig, ps );
    bufcnt.count_buffers();
    CHECK( bufcnt.depth() == 5u );
    CHECK( bufcnt.num_buffers() == 23u );
    auto const buffered = bufcnt.dump_buffered_network<buffered_aig_network>();
    CHECK( verify_aqfp_buffer( buffered, ps ) == true );
  }

  /* branch PI, balance only PI */
  ps.branch_pis = true;
  ps.balance_pis = true;
  ps.balance_pos = false;
  {
    aqfp_buffer bufcnt( aig, ps );
    bufcnt.count_buffers();
    CHECK( bufcnt.depth() == 5u );
    CHECK( bufcnt.num_buffers() == 11u );
    auto const buffered = bufcnt.dump_buffered_network<buffered_aig_network>();
    CHECK( verify_aqfp_buffer( buffered, ps ) == true );
  }

  /* branch PI, balance only PO */
  ps.branch_pis = true;
  ps.balance_pis = false;
  ps.balance_pos = true;
  {
    aqfp_buffer bufcnt( aig, ps );
    bufcnt.count_buffers();
    CHECK( bufcnt.depth() == 5u );
    CHECK( bufcnt.num_buffers() == 23u );
    auto const buffered1 = bufcnt.dump_buffered_network<buffered_aig_network>();
    CHECK( verify_aqfp_buffer( buffered1, ps ) == true );

    bufcnt.ALAP();
    bufcnt.count_buffers();
    CHECK( bufcnt.depth() == 5u );
    CHECK( bufcnt.num_buffers() == 11u );
    auto const buffered2 = bufcnt.dump_buffered_network<buffered_aig_network>();
    CHECK( verify_aqfp_buffer( buffered2, ps ) == true );
  }

  /* branch PI, balance neither */
  ps.branch_pis = true;
  ps.balance_pis = false;
  ps.balance_pos = false;
  {
    aqfp_buffer bufcnt( aig, ps );
    bufcnt.count_buffers();
    CHECK( bufcnt.depth() == 5u );
    CHECK( bufcnt.num_buffers() == 11u );
    auto const buffered1 = bufcnt.dump_buffered_network<buffered_aig_network>();
    CHECK( verify_aqfp_buffer( buffered1, ps ) == true );

    bufcnt.ALAP();
    bufcnt.count_buffers();
    CHECK( bufcnt.depth() == 5u );
    CHECK( bufcnt.num_buffers() == 9u );
    auto const buffered2 = bufcnt.dump_buffered_network<buffered_aig_network>();
    CHECK( verify_aqfp_buffer( buffered2, ps ) == true );
  }

  /* don't branch PI, balance PO */
  ps.branch_pis = false;
  ps.balance_pis = false;
  ps.balance_pos = true;
  {
    aqfp_buffer bufcnt( aig, ps );
    bufcnt.count_buffers();
    CHECK( bufcnt.depth() == 3u );
    CHECK( bufcnt.num_buffers() == 5u );
    auto const buffered = bufcnt.dump_buffered_network<buffered_aig_network>();
    CHECK( verify_aqfp_buffer( buffered, ps ) == true );
  }

  /* don't branch PI, balance neither */
  ps.branch_pis = false;
  ps.balance_pis = false;
  ps.balance_pos = false;
  {
    aqfp_buffer bufcnt( aig, ps );
    bufcnt.count_buffers();
    CHECK( bufcnt.depth() == 3u );
    CHECK( bufcnt.num_buffers() == 2u );
    auto const buffered = bufcnt.dump_buffered_network<buffered_aig_network>();
    CHECK( verify_aqfp_buffer( buffered, ps ) == true );
  }
}

TEST_CASE( "buffer optimization (quality test)", "[aqfp_buffer]" )
{
  aig_network aig;
  auto const result = lorina::read_aiger( std::string( BENCHMARKS_PATH ) + "/c880.aig", aiger_reader( aig ) );
  CHECK( result == lorina::return_code::success );

  aqfp_buffer_params ps;
  ps.branch_pis = true;
  ps.balance_pis = true;
  ps.balance_pos = true;
  ps.splitter_capacity = 2u;
  aqfp_buffer bufcnt( aig, ps );

  bufcnt.ALAP();
  bufcnt.count_buffers();
  CHECK( bufcnt.num_buffers() == 3074 );

  bufcnt.ASAP();
  bufcnt.count_buffers();
  CHECK( bufcnt.num_buffers() == 2401 );

  while ( bufcnt.optimize() ) {}
  bufcnt.count_buffers();
  CHECK( bufcnt.num_buffers() == 2370 );

  auto const buffered = bufcnt.dump_buffered_network<buffered_aig_network>();
  CHECK( verify_aqfp_buffer( buffered, ps ) == true );
}
