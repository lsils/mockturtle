#include <catch.hpp>
#include <kitty/kitty.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/cost_view.hpp>
#include <mockturtle/algorithms/experimental/cost_resyn.hpp>

using namespace mockturtle;
using namespace mockturtle::experimental;

TEST_CASE( "cost-generic resynthesis (1-resub MC cost)", "[cost_generic_resyn]" )
{
  xag_network xag;
  cost_view<xag_network, xag_multiplicative_complexity_cost_function<xag_network>> cost_xag( xag, xag_multiplicative_complexity_cost_function<xag_network>() );

  using ViedNtk = decltype( cost_xag );

  using signal = xag_network::signal;

  const signal a = cost_xag.create_pi();
  const signal b = cost_xag.create_pi();
  const signal c = cost_xag.create_pi();
  
  const signal divisor1 = cost_xag.create_and( a, b );
  const signal divisor2 = cost_xag.create_and( a, c );
  const signal divisor3 = cost_xag.create_and( divisor1, divisor2 );

  signal f = cost_xag.create_or( divisor1, divisor2 );

  /**
   * a: 11110000
   * b: 11001100
   * c: 10101010
   * 
   * divisor1: 11000000
   * divisor2: 10100000
   * divisor3: 10000000
   * 
   * f: 11100000
   * 
   * f = (a & b) | (a & c)
   * 
   * or, f = a & (b | c)
   * also, f = a ^ (b ^ c)
  */

  cost_resyn_params ps;
  cost_resyn_stats st;

  cost_resyn<ViedNtk, kitty::dynamic_truth_table> resyn( cost_xag, ps, st );

  std::vector<signal> leaves{ a, b, c };
  std::vector<signal> divs{ a, b, c, divisor1, divisor2, divisor3 };
  std::vector<signal> mffcs{ f };

  CHECK( cost_xag.get_cost( xag.get_node( f ), divs ) == 1 );

  const auto res = resyn( leaves, divs, mffcs, f );

  CHECK( res );
  CHECK( ( *res ).num_gates() == 5u ); /* we count the size of the dependency taking leaves as inputs */
}