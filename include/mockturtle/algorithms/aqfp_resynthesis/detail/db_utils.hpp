#pragma once

#include <iostream>
#include <thread>

#include <kitty/kitty.hpp>

#include "./dag.hpp"
#include "./dag_cost.hpp"
#include "./dag_gen.hpp"

namespace mockturtle
{

void generate_aqfp_dags( const mockturtle::dag_generator_params& params, const std::string& file_prefix, uint32_t num_threads )
{
  auto t0 = std::chrono::high_resolution_clock::now();

  std::vector<std::ofstream> os;
  for ( auto i = 0u; i < num_threads; i++ )
  {
    auto file_path = fmt::format( "{}_{:02d}.txt", file_prefix, i );
    os.emplace_back( file_path );
    assert( os[i].is_open() );
  }

  auto gen = mockturtle::dag_generator<int>( params, num_threads );

  std::atomic<uint32_t> count = 0u;
  std::vector<std::atomic<uint64_t>> counts_inp( 6u );

  gen.for_each_dag( [&]( const auto& net, uint32_t thread_id ) {
    counts_inp[net.input_slots.size()]++;

    os[thread_id] << fmt::format( "{}\n", net.encode_as_string() );

    if ( (++count) % 100000 == 0u )
    {
      auto t1 = std::chrono::high_resolution_clock::now();
      auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>( t1 - t0 );

      std::cerr << fmt::format( "Number of DAGs generated {:10d}\nTime so far in seconds {:9.3f}\n", count, d1.count() / 1000.0 );
    }
  } );

  for ( auto& file : os )
  {
    file.close();
  }

  auto t2 = std::chrono::high_resolution_clock::now();
  auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t0 );
  std::cerr << fmt::format( "Number of DAGs generated {:10d}\nTime elapsed in seconds {:9.3f}\n", count, d2.count() / 1000.0 );

  std::cerr << fmt::format( "Number of DAGs of different input counts: [3 -> {},  4 -> {}, 5 -> {}]\n", counts_inp[3u], counts_inp[4u], counts_inp[5u] );
}

void compute_aqfp_dag_costs( const std::unordered_map<uint32_t, double>& gate_costs, const std::unordered_map<uint32_t, double>& splitters,
                             const std::string& dag_file_prefix, const std::string& cost_file_prefix, uint32_t num_threads )
{
  auto t0 = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;

  std::atomic<uint64_t> count = 0u;

  for ( auto i = 0u; i < num_threads; i++ )
  {
    threads.emplace_back(
        [&]( auto id ) {
          std::ifstream is( fmt::format( "{}_{:02d}.txt", dag_file_prefix, id ) );
          assert( is.is_open() );

          std::ofstream os( fmt::format( "{}_{:02d}.txt", cost_file_prefix, id ) );
          assert( os.is_open() );
          mockturtle::dag_aqfp_cost_all_configs<mockturtle::aqfp_dag<>> cc( gate_costs, splitters );

          std::string temp;
          while ( getline( is, temp ) )
          {
            if ( temp.length() > 0 )
            {
              mockturtle::aqfp_dag<> net( temp );
              auto costs = cc( net );

              os << costs.size() << std::endl;
              for ( auto it = costs.begin(); it != costs.end(); it++ )
              {
                os << fmt::format( "{:08x} {}\n", it->first, it->second );
              }

              if ( (++count) % 100000u == 0u )
              {
                auto t1 = std::chrono::high_resolution_clock::now();
                auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>( t1 - t0 );

                std::cerr << fmt::format( "Number of DAGs processed {:10d}\nTime so far in seconds {:9.3f}\n", count, d1.count() / 1000.0 );
              }
            }
          }

          is.close();
          os.close();
        },
        i );
  }

  for ( auto& t : threads )
  {
    t.join();
  }

  auto t2 = std::chrono::high_resolution_clock::now();
  auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t0 );
  std::cerr << fmt::format( "Number of DAGs processed {:10d}\nTime elapsed in seconds {:9.3f}\n", count, d2.count() / 1000.0 );
}

} // namespace mockturtle
