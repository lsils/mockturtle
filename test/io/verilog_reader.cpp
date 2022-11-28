#include <catch.hpp>

#include <sstream>
#include <string>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/buffered.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/muxig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>

#include <kitty/kitty.hpp>
#include <lorina/verilog.hpp>

using namespace mockturtle;

TEST_CASE( "read a VERILOG file into MIG network", "[verilog_reader]" )
{
  mig_network mig;

  std::string file{
      "module top( y1, y2, a, b, c ) ;\n"
      "  input a , b , c ;\n"
      "  output y1 , y2 ;\n"
      "  wire zero, g0, g1 , g2 , g3 , g4 ;\n"
      "  assign zero = 0 ;\n"
      "  assign g0 = a ;\n"
      "  assign g1 = ~c ;\n"
      "  assign g2 = g0 & g1 ;\n"
      "  assign g3 = a | g2 ;\n"
      "  assign g4 = ( ~a & b ) | ( ~a & c ) | ( b & c ) ;\n"
      "  assign g5 = g2 ^ g3 ^ g4;\n"
      "  assign g6 = ~( g4 & g5 );\n"
      "  assign y1 = g3 ;\n"
      "  assign y2 = g4 ;\n"
      "endmodule\n" };

  std::istringstream in( file );
  auto const result = lorina::read_verilog( in, verilog_reader( mig ) );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( mig.size() == 11 );
  CHECK( mig.num_pis() == 3 );
  CHECK( mig.num_pos() == 2 );
  CHECK( mig.num_gates() == 7 );

  /* functional checks */
  default_simulator<kitty::dynamic_truth_table> sim( mig.num_pis() );
  const auto tts = simulate<kitty::dynamic_truth_table>( mig, sim );
  mig.foreach_po( [&]( auto const&, auto i ) {
    switch ( i )
    {
    case 0:
      CHECK( kitty::to_hex( tts[i] ) == "aa" );
      break;
    case 1:
      CHECK( kitty::to_hex( tts[i] ) == "d4" );
      break;
    }
  } );
}

TEST_CASE( "read a VERILOG file into XMG network", "[verilog_reader]" )
{
  xmg_network xmg;

  std::string file{
      "module top( y1, y2, a, b, c ) ;\n"
      "  input a , b , c ;\n"
      "  output y1 , y2 ;\n"
      "  wire zero, g0, g1 , g2 , g3 , g4 ;\n"
      "  assign zero = 0 ;\n"
      "  assign g0 = a ;\n"
      "  assign g1 = ~c ;\n"
      "  assign g2 = g0 & g1 ;\n"
      "  assign g3 = a | g2 ;\n"
      "  assign g4 = ( ~a & b ) | ( ~a & c ) | ( b & c ) ;\n"
      "  assign g5 = g2 ^ g3 ^ g4;\n"
      "  assign g6 = ~( g4 & g5 );\n"
      "  assign y1 = g3 ;\n"
      "  assign y2 = g4 ;\n"
      "endmodule\n" };

  std::istringstream in( file );
  auto const result = lorina::read_verilog( in, verilog_reader( xmg ) );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( xmg.size() == 9 );
  CHECK( xmg.num_pis() == 3 );
  CHECK( xmg.num_pos() == 2 );
  CHECK( xmg.num_gates() == 5 );

  /* functional checks */
  default_simulator<kitty::dynamic_truth_table> sim( xmg.num_pis() );
  const auto tts = simulate<kitty::dynamic_truth_table>( xmg, sim );
  xmg.foreach_po( [&]( auto const&, auto i ) {
    switch ( i )
    {
    case 0:
      CHECK( kitty::to_hex( tts[i] ) == "aa" );
      break;
    case 1:
      CHECK( kitty::to_hex( tts[i] ) == "d4" );
      break;
    }
  } );
}

