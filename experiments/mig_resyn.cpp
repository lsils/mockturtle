/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "experiments.hpp"

#include <mockturtle/algorithms/resyn_engines/mig_resyn.hpp>
#include <mockturtle/algorithms/resyn_engines/mux_resyn.hpp>
#include <mockturtle/algorithms/resyn_engines/mig_enumerative.hpp>
#include <mockturtle/algorithms/resyn_engines/dump_resyn.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/algorithms/circuit_validator.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/algorithms/mig_resub.hpp>

#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>

#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>

#include <mockturtle/utils/stopwatch.hpp>

#include <lorina/aiger.hpp>
#include <kitty/kitty.hpp>

#include <fmt/format.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>

int main( int argc, char* argv[] )
{
  if ( argc < 3 ) return -1;

  using namespace mockturtle;
  using truth_table_type = kitty::partial_truth_table;
  //using resyn_engine_t = mig_resyn_topdown<truth_table_type, mig_resyn_static_params>;
  //using resyn_engine_t = mig_resyn_akers<mig_resyn_static_params>;
  //using resyn_engine_t = mig_enumerative_resyn<truth_table_type>;
  using resyn_engine_t = mux_resyn<truth_table_type>;
  typename resyn_engine_t::stats st;

  uint32_t num_probs{0}, num_success{0}, total_size{0};
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
  fmt::print( "avg. size = {:.2f}, avg. ratio = {:.2f}\n", float( total_size ) / num_success, float( sum_ratios ) / num_success );
  fmt::print( "total runtime = {:.3f}, avg. runtime = {:.5f}\n", to_seconds( total_time ), to_seconds( total_time ) / num_probs );
  return 0;
}

int main2()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float>
      exp( "mig_resyn", "benchmark", "size0", "size1", "size2", "#probs", "est. gain", "time" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if ( benchmark != "voter" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network aig;
    mig_network mig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* cut rewriting on AIG */
    {
      xag_npn_resynthesis<aig_network> resyn1;
      cut_rewriting_params ps;
      ps.cut_enumeration_ps.cut_size = 4;
      aig = cut_rewriting( aig, resyn1, ps );
    }

    /* map AIG to MIG */
    {
      mig_npn_resynthesis resyn2{ true };
      exact_library<mig_network, mig_npn_resynthesis> exact_lib( resyn2 );

      map_params ps;
      ps.skip_delay_round = true;
      ps.required_time = std::numeric_limits<double>::max();
      mig = map( aig, exact_lib, ps );

      /* remap */
      ps.area_flow_rounds = 2;
      mig = map( mig, exact_lib, ps );

      ps.area_flow_rounds = 1;
      ps.enable_logic_sharing = true; /* high-effort remap */
      mig = map( mig, exact_lib, ps );
    }

    uint32_t size_before, size_middle, tmp;
    /* simple MIG resub */
    {
      resubstitution_params ps;
      ps.max_pis = 8u;
      ps.max_inserts = 2u;

      size_before = mig.num_gates();
      do
      {
        tmp = mig.num_gates();
        depth_view depth_mig{ mig };
        fanout_view fanout_mig{ depth_mig };
        mig_resubstitution( fanout_mig, ps );
        mig = cleanup_dangling( mig );
      } while ( mig.num_gates() > tmp );
      size_middle = mig.num_gates();
    }

    {
      resubstitution_params ps;
      resubstitution_stats st;
      ps.max_pis = 8u;
      ps.max_inserts = std::numeric_limits<uint32_t>::max();
      std::string pat_filename = fmt::format( "pats/{}.pat", benchmark );
      if ( std::filesystem::exists( pat_filename ) )
        ps.pattern_filename = pat_filename;
      else
      {
        assert( false );
        ps.save_patterns = pat_filename;
      }

      depth_view depth_mig{ mig };
      fanout_view fanout_mig{ depth_mig };

      using resub_view_t = fanout_view<depth_view<mig_network>>;
      //using resyn_engine_t = mig_resyn_topdown<kitty::partial_truth_table, mig_resyn_static_params>;
      //using resyn_engine_t = mig_resyn_akers<mig_resyn_static_params>;
      //using resyn_engine_t = mig_enumerative_resyn<kitty::partial_truth_table>;
      using resyn_engine_t = resyn_dumper<kitty::partial_truth_table, mig_index_list>;

      using validator_t = circuit_validator<resub_view_t, bill::solvers::bsat2, false, true, false>;
      using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t, validator_t, resyn_engine_t>>;
      
      typename resub_impl_t::engine_st_t engine_st;
      typename resub_impl_t::collector_st_t collector_st;

      resub_impl_t p( fanout_mig, ps, st, engine_st, collector_st );
      p.run( "hardest_problems/" + benchmark );
      mig = cleanup_dangling( mig );
      //p.run( []( auto& ntk, auto const& n, auto const& g ){ (void)ntk; (void)n; (void)g; return false; } );

      exp( benchmark, size_before, size_middle, mig.num_gates(), engine_st.num_resyn, st.estimated_gain, to_seconds( engine_st.time_resyn ) );
    }
  }

  exp.save();
  exp.table();

  return 0;
}


