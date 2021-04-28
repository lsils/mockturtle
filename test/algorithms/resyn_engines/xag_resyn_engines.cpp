#include <catch.hpp>

#include <kitty/kitty.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/index_list.hpp>
#include <mockturtle/algorithms/resyn_engines/xag_resyn_engines.hpp>
#include <mockturtle/algorithms/simulation.hpp>

using namespace mockturtle;

void test_aig_kresub( kitty::partial_truth_table const& target, std::vector<kitty::partial_truth_table> const& tts, uint32_t num_inserts )
{
  xag_resyn_engine_stats st;
  xag_resyn_engine_params ps;
  ps.max_size = num_inserts;
  xag_resyn_engine<kitty::partial_truth_table, aig_network, uint32_t, std::vector<kitty::partial_truth_table>> engine( target, ~target.construct(), tts, st, ps );
  for ( auto i = 0u; i < tts.size(); ++i )
  {
    engine.add_divisor( i );
  }
  const auto res = engine();
  CHECK( res );
  CHECK( (*res).num_gates() == num_inserts );

  aig_network aig;
  decode( aig, *res );
  partial_simulator sim( tts );
  const auto ans = simulate<kitty::partial_truth_table, aig_network, partial_simulator>( aig, sim )[0];
  CHECK( target == ans );
}

TEST_CASE( "AIG/XAG resynthesis -- 0-resub with don't care", "[xag_resyn]" )
{
  using engine_t = xag_resyn_engine<kitty::partial_truth_table, aig_network, uint32_t, std::vector<kitty::partial_truth_table>>;
  std::vector<kitty::partial_truth_table> tts( 1, kitty::partial_truth_table( 8 ) );
  kitty::partial_truth_table target( 8 );
  kitty::partial_truth_table care( 8 );
  xag_resyn_engine_stats st;
  xag_resyn_engine_params ps;
  ps.max_size = 0;

  /* const */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "11001100" );
  engine_t engine1( target, care, tts, st, ps );
  const auto res1 = engine1();
  CHECK( res1 );
  CHECK( to_index_list_string( *res1 ) == "{0, 1, 0, 0}" );

  /* buffer */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "00111100" );
  kitty::create_from_binary_string( tts[0], "11110000" );
  engine_t engine2( target, care, tts, st, ps );
  engine2.add_divisor( 0 );
  const auto res2 = engine2();
  CHECK( res2 );
  CHECK( to_index_list_string( *res2 ) == "{1, 1, 0, 2}" );

  /* inverter */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "00110110" );
  kitty::create_from_binary_string( tts[0], "00000101" );
  engine_t engine3( target, care, tts, st, ps );
  engine3.add_divisor( 0 );
  const auto res3 = engine3();
  CHECK( res3 );
  CHECK( to_index_list_string( *res3 ) == "{1, 1, 0, 3}" );
}

TEST_CASE( "AIG resynthesis -- 1 <= k <= 3", "[xag_resyn]" )
{
  std::vector<kitty::partial_truth_table> tts( 4, kitty::partial_truth_table( 8 ) );
  kitty::partial_truth_table target( 8 );

  kitty::create_from_binary_string( target, "11110000" ); // target
  kitty::create_from_binary_string( tts[0], "11000000" );
  kitty::create_from_binary_string( tts[1], "00110000" );
  kitty::create_from_binary_string( tts[2], "01011111" ); // binate
  test_aig_kresub( target, tts, 1 ); // 1 | 2

  kitty::create_from_binary_string( target, "11110000" ); // target
  kitty::create_from_binary_string( tts[0], "11001100" ); // binate
  kitty::create_from_binary_string( tts[1], "11111100" );
  kitty::create_from_binary_string( tts[2], "00001100" );
  test_aig_kresub( target, tts, 1 ); // 2 & ~3

  kitty::create_from_binary_string( target, "11110000" ); // target
  kitty::create_from_binary_string( tts[0], "01110010" ); // binate
  kitty::create_from_binary_string( tts[1], "11111100" ); 
  kitty::create_from_binary_string( tts[2], "10000011" ); // binate
  test_aig_kresub( target, tts, 2 ); // 2 & (1 | 3)

  tts.emplace_back( 8 );
  kitty::create_from_binary_string( target, "11110000" ); // target
  kitty::create_from_binary_string( tts[0], "01110010" ); // binate
  kitty::create_from_binary_string( tts[1], "00110011" ); // binate
  kitty::create_from_binary_string( tts[2], "10000011" ); // binate
  kitty::create_from_binary_string( tts[3], "11001011" ); // binate
  test_aig_kresub( target, tts, 3 ); // ~(2 & 4) & (1 | 3)
}

