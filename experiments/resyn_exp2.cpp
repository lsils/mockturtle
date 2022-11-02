#include "experiments.hpp"

//#include <mockturtle/algorithms/resyn_engines/mig_resyn.hpp>
#include <mockturtle/algorithms/resyn_engines/mux_resyn.hpp>
//#include <mockturtle/algorithms/resyn_engines/mig_enumerative.hpp>
#include <mockturtle/algorithms/resyn_engines/dump_resyn.hpp>
//#include <mockturtle/algorithms/resubstitution.hpp>
//#include <mockturtle/algorithms/circuit_validator.hpp>
//#include <mockturtle/algorithms/sim_resub.hpp>
//#include <mockturtle/algorithms/mig_resub.hpp>

#include <mockturtle/algorithms/experimental/boolean_optimization.hpp>
#include <mockturtle/algorithms/experimental/window_resub.hpp>

#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>

//#include <mockturtle/algorithms/mapper.hpp>
//#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
//#include <mockturtle/algorithms/cleanup.hpp>
//#include <mockturtle/algorithms/cut_rewriting.hpp>
//#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>

#include <mockturtle/utils/stopwatch.hpp>

#include <lorina/aiger.hpp>
#include <lorina/verilog.hpp>
#include <kitty/kitty.hpp>

#include <fmt/format.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace mockturtle;
using namespace mockturtle::experimental;
using namespace mockturtle::experimental::detail;
using namespace experiments;

int main1( int argc, char* argv[] )
{
  if ( argc < 3 ) return -1;

  using namespace mockturtle;
  using truth_table_type = kitty::partial_truth_table;
  //using resyn_engine_t = mig_resyn_topdown<truth_table_type, mig_resyn_static_params>;
  //using resyn_engine_t = mig_resyn_akers<mig_resyn_static_params>;
  //using resyn_engine_t = mig_enumerative_resyn<truth_table_type>;
  //using resyn_engine_t = mig_resyn_topdown<kitty::partial_truth_table, mig_resyn_static_params>;
  //using resyn_engine_t = mig_resyn_akers<mig_resyn_static_params>;
  //using resyn_engine_t = xag_resyn_decompose<truth_table_type, aig_resyn_static_params_for_sim_resub<resub_view_t>>;
  //using resyn_engine_t = xag_resyn_decompose<truth_table_type>;
  //using resyn_engine_t = mux_resyn<truth_table_type>;
  using resyn_engine_t = mux_resyn<truth_table_type>;
  typename resyn_engine_t::stats st;

  uint32_t num_probs{0}, num_success{0}, total_size{0}, total_max_size{0};
  float sum_ratios{0.0};
  stopwatch<>::duration total_time{0};

  const std::filesystem::path problem_set{ argv[1] };
  for ( auto const& dir_entry : std::filesystem::directory_iterator{problem_set} )
  {
    std::string filename = dir_entry.path();
    if ( filename.size() < 6 || filename.substr( filename.size() - 6, 6 ) != ".resyn" )
      continue;

    if ( argc > 3 && filename != std::string( argv[3] ) )
      continue;

    /* data structures */
    std::vector<truth_table_type> divisor_functions;
    std::vector<uint32_t> divisors;
    truth_table_type onset, offset, care;
    uint32_t I{0}, N{0}, T{0}, L{0}, max_size{0};

    /* open file */
    std::ifstream in( filename, std::ifstream::in );
    if ( !in.is_open() )
    {
      std::cerr << "[e] Cannot open file " << filename << "\n";
      continue;
    }

    /* parse */
    std::string line, element;
    std::vector<std::string> strings;
    while ( std::getline( in, line ) )
    {
      std::istringstream iss( line );
      while ( std::getline( iss, element, ' ' ) )
        strings.emplace_back( element );
      if ( strings.size() != 5 )
        continue;
      if ( strings[0] == "resyn" )
      {
        I = std::stoi( strings[1] );
        N = std::stoi( strings[2] );
        T = std::stoi( strings[3] );
        L = std::stoi( strings[4] );
        break;
      }
    }
    for ( auto i = 0u; i < I + N; ++i )
    {
      std::getline( in, line );
      assert( line.size() == L );
      divisor_functions.emplace_back( L );
      kitty::create_from_binary_string( divisor_functions.back(), line );
      divisors.emplace_back( i );
    }
    assert( T == 1 ); (void)T;
    // offset
    std::getline( in, line );
    assert( line.size() == L );
    offset = truth_table_type( L );
    kitty::create_from_binary_string( offset, line );
    // onset
    std::getline( in, line );
    assert( line.size() == L );
    onset = truth_table_type( L );
    kitty::create_from_binary_string( onset, line );
    // care
    assert( kitty::is_const0( onset & offset ) );
    care = onset | offset;
    // comment
    std::getline( in, line );
    assert( line == "c" );
    std::getline( in, line );
    std::istringstream iss( line );
    while ( std::getline( iss, element, ' ' ) )
      strings.emplace_back( element );
    max_size = std::stoi( strings.back() );
    in.close();

    /* resynthesize */
    resyn_engine_t engine( st );
    auto res = call_with_stopwatch( total_time, [&]() {
      return engine( onset, care, divisors.begin(), divisors.end(), divisor_functions, max_size + std::stoi( argv[2] ) );
    });
    num_probs++;
    total_max_size += max_size;
    if ( res )
    {
      num_success++;
      total_size += res->num_gates();
      if ( max_size == 0 )
      {
        if ( res->num_gates() != 0 )
          fmt::print( "did not find size-0 solution for file {}\n", filename );
        else
          sum_ratios += 1;
      }
      else
      {
        sum_ratios += float( res->num_gates() ) / float( max_size );
      }
    }
  }

  fmt::print( "#success / #problems = {} / {} = {:.2f}%\n", num_success, num_probs, float( num_success ) / num_probs * 100.0 );
  fmt::print( "avg. size = {:.2f}, avg. max size = {:.2f}, avg. ratio = {:.2f}\n", float( total_size ) / num_success, float( total_max_size ) / num_probs, float( sum_ratios ) / num_success );
  fmt::print( "total runtime = {:.3f}, avg. runtime = {:.5f}\n", to_seconds( total_time ), to_seconds( total_time ) / num_probs );
  return 0;
}

