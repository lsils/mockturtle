#include <catch.hpp>

#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/traits.hpp>

using namespace mockturtle;

TEST_CASE( "Resubstitution of MAJ", "[resubstitution]" )
{
  mig_network mig;
  const auto a = mig.create_pi();
  const auto b = mig.create_pi();
  const auto c = mig.create_pi();

  const auto f = mig.create_maj( a, mig.create_maj( a, b, c ), c );
  mig.create_po( f );

  resubstitution( mig );

  mig = cleanup_dangling( mig );

  CHECK( mig.size() == 5 );
  CHECK( mig.num_pis() == 3 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 1 );
}
