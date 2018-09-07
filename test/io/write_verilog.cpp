#include <catch.hpp>

#include <sstream>

#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>

using namespace mockturtle;

TEST_CASE( "write single-gate AIG into Verilog file", "[write_verilog]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();

  const auto f1 = aig.create_or( a, b );
  aig.create_po( f1 );

  std::ostringstream out;
  write_verilog( aig, out );

  CHECK( out.str() == "module top(x0, x1, y0);\n"
                      "  input x0, x1;\n"
                      "  output y0;\n"
                      "  assign n3 = ~x0 & ~x1;\n"
                      "  assign y0 = ~n3;\n"
                      "endmodule\n" );
}

TEST_CASE( "write AIG for XOR into Verilog file", "[write_verilog]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();

  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  std::ostringstream out;
  write_verilog( aig, out );

  CHECK( out.str() == "module top(x0, x1, y0);\n"
                      "  input x0, x1;\n"
                      "  output y0;\n"
                      "  assign n3 = x0 & x1;\n"
                      "  assign n4 = x0 & ~n3;\n"
                      "  assign n5 = x1 & ~n3;\n"
                      "  assign n6 = ~n4 & ~n5;\n"
                      "  assign y0 = ~n6;\n"
                      "endmodule\n" );
}

TEST_CASE( "write MIG into Verilog file", "[write_verilog]" )
{
  mig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto c = aig.create_pi();

  const auto f1 = aig.create_and( a, b );
  const auto f2 = aig.create_or( a, b );
  const auto f3 = aig.create_maj( f1, f2, c );
  aig.create_po( f3 );

  std::ostringstream out;
  write_verilog( aig, out );

  CHECK( out.str() == "module top(x0, x1, x2, y0);\n"
                      "  input x0, x1, x2;\n"
                      "  output y0;\n"
                      "  assign n4 = x0 & x1;\n"
                      "  assign n5 = x0 | x1;\n"
                      "  assign n6 = (x2 & n4) | (x2 & n5) | (n4 & n5);\n"
                      "  assign y0 = n6;\n"
                      "endmodule\n" );
}
