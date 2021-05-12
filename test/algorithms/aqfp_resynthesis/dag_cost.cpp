#include <catch.hpp>

#include <optional>
#include <set>

#include <fmt/format.h>

#include <mockturtle/algorithms/aqfp_resynthesis/detail/dag.hpp>
#include <mockturtle/algorithms/aqfp_resynthesis/detail/dag_cost.hpp>

using namespace mockturtle;

TEST_CASE( "Computing gate cost", "[aqfp_resyn]" )
{
  mockturtle::dag_gate_cost<mockturtle::aqfp_dag<>> gate_cc( { { 3u, 6.0 }, { 5u, 10.0 } } );

  mockturtle::aqfp_dag<> net1 = { { { 1, 4, 5 }, { 2, 4, 5 }, { 3, 5, 6 }, { 5, 7, 8 }, {}, {}, {}, {}, {} }, { 4, 5, 6, 7, 8 }, 5u };
  mockturtle::aqfp_dag<> net2 = { { { 1, 2, 6 }, { 4, 5, 6 }, { 3, 6, 7 }, { 6, 8, 9 }, { 6, 7, 10 }, { 6, 8, 9 }, {}, {}, {}, {}, {} }, { 6, 7, 8, 9, 10 }, 0u };

  CHECK( gate_cc( net1 ) == 24.0 );
  CHECK( gate_cc( net2 ) == 36.0 );
}

TEST_CASE( "Computing AQFP cost", "[aqfp_resyn]" )
{
  mockturtle::dag_aqfp_cost<mockturtle::aqfp_dag<>> aqfp_cc( { { 3u, 3.0 }, { 5u, 5.0 } }, { { 1u, 1.0 }, { 3u, 3.0 } } );

  mockturtle::aqfp_dag<> net1 = { { { 1, 4, 5 }, { 2, 4, 5 }, { 3, 5, 6 }, { 5, 7, 8 }, {}, {}, {}, {}, {} }, { 4, 5, 6, 7, 8 }, 5u };
  mockturtle::aqfp_dag<> net2 = { { { 1, 2, 6 }, { 4, 5, 6 }, { 3, 6, 7 }, { 6, 8, 9 }, { 6, 7, 10 }, { 6, 8, 9 }, {}, {}, {}, {}, {} }, { 6, 7, 8, 9, 10 }, 0u };

  CHECK( aqfp_cc( net1 ) == 18.0 );
  CHECK( aqfp_cc( net2 ) == 42.0 );
}