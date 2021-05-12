#include <catch.hpp>

#include <optional>
#include <set>

#include <fmt/format.h>

#include <mockturtle/algorithms/aqfp_resynthesis/detail/dag_util.hpp>
#include <mockturtle/algorithms/aqfp_resynthesis/detail/dag_gen.hpp>

using namespace mockturtle;

TEST_CASE( "DAG generation", "[aqfp_resyn]" )
{
  using Ntk = mockturtle::aqfp_dag<>;

  mockturtle::dag_generator_params params;

  params.max_gates = 3u;         // allow at most 3 gates in total
  params.max_num_fanout = 1000u; // limit the maximum fanout of a gate
  params.max_width = 1000u;      // maximum number of gates at any level
  params.max_num_in = 4u;        // maximum number of inputs slots (need extra one for the constant)
  params.max_levels = 3u;        // maximum number of gate levels in a DAG

  params.allowed_num_fanins = { 3u, 5u };
  params.max_gates_of_fanin = { { 3u, 3u }, { 5u, 1u } };

  params.verbose = true;

  std::vector<Ntk> generated_dags;

  mockturtle::dag_generator<> gen( params, 1u );
  gen.for_each_dag( [&]( const auto& dag, auto thread_id ) { (void)thread_id; generated_dags.push_back( dag ); } );

  CHECK( generated_dags.size() == 3018u );
}
