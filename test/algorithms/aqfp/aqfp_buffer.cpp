#include <catch.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/networks/mig.hpp>
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
  bufcnt.count_buffers();

  CHECK( bufcnt.level( mig.get_node( f1 ) ) == 2u );
  CHECK( bufcnt.level( mig.get_node( f2 ) ) == 2u );
  CHECK( bufcnt.level( mig.get_node( f3 ) ) == 3u );
  CHECK( bufcnt.level( mig.get_node( f4 ) ) == 3u );
  CHECK( bufcnt.depth() == 3u );

  CHECK( bufcnt.num_buffers( mig.get_node( b ) ) == 1u );
  CHECK( bufcnt.num_buffers( mig.get_node( c ) ) == 1u );
  CHECK( bufcnt.num_buffers( mig.get_node( e ) ) == 1u );
  CHECK( bufcnt.num_buffers( mig.get_node( f ) ) == 1u );
  CHECK( bufcnt.num_buffers() == 4u );
}

TEST_CASE( "buffer optimization with ALAP and chunked movement", "[aqfp_buffer]" )
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
  ps.branch_pis = true;
  ps.balance_pis = true;
  ps.balance_pos = true;
  ps.splitter_capacity = 2u;
  aqfp_buffer bufcnt( mig, ps );
  bufcnt.count_buffers();
  std::cout << "initial: " << bufcnt.num_buffers() << "\n";

  bufcnt.ALAP();
  bufcnt.count_buffers();
  std::cout << "ALAP: " << bufcnt.num_buffers() << "\n";

  bufcnt.optimize();
  bufcnt.count_buffers();
  std::cout << "optimized: " << bufcnt.num_buffers() << "\n";
}