TEST_CASE( "AIG resynthesis -- recursive", "[xag_resyn]" )
{
  std::vector<kitty::partial_truth_table> tts( 6, kitty::partial_truth_table( 16 ) );
  kitty::partial_truth_table target( 16 );

  kitty::create_from_binary_string( target, "1111000011111111" ); // target
  kitty::create_from_binary_string( tts[0], "0111001000000000" ); // binate
  kitty::create_from_binary_string( tts[1], "0011001100000000" ); // binate
  kitty::create_from_binary_string( tts[2], "1000001100000000" ); // binate
  kitty::create_from_binary_string( tts[3], "1100101100000000" ); // binate
  kitty::create_from_binary_string( tts[4], "0000000011111111" ); // unate
  test_aig_kresub( target, tts, 4 ); // 5 | ( ~(2 & 4) & (1 | 3) )

  tts.emplace_back( 16 );
  kitty::create_from_binary_string( target, "1111000011111100" ); // target
  kitty::create_from_binary_string( tts[0], "0111001000000000" ); // binate
  kitty::create_from_binary_string( tts[1], "0011001100000000" ); // binate
  kitty::create_from_binary_string( tts[2], "1000001100000000" ); // binate
  kitty::create_from_binary_string( tts[3], "1100101100000000" ); // binate
  kitty::create_from_binary_string( tts[4], "0000000011111110" ); // binate
  kitty::create_from_binary_string( tts[5], "0000000011111101" ); // binate
  test_aig_kresub( target, tts, 5 ); // (5 & 6) | ( ~(2 & 4) & (1 | 3) )
}

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

TEST_CASE( "Synthesize AIGs for all 3-input functions", "[xag_resyn]" )
{
  constexpr uint32_t const num_vars = 3u;
  using truth_table_type = kitty::static_truth_table<num_vars>;

  xag_resyn_engine_stats st;
  xag_resyn_engine_params ps;
  ps.max_size = std::numeric_limits<uint32_t>::max();

  std::vector<truth_table_type> divisor_functions;
  truth_table_type x;
  for ( uint32_t i = 0; i < num_vars; ++i )
  {
    kitty::create_nth_var( x, i );
    divisor_functions.emplace_back( x );
  }

  truth_table_type target, care;
  care = ~target.construct();

  uint32_t success_counter{0};
  uint32_t failed_counter{0};
  do
  {
    xag_resyn_engine<truth_table_type, aig_network, uint32_t, std::vector<truth_table_type>, true, false>
      engine( target, care, divisor_functions, st, ps );
    for ( auto i = 0u; i < divisor_functions.size(); ++i )
    {
      engine.add_divisor( i );
    }

    auto const index_list = engine();
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

  CHECK( success_counter == 254 );
  CHECK( failed_counter == 2 );
}

TEST_CASE( "Synthesize XAGs for all 4-input functions", "[xag_resyn]" )
{
  constexpr uint32_t const num_vars = 4u;
  using truth_table_type = kitty::static_truth_table<num_vars>;

  xag_resyn_engine_stats st;
  xag_resyn_engine_params ps;
  ps.max_size = std::numeric_limits<uint32_t>::max();

  std::vector<truth_table_type> divisor_functions;
  truth_table_type x;
  for ( uint32_t i = 0; i < num_vars; ++i )
  {
    kitty::create_nth_var( x, i );
    divisor_functions.emplace_back( x );
  }

  truth_table_type target, care;
  care = ~target.construct();

  uint32_t success_counter{0};
  uint32_t failed_counter{0};
  do
  {
    xag_resyn_engine<truth_table_type, xag_network, uint32_t, std::vector<truth_table_type>, true, true>
      engine( target, care, divisor_functions, st, ps );
    for ( auto i = 0u; i < divisor_functions.size(); ++i )
    {
      engine.add_divisor( i );
    }

    auto const index_list = engine();
    if ( index_list )
    {
      ++success_counter;

      /* verify index_list using simulation */
      xag_network xag;
      decode( xag, *index_list );

      simulator<num_vars> sim( divisor_functions );
      auto const tts = simulate<truth_table_type, xag_network>( xag, sim );
      CHECK( target == tts[0] );
    }
    else
    {
      ++failed_counter;
    }

    kitty::next_inplace( target );
  } while ( !kitty::is_const0( target ) );

  CHECK( success_counter == 54622 );
  CHECK( failed_counter == 10914 );
}
