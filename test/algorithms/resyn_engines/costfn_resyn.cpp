#include <catch.hpp>

#include <kitty/kitty.hpp>

#include <mockturtle/algorithms/resyn_engines/xag_costfn_resyn.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/index_list.hpp>
#include <mockturtle/views/depth_view.hpp>

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
void test_aig_costfn_kresub( TT const& target, TT const& care, std::vector<TT> const& tts, LeafFn&& lf, NodeFn&& nf, CmpFn&& cmp, uint32_t correct_size, uint32_t correct_depth )
{
  xag_costfn_resyn_stats st;
  std::vector<uint32_t> divs;
  for ( auto i = 0u; i < tts.size(); ++i )
  {
    divs.emplace_back( i );
  }

  (void)correct_size;
  (void)correct_depth;

  partial_simulator sim( tts );
  xag_costfn_resyn_solver<TT, aig_costfn_resyn_sparams_costfn<TT>> engine( st );
  const auto res = engine(
      target, care, divs.begin(), divs.end(), tts,
      lf, nf, cmp, std::pair(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()) );

  CHECK( res );
  //   std::cout << to_index_list_string( *res ) << "\n";
  CHECK( ( *res ).num_gates() == correct_size );
  aig_network aig;
  decode( aig, *res );
  const auto ans = simulate<TT, aig_network, partial_simulator>( aig, sim )[0];
  CHECK( kitty::implies( target & care, ans ) );
  CHECK( kitty::implies( ~target & care, ~ans ) );
}

TEST_CASE( "AIG costfn resynthesis -- area optimization for wire", "[costfn_resyn]" )
{

  uint32_t num_var = 2;
  std::vector<kitty::partial_truth_table> tts( 1, kitty::partial_truth_table( 1 << num_var ) );
  kitty::partial_truth_table target( 1 << num_var );
  kitty::partial_truth_table care = ~target.construct();

  kitty::create_from_binary_string( target, "1000" ); // target = a
  kitty::create_from_binary_string( tts[0], "1000" ); // a

  using cost_t = std::pair<uint32_t, uint32_t>;
  test_aig_costfn_kresub( target, care, tts,
      [&]( auto n ) { return std::pair( 0, 0 ); },
      []( cost_t x, cost_t y, bool is_xor = false ) {
        (void) is_xor; 
        return std::pair(
          x.first + y.first + 1, 
          std::max(x.second, y.second) + 1);
      },
      []( cost_t x, cost_t y ) { return x.second < y.second; },
      0, 0 ); 

}

TEST_CASE( "AIG costfn resynthesis -- area optimization for small circuit", "[costfn_resyn]" )
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

TEST_CASE( "AIG costfn resynthesis -- depth optimization for small circuit", "[costfn_resyn]" )
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

TEST_CASE( "AIG costfn resynthesis -- area optimization with small binate divisors", "[costfn_resyn]" )
{
  uint32_t num_var = 3;
  std::vector<kitty::partial_truth_table> tts( 3, kitty::partial_truth_table( 1 << num_var ) );
  kitty::partial_truth_table target( 1 << num_var );
  kitty::partial_truth_table care = ~target.construct();

  kitty::create_from_binary_string( target, "11100010" ); // target = ab + (~b)c
  kitty::create_from_binary_string( tts[0], "11110000" ); // a
  kitty::create_from_binary_string( tts[1], "11001100" ); // b
  kitty::create_from_binary_string( tts[2], "10101010" ); // c

  std::vector<uint32_t> levels = { 0, 0, 0 };
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
      3, 2 ); 
}

