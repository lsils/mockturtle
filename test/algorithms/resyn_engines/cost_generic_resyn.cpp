#include <catch.hpp>
#include <kitty/kitty.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/experimental/cost_resyn.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/index_list.hpp>
#include <mockturtle/views/cost_view.hpp>

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

  // std::cout << to_index_list_string( *res );
  // "{3, 1, 5, 2, 4, 2, 6, 8, 10, 12, 8, 14, 10, 16}",
  // but the optimal solution is not unique
  // so let us skip this check

  CHECK( res );
  CHECK( ( *res ).num_gates() == 5u ); /* we count the size of the dependency taking leaves as inputs */
}

TEST_CASE( "cost-generic resynthesis (1-resub MC cost) with XNOR", "[cost_generic_resyn]" )
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

  signal f = !cost_xag.create_or( divisor1, divisor2 );

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

  // std::cout << to_index_list_string( *res );
  // "{3, 1, 5, 2, 4, 2, 6, 8, 10, 12, 8, 14, 10, 17}",
  // but the optimal solution is not unique
  // so let us skip this check

  CHECK( res );
  CHECK( ( *res ).num_gates() == 5u ); /* we count the size of the dependency taking leaves as inputs */
}

TEST_CASE( "cost-generic resynthesis (1-resub size cost)", "[cost_generic_resyn]" )
{
  xag_network xag;
  cost_view<xag_network, xag_size_cost_function<xag_network>> cost_xag( xag, xag_size_cost_function<xag_network>() );

  using ViedNtk = decltype( cost_xag );

  using signal = xag_network::signal;

  const signal a = cost_xag.create_pi();
  const signal b = cost_xag.create_pi();
  const signal c = cost_xag.create_pi();

  const signal divisor1 = cost_xag.create_and( a, b );
  const signal divisor2 = cost_xag.create_and( a, c );
  const signal divisor3 = cost_xag.create_and( divisor1, divisor2 );

  signal f1 = cost_xag.create_xor( divisor1, divisor2 );
  signal f2 = cost_xag.create_xor( divisor3, f1 );

  /**
   * a: 11110000
   * b: 11001100
   * c: 10101010
   * 
   * divisor1: 11000000
   * divisor2: 10100000
   * divisor3: 10000000
   * 
   * f1: 01100000
   * f2: 11100000
   * 
   * f2 = (a & b) | (a & c)
   * 
   * or, f2 = a & (b | c)
   * also, f2 = a ^ (b ^ c)
  */

  cost_resyn_params ps;
  cost_resyn_stats st;

  cost_resyn<ViedNtk, kitty::dynamic_truth_table> resyn( cost_xag, ps, st );

  std::vector<signal> leaves{ a, b, c };
  std::vector<signal> divs{ a, b, c, divisor1, divisor2, divisor3 };
  std::vector<signal> mffcs{ f1, f2 };

  CHECK( cost_xag.get_cost( xag.get_node( f2 ), divs ) == 2 ); /* initially there are 2 XORs in the mffc */

  const auto res = resyn( leaves, divs, mffcs, f2 );

  // std::cout << to_index_list_string( *res );
  // "{3, 1, 3, 2, 4, 2, 6, 9, 11, 13}",
  // but the optimal solution is not unique
  // so let us skip this check

  CHECK( res );
  CHECK( ( *res ).num_gates() == 3u ); /* we count the size of the dependency taking leaves as inputs */
}

TEST_CASE( "cost-generic resynthesis (2-resub size cost)", "[cost_generic_resyn]" )
{
  xag_network xag;
  cost_view<xag_network, xag_size_cost_function<xag_network>> cost_xag( xag, xag_size_cost_function<xag_network>() );

  using ViedNtk = decltype( cost_xag );

  using signal = xag_network::signal;

  const signal a = cost_xag.create_pi();
  const signal b = cost_xag.create_pi();
  const signal c = cost_xag.create_pi();

  const signal divisor1 = cost_xag.create_and( a, b );
  const signal divisor2 = cost_xag.create_and( a, c );
  const signal divisor3 = cost_xag.create_and( divisor1, divisor2 );

  signal f1 = cost_xag.create_xor( divisor1, divisor2 );
  signal f2 = cost_xag.create_xor( divisor3, f1 );

  /**
   * a: 11110000
   * b: 11001100
   * c: 10101010
   * 
   * divisor1: 11000000
   * divisor2: 10100000
   * divisor3: 10000000
   * 
   * f1: 01100000
   * f2: 11100000
   * 
   * f2 = (a & b) | (a & c)
   * 
   * or, f2 = a & (b | c)
   * also, f2 = a ^ (b ^ c)
  */

  cost_resyn_params ps;
  cost_resyn_stats st;

  cost_resyn<ViedNtk, kitty::dynamic_truth_table> resyn( cost_xag, ps, st );

  std::vector<signal> leaves{ a, b, c };
  std::vector<signal> divs{ a, b, c };
  std::vector<signal> mffcs{ divisor1, divisor2, divisor3, f1, f2 };

  CHECK( cost_xag.get_cost( xag.get_node( f2 ), divs ) == 5 ); /* initially there are 2 XORs in the mffc */

  const auto res = resyn( leaves, divs, mffcs, f2 );

  // std::cout << to_index_list_string( *res );
  // "{3, 1, 2, 5, 7, 2, 9, 10}",
  // but the optimal solution is not unique
  // so let us skip this check

  CHECK( res );
  CHECK( ( *res ).num_gates() == 2u ); /* we count the size of the dependency taking leaves as inputs */
}