int main_aig_win()
{
  using Ntk = aig_network;
  using ViewedNtk = depth_view<fanout_view<Ntk>>;
  using TT = typename kitty::dynamic_truth_table;
  using windowing_t = complete_tt_windowing<ViewedNtk, TT>;
  using resyn_t = complete_tt_resynthesis<ViewedNtk, TT, xag_resyn_decompose<TT, aig_resyn_static_params_default<TT>>>;
  //using resyn_t = complete_tt_resynthesis<ViewedNtk, TT, aig_enumerative_resyn<TT, true>>;
  //using resyn_t = exact_resynthesis<ViewedNtk, TT, xag_index_list<false>>;
  using stats_t = boolean_optimization_stats<complete_tt_windowing_stats, typename resyn_t::stats_t>;
  using opt_t = boolean_optimization_impl<ViewedNtk, windowing_t, resyn_t>;

  stats_t st;
  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if ( benchmark != "mem_ctrl" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network ntk;
    if ( lorina::read_aiger( fmt::format( "compress2rs/{}.aig", benchmark ), aiger_reader( ntk ) ) != lorina::return_code::success )
    {
      continue;
    }

    window_resub_params ps;
    ps.dry_run = true;
    ps.dry_run_verbose = false;
    ps.wps.max_pis = 6;
    //ps.wps.max_inserts = 3;
    ps.wps.max_inserts = std::numeric_limits<uint32_t>::max();
    //ps.wps.min_size = 4;
    ps.wps.normalize = true;
    ps.wps.use_dont_cares = true;

    fanout_view<Ntk> fntk( ntk );
    ViewedNtk viewed( fntk );

    opt_t p( viewed, ps, st );
    p.run();
    //st.report();
  }

  st.report();

  return 0;
}

int main()
{
  return main_aig_win();
}