#include <catch.hpp>

#include <kitty/kitty.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/index_list.hpp>
#include <mockturtle/algorithms/resyn_engines/xag_resyn.hpp>
#include <mockturtle/algorithms/simulation.hpp>

using namespace mockturtle;

template<class TT>
struct aig_resyn_sparams_copy : public xag_resyn_static_params_default<TT>
{
  static constexpr bool preserve_depth = true;
  static constexpr bool uniform_div_cost = false;
  static constexpr bool collect_sols = true;
  static constexpr bool use_xor = false;
  static constexpr bool copy_tts = true;
};

template<class TT>
struct aig_resyn_sparams_no_copy : public xag_resyn_static_params_default<TT>
{
  static constexpr bool preserve_depth = true;
  static constexpr bool uniform_div_cost = false;
  static constexpr bool collect_sols = true;
  static constexpr bool use_xor = false;
  static constexpr bool copy_tts = false;
};

template<class TT = kitty::partial_truth_table>
void test_aig_kresub( TT const& target, TT const& care, std::vector<TT> const& tts, uint32_t num_inserts )
{
  xag_resyn_stats st;
  std::vector<uint32_t> divs;
  for ( auto i = 0u; i < tts.size(); ++i )
  {
    divs.emplace_back( i );
  }
  partial_simulator sim( tts );
  using cost_t = std::pair<uint32_t, uint32_t>;
  xag_resyn_decompose<TT, aig_resyn_sparams_copy<TT>> engine_copy( st );
  const auto res = engine_copy( target, care, divs.begin(), divs.end(), tts, 
    [](auto n) {return std::pair(0,0);},
    [](cost_t x, cost_t y, bool is_xor=false) {return std::pair(0,0);},
    [](cost_t x, cost_t y) {return false;},
    num_inserts, std::numeric_limits<uint32_t>::max() );
  CHECK( res );
  CHECK( (*res).num_gates() == num_inserts );
  aig_network aig;
  decode( aig, *res );
  const auto ans = simulate<TT, aig_network, partial_simulator>( aig, sim )[0];
  CHECK( kitty::implies( target & care, ans ) );
  CHECK( kitty::implies( ~target & care, ~ans ) );

  xag_resyn_decompose<TT, aig_resyn_sparams_no_copy<TT>> engine_no_copy( st );
  const auto res2 = engine_copy( target, care, divs.begin(), divs.end(), tts, 
    [](auto n) {return std::pair(0,0);},
    [](cost_t x, cost_t y, bool is_xor=false) {return std::pair(0,0);},
    [](cost_t x, cost_t y) {return false;},
    num_inserts, std::numeric_limits<uint32_t>::max() );  CHECK( res2 );
  CHECK( (*res2).num_gates() == num_inserts );
  aig_network aig2;
  decode( aig2, *res2 );
  const auto ans2 = simulate<TT, aig_network, partial_simulator>( aig2, sim )[0];
  CHECK( kitty::implies( target & care, ans2 ) );
  CHECK( kitty::implies( ~target & care, ~ans2 ) );
}

TEST_CASE( "AIG/XAG costfn resynthesis -- 0-resub with don't care", "[costfn_resyn]" )
{
  std::vector<kitty::partial_truth_table> tts( 1, kitty::partial_truth_table( 8 ) );
  kitty::partial_truth_table target( 8 );
  kitty::partial_truth_table care( 8 );

  /* const */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "11001100" );
  test_aig_kresub( target, care, tts, 0 );

  /* buffer */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "00111100" );
  kitty::create_from_binary_string( tts[0], "11110000" );
  test_aig_kresub( target, care, tts, 0 );

  /* inverter */
  kitty::create_from_binary_string( target, "00110011" );
  kitty::create_from_binary_string(   care, "00110110" );
  kitty::create_from_binary_string( tts[0], "00000101" );
  test_aig_kresub( target, care, tts, 0 );
}

TEST_CASE( "AIG costfn resynthesis -- 1 <= k <= 3", "[costfn_resyn]" )
{
  std::vector<kitty::partial_truth_table> tts( 4, kitty::partial_truth_table( 8 ) );
  kitty::partial_truth_table target( 8 );
  kitty::partial_truth_table care = ~target.construct();

  kitty::create_from_binary_string( target, "11110000" ); // target
  kitty::create_from_binary_string( tts[0], "11000000" );
  kitty::create_from_binary_string( tts[1], "00110000" );
  kitty::create_from_binary_string( tts[2], "01011111" ); // binate
  test_aig_kresub( target, care, tts, 1 ); // 1 | 2

  kitty::create_from_binary_string( target, "11110000" ); // target
  kitty::create_from_binary_string( tts[0], "11001100" ); // binate
  kitty::create_from_binary_string( tts[1], "11111100" );
  kitty::create_from_binary_string( tts[2], "00001100" );
  test_aig_kresub( target, care, tts, 1 ); // 2 & ~3

  kitty::create_from_binary_string( target, "11110000" ); // target
  kitty::create_from_binary_string( tts[0], "01110010" ); // binate
  kitty::create_from_binary_string( tts[1], "11111100" ); 
  kitty::create_from_binary_string( tts[2], "10000011" ); // binate
  test_aig_kresub( target, care, tts, 2 ); // 2 & (1 | 3)

  tts.emplace_back( 8 );
  kitty::create_from_binary_string( target, "11110000" ); // target
  kitty::create_from_binary_string( tts[0], "01110010" ); // binate
  kitty::create_from_binary_string( tts[1], "00110011" ); // binate
  kitty::create_from_binary_string( tts[2], "10000011" ); // binate
  kitty::create_from_binary_string( tts[3], "11001011" ); // binate
  test_aig_kresub( target, care, tts, 3 ); // ~(2 & 4) & (1 | 3)
}

TEST_CASE( "AIG costfn resynthesis -- recursive", "[costfn_resyn]" )
{
  std::vector<kitty::partial_truth_table> tts( 6, kitty::partial_truth_table( 16 ) );
  kitty::partial_truth_table target( 16 );
  kitty::partial_truth_table care = ~target.construct();

  kitty::create_from_binary_string( target, "1111000011111111" ); // target
  kitty::create_from_binary_string( tts[0], "0111001000000000" ); // binate
  kitty::create_from_binary_string( tts[1], "0011001100000000" ); // binate
  kitty::create_from_binary_string( tts[2], "1000001100000000" ); // binate
  kitty::create_from_binary_string( tts[3], "1100101100000000" ); // binate
  kitty::create_from_binary_string( tts[4], "0000000011111111" ); // unate
  test_aig_kresub( target, care, tts, 4 ); // 5 | ( ~(2 & 4) & (1 | 3) )

  tts.emplace_back( 16 );
  kitty::create_from_binary_string( target, "1111000011111100" ); // target
  kitty::create_from_binary_string( tts[0], "0111001000000000" ); // binate
  kitty::create_from_binary_string( tts[1], "0011001100000000" ); // binate
  kitty::create_from_binary_string( tts[2], "1000001100000000" ); // binate
  kitty::create_from_binary_string( tts[3], "1100101100000000" ); // binate
  kitty::create_from_binary_string( tts[4], "0000000011111110" ); // binate
  kitty::create_from_binary_string( tts[5], "0000000011111101" ); // binate
  test_aig_kresub( target, care, tts, 5 ); // (5 & 6) | ( ~(2 & 4) & (1 | 3) )
}
