#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <mockturtle/algorithms/aqfp/aqfp_legalization.hpp>
#include <mockturtle/algorithms/aqfp/buffer_verification.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/aqfp.hpp>
#include <mockturtle/networks/buffered.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <experiments.hpp>

using namespace mockturtle;
using namespace experiments;

// relative path to repo cloned from https://github.com/lsils/SCE-benchmarks
static const std::string benchmark_repo_path = "../../SCE-benchmarks";

/* AQFP benchmarks */
std::vector<std::string> aqfp_benchmarks = {
    "adder1", "adder8", "mult8", "counter16", "counter32", "counter64", "counter128", "c17",
    "c432", "c499", "c880", "c1355", "c1908", "c2670", "c3540", "c5315", "c6288", "c7552",
    "sorter32", "sorter48", "alu32"};

std::string benchmark_aqfp_path( std::string const& benchmark_name )
{
  return fmt::format( "{}/ISCAS/strashed/{}.v", benchmark_repo_path, benchmark_name );
}

template<class Ntk>
inline bool abc_cec_aqfp( Ntk const& ntk, std::string const& benchmark )
{
  return abc_cec_impl( ntk, benchmark_aqfp_path( benchmark ) );
}

int main()
{
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, double, bool> exp(
      "aqfp_tcad", "Bench", "Size_init", "Depth_init", "B/S", "JJs", "Depth", "Time (s)", "cec" );

  for ( auto const& benchmark : aqfp_benchmarks )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    mig_network mig;
    if ( lorina::read_verilog( benchmark_aqfp_path( benchmark ), verilog_reader( mig ) ) != lorina::return_code::success )
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

    aqfp_legalization_params ps;
    ps.aqfp_assumptions_ps = aqfp_ps;
    ps.legalization_mode = aqfp_legalization_params::portfolio;
    ps.verbose = true;
    ps.max_chunk_size = UINT32_MAX;
    ps.retime_iterations = UINT32_MAX;
    ps.optimization_rounds = UINT32_MAX;
    aqfp_legalization_stats st;

    buffered_aqfp_network res = aqfp_legalization( mig_opt, ps, &st );

    /* cec */
    auto cec = abc_cec_aqfp( res, benchmark );
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
