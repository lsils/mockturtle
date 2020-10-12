#include <catch.hpp>

#include <mockturtle/networks/mig.hpp>
#include <mockturtle/utils/index_list.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <kitty/static_truth_table.hpp>

using namespace mockturtle;

TEST_CASE( "decode mig_index_list into mig_network", "[index_list]" )
{
  std::vector<uint64_t> const raw_list{4, 1, 2, 4, 6, 10, 4, 8, 12};
  mig_index_list mig_il{raw_list};

  mig_network mig;
  decode( mig, mig_il );

  CHECK( mig.num_gates() == 2u );
  CHECK( mig.num_pis() == 4u );
  CHECK( mig.num_pos() == 1u );

  const auto tt = simulate<kitty::static_truth_table<2u>>( mig )[0];
  CHECK( tt._bits == 0x8 );
}

TEST_CASE( "encode mig_network into mig_index_list", "[index_list]" )
{
  mig_network mig;
  auto const a = mig.create_pi();
  auto const b = mig.create_pi();
  auto const c = mig.create_pi();
  auto const d = mig.create_pi();
  auto const t0 = mig.create_maj( a, b, c );
  auto const t1 = mig.create_maj( t0, b, d );
  mig.create_po( t1 );

  mig_index_list mig_il;
  encode( mig_il, mig );

  CHECK( mig_il.num_pis() == 4u );
  CHECK( mig_il.num_pos() == 1u );
  CHECK( mig_il.num_entries() == 2u );
  CHECK( mig_il.size() == 9u );
  CHECK( mig_il.raw() == std::vector<uint64_t>{4, 1, 2, 4, 6, 4, 8, 10, 12} );
}