int main3()
{
  using namespace mockturtle;

  using truth_table_type = kitty::dynamic_truth_table;
  
  std::vector<truth_table_type> divisor_functions;
  std::vector<uint32_t> divisors;
  truth_table_type x( 4 ), target( 4 ), care( 4 );
  care = ~care;
  for ( uint32_t i = 0; i < 4; ++i )
  {
    kitty::create_nth_var( x, i );
    divisor_functions.emplace_back( x );
    divisors.emplace_back( i );
  }

  using resyn_engine_t = resyn_dumper<truth_table_type, mig_index_list>;
  typename resyn_engine_t::stats st;

  const std::vector<uint16_t> classes{ { 0x1ee1, 0x1be4, 0x1bd8, 0x18e7, 0x17e8, 0x17ac, 0x1798, 0x1796, 0x178e, 0x177e, 0x16e9, 0x16bc, 0x169e, 0x003f, 0x0359, 0x0672, 0x07e9, 0x0693, 0x0358, 0x01bf, 0x6996, 0x0356, 0x01bd, 0x001f, 0x01ac, 0x001e, 0x0676, 0x01ab, 0x01aa, 0x001b, 0x07e1, 0x07e0, 0x0189, 0x03de, 0x035a, 0x1686, 0x0186, 0x03db, 0x0357, 0x01be, 0x1683, 0x0368, 0x0183, 0x03d8, 0x07e6, 0x0182, 0x03d7, 0x0181, 0x03d6, 0x167e, 0x016a, 0x007e, 0x0169, 0x006f, 0x0069, 0x0168, 0x0001, 0x019a, 0x036b, 0x1697, 0x0369, 0x0199, 0x0000, 0x169b, 0x003d, 0x036f, 0x0666, 0x019b, 0x0187, 0x03dc, 0x0667, 0x0003, 0x168e, 0x06b6, 0x01eb, 0x07e2, 0x017e, 0x07b6, 0x007f, 0x19e3, 0x06b7, 0x011a, 0x077e, 0x018b, 0x00ff, 0x0673, 0x01a8, 0x000f, 0x1696, 0x036a, 0x011b, 0x0018, 0x0117, 0x1698, 0x036c, 0x01af, 0x0016, 0x067a, 0x0118, 0x0017, 0x067b, 0x0119, 0x169a, 0x003c, 0x036e, 0x07e3, 0x017f, 0x03d4, 0x06f0, 0x011e, 0x037c, 0x012c, 0x19e6, 0x01ef, 0x16a9, 0x037d, 0x006b, 0x012d, 0x012f, 0x01fe, 0x0019, 0x03fc, 0x179a, 0x013c, 0x016b, 0x06f2, 0x03c0, 0x033c, 0x1668, 0x0669, 0x019e, 0x013d, 0x0006, 0x019f, 0x013e, 0x0776, 0x013f, 0x016e, 0x03c3, 0x3cc3, 0x033f, 0x166b, 0x016f, 0x011f, 0x035e, 0x0690, 0x0180, 0x03d5, 0x06f1, 0x06b0, 0x037e, 0x03c1, 0x03c5, 0x03c6, 0x01a9, 0x166e, 0x03cf, 0x03d9, 0x07bc, 0x01bc, 0x1681, 0x03dd, 0x03c7, 0x06f9, 0x0660, 0x0196, 0x0661, 0x0197, 0x0662, 0x07f0, 0x0198, 0x0663, 0x07f1, 0x0007, 0x066b, 0x033d, 0x1669, 0x066f, 0x01ad, 0x0678, 0x01ae, 0x0679, 0x067e, 0x168b, 0x035f, 0x0691, 0x0696, 0x0697, 0x06b1, 0x0778, 0x16ac, 0x06b2, 0x0779, 0x16ad, 0x01e8, 0x06b3, 0x0116, 0x077a, 0x01e9, 0x06b4, 0x19e1, 0x01ea, 0x06b5, 0x01ee, 0x06b9, 0x06bd, 0x06f6, 0x07b0, 0x07b1, 0x07b4, 0x07b5, 0x07f2, 0x07f8, 0x018f, 0x0ff0, 0x166a, 0x035b, 0x1687, 0x1689, 0x036d, 0x069f, 0x1699 } };
  mig_npn_resynthesis db_resyn;

  resyn_engine_t engine( st );
  engine.reset_filename( "npn4/" );
  for ( auto func : classes )
  {
    target._bits[0] = func;

    mig_network opt_mig;
    const auto a = opt_mig.create_pi();
    const auto b = opt_mig.create_pi();
    const auto c = opt_mig.create_pi();
    const auto d = opt_mig.create_pi();
    std::vector<mig_network::signal> pis = { a, b, c, d };
    db_resyn( opt_mig, target, pis.begin(), pis.end(), [&]( auto const& f ) {
      opt_mig.create_po( f );
      return false;
    } );
    uint32_t opt_size = opt_mig.num_gates();

    auto res = engine( target, ~target.construct(), divisors.begin(), divisors.end(), divisor_functions, opt_size );
  }

#if 0
  do
  {
    auto res = engine( target, ~target.construct(), divisors.begin(), divisors.end(), divisor_functions, std::numeric_limits<uint32_t>::max() );
    kitty::next_inplace( target );
  } while ( !kitty::is_const0( target ) );
#endif

  return 0;
}
