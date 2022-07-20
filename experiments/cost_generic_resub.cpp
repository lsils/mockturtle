#include <experiments.hpp>
#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/experimental/cost_generic_resub.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/utils/stopwatch.hpp>

using namespace mockturtle;

int main()
{

  using namespace mockturtle::experimental;
  using namespace experiments;

  /* run the actual experiments */
  experiment<std::string, uint32_t, uint32_t, float, bool> exp( "cost_generic_resub", "benchmark", "cost before", "cost after", "runtime", "cec" );
  for ( auto const& benchmark : epfl_benchmarks() )
  {
    float run_time = 0;

    fmt::print( "[i] processing {}\n", benchmark );
    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    assert( result == lorina::return_code::success );
    (void)result;

    auto costfn = t_depth_cost_function<xag_network>();

    auto c1 = cost_view( xag, costfn ).get_cost();

    cost_generic_resub_params ps;
    cost_generic_resub_stats st;
    ps.verbose = true;

    cost_generic_resub( xag, costfn, ps, &st );
    xag = cleanup_dangling( xag );

    run_time = to_seconds( st.time_total );

    auto _c1 = cost_view( xag, costfn ).get_cost();

    const auto cec = benchmark == "hyp" ? true : abc_cec( xag, benchmark );
    exp( benchmark, c1, _c1, run_time, cec );
  }
  exp.save();
  exp.table();
  return 0;
}