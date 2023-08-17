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

/*
  \file aqfp_flow_aspdac.cpp
  \brief AQFP synthesis flow

  This file contains the code to reproduce the experiment (Table I)
  in the following paper:
  "Depth-optimal Buffer and Splitter Insertion and Optimization in AQFP Circuits",
  ASP-DAC 2023, by Alessandro Tempia Calvino and Giovanni De Micheli.

  This version runs on the ISCAS benchmarks. The benchmarks for Table 1 can be
  downloaded at https://github.com/lsils/SCE-benchmarks
 */

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <mockturtle/algorithms/aqfp/aqfp_mapping.hpp>
#include <mockturtle/algorithms/aqfp/buffer_verification.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aqfp.hpp>
#include <mockturtle/networks/buffered.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <experiments.hpp>

using namespace mockturtle;

int main()
{
  using namespace experiments;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, double, bool> exp(
      "aqfp_tcad", "Bench", "Size_init", "Depth_init", "B/S", "JJs", "Depth", "Time (s)", "cec" );

  for ( auto const& benchmark : iscas_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    mig_network mig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( mig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* MIG-based logic optimization can be added here */
    auto mig_opt = cleanup_dangling( mig );

    const uint32_t size_before = mig_opt.num_gates();
    const uint32_t depth_before = depth_view( mig_opt ).depth();

    aqfp_assumptions_legacy aqfp_ps;
    aqfp_ps.splitter_capacity = 4;
    aqfp_ps.branch_pis = true;
    aqfp_ps.balance_pis = true;
    aqfp_ps.balance_pos = true;

    aqfp_mapping_params ps;
    ps.aqfp_assumptions_ps = aqfp_ps;
    ps.mapping_mode = aqfp_mapping_params::portfolio;
    aqfp_mapping_stats st;

    buffered_aqfp_network res = aqfp_mapping( mig_opt, ps, &st );

    /* cec */
    auto cec = abc_cec( res, benchmark );
    std::vector<uint32_t> pi_levels;
    for ( auto i = 0u; i < res.num_pis(); ++i )
      pi_levels.emplace_back( 0 );
    cec &= verify_aqfp_buffer( res, aqfp_ps, pi_levels );

    exp( benchmark, size_before, depth_before, st.num_bufs, st.num_jjs, st.depth, to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}