TEST_CASE( "cost-generic resynthesis with XOR", "[cost_generic_resyn]" )
{
  xag_network xag;
  cost_view<xag_network, xag_multiplicative_complexity_cost_function<xag_network>> cost_xag( xag, xag_multiplicative_complexity_cost_function<xag_network>() );

  using ViedNtk = decltype( cost_xag );

  using signal = xag_network::signal;

  signal pi0 = cost_xag.create_pi();
  signal pi1 = cost_xag.create_pi();
  signal pi2 = cost_xag.create_pi();

  signal n8 = cost_xag.create_and( !pi0, !pi1 );
  signal n9 = cost_xag.create_and( pi0, !pi1 );
  signal n10 = cost_xag.create_and( n9, !pi2 );
  signal n11 = cost_xag.create_and( pi1, !pi0 );
  signal n12 = !cost_xag.create_and( !n11, !pi0 );
  signal n13 = cost_xag.create_and( !n10, !n8 );

  /**
   * pi0: 11110000
   * pi1: 11001100
   * pi2: 10101010
   * 
   * n8: 00000011
   * n9: 00110000
   * n10: 01000000
   * n11: 00001100
   * n12: 11001100
   * n13: 10111100
  */

  cost_resyn_params ps;
  cost_resyn_stats st;

  cost_resyn<ViedNtk, kitty::dynamic_truth_table> resyn( cost_xag, ps, st );

  std::vector<signal> leaves{ pi0, pi1, pi2 };
  std::vector<signal> divs{ pi0, pi1, pi2 };
  std::vector<signal> mffcs{ n8, n9, n10, n11, n12, n13 };

  signal root = n13;
  xag.create_po( root );

  const auto res = resyn( leaves, divs, mffcs, root );

  xag_network xag_new;
  decode( xag_new, *res );

  CHECK( res );
  CHECK( *equivalence_checking( *miter<xag_network>( xag_new, xag ) ) == true );
}

TEST_CASE( "cost-generic resynthesis with negated leaves", "[cost_generic_resyn]" )
{
  xag_network xag;
  cost_view<xag_network, xag_multiplicative_complexity_cost_function<xag_network>> cost_xag( xag, xag_multiplicative_complexity_cost_function<xag_network>() );

  using ViedNtk = decltype( cost_xag );

  using signal = xag_network::signal;

  signal pi0 = cost_xag.create_pi();
  signal pi1 = cost_xag.create_pi();
  signal pi2 = cost_xag.create_pi();

  signal n9 = cost_xag.create_and( pi0, !pi1 );
  signal n10 = cost_xag.create_xor( n9, pi1 );
  signal n11 = cost_xag.create_and( n9, !pi2 );
  signal n12 = cost_xag.create_and( !n11, n10 );

  cost_resyn_params ps;
  cost_resyn_stats st;

  cost_resyn<ViedNtk, kitty::dynamic_truth_table> resyn( cost_xag, ps, st );

  std::vector<signal> leaves{ pi0, pi1, pi2 };
  std::vector<signal> divs{ pi0, pi1, pi2, n9, n10, n11 };
  std::vector<signal> mffcs{ n12 };

  signal root = n12;
  xag.create_po( root );

  const auto res = resyn( leaves, divs, mffcs, root );

  // std::cout << to_index_list_string( *res );
  // "{3, 1, 4, 2, 5, 7, 8, 8, 4, 12, 10, 14}",
  // but the optimal solution is not unique
  // so let us skip this check

  xag_network xag_new;
  decode( xag_new, *res );

  CHECK( res );
  CHECK( *equivalence_checking( *miter<xag_network>( xag_new, xag ) ) == true );
}
