/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2023  EPFL
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

#include <string>
#include <vector>

#include <fmt/format.h>
#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/experimental/decompose_multioutput.hpp>
#include <mockturtle/algorithms/aig_balancing.hpp>
#include <mockturtle/algorithms/extract_adders.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/box_aig.hpp>
#include <mockturtle/networks/block.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>

#include <experiments.hpp>

using namespace experiments;
using namespace mockturtle;

void simplify_adders( box_aig_network& ntk )
{
  (void)ntk;
  //TODO
}

void exp_whitebox()
{
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool> exp(
      "white-box", "benchmark", "size", "HA", "FA", "|bntk1|", "|wb-aig|", "#dt", 
      "|wb-aig-opt|", "#dt-opt", "HA2", "FA2", "|bntk2|", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network aig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* Remove structural redundancies (increases the number of discoverable HAs/FAs) */
    aig_balancing_params bps;
    bps.minimize_levels = false;
    bps.fast_mode = false;
    aig_balance( aig, bps );

    const uint32_t size_before = aig.num_gates();

    /* Map HAs/FAs */
    extract_adders_params ps;
    extract_adders_stats st;
    block_network bntk1 = extract_adders( aig, ps, &st );

    box_aig_network wb_aig = extract_adders_whiteboxed( aig, ps );

    resubstitution_params rps;
    // rps.pattern_filename = "1024sa1/" + benchmark + ".pat";
    rps.max_inserts = 20;
    rps.max_pis = 8;
    rps.max_divisors = std::numeric_limits<uint32_t>::max();

    const uint32_t wb_aig_size_before = wb_aig.num_gates();
    const uint32_t dt_before = wb_aig.num_dont_touch_gates();
    sim_resubstitution( wb_aig, rps );
    wb_aig = cleanup_dangling_with_boxes( wb_aig );

    extract_adders_stats st2;
    block_network bntk2 = extract_adders( wb_aig, ps, &st2 );

    bool const cec = benchmark == "hyp" ? true : abc_cec( wb_aig, benchmark );

    exp( benchmark, size_before, st.mapped_ha, st.mapped_fa, bntk1.num_gates(),
          wb_aig_size_before, dt_before,
          wb_aig.num_gates(), wb_aig.num_dont_touch_gates(),
          st2.mapped_ha, st2.mapped_fa, bntk2.num_gates(),
          cec );
  }

  exp.save();
  exp.table();
}

void unbox_blackboxed_adders( box_aig_network& ntk )
{
  for ( auto b = 1u; b <= ntk.num_boxes(); ++b )
  {
    if ( ntk.get_box_tag( b ) == "ha" )
    {
      auto i0 = ntk.get_box_input( b, 0 );
      auto i1 = ntk.get_box_input( b, 1 );
      ntk.delete_blackbox( b, {ntk.create_and( i0, i1 ), ntk.create_xor( i0, i1 )} );
    }
    else if ( ntk.get_box_tag( b ) == "fa" )
    {
      auto i0 = ntk.get_box_input( b, 0 );
      auto i1 = ntk.get_box_input( b, 1 );
      auto i2 = ntk.get_box_input( b, 2 );
      ntk.delete_blackbox( b, {ntk.create_maj( i0, i1, i2 ), ntk.create_xor3( i0, i1, i2 )} );
    }
    else
    {
      std::cout << "cannot recognize box " << b << "\n";
    }
  }
}

void exp_blackbox()
{
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool> exp(
      "black-box", "benchmark", "|aig|", "HA", "FA", "|bntk|", "|bb-aig|", "|bb-aig-opt|", "|unboxed-aig|", "HA2", "FA2", "|bntk2|", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network aig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* Remove structural redundancies (increases the number of discoverable HAs/FAs) */
    aig_balancing_params bps;
    bps.minimize_levels = false;
    bps.fast_mode = false;
    aig_balance( aig, bps );

    const uint32_t size_before = aig.num_gates();

    /* Map HAs/FAs */
    extract_adders_params ps;
    extract_adders_stats st;
    block_network bntk1 = extract_adders( aig, ps, &st );
    box_aig_network bb_aig = extract_adders_blackboxed( aig, ps );

    resubstitution_params rps;
    // rps.pattern_filename = "1024sa1/" + benchmark + ".pat";
    rps.max_inserts = 20;
    rps.max_pis = 8;
    rps.max_divisors = std::numeric_limits<uint32_t>::max();

    const uint32_t bb_aig_size_before = bb_aig.num_hashed_gates();
    sim_resubstitution( bb_aig, rps );
    bb_aig = cleanup_dangling_with_boxes( bb_aig );
    const uint32_t bb_aig_size_after = bb_aig.num_hashed_gates();

    // substitute adder implementation back
    unbox_blackboxed_adders( bb_aig );
    bb_aig = cleanup_dangling( bb_aig );
    //std::cout << bb_aig.num_dont_touch_gates() << "\n"; // TODO: figure out why not always 0
    extract_adders_stats st2;
    block_network bntk2 = extract_adders( bb_aig, ps, &st2 );

    bool const cec = benchmark == "hyp" ? true : abc_cec( bb_aig, benchmark );

    exp( benchmark, size_before, st.mapped_ha, st.mapped_fa, bntk1.num_gates(),
          bb_aig_size_before, bb_aig_size_after, bb_aig.num_gates(),
          st2.mapped_ha, st2.mapped_fa, bntk2.num_gates(), cec );
  }

  exp.save();
  exp.table();
}

void exp_no_box()
{
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool> exp(
      "no-box", "benchmark", "|aig|", "|aig-opt|", "HA", "FA", "|bntk|", "cec" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    aig_network aig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* Remove structural redundancies (increases the number of discoverable HAs/FAs) */
    aig_balancing_params bps;
    bps.minimize_levels = false;
    bps.fast_mode = false;
    aig_balance( aig, bps );

    const uint32_t size_before = aig.num_gates();

    resubstitution_params rps;
    // rps.pattern_filename = "1024sa1/" + benchmark + ".pat";
    rps.max_inserts = 20;
    rps.max_pis = 8;
    rps.max_divisors = std::numeric_limits<uint32_t>::max();

    sim_resubstitution( aig, rps );
    aig = cleanup_dangling( aig );

    /* Map HAs/FAs */
    extract_adders_params ps;
    extract_adders_stats st;
    block_network bntk = extract_adders( aig, ps, &st );

    bool const cec = benchmark == "hyp" ? true : abc_cec( aig, benchmark );

    exp( benchmark, size_before, aig.num_gates(), st.mapped_ha, st.mapped_fa, bntk.num_gates(), cec );
  }

  exp.save();
  exp.table();
}

int main()
{
  exp_whitebox();
  exp_blackbox();
  exp_no_box();
  return 0;
}
