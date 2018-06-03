#if 0
#include <catch.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/networks/klut.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>

using namespace mockturtle;

TEST_CASE( "Cut rewriting of bad XOR", "[cut_rewriting]" )
{
  klut_network klut;
  const auto a = klut.create_pi();
  const auto b = klut.create_pi();

  kitty::dynamic_truth_table _nand( 2u );
  kitty::create_from_binary_string( _nand, "0001" );

  const auto f1 = klut.create_node( {a, b}, _nand );
  const auto f2 = klut.create_node( {a, f1}, _nand );
  const auto f3 = klut.create_node( {b, f1}, _nand );
  const auto f4 = klut.create_node( {f2, f3}, _nand );

  klut.create_po( f4 );

  cut_rewriting( klut );
}
#endif