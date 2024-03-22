#include <catch.hpp>

#include <mockturtle/networks/klut.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/mph_view.hpp>

#include <map>
#include <vector>

using namespace mockturtle;

TEST_CASE( "mph_view functionalities", "[mph_view]" )
{
  klut_network ntk;
  mph_view<klut_network, 4> mph_ntk(ntk);

  // Creating primary inputs (PIs)
  const auto pi1 = mph_ntk.create_pi();
  const auto pi2 = mph_ntk.create_pi();
  const auto pi3 = mph_ntk.create_pi();

  // Check that the PI-s are assigned the type PI_GATE
  CHECK( mph_ntk.get_type(pi1) == PI_GATE );
  CHECK( mph_ntk.get_type(pi2) == PI_GATE );
  CHECK( mph_ntk.get_type(pi3) == PI_GATE );

  // Creating gates
  const auto gate1 = mph_ntk.create_and(pi1, pi2);
  const auto gate2 = mph_ntk.create_and(pi3, gate1);
  mph_ntk.create_po(gate2);


  // Setting types for nodes
  mph_ntk.set_type(  pi1, PI_GATE);
  mph_ntk.set_type(gate1, AS_GATE);
  mph_ntk.set_type(gate2, T1_GATE);

  // Test getting types
  CHECK( mph_ntk.get_type(  pi1) == PI_GATE );
  CHECK( mph_ntk.get_type(gate1) == AS_GATE );
  CHECK( mph_ntk.get_type(gate2) == T1_GATE );

  // Setting and getting stages, epochs, and phases
  mph_ntk.set_stage(gate1, 11);
  mph_ntk.set_stage_type(gate2, 20, SA_GATE);

  CHECK( mph_ntk.get_stage(gate1) == 11 );
  CHECK( std::get<0>(mph_ntk.get_stage_type(gate2)) == 20 );
  CHECK( std::get<1>(mph_ntk.get_stage_type(gate2)) == SA_GATE );

  // Test epoch and phase calculations
  CHECK( mph_ntk.get_epoch(gate1) == 2 ); // 11 / 4 phases
  CHECK( mph_ntk.get_phase(gate1) == 3 ); // 11 % 4 phases

  // Testing explicit_buffer functionality
  auto buffer_signal = mph_ntk.explicit_buffer(pi1, AS_GATE);

  CHECK( buffer_signal == mph_ntk.size() - 1 ); // Last gate should be the buffer

  // Check if the buffer is of the correct type
  CHECK( mph_ntk.get_type(buffer_signal) == AS_GATE ); // Assuming explicit_buffer sets it to AS_GATE by default
}