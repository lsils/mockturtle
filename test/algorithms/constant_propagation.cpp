#include <catch.hpp>

#include <mockturtle/algorithms/constant_propagation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <unordered_map>

using namespace mockturtle;

TEST_CASE( "simplify network", "[constant_propagation]" )
{
  aig_network aig;
  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto f1 = aig.create_and( a, b );
  aig.create_po( f1 );

  CHECK( aig.size() == 4u );
  CHECK( aig.num_pis() == 2u );
  CHECK( aig.num_gates() == 1u );

  /* replace a with 1 */
  std::unordered_map<aig_network::node, bool> values;
  values.emplace( aig.pi_at( 0u ), true );
  const auto aig2 = constant_propagation( aig, values );
  CHECK( aig2.num_pis() == 2u );
  CHECK( aig2.num_gates() == 0u );
  CHECK( aig2.po_at( 0u ) == aig2.make_signal( aig2.pi_at( 0u ) ) );

  /* replace b with 1 */
  values.clear();
  values.emplace( aig.pi_at( 1u ), true );
  const auto aig3 = constant_propagation( aig, values );
  CHECK( aig3.num_pis() == 2u );
  CHECK( aig3.num_gates() == 0u );
  CHECK( aig3.po_at( 0u ) == aig2.make_signal( aig3.pi_at( 0u ) ) );

  /* replace a with 0 */
  values.clear();
  values.emplace( aig.pi_at( 0u ), false );
  const auto aig4 = constant_propagation( aig, values );
  CHECK( aig4.num_pis() == 2u );
  CHECK( aig4.num_gates() == 0u );
  CHECK( aig4.po_at( 0u ) == aig4.get_constant( false ) );

  /* replace b with 0 */
  values.clear();
  values.emplace( aig.pi_at( 1u ), false );
  const auto aig5 = constant_propagation( aig, values );
  CHECK( aig5.num_pis() == 2u );
  CHECK( aig5.num_gates() == 0u );
  CHECK( aig5.po_at( 0u ) == aig5.get_constant( false ) );
}