TEST_CASE( "read a VERILOG file into MuxIG network", "[verilog_reader]" )
{
  muxig_network ntk;

  std::string file{
      "module top( y1, a, b, c ) ;\n"
      "  input a , b , c ;\n"
      "  output y1 ;\n"
      "  wire zero, g1 , g2 , g3 , g4 ;\n"
      "  assign g1 = a & b ;\n"
      "  assign g2 = a | b ;\n"
      "  assign g3 = ~g2 ;\n"
      "  assign g4 = c ? g1 : g3 ;\n"
      "  assign y1 = g4 ;\n"
      "endmodule\n" };

  std::istringstream in( file );
  auto const result = lorina::read_verilog( in, verilog_reader( ntk ) );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( ntk.size() == 7 );
  CHECK( ntk.num_pis() == 3 );
  CHECK( ntk.num_pos() == 1 );
  CHECK( ntk.num_gates() == 3 );

  /* functional checks */
  default_simulator<kitty::dynamic_truth_table> sim( ntk.num_pis() );
  const auto tts = simulate<kitty::dynamic_truth_table>( ntk, sim );
  CHECK( kitty::to_hex( tts[0] ) == "81" );
}

TEST_CASE( "read a VERILOG file with instances", "[verilog_reader]" )
{
  mig_network mig;

  std::string file{
      "module ripple_carry_adder( x1, x2, y );\n"
      "  input x1, x2;\n"
      "  output y;\n"
      "endmodule\n"
      "module top( a, b, c );\n"
      "  input [7:0] a, b ;\n"
      "  output [8:0] c;\n"
      "  ripple_carry_adder #(8) add1(.x1(a), .x2(b), .y(c));\n"
      "endmodule\n" };

  std::istringstream in( file );
  const auto result = lorina::read_verilog( in, verilog_reader( mig ) );
  mig = cleanup_dangling( mig );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( mig.num_pis() == 16 );
  CHECK( mig.num_pos() == 9 );
  CHECK( mig.num_gates() == 32 );
}

TEST_CASE( "read a VERILOG file to create large Montgomery multiplier", "[verilog_reader]" )
{
  xag_network xag;

  std::string file{
      "module montgomery_multiplier( x1, x2, y );\n"
      "  input x1, x2;\n"
      "  output y;\n"
      "endmodule\n"
      "module top( a, b, c );\n"
      "  input [383:0] a, b;\n"
      "  output [383:0] c;\n"
      "  montgomery_multiplier #(384, 384'hfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff, 384'h14000000140000000c00000002fffffffcfffffffafffffffbfffffffe00000000000000010000000100000001) mult(.x1(a), .x2(b), .y(c));\n"
      "endmodule\n" };

  std::istringstream in( file );
  verilog_reader reader( xag );
  const auto result = lorina::read_verilog( in, reader );
  xag = cleanup_dangling( xag );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( xag.num_pis() == 768u );
  CHECK( xag.num_pos() == 384u );
  CHECK( xag.num_gates() == 909459u );

  /* name checks */
  CHECK( reader.name() == "top" );
  CHECK( reader.input_names() == std::vector<std::pair<std::string, uint32_t>>{ { { "a", 384 }, { "b", 384 } } } );
  CHECK( reader.output_names() == std::vector<std::pair<std::string, uint32_t>>{ { { "c", 384 } } } );
}

TEST_CASE( "read a VERILOG file with buffers", "[verilog_reader]" )
{
  buffered_mig_network mig;

  std::string file{
      "module buffer( i , o );\n"
      "  input i ;\n"
      "  output o ;\n"
      "endmodule\n"
      "module inverter( i , o );\n"
      "  input i ;\n"
      "  output o ;\n"
      "endmodule\n"
      "module top( x0 , x1 , y0 );\n"
      "  input x0 , x1 ;\n"
      "  output y0 ;\n"
      "  wire n3 , n4 , n5 , n6 ;\n"
      "  buffer  buf_n3( .i (x0), .o (n3) );\n"
      "  buffer  buf_n4( .i (n3), .o (n4) );\n"
      "  assign n5 = ~x1 & ~n4 ;\n"
      "  inverter  inv_n6( .i (n5), .o (n6) );\n"
      "  assign y0 = n6 ;\n"
      "endmodule\n" };

  std::istringstream in( file );
  const auto result = lorina::read_verilog( in, verilog_reader( mig ) );

  /* structural checks */
  CHECK( result == lorina::return_code::success );
  CHECK( mig.num_pis() == 2 );
  CHECK( mig.num_pos() == 1 );
  CHECK( mig.num_gates() == 1 );
  CHECK( mig.size() == 7 ); // 1 constant, 2 PIs, 1 gate, 3 buffers

  /* functional check */
  auto const po_values = simulate_buffered<2>( mig );
  CHECK( po_values[0]._bits == 0xe ); // or
}
