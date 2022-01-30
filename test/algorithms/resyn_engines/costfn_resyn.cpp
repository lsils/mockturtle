#include <catch.hpp>

#include <kitty/kitty.hpp>

#include <mockturtle/algorithms/resyn_engines/xag_costfn_resyn.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/index_list.hpp>

using namespace mockturtle;

template<class TT>
struct aig_costfn_resyn_sparams_costfn : public xag_costfn_resyn_static_params_default<TT>
{
  static constexpr bool preserve_depth = true;
  static constexpr bool uniform_div_cost = false;
  static constexpr bool collect_sols = true;
  static constexpr bool use_xor = false;
  static constexpr bool copy_tts = false;
};

template<class TT = kitty::partial_truth_table, class LeafFn, class NodeFn, class CmpFn>
void test_aig_costfn_kresub( TT const& target, TT const& care, std::vector<TT> const& tts, LeafFn&& lf, NodeFn&& nf, CmpFn&& cmp, uint32_t num_inserts, uint32_t num_depth )
{
  xag_costfn_resyn_stats st;
  std::vector<uint32_t> divs;
  for ( auto i = 0u; i < tts.size(); ++i )
  {
    divs.emplace_back( i );
  }

  (void)num_inserts;
  (void)num_depth;

  partial_simulator sim( tts );
  xag_costfn_resyn_solver<TT, aig_costfn_resyn_sparams_costfn<TT>> engine( st );
  const auto res = engine(
      target, care, divs.begin(), divs.end(), tts,
      lf, nf, cmp, std::pair(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()) );

  CHECK( res );
  //   std::cout << to_index_list_string( *res ) << "\n";
  CHECK( ( *res ).num_gates() == num_inserts );
  aig_network aig;
  decode( aig, *res );
  const auto ans = simulate<TT, aig_network, partial_simulator>( aig, sim )[0];
  CHECK( kitty::implies( target & care, ans ) );
  CHECK( kitty::implies( ~target & care, ~ans ) );
}

TEST_CASE( "AIG costfn resynthesis -- area opt", "[costfn_resyn]" )
{

  uint32_t num_var = 2;
  std::vector<kitty::partial_truth_table> tts( 3, kitty::partial_truth_table( 1 << num_var ) );
  kitty::partial_truth_table target( 1 << num_var );
  kitty::partial_truth_table care = ~target.construct();

  kitty::create_from_binary_string( target, "1000" ); // target = a || target = bc
  kitty::create_from_binary_string( tts[0], "1000" ); // a
  kitty::create_from_binary_string( tts[1], "1100" ); // b
  kitty::create_from_binary_string( tts[2], "1010" ); // c

  std::vector<uint32_t> levels = { 2, 0, 0 };
  using cost_t = std::pair<uint32_t, uint32_t>;
  test_aig_costfn_kresub( target, care, tts,
      [&]( auto n ) { return std::pair( 0, levels[n] ); },
      []( cost_t x, cost_t y, bool is_xor = false ) {
        (void) is_xor; 
        return std::pair(
          x.first + y.first + 1, 
          std::max(x.second, y.second) + 1);
      },
      []( cost_t x, cost_t y ) { return x.first < y.first; },
      0, 2 ); 
}

TEST_CASE( "AIG costfn resynthesis -- depth opt", "[costfn_resyn]" )
{

  uint32_t num_var = 2;
  std::vector<kitty::partial_truth_table> tts( 3, kitty::partial_truth_table( 1 << num_var ) );
  kitty::partial_truth_table target( 1 << num_var );
  kitty::partial_truth_table care = ~target.construct();

  kitty::create_from_binary_string( target, "1000" ); // target
  kitty::create_from_binary_string( tts[0], "1000" ); // area-opt
  kitty::create_from_binary_string( tts[1], "1100" ); // depth-opt
  kitty::create_from_binary_string( tts[2], "1010" ); // depth-opt

  std::vector<uint32_t> levels = { 2, 0, 0 };
  using cost_t = std::pair<uint32_t, uint32_t>;
  test_aig_costfn_kresub( target, care, tts,
      [&]( auto n ) { return std::pair( 0, levels[n] ); },
      []( cost_t x, cost_t y, bool is_xor = false ) {
        return std::pair(
          x.first + y.first + 1, 
          std::max(x.second, y.second) + 1);
      },
      []( cost_t x, cost_t y ) { return x.second < y.second; },
      1, 1 );
}
