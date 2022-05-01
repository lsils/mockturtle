#include <lorina/aiger.hpp>
#include <experiments.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/algorithms/experimental/costfn_window.hpp>
#include <mockturtle/algorithms/experimental/costfn_resyn.hpp>
#include <mockturtle/algorithms/experimental/window_resub.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/testcase_minimizer.hpp>
#include <mockturtle/utils/debugging_utils.hpp>

using namespace mockturtle;

int main()
{

  using namespace mockturtle::experimental;
  using namespace experiments;

  /* run the actual experiments */
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "cost_aware", "benchmark", "C1", "C1\'", "C2", "C2\'", "C3", "C3\'", "C4", "C4\'", "C5", "C5\'", "runtime", "cec" );
  for ( auto const& benchmark : epfl_benchmarks() )
  {
    // if ( benchmark != "voter" ) continue;
    float run_time = 0;

    fmt::print( "[i] processing {}\n", benchmark );
    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    assert( result == lorina::return_code::success ); (void)result;

    auto costfn1 = gate_cost<fanout_view<xag_network>>();
    auto costfn2 = and_cost<fanout_view<xag_network>>();
    auto costfn3 = level_cost<fanout_view<xag_network>>();
    auto costfn4 = adp_cost<fanout_view<xag_network>>();
    auto costfn5 = supp_cost<fanout_view<xag_network>>();

    auto resynfn = level_cost<fanout_view<xag_network>>();
    auto c1 = cost_view( fanout_view( xag ), costfn1 ).get_cost();
    auto c2 = cost_view( fanout_view( xag ), costfn2 ).get_cost();
    auto c3 = cost_view( fanout_view( xag ), costfn3 ).get_cost();
    auto c4 = cost_view( fanout_view( xag ), costfn4 ).get_cost();
    auto c5 = cost_view( fanout_view( xag ), costfn5 ).get_cost();

    cost_aware_params ps;
    cost_aware_stats st;
    ps.verbose = true;
    bool until_conv = false;
    while ( true )
    {
      fanout_view ntk( xag );
      uint32_t _cost = cost_view( ntk, resynfn ).get_cost();
      cost_aware_optimization( ntk, resynfn, ps, &st );
      xag = cleanup_dangling( xag );        
      if ( !until_conv || cost_view( fanout_view( xag ), resynfn ).get_cost() == _cost )
      {
        break;
      }
    }

    run_time = to_seconds( st.time_total );

    auto _c1 = cost_view( fanout_view( xag ), costfn1 ).get_cost();
    auto _c2 = cost_view( fanout_view( xag ), costfn2 ).get_cost();
    auto _c3 = cost_view( fanout_view( xag ), costfn3 ).get_cost();
    auto _c4 = cost_view( fanout_view( xag ), costfn4 ).get_cost();
    auto _c5 = cost_view( fanout_view( xag ), costfn5 ).get_cost();

    const auto cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    exp( benchmark, c1, _c1, c2, _c2, c3, _c3, c4, _c4, c5, _c5, run_time, cec );
  }
  exp.save();
  exp.table();
  return 0;
}