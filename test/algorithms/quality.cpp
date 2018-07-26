#include <catch.hpp>

#include <vector>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/collapse_mapped.hpp>
#include <mockturtle/algorithms/cut_enumeration.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/lut_mapping.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/akers.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/write_bench.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/mapping_view.hpp>

#include <fmt/format.h>
#include <lorina/aiger.hpp>

using namespace mockturtle;

template<class Ntk, class Fn, class Ret = std::result_of_t<Fn( Ntk&, int )>>
std::vector<Ret> foreach_benchmark( Fn&& fn )
{
  std::vector<Ret> v;
  for ( auto const& id : {17, 432, 499, 880, 1355, 1908, 2670, 3540, 5315, 6288, 7552} )
  {
    Ntk ntk;
    lorina::read_aiger( fmt::format( "{}/c{}.aig", BENCHMARKS_PATH, id ), aiger_reader( ntk ) );
    v.emplace_back( fn( ntk, id ) );
  }
  return v;
}

TEST_CASE( "Test quality of cut_enumeration", "[quality]" )
{
  const auto v = foreach_benchmark<aig_network>( []( auto& ntk, auto ) {
    return cut_enumeration( ntk ).total_cuts();
  } );

  CHECK( v == std::vector<std::size_t>{{19, 1387, 3154, 1717, 5466, 2362, 4551, 6994, 11849, 34181, 12442}} );
}

TEST_CASE( "Test quality of lut_mapping", "[quality]" )
{
  const auto v = foreach_benchmark<aig_network>( []( auto& ntk, auto ) {
    mapping_view<aig_network, true> mapped{ntk};
    lut_mapping<mapping_view<aig_network, true>, true>( mapped );
    return mapped.num_cells();
  } );

  CHECK( v == std::vector<uint32_t>{{2, 50, 68, 77, 68, 71, 97, 231, 275, 453, 347}} );
}

TEST_CASE( "Test quality of MIG networks", "[quality]" )
{
  const auto v = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    return ntk.num_gates();
  } );

  CHECK( v == std::vector<uint32_t>{{6, 208, 398, 325, 502, 341, 716, 1024, 1776, 2337, 1469}} );
}

TEST_CASE( "Test quality of node resynthesis with NPN4 resynthesis", "[quality]" )
{
  const auto v = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    mapping_view<mig_network, true> mapped{ntk};
    lut_mapping_params ps;
    ps.cut_enumeration_ps.cut_size = 4;
    lut_mapping<mapping_view<mig_network, true>, true>( mapped, ps );
    auto lut = *collapse_mapped_network<klut_network>( mapped );
    mig_npn_resynthesis resyn;
    auto mig = node_resynthesis<mig_network>( lut, resyn );
    return mig.num_gates();
  } );

  CHECK( v == std::vector<uint32_t>{{7, 176, 316, 300, 316, 299, 502, 929, 1319, 1061, 1418}} );
}

TEST_CASE( "Test quality of cut rewriting with NPN4 resynthesis", "[quality]" )
{
  const auto v = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    mig_npn_resynthesis resyn;
    cut_rewriting_params ps;
    ps.cut_enumeration_ps.cut_size = 4;
    cut_rewriting( ntk, resyn, ps );
    ntk = cleanup_dangling( ntk );
    return ntk.num_gates();
  } );

  CHECK( v == std::vector<uint32_t>{{6, 189, 318, 276, 400, 263, 515, 893, 1266, 2335, 1211}} );
}

TEST_CASE( "Test quality of MIG refactoring with Akers resynthesis", "[quality]" )
{
  const auto v = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    akers_resynthesis resyn;
    refactoring( ntk, resyn );
    ntk = cleanup_dangling( ntk );
    return ntk.num_gates();
  } );

  CHECK( v == std::vector<uint32_t>{{6, 190, 364, 303, 388, 286, 575, 909, 1353, 1888, 1402}} );
}

TEST_CASE( "Test quality of MIG resubstitution", "[quality]" )
{
  const auto v = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    resubstitution( ntk );
    ntk = cleanup_dangling( ntk );
    return ntk.num_gates();
  } );

  CHECK( v == std::vector<uint32_t>{{6, 206, 398, 325, 502, 338, 703, 1015, 1738, 2335, 1467}} );
}
