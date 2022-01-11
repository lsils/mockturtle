#include <catch.hpp>
#include <vector>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>
#include <kitty/static_truth_table.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/algorithms/factoring.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>

using namespace mockturtle;

template<class Ntk>
void test_factoring_trivial_network()
{
  Ntk ntk;
  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();
  ntk.create_po( ntk.create_and( a, b ) );

  const auto [c_a0, c_a1] = factoring( ntk, a );
  CHECK(c_a0.num_gates() == 0);
  CHECK(c_a1.num_gates() == 0);
  auto func = simulate<kitty::static_truth_table<2u>>( ntk )[0];
  CHECK( simulate<kitty::static_truth_table<2u>>( c_a0 )[0] == kitty::cofactor0( func, 0 ));
  CHECK( simulate<kitty::static_truth_table<2u>>( c_a1 )[0] == kitty::cofactor1( func, 0 ));
}

template<class Ntk>
void test_factoring_network()
{
  Ntk ntk;
  const auto a = ntk.create_pi();
  const auto b = ntk.create_pi();

  const auto f1 = ntk.create_nand( a, b );
  const auto f2 = ntk.create_nand( a, f1 );
  const auto f3 = ntk.create_nand( b, f1 );
  ntk.create_po( ntk.create_nand( f2, f3 ) );

  const auto [c_a0, c_a1] = factoring( ntk, a );
  auto func = simulate<kitty::static_truth_table<2u>>( ntk )[0];
  CHECK( simulate<kitty::static_truth_table<2u>>( c_a0 )[0] == kitty::cofactor0( func, 0 ));
  CHECK( simulate<kitty::static_truth_table<2u>>( c_a1 )[0] == kitty::cofactor1( func, 0 ));
}

template<class Ntk>
void test_factoring_network_with_n_vars(uint32_t num_vars)
{
  Ntk ntk;
  uint32_t width = 3;
  std::vector<typename Ntk::signal> a( width ), b( width );
  std::generate( a.begin(), a.end(), [&ntk]() { return ntk.create_pi(); } );
  std::generate( b.begin(), b.end(), [&ntk]() { return ntk.create_pi(); } );
  auto carry = ntk.get_constant( false );

  carry_ripple_adder_inplace( ntk, a, b, carry );

  std::for_each( a.begin(), a.end(), [&]( auto f ) { ntk.create_po( f ); } );
  ntk.create_po( carry );

  const auto [cubes, networks] = factoring( ntk, num_vars );
  CHECK( cubes.size() ==  1 << num_vars );
  CHECK( networks.size() ==  1 << num_vars );

  auto func = simulate<kitty::static_truth_table<6u>>( ntk );
  std::vector<std::vector<kitty::static_truth_table<6u>>> factored_tt;
  for ( auto network : networks )
  {
    factored_tt.push_back( simulate<kitty::static_truth_table<6u>>( network ) );
  }

  for ( uint32_t i = 0; i < networks.size(); ++i )
  {
    auto const& network = networks.at(i);
    auto const sim_func = simulate<kitty::static_truth_table<6u>>( network );
    for ( uint32_t j = 0; j < func.size(); ++j ) 
    {
      auto f = func.at(j);
      for ( auto pi: cubes.at( i ) )
      {
        auto pi_idx = network.pi_index( network.get_node( pi ) );
        if ( network.is_complemented( pi ) )
          f = kitty::cofactor0( f, pi_idx );
        else 
          f = kitty::cofactor1( f, pi_idx );
      }
      CHECK( sim_func[j] == f );
    }
  }
}

TEST_CASE( "Factoring trivial networks", "[factoring]" )
{
  test_factoring_trivial_network<aig_network>();
  test_factoring_trivial_network<xag_network>();
  test_factoring_trivial_network<mig_network>();
  test_factoring_trivial_network<xmg_network>();
}

TEST_CASE( "Factoring networks", "[factoring]" )
{
  test_factoring_network<aig_network>();
  test_factoring_network<xag_network>();
  test_factoring_network<mig_network>();
  test_factoring_network<xmg_network>();
}

TEST_CASE( "Factoring n vars from networks", "[factoring]" )
{
  test_factoring_network_with_n_vars<aig_network>(4);
  test_factoring_network_with_n_vars<xag_network>(4);
  test_factoring_network_with_n_vars<mig_network>(4);
  test_factoring_network_with_n_vars<xmg_network>(4);
}
