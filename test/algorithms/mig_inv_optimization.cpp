#include <catch.hpp>

#include <mockturtle/algorithms/mig_algebraic_rewriting.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/fanout_view.hpp>

using namespace mockturtle;

namespace mockturtle
{
namespace inv_opt_test
{
template<typename Ntk>
int number_of_inverted( Ntk const& ntk )
{
  int num_inverted{ 0 };
  ntk.foreach_gate( [&]( auto const& n ) {
    ntk.foreach_fanin( n, [&]( auto const& f ) {
      if ( ntk.is_complemented( f ) )
      {
        num_inverted++;
      }
    } );
  } );
  ntk.foreach_po( [&]( auto const& f ) {
    if ( ntk.is_complemented( f ) )
    {
      num_inverted++;
    }
  } );
  return num_inverted;
}
} // namespace inv_opt_test
} // namespace mockturtle

TEST_CASE( "MIG inverter optimization basic", "[mig_algebraic_rewriting]" )
{
  mig_network mig;

  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();
  const auto d = mig.create_pi();

  const auto f1 = mig.create_maj( !a, b, c );
  const auto f2 = mig.create_maj( !a, b, d );
  const auto f3 = mig.create_maj( a, !f1, f2 );
  const auto f4 = mig.create_maj( a, !f1, b );

  mig.create_po( f3 );
  mig.create_po( f4 );

  fanout_view fanout_mig{ mig };

  mig_inv_optimization( fanout_mig );
}

TEST_CASE( "MIG inverter optimization constant input 0", "[mig_algebraic_rewriting]" )
{
  mig_network mig;

  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();

  const auto f1 = mig.create_maj( !a, b, mig.get_constant( 0 ) );
  const auto f2 = mig.create_maj( !a, b, c );
  const auto f3 = mig.create_maj( a, !f1, f2 );

  mig.create_po( f3 );

  fanout_view fanout_mig{ mig };

  mig_inv_optimization( fanout_mig );
}
TEST_CASE( "MIG inverter optimization constant input 1", "[mig_algebraic_rewriting]" )
{
  mig_network mig;

  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();

  const auto f1 = mig.create_maj( a, b, mig.get_constant( 1 ) );
  const auto f2 = mig.create_maj( !a, b, c );
  const auto f3 = mig.create_maj( a, !f1, f2 );
  const auto f4 = mig.create_maj( a, !f1, c );

  mig.create_po( f3 );
  mig.create_po( f4 );

  fanout_view fanout_mig{ mig };

  mig_inv_optimization( fanout_mig );
}
TEST_CASE( "MIG inverter optimization output", "[mig_algebraic_rewriting]" )
{
  mig_network mig;

  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();
  const auto d = mig.create_pi();

  const auto f1 = mig.create_maj( !a, b, c );
  const auto f2 = mig.create_maj( !a, b, d );
  const auto f3 = mig.create_maj( a, !f1, f2 );

  mig.create_po( f3 );
  mig.create_po( !f1 );

  fanout_view fanout_mig{ mig };

  mig_inv_optimization( fanout_mig );
}