TEST_CASE( "AIG costfn resynthesis -- area optimization with binate divisors", "[costfn_resyn]" )
{

  uint32_t num_var = 3;
  std::vector<kitty::partial_truth_table> tts( 3, kitty::partial_truth_table( 1 << num_var ) );
  kitty::partial_truth_table target( 1 << num_var );
  kitty::partial_truth_table care = ~target.construct();

  /*
    target = ab+ac+bc (MAJ)
        ab+c(a+b)
                c(a+b)
      ab  a+b
    a         b       c
    requires 4 new nodes, with depth 3.
   */
  kitty::create_from_binary_string( target, "11101000" ); 
  kitty::create_from_binary_string( tts[0], "11110000" ); // a
  kitty::create_from_binary_string( tts[1], "11001100" ); // b
  kitty::create_from_binary_string( tts[2], "10101010" ); // c

  std::vector<uint32_t> levels = { 0, 0, 0 };
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
      4, 3 ); 
}

TEST_CASE( "AIG costfn resynthesis -- depth optimization with larger binate divisors", "[costfn_resyn]" )
{
  uint32_t num_var = 4;
  std::vector<kitty::partial_truth_table> tts( 4, kitty::partial_truth_table( 1 << num_var ) );
  kitty::partial_truth_table target( 1 << num_var );
  kitty::partial_truth_table care = ~target.construct();

  kitty::create_from_binary_string( target, "1111100010001000" ); // target = ab + cd
  kitty::create_from_binary_string( tts[0], "1111111100000000" ); // a
  kitty::create_from_binary_string( tts[1], "1111000011110000" ); // b
  kitty::create_from_binary_string( tts[2], "1100110011001100" ); // c
  kitty::create_from_binary_string( tts[3], "1010101010101010" ); // d

  std::vector<uint32_t> levels = { 0, 0, 0, 0 };
  using cost_t = std::pair<uint32_t, uint32_t>;
  test_aig_costfn_kresub( target, care, tts,
      [&]( auto n ) { return std::pair( 0, levels[n] ); },
      []( cost_t x, cost_t y, bool is_xor = false ) {
        (void) is_xor; 
        return std::pair(
          x.first + y.first + 1, 
          std::max(x.second, y.second) + 1);
      },
      []( cost_t x, cost_t y ) { return x.second < y.second; },
      3, 2 ); 
}

TEST_CASE( "AIG costfn resynthesis -- depth optimization of balance", "[costfn_resyn]" )
{
  uint32_t num_var = 4;
  std::vector<kitty::partial_truth_table> tts( 4, kitty::partial_truth_table( 1 << num_var ) );
  kitty::partial_truth_table target( 1 << num_var );
  kitty::partial_truth_table care = ~target.construct();

  kitty::create_from_binary_string( target, "1000000000000000" ); // target = abcd
  kitty::create_from_binary_string( tts[0], "1111111100000000" ); // a
  kitty::create_from_binary_string( tts[1], "1111000011110000" ); // b
  kitty::create_from_binary_string( tts[2], "1100110011001100" ); // c
  kitty::create_from_binary_string( tts[3], "1010101010101010" ); // d

  std::vector<uint32_t> levels = { 0, 0, 0, 0 };
  using cost_t = std::pair<uint32_t, uint32_t>;
  using TT = kitty::partial_truth_table;
  xag_costfn_resyn_stats st;
  std::vector<uint32_t> divs;
  for ( auto i = 0u; i < tts.size(); ++i )
  {
    divs.emplace_back( i );
  }

  xag_costfn_resyn_solver<TT, aig_costfn_resyn_sparams_costfn<TT>> engine( st );
  const auto res = engine( target, care, divs.begin(), divs.end(), tts,
    [&]( auto n ) { (void)n; return std::pair( 0, levels[n] ); },
    []( cost_t x, cost_t y, bool is_xor = false ) {
      (void) is_xor; 
      return std::pair(
        x.first + y.first + 1, 
        std::max(x.second, y.second) + 1);
    },
    []( cost_t x, cost_t y ) { return x.second < y.second; },
    std::pair(std::numeric_limits<uint32_t>::max(), 
    std::numeric_limits<uint32_t>::max()) );

  CHECK( res );
  std::cout << to_index_list_string( *res ) << std::endl;
  aig_network aig;
  decode( aig, *res );
  depth_view daig{aig};
  CHECK( aig.num_gates() == 3 );
  // CHECK( daig.depth() == 2 ); // TODO: fix this mistake
}