#include <catch.hpp>

#include <mockturtle/algorithms/cover.hpp>
#include <mockturtle/networks/aig.hpp>

TEST_CASE( "Solve cover greedily", "[cover]" )
{
  using namespace mockturtle;

  kitty::partial_truth_table d0{6}, d1{6}, d2{6}, d3{6}, target{6};
  d0.add_bit( 1 );
  d0.add_bit( 1 );
  d0.add_bit( 1 );
  d0.add_bit( 1 );
  d0.add_bit( 0 );
  d0.add_bit( 0 );

  d1.add_bit( 1 );
  d1.add_bit( 0 );
  d1.add_bit( 0 );
  d1.add_bit( 1 );
  d1.add_bit( 1 );
  d1.add_bit( 0 );

  d2.add_bit( 0 );
  d2.add_bit( 0 );
  d2.add_bit( 1 );
  d2.add_bit( 1 );
  d2.add_bit( 1 );
  d2.add_bit( 1 );

  d3.add_bit( 0 );
  d3.add_bit( 0 );
  d3.add_bit( 0 );
  d3.add_bit( 1 );
  d3.add_bit( 0 );
  d3.add_bit( 1 );

  target.add_bit( 1 );
  target.add_bit( 0 );
  target.add_bit( 1 );
  target.add_bit( 1 );
  target.add_bit( 0 );
  target.add_bit( 1 );

  /* target = d1 ^ d2 ^ d3 and d0 is an additional divisor to distract the solver */
  divisor_cover cov( target );
  cov.add_divisor( d0 );
  cov.add_divisor( d1 );
  cov.add_divisor( d2 );
  cov.add_divisor( d3 );

  greedy_covering_solver greedy;

  uint32_t solution_counter = 0u;
  cov.solve( greedy, [&]( std::vector<uint32_t> cover ){
      std::sort( std::begin( cover ), std::end( cover ) );
      switch ( solution_counter )
      {
      case 0u:
        CHECK( cover == std::vector{ 0u, 1u, 2u } );
        break;
      default:
        CHECK( false );
        break;
      }
      ++solution_counter;
    });

  CHECK( solution_counter == 1u );
}
