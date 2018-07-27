#ifndef _MSC_VER

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
    depth_view depth_ntk{ntk};
    return std::pair{ntk.num_gates(), depth_ntk.depth()};
  } );

  CHECK( v == std::vector<std::pair<uint32_t, uint32_t>>{{{6, 3},         // 17
                                                          {208, 26},      // 432
                                                          {398, 19},      // 499
                                                          {325, 25},      // 880
                                                          {502, 25},      // 1355
                                                          {341, 27},      // 1908
                                                          {716, 20},      // 2670
                                                          {1024, 41},     // 3540
                                                          {1776, 37},     // 5315
                                                          {2337, 120},    // 6288
                                                          {1469, 26}}} ); // 7552
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

TEST_CASE( "Test quality improvement of cut rewriting with NPN4 resynthesis", "[quality]" )
{
  // without zero gain
  const auto v = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    const auto before = ntk.num_gates();
    mig_npn_resynthesis resyn;
    cut_rewriting_params ps;
    ps.cut_enumeration_ps.cut_size = 4;
    cut_rewriting( ntk, resyn, ps );
    ntk = cleanup_dangling( ntk );
    return before - ntk.num_gates();
  } );

  CHECK( v == std::vector<uint32_t>{{0, 19, 80, 49, 102, 78, 201, 131, 510, 2, 258}} );

  // with zero gain
  const auto v2 = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    const auto before = ntk.num_gates();
    mig_npn_resynthesis resyn;
    cut_rewriting_params ps;
    ps.allow_zero_gain = true;
    ps.cut_enumeration_ps.cut_size = 4;
    cut_rewriting( ntk, resyn, ps );
    ntk = cleanup_dangling( ntk );
    return before - ntk.num_gates();
  } );

  CHECK( v2 == std::vector<uint32_t>{{0, 3, 36, 12, 72, 13, 84, 47, 102, 2, 258}} );
}

TEST_CASE( "Test quality improvement of MIG refactoring with Akers resynthesis", "[quality]" )
{
  // without zero gain
  const auto v = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    const auto before = ntk.num_gates();
    akers_resynthesis resyn;
    refactoring( ntk, resyn );
    ntk = cleanup_dangling( ntk );
    return before - ntk.num_gates();
  } );

  CHECK( v == std::vector<uint32_t>{{0, 18, 34, 22, 114, 55, 141, 115, 423, 449, 67}} );

  // with zero gain
  const auto v2 = foreach_benchmark<mig_network>( []( auto& ntk, auto ) {
    const auto before = ntk.num_gates();
    akers_resynthesis resyn;
    refactoring_params ps;
    ps.allow_zero_gain = true;
    refactoring( ntk, resyn, ps );
    ntk = cleanup_dangling( ntk );
    return before - ntk.num_gates();
  } );

  CHECK( v2 == std::vector<uint32_t>{{0, 18, 34, 21, 114, 54, 143, 122, 417, 449, 66}} );
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

#endif
