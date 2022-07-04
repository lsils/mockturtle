#include <experiments.hpp>
#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/experimental/costfn_window.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/utils/stopwatch.hpp>

using namespace mockturtle;

template<class Ntk>
struct and_cost
{
public:
  using cost_t = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    (void)fanin_costs;
    if ( ntk.is_and( n ) )
      tot_cost += 1; /* add dissipate cost */
    return 0;        /* return accumulate cost */
  }
};

template<class Ntk>
struct gate_cost
{
public:
  using cost_t = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    (void)fanin_costs;
    if ( ntk.is_pi( n ) == false )
      tot_cost += 1; /* add dissipate cost */
    return 0;        /* return accumulate cost */
  }
};

template<class Ntk>
struct supp_cost
{
public:
  using cost_t = kitty::partial_truth_table;
  cost_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<cost_t> const& fanin_costs = {} ) const
  {
    cost_t _c( ntk.num_pis() );
    if ( ntk.is_pi( n ) )
    {
      set_bit( _c, ntk.pi_index( n ) );
    }
    std::for_each( std::begin( fanin_costs ), std::end( fanin_costs ), [&_c]( cost_t c ) { _c |= c; } );
    if ( !ntk.is_pi( n ) )
      tot_cost += kitty::count_ones( _c );
    return _c; /* return accumulate cost */
  }
};

template<class Ntk>
struct level_cost
{
public:
  using cost_t = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    uint32_t _cost = ntk.is_pi( n ) ? 0 : *std::max_element( std::begin( fanin_costs ), std::end( fanin_costs ) ) + 1;
    tot_cost = std::max( tot_cost, _cost ); /* update dissipate cost */
    return _cost;                           /* return accumulate cost */
  }
};

template<class Ntk>
struct adp_cost
{
public:
  using cost_t = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    uint32_t _cost = ntk.is_pi( n ) ? 1 : *std::max_element( std::begin( fanin_costs ), std::end( fanin_costs ) ) + 1;
    if ( !ntk.is_pi( n ) )
      tot_cost += _cost; /* sum of level over each node */
    return _cost;        /* return accumulate cost */
  }
};

int main()
{

  using namespace mockturtle::experimental;
  using namespace experiments;

  /* run the actual experiments */
  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, bool> exp( "cost_aware_optimization", "benchmark", "C1", "C1\'", "C2", "C2\'", "C3", "C3\'", "C4", "C4\'", "C5", "C5\'", "runtime", "cec" );
  for ( auto const& benchmark : epfl_benchmarks() )
  {
    float run_time = 0;

    fmt::print( "[i] processing {}\n", benchmark );
    xag_network xag;
    auto const result = lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( xag ) );
    assert( result == lorina::return_code::success );
    (void)result;

    auto costfn1 = gate_cost<xag_network>();
    auto costfn2 = and_cost<xag_network>();
    auto costfn3 = level_cost<xag_network>();
    auto costfn4 = adp_cost<xag_network>();
    auto costfn5 = supp_cost<xag_network>();

    auto resynfn = and_cost<xag_network>();
    auto c1 = cost_view( xag, costfn1 ).get_cost();
    auto c2 = cost_view( xag, costfn2 ).get_cost();
    auto c3 = cost_view( xag, costfn3 ).get_cost();
    auto c4 = cost_view( xag, costfn4 ).get_cost();
    auto c5 = cost_view( xag, costfn5 ).get_cost();

    cost_aware_params ps;
    cost_aware_stats st;
    ps.verbose = true;

    cost_aware_optimization( xag, resynfn, ps, &st );
    xag = cleanup_dangling( xag );

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