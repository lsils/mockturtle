#include <catch.hpp>

#include <optional>
#include <set>

#include <fmt/format.h>

#include <mockturtle/algorithms/aqfp_resynthesis/detail/dag_util.hpp>
#include <mockturtle/algorithms/aqfp_resynthesis/detail/dag_gen.hpp>

using namespace mockturtle;

TEST_CASE( "join a threads", "[thread]" )
{
  std::thread thread( []( auto ) {}, 0 );

  if ( thread.joinable() )
  {
    std::cout << "thread is joinable" << std::endl;
  }

  std::cout << "try to join" << std::endl;  
  thread.join();
  std::cout << "finished joining" << std::endl;
}

TEST_CASE( "join on threads in vector", "[thread]" )
{
  uint32_t const num_threads{1};

  std::vector<std::thread> threads;
  for ( auto i = 0u; i < num_threads; i++ )
  {
    std::cout << "emplace thread " << i << std::endl;
    threads.emplace_back( []( auto id ) {}, i );
    std::cout << "finished emplacing thread " << i << std::endl;
  }

  for ( auto i = 0u; i < num_threads; i++ )
  {
    if ( threads[i].joinable() )
    {
      std::cout << "thread " << i << " is joinable" << std::endl;
    }

    
    std::cout << "join thread " << i << std::endl;
    try
    {
      threads[i].join();
    }
    catch ( const std::system_error& e )
    {
      std::cout << e.what() << '\n';
      std::cout << e.code() << '\n';
    }
    catch ( const std::exception& e )
    {
      std::cout << e.what() << '\n';
    }
    std::cout << "finished joining thread " << i << std::endl;
  }
}  

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
