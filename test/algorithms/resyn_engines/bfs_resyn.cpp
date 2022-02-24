#include <catch.hpp>

#include <kitty/kitty.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/index_list.hpp>
#include <mockturtle/algorithms/resyn_engines/xag_resyn.hpp>
#include <mockturtle/algorithms/simulation.hpp>

using namespace mockturtle;

template<uint32_t num_vars>
class simulator
{
public:
  explicit simulator( std::vector<kitty::static_truth_table<num_vars>> const& input_values )
    : input_values_( input_values )
  {}

  kitty::static_truth_table<num_vars> compute_constant( bool value ) const
  {
    kitty::static_truth_table<num_vars> tt;
    return value ? ~tt : tt;
  }

  kitty::static_truth_table<num_vars> compute_pi( uint32_t index ) const
  {
    assert( index < input_values_.size() );
    return input_values_[index];
  }

  kitty::static_truth_table<num_vars> compute_not( kitty::static_truth_table<num_vars> const& value ) const
  {
    return ~value;
  }

private:
  std::vector<kitty::static_truth_table<num_vars>> const& input_values_;
}; /* simulator */

template<typename engine_type, uint32_t num_vars>
void test_xag_n_input_functions( uint32_t& success_counter, uint32_t& failed_counter )
{
  using truth_table_type = typename engine_type::truth_table_t;
  typename engine_type::stats st;
  std::vector<truth_table_type> divisor_functions;
  std::vector<uint32_t> divisors;
  truth_table_type x;
  for ( uint32_t i = 0; i < num_vars; ++i )
  {
    kitty::create_nth_var( x, i );
    divisor_functions.emplace_back( x );
    divisors.emplace_back( i );
  }
  std::function<uint32_t(uint32_t)> const size_cost_fn = [&]( uint32_t idx ){ return 0u; };
  std::function<uint32_t(uint32_t)> const depth_cost_fn = [&]( uint32_t idx ){ return 0u; };
  truth_table_type target, care;
  care = ~target.construct();

  do
  {
    engine_type engine( st );

    auto const index_list = engine( target, care, divisors.begin(), divisors.end(), divisor_functions, size_cost_fn, depth_cost_fn, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max() );
    if ( index_list )
    {
      ++success_counter;

      /* verify index_list using simulation */
      aig_network aig;
      decode( aig, *index_list );

      simulator<num_vars> sim( divisor_functions );
      auto const tts = simulate<truth_table_type, aig_network>( aig, sim );
      CHECK( target == tts[0] );
    }
    else
    {
      ++failed_counter;
    }

    kitty::next_inplace( target );
  } while ( !kitty::is_const0( target ) );
}

TEST_CASE( "Synthesize XAGs using BFS for all 3-input functions", "[bfs_resyn]" )
{
  using TT = kitty::static_truth_table<3>;
  using engine_t = xag_resyn_decompose<TT, xag_resyn_static_params_preserve_depth<TT>>;;
  uint32_t success_counter{0};
  uint32_t failed_counter{0};
  test_xag_n_input_functions<engine_t, 3>( success_counter, failed_counter );
}

TEST_CASE( "Synthesize XAGs using BFS for all 4-input functions", "[bfs_resyn]" )
{
  using TT = kitty::static_truth_table<4>;
  using engine_t = xag_resyn_decompose<TT, xag_resyn_static_params_preserve_depth<TT>>;;
  uint32_t success_counter{0};
  uint32_t failed_counter{0};
  test_xag_n_input_functions<engine_t, 4>( success_counter, failed_counter );
}

