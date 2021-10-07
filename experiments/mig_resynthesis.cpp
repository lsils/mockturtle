/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
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

#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/algorithms/resyn_engines/mig_resyn.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <lorina/aiger.hpp>
#include <lorina/verilog.hpp>

#include <experiments.hpp>
#include <fmt/format.h>
#include <string>

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, uint32_t, float, bool>
    exp( "mig_resubstitution", "benchmark", "size_before", "size_after", "runtime", "equivalent" );

  /* load the npn database in the library (for AIG-MIG mapping) */
  mig_npn_resynthesis resyn{ true };
  exact_library<mig_network, mig_npn_resynthesis> exact_lib( resyn );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

#if 0
    /* read in as AIG */
    mig_network aig;
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }
    uint32_t const size_aig = aig.num_gates();

    /* graph mapping to convert from AIG to MIG */
    map_params map_ps;
    map_ps.skip_delay_round = true;
    map_ps.required_time = std::numeric_limits<double>::max();
    mig_network mig = map( aig, exact_lib, map_ps );
    uint32_t const size_mapped_mig = mig.num_gates();

    write_verilog( mig, "mapped_MIGs/" + benchmark + ".v" );
    std::cout << "  AIG size = " << size_aig << ", mapped MIG size = " << size_mapped_mig << "\n";
#else
    mig_network mig;
    if ( lorina::read_verilog( "mapped_MIGs/" + benchmark + ".v", verilog_reader( mig ) ) != lorina::return_code::success )
    {
      continue;
    }
    uint32_t const size_mapped_mig = mig.num_gates();
#endif

    /* resubstitution using heuristic MIG resynthesis */
    resubstitution_params ps;
    resubstitution_stats st;
    ps.max_pis = 8u;
    ps.max_inserts = 1u;
    ps.pattern_filename = "patterns/" + benchmark + ".pat";
    ps.save_patterns = "patterns/" + benchmark + ".pat";

    depth_view depth_mig{mig};
    fanout_view fanout_mig{depth_mig};

    using resub_view_t = fanout_view<depth_view<mig_network>>;
    using truthtable_t = kitty::partial_truth_table;
    using functor_t = mig_resyn_topdown<truthtable_t>;
    using validator_t = circuit_validator<resub_view_t, bill::solvers::bsat2, false, true, false>;
    using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t, validator_t, functor_t>>;
    typename resub_impl_t::engine_st_t engine_st;
    typename resub_impl_t::collector_st_t collector_st;

    resub_impl_t p( fanout_mig, ps, st, engine_st, collector_st );
    p.run();

    mig = cleanup_dangling( mig );

    bool const cec = benchmark == "hyp" ? true : abc_cec( fanout_mig, benchmark );
    exp( benchmark, size_mapped_mig, mig.num_gates(), to_seconds( st.time_total ), cec );
  }

  exp.save();
  exp.table();

  return 0;
}
