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
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/muxig.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>

#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/cleanup.hpp>

#include <mockturtle/utils/stopwatch.hpp>

#include <lorina/aiger.hpp>
#include <lorina/verilog.hpp>
#include <kitty/kitty.hpp>

#include <fmt/format.h>
#include <string>
#include <filesystem>
#include <iostream>

using namespace experiments;
using namespace mockturtle;

int main0()
{
  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if ( benchmark != "voter" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );
    //std::string command = fmt::format( "abc -c \"read {}; time -c; compress2rs; if -K 2; time; ps; write_verilog xag/{}.v\"", benchmark_path( benchmark ), benchmark );
    std::string command = fmt::format( "abc -c \"read xag/{}.v; time -c; mfs; time; ps;\"", benchmark, benchmark );

    //stopwatch<>::duration time{0};
    {
      //stopwatch timer(time);
      std::system( command.c_str() );
    }
  }
  return 0;
}

int main_mig()
{
  experiment<std::string, uint32_t, float, uint32_t, uint32_t, float, float>
      exp( "mig_resyn", "benchmark", "size_before", "time map", "size_after", "gain", "time", "time_resyn" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if ( benchmark != "voter" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    stopwatch<>::duration time_preopt{0};

    aig_network aig;
    mig_network mig;
    if ( lorina::read_aiger( fmt::format( "compress2rs/{}.aig", benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* map AIG to MIG */
    {
      stopwatch t( time_preopt );
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

    /* simple MIG resub */
    {
      stopwatch t( time_preopt );
      resubstitution_params ps;
      ps.max_pis = 8u;
      ps.max_inserts = 2u;
      uint32_t tmp;

      do
      {
        tmp = mig.num_gates();
        depth_view depth_mig{ mig };
        fanout_view fanout_mig{ depth_mig };
        mig_resubstitution( fanout_mig, ps );
        mig = cleanup_dangling( mig );
      } while ( mig.num_gates() > tmp );
    }

    uint32_t size_before = mig.num_gates();
    {
      resubstitution_params ps;
      resubstitution_stats st;
      ps.max_pis = 8u;
      ps.max_inserts = std::numeric_limits<uint32_t>::max();
      std::string pat_filename = fmt::format( "pats_mig/{}.pat", benchmark );
      //ps.save_patterns = pat_filename;
      assert( std::filesystem::exists( pat_filename ) );
      ps.pattern_filename = pat_filename;

      depth_view depth_mig{ mig };
      fanout_view fanout_mig{ depth_mig };

      using resub_view_t = fanout_view<depth_view<mig_network>>;
      using resyn_engine_t = mig_resyn_topdown<kitty::partial_truth_table, mig_resyn_static_params>;

      using validator_t = circuit_validator<resub_view_t, bill::solvers::bsat2, false, true, false>;
      using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t, validator_t, resyn_engine_t>>;
      
      typename resub_impl_t::engine_st_t engine_st;
      typename resub_impl_t::collector_st_t collector_st;

      resub_impl_t p( fanout_mig, ps, st, engine_st, collector_st );
      p.run();
      st.time_resub -= engine_st.time_patgen;
      st.time_total -= engine_st.time_patgen + engine_st.time_patsave;
      //st.report(); engine_st.report();
      mig = cleanup_dangling( mig );

      exp( benchmark, size_before, to_seconds(time_preopt), mig.num_gates(), size_before - mig.num_gates(), to_seconds( st.time_total ), to_seconds( engine_st.time_resyn ) );
    }
  }

  exp.save();
  exp.table();

/*
|  benchmark | size_before | time map | size_after | gain | time | time_resyn |
|      adder |         384 |     0.11 |        384 |    0 | 0.00 |       0.00 |
|        bar |        2594 |     0.29 |       2588 |    6 | 0.03 |       0.03 |
|        div |       12565 |     0.93 |      12532 |   33 | 0.32 |       0.11 |
|        hyp |      127877 |    13.01 |     124177 | 3700 | 9.10 |       0.86 |
|       log2 |       23643 |     3.00 |      23109 |  534 | 6.41 |       0.34 |
|        max |        2210 |     0.32 |       2210 |    0 | 0.03 |       0.03 |
| multiplier |       18700 |     1.76 |      18440 |  260 | 0.34 |       0.20 |
|        sin |        4018 |     0.81 |       3967 |   51 | 0.19 |       0.07 |
|       sqrt |       12513 |     1.09 |      12423 |   90 | 3.25 |       0.16 |
|     square |        9573 |     1.03 |       9498 |   75 | 0.08 |       0.05 |
|    arbiter |        6866 |     1.38 |       6719 |  147 | 0.17 |       0.14 |
|      cavlc |         541 |     0.83 |        533 |    8 | 0.02 |       0.02 |
|       ctrl |          80 |     0.21 |         79 |    1 | 0.01 |       0.01 |
|        dec |         304 |     0.09 |        304 |    0 | 0.01 |       0.01 |
|        i2c |         951 |     0.12 |        932 |   19 | 0.02 |       0.02 |
|  int2float |         190 |     0.09 |        181 |    9 | 0.01 |       0.01 |
|   mem_ctrl |       38179 |     3.86 |      34777 | 3402 | 2.24 |       1.24 |
|   priority |         449 |     0.10 |        431 |   18 | 0.01 |       0.01 |
|     router |         170 |     0.07 |        151 |   19 | 0.00 |       0.00 |
|      voter |        4729 |     0.53 |       4561 |  168 | 0.05 |       0.03 |
*/

  return 0;
}

int main_aig()
{
  experiment<std::string, uint32_t, uint32_t, uint32_t, float, float>
      exp( "aig_resyn", "benchmark", "size_before", "size_after", "gain", "time", "time_resyn" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if ( benchmark != "voter" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    uint32_t size_before;

    aig_network aig;
    if ( lorina::read_aiger( fmt::format( "highly_optimized/{}.aig", benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }
    size_before = aig.num_gates();

    {
      resubstitution_params ps;
      resubstitution_stats st;
      ps.max_pis = 8u;
      ps.max_inserts = std::numeric_limits<uint32_t>::max();
      //ps.odc_levels = 3;
      std::string pat_filename = fmt::format( "pats2/{}.pat", benchmark );
      //ps.save_patterns = pat_filename;
      assert( std::filesystem::exists( pat_filename ) );
      ps.pattern_filename = pat_filename;

      depth_view depthv{ aig };
      fanout_view fanoutv{ depthv };

      using resub_view_t = fanout_view<depth_view<aig_network>>;
      using truth_table_type = kitty::partial_truth_table;

      //using resyn_engine_t = mig_resyn_topdown<kitty::partial_truth_table, mig_resyn_static_params>;
      //using resyn_engine_t = mig_resyn_akers<mig_resyn_static_params>;
      using resyn_engine_t = xag_resyn_decompose<truth_table_type, aig_resyn_static_params_for_sim_resub<resub_view_t>>;
      //using resyn_engine_t = xag_resyn_decompose<truth_table_type>;

      using validator_t = circuit_validator<resub_view_t, bill::solvers::bsat2, false, true, false>;
      using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t, validator_t, resyn_engine_t>>;
      
      typename resub_impl_t::engine_st_t engine_st;
      typename resub_impl_t::collector_st_t collector_st;

      resub_impl_t p( fanoutv, ps, st, engine_st, collector_st );
      p.run();
      st.time_resub -= engine_st.time_patgen;
      st.time_total -= engine_st.time_patgen + engine_st.time_patsave;
      aig = cleanup_dangling( aig );

      exp( benchmark, size_before, aig.num_gates(), size_before - aig.num_gates(), to_seconds( st.time_total ), to_seconds( engine_st.time_resyn ) );
    }
  }

  exp.save();
  exp.table();

/*
compress2rs once
|  benchmark | size_before | size_after | gain |  time | time_resyn |
|      adder |         892 |        892 |    0 |  0.00 |       0.00 |
|        bar |        3141 |       3060 |   81 |  0.04 |       0.03 |
|        div |       20725 |      20541 |  184 |  0.23 |       0.05 |
|        hyp |      204533 |     204219 |  314 | 14.93 |       0.15 |
|       log2 |       29192 |      28686 |  506 |  5.41 |       0.22 |
|        max |        2832 |       2832 |    0 |  0.01 |       0.01 |
| multiplier |       24337 |      24313 |   24 |  0.28 |       0.12 |
|        sin |        5019 |       4951 |   68 |  0.44 |       0.04 |
|       sqrt |       18255 |      18200 |   55 |  4.39 |       0.03 |
|     square |       15891 |      15786 |  105 |  0.13 |       0.01 |
|    arbiter |       11839 |      11839 |    0 |  0.15 |       0.12 |
|      cavlc |         635 |        608 |   27 |  0.09 |       0.08 |
|       ctrl |          90 |         90 |    0 |  0.00 |       0.00 |
|        dec |         304 |        304 |    0 |  0.00 |       0.00 |
|        i2c |        1069 |       1038 |   31 |  0.02 |       0.01 |
|  int2float |         209 |        207 |    2 |  0.03 |       0.03 |
|   mem_ctrl |       43924 |      36916 | 7008 |  1.76 |       0.98 |
|   priority |         466 |        463 |    3 |  0.00 |       0.00 |
|     router |         183 |        145 |   38 |  0.00 |       0.00 |
|      voter |        7946 |       7932 |   14 |  0.02 |       0.01 |
until sat.
|  benchmark | size_before | size_after | gain |  time | time_resyn |
|      adder |         892 |        892 |    0 |  0.00 |       0.00 |
|        bar |        3141 |       3079 |   62 |  0.04 |       0.03 |
|        div |       20508 |      20507 |    1 |  0.13 |       0.05 |
|        hyp |      204209 |     204109 |  100 | 12.55 |       0.15 |
|       log2 |       29029 |      28595 |  434 |  5.56 |       0.22 |
|        max |        2824 |       2824 |    0 |  0.01 |       0.01 |
| multiplier |       24315 |      24313 |    2 |  0.27 |       0.12 |
|        sin |        4969 |       4910 |   59 |  0.34 |       0.04 |
|       sqrt |       18253 |      18205 |   48 |  4.38 |       0.03 |
|     square |       15800 |      15785 |   15 |  0.07 |       0.01 |
|    arbiter |       11839 |      11839 |    0 |  0.13 |       0.10 |
|      cavlc |         623 |        600 |   23 |  0.08 |       0.08 |
|       ctrl |          90 |         90 |    0 |  0.00 |       0.00 |
|        dec |         304 |        304 |    0 |  0.00 |       0.00 |
|        i2c |        1016 |        993 |   23 |  0.02 |       0.01 |
|  int2float |         207 |        206 |    1 |  0.03 |       0.03 |
|   mem_ctrl |       38608 |      33199 | 5409 |  1.30 |       0.72 |
|   priority |         428 |        427 |    1 |  0.00 |       0.00 |
|     router |         145 |        131 |   14 |  0.00 |       0.00 |
|      voter |        7936 |       7929 |    7 |  0.02 |       0.01 |
*/

  return 0;
}

int main_xag()
{
  experiment<std::string, uint32_t, uint32_t, uint32_t, float, float>
      exp( "xag_resyn", "benchmark", "size_before", "size_after", "gain", "time", "time_resyn" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if ( benchmark != "square" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );
    std::string top_module_name = fmt::format( "/Users/sylee/Documents/GitHub/mockturtle/experiments/benchmarks/{}", benchmark );

    xag_network ntk;
    lorina::text_diagnostics consumer;
    lorina::diagnostic_engine diag( &consumer );
    if ( lorina::read_verilog( fmt::format( "xag/{}.v", benchmark ), verilog_reader( ntk, top_module_name ), &diag ) != lorina::return_code::success )
    {
      continue;
    }
    uint32_t size_before = ntk.num_gates();

    {
      resubstitution_params ps;
      resubstitution_stats st;
      ps.max_pis = 8u;
      ps.max_inserts = std::numeric_limits<uint32_t>::max();
      //ps.conflict_limit = 100;
      //ps.odc_levels = 3;
      std::string pat_filename = fmt::format( "pats/{}.pat", benchmark );
      //ps.save_patterns = pat_filename;
      assert( std::filesystem::exists( pat_filename ) );
      ps.pattern_filename = pat_filename;

      depth_view depthv{ ntk };
      fanout_view fanoutv{ depthv };

      using resub_view_t = fanout_view<depth_view<xag_network>>;
      using truth_table_type = kitty::partial_truth_table;

      using resyn_engine_t = xag_resyn_decompose<truth_table_type, xag_resyn_static_params_for_sim_resub<resub_view_t>>;

      using validator_t = circuit_validator<resub_view_t, bill::solvers::bsat2, false, true, false>;
      using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t, validator_t, resyn_engine_t>>;
      
      typename resub_impl_t::engine_st_t engine_st;
      typename resub_impl_t::collector_st_t collector_st;

      resub_impl_t p( fanoutv, ps, st, engine_st, collector_st );
      p.run();
      st.time_resub -= engine_st.time_patgen;
      st.time_total -= engine_st.time_patgen + engine_st.time_patsave;
      //st.report(); engine_st.report();
      ntk = cleanup_dangling( ntk );

      exp( benchmark, size_before, ntk.num_gates(), size_before - ntk.num_gates(), to_seconds( st.time_total ), to_seconds( engine_st.time_resyn ) );
    }
  }

  exp.save();
  exp.table();

/*
|  benchmark | size_before | size_after | gain |  time | time_resyn |
|      adder |         637 |        637 |    0 |  0.00 |       0.00 |
|        bar |        3141 |       3075 |   66 |  0.04 |       0.04 |
|        div |       16809 |      16733 |   76 |  0.17 |       0.06 |
|        hyp |      160292 |     152146 | 8146 | 49.99 |       0.54 |
|       log2 |       24022 |      23602 |  420 |  2.10 |       0.26 |
|        max |        2832 |       2832 |    0 |  0.02 |       0.02 |
| multiplier |       18572 |      18548 |   24 |  0.28 |       0.16 |
|        sin |        4288 |       4174 |  114 |  0.28 |       0.04 |
|       sqrt |       14414 |      12542 | 1872 |  3.45 |       0.06 |
|     square |       12453 |      12440 |   13 |  0.08 |       0.05 |
|    arbiter |       11839 |      11839 |    0 |  0.21 |       0.17 |
|      cavlc |         634 |        599 |   35 |  0.15 |       0.14 |
|       ctrl |          90 |         86 |    4 |  0.00 |       0.00 |
|        dec |         304 |        304 |    0 |  0.00 |       0.00 |
|        i2c |        1068 |       1025 |   43 |  0.02 |       0.02 |
|  int2float |         209 |        201 |    8 |  0.05 |       0.05 |
|   mem_ctrl |       43511 |      35763 | 7748 |  2.26 |       1.37 |
|   priority |         466 |        460 |    6 |  0.01 |       0.00 |
|     router |         175 |        143 |   32 |  0.00 |       0.00 |
|      voter |        5721 |       5710 |   11 |  0.27 |       0.02 |
*/

  return 0;
}

int main_muxig()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, float, uint32_t, uint32_t, float, float>
      exp( "muxig_resub", "benchmark", "size0", "time1", "size2", "gain", "time2", "time_resyn" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    //if ( benchmark != "ctrl" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );

    uint32_t size_before;
    stopwatch<>::duration time_preopt{0};
    
    aig_network aig;
    muxig_network muxig;
    if ( lorina::read_aiger( fmt::format( "compress2rs/{}.aig", benchmark ), aiger_reader( aig ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* cleanup into MuxIG */
    {
      stopwatch t( time_preopt );
      muxig = cleanup_dangling<aig_network, muxig_network>( aig );
      size_before = muxig.num_gates();
    }

    {
      resubstitution_params ps;
      resubstitution_stats st;
      ps.max_pis = 8u;
      ps.max_inserts = 20;
      std::string pat_filename = fmt::format( "pats/{}.pat", benchmark );
      //ps.save_patterns = pat_filename;
      assert( std::filesystem::exists( pat_filename ) );
      ps.pattern_filename = pat_filename;

      depth_view depth_mig{ muxig };
      fanout_view fanout_mig{ depth_mig };

      using resub_view_t = fanout_view<depth_view<muxig_network>>;
      using resyn_engine_t = mux_resyn<kitty::partial_truth_table>;

      using validator_t = circuit_validator<resub_view_t, bill::solvers::bsat2, false, true, false>;
      using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t, validator_t, resyn_engine_t>>;
      
      typename resub_impl_t::engine_st_t engine_st;
      typename resub_impl_t::collector_st_t collector_st;

      resub_impl_t p( fanout_mig, ps, st, engine_st, collector_st );
      p.run();
      st.time_resub -= engine_st.time_patgen;
      st.time_total -= engine_st.time_patgen + engine_st.time_patsave;
      //st.report(); engine_st.report();
      muxig = cleanup_dangling( muxig );

      const auto cec = benchmark == "hyp" ? true : abc_cec( muxig, benchmark );
      if ( !cec )
        fmt::print( "[e] benchmark {} not equivalent!\n", benchmark );

      exp( benchmark, size_before, to_seconds( time_preopt ), muxig.num_gates(), size_before - muxig.num_gates(), to_seconds( st.time_total ), to_seconds( engine_st.time_resyn ) );
    }
  }

  exp.save();
  exp.table();

/*
|  benchmark |  size0 | time1 |  size2 |  gain |  time2 | time_resyn |
|      adder |    892 |  0.00 |    638 |   254 |   0.03 |       0.01 |
|        bar |   3141 |  0.00 |   1779 |  1362 |   0.07 |       0.02 |
|        div |  20725 |  0.00 |  12593 |  8132 |   2.64 |       0.10 |
|        hyp | 204533 |  0.05 | 160430 | 44103 | 104.69 |       1.05 |
|       log2 |  29192 |  0.00 |  24837 |  4355 |  21.23 |       0.24 |
|        max |   2832 |  0.00 |   2030 |   802 |   0.08 |       0.02 |
| multiplier |  24337 |  0.00 |  19681 |  4656 |   4.51 |       0.20 |
|        sin |   5019 |  0.00 |   4263 |   756 |   0.77 |       0.05 |
|       sqrt |  18255 |  0.00 |  14538 |  3717 |   4.35 |       0.11 |
|     square |  15891 |  0.00 |  10985 |  4906 |   1.06 |       0.08 |
|    arbiter |  11839 |  0.00 |  11711 |   128 |   0.42 |       0.33 |
|      cavlc |    635 |  0.00 |    546 |    89 |   0.02 |       0.01 |
|       ctrl |     90 |  0.00 |     76 |    14 |   0.00 |       0.00 |
|        dec |    304 |  0.00 |    304 |     0 |   0.01 |       0.01 |
|        i2c |   1069 |  0.00 |    862 |   207 |   0.02 |       0.01 |
|  int2float |    209 |  0.00 |    183 |    26 |   0.00 |       0.00 |
|   mem_ctrl |  43924 |  0.01 |  33721 | 10203 |   2.78 |       0.97 |
|   priority |    466 |  0.00 |    404 |    62 |   0.01 |       0.00 |
|     router |    183 |  0.00 |    143 |    40 |   0.00 |       0.00 |
|      voter |   7946 |  0.00 |   6140 |  1806 |   0.29 |       0.04 |
*/

  return 0;
}

int main_muxig_cyclic()
{
  using namespace experiments;
  using namespace mockturtle;

  experiment<std::string, uint32_t, float, uint32_t, uint32_t, float, float>
      exp( "muxig_resub", "benchmark", "size0", "time1", "size2", "gain", "time2", "time_resyn" );

  for ( auto const& benchmark : epfl_benchmarks() )
  {
    if ( benchmark != "hyp" ) continue;
    fmt::print( "[i] processing {}\n", benchmark );
    std::string top_module_name = fmt::format( "/Users/sylee/Documents/GitHub/mockturtle/experiments/benchmarks/{}", benchmark );

    uint32_t size_before;
    stopwatch<>::duration time_preopt{0};
    
    xag_network aig;
    muxig_network muxig;
    if ( lorina::read_verilog( fmt::format( "xag/{}.v", benchmark ), verilog_reader( aig, top_module_name ) ) != lorina::return_code::success )
    {
      continue;
    }

    /* cleanup into MuxIG */
    {
      stopwatch t( time_preopt );
      muxig = cleanup_dangling<xag_network, muxig_network>( aig );
      size_before = muxig.num_gates();
    }

    {
      resubstitution_params ps;
      resubstitution_stats st;
      ps.max_pis = 8u;
      ps.max_inserts = 20;
      std::string pat_filename = fmt::format( "pats/{}.pat", benchmark );
      //ps.save_patterns = pat_filename;
      assert( std::filesystem::exists( pat_filename ) );
      ps.pattern_filename = pat_filename;

      depth_view depth_mig{ muxig };
      fanout_view fanout_mig{ depth_mig };

      using resub_view_t = fanout_view<depth_view<muxig_network>>;
      using resyn_engine_t = mux_resyn<kitty::partial_truth_table>;

      using validator_t = circuit_validator<resub_view_t, bill::solvers::bsat2, false, true, false>;
      using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t, validator_t, resyn_engine_t>>;
      
      typename resub_impl_t::engine_st_t engine_st;
      typename resub_impl_t::collector_st_t collector_st;

      resub_impl_t p( fanout_mig, ps, st, engine_st, collector_st );
      p.run();
      st.time_resub -= engine_st.time_patgen;
      st.time_total -= engine_st.time_patgen + engine_st.time_patsave;
      //st.report(); engine_st.report();
      muxig = cleanup_dangling( muxig );

      const auto cec = benchmark == "hyp" ? true : abc_cec( muxig, benchmark );
      if ( !cec )
        fmt::print( "[e] benchmark {} not equivalent!\n", benchmark );

      exp( benchmark, size_before, to_seconds( time_preopt ), muxig.num_gates(), size_before - muxig.num_gates(), to_seconds( st.time_total ), to_seconds( engine_st.time_resyn ) );
    }
  }

  exp.save();
  exp.table();

  return 0;
}

int main()
{
  
}
