#include <catch.hpp>

#include <algorithm>
#include <sstream>
#include <vector>

#include <lorina/blif.hpp>
#include <mockturtle/views/names_view.hpp>
#include <mockturtle/io/write_blif.hpp>
#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/sequential.hpp>

using namespace mockturtle;

template<class Ntk>
void blif_read_after_write_test( const Ntk& to_write )
{
  std::ostringstream out;
  write_blif_params ps;
  ps.skip_feedthrough = true;
  write_blif( to_write, out, ps );

  Ntk to_read;
  std::istringstream in( out.str() );
  auto const ret = lorina::read_blif( in, blif_reader( to_read ) );

  CHECK( ret == lorina::return_code::success );

  CHECK( to_read.size() == to_write.size() ); 
  CHECK( to_read.num_pis() == to_write.num_pis() ); 
  CHECK( to_read.num_pos() == to_write.num_pos() ); 
  CHECK( to_read.num_gates() == to_write.num_gates() );

  if constexpr ( has_num_registers_v<Ntk> )
  {
    CHECK( to_read.num_registers() == to_write.num_registers() ); 
  }
}

TEST_CASE( "write a simple combinational k-LUT into BLIF file", "[write_blif]" )
{
  klut_network klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();

  const auto f1 = klut.create_or( a, b );
  klut.create_po( f1 );

  CHECK( klut.num_gates() == 1 );
  CHECK( klut.num_pis() == 2 );
  CHECK( klut.num_pos() == 1 );

  std::ostringstream out;
  write_blif( klut, out );

  /* additional spaces at the end of .inputs and .outputs */
  CHECK( out.str() == ".model top\n"
                      ".inputs pi2 pi3 \n"
                      ".outputs po0 \n"
                      ".names new_n0\n"
                      "0\n"
                      ".names new_n1\n"
                      "1\n"
                      ".names pi2 pi3 new_n4\n"
                      "-1 1\n"
                      "1- 1\n"
                      ".names new_n4 po0\n"
                      "1 1\n"
                      ".end\n" );

  blif_read_after_write_test( klut );
}

TEST_CASE( "write a simple sequential k-LUT into BLIF file", "[write_blif]" )
{
  sequential<klut_network> klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();

  const auto f1 = klut.create_or( a, b );
  const auto f2 = klut.create_ro(); // f2 <- f1
  const auto f3 = klut.create_or( f2, c );

  klut.create_po( f3 );
  klut.create_ri( f1 ); // f2 <- f1

  CHECK( klut.num_gates() == 2 );
  CHECK( klut.num_registers() == 1 );
  CHECK( klut.num_pis() == 3 );
  CHECK( klut.num_pos() == 1 );

  std::ostringstream out;
  write_blif( klut, out );

  /* additional spaces at the end of .inputs and .outputs */
  CHECK( out.str() == ".model top\n"
                      ".inputs pi2 pi3 pi4 \n"
                      ".outputs po0 \n"
                      ".latch li0 new_n6   3\n"
                      ".names new_n0\n"
                      "0\n"
                      ".names new_n1\n"
                      "1\n"
                      ".names new_n6 pi4 new_n7\n"
                      "-1 1\n"
                      "1- 1\n"
                      ".names pi2 pi3 new_n5\n"
                      "-1 1\n"
                      "1- 1\n"
                      ".names new_n7 po0\n"
                      "1 1\n"
                      ".names new_n5 li0\n"
                      "1 1\n"
                      ".end\n" );

  blif_read_after_write_test( klut );
}

TEST_CASE( "write a sequential k-LUT with multiple fanout registers into BLIF file", "[write_blif]" )
{
  sequential<klut_network> klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();

  const auto f1 = klut.create_maj( a, b, c );
  const auto f2 = klut.create_ro(); // f2 <- f1
  const auto f3 = klut.create_ro(); // f3 <- f1
  const auto f4 = klut.create_xor( f2, f3 );

  klut.create_po( f4 );
  klut.create_ri( f1 ); // f2 <- f1
  klut.create_ri( f1 ); // f3 <- f1

  CHECK( klut.num_gates() == 2 );
  CHECK( klut.num_registers() == 2 );
  CHECK( klut.num_pis() == 3 );
  CHECK( klut.num_pos() == 1 );

  std::ostringstream out;
  write_blif( klut, out );

  /* additional spaces at the end of .inputs and .outputs */
  CHECK( out.str() == ".model top\n"
                      ".inputs pi2 pi3 pi4 \n"
                      ".outputs po0 \n"
                      ".latch li0 new_n6   3\n"
                      ".latch li1 new_n7   3\n"
                      ".names new_n0\n"
                      "0\n"
                      ".names new_n1\n"
                      "1\n"
                      ".names new_n6 new_n7 new_n8\n"
                      "10 1\n"
                      "01 1\n"
                      ".names pi2 pi3 pi4 new_n5\n"
                      "-11 1\n"
                      "1-1 1\n"
                      "11- 1\n"
                      ".names new_n8 po0\n"
                      "1 1\n"
                      ".names new_n5 li0\n"
                      "1 1\n"
                      ".names new_n5 li1\n"
                      "1 1\n"
                      ".end\n" );

  blif_read_after_write_test( klut );
}

TEST_CASE( "write a sequential k-LUT with name view", "[write_blif]" )
{
  names_view<sequential<klut_network>> klut;

  const auto a = klut.create_pi( "a" );
  const auto b = klut.create_pi( "b" );
  const auto c = klut.create_pi( "c" );

  const auto f1 = klut.create_maj( a, b, c );
  const auto f2 = klut.create_ro(); // f2 <- f1
  const auto f3 = klut.create_ro(); // f3 <- f1
  const auto f4 = klut.create_xor( f2, f3 );
  
  klut.set_name( f1, "f1" );
  klut.set_name( f2, "f2" );
  klut.set_name( f3, "f3" );
  klut.set_name( f4, "f4" );

  klut.create_po( f4, "output" );
  klut.create_ri( f1 ); // f2 <- f1
  klut.create_ri( f1 ); // f3 <- f1

  CHECK( klut.num_gates() == 2 );
  CHECK( klut.num_registers() == 2 );
  CHECK( klut.num_pis() == 3 );
  CHECK( klut.num_pos() == 1 );

  std::ostringstream out;
  write_blif( klut, out );
  write_blif( klut, "test.blif" );

  /* additional spaces at the end of .inputs and .outputs */
  CHECK( out.str() == ".model top\n"
                      ".inputs a b c \n"
                      ".outputs output \n"
                      ".latch li0 f2   3\n"
                      ".latch li1 f3   3\n"
                      ".names new_n0\n"
                      "0\n"
                      ".names new_n1\n"
                      "1\n"
                      ".names f2 f3 f4\n"
                      "10 1\n"
                      "01 1\n"
                      ".names a b c f1\n"
                      "-11 1\n"
                      "1-1 1\n"
                      "11- 1\n"
                      ".names f4 output\n"
                      "1 1\n"
                      ".names f1 li0\n"
                      "1 1\n"
                      ".names f1 li1\n"
                      "1 1\n"
                      ".end\n" );

  blif_read_after_write_test( klut );
}

TEST_CASE( "write a sequential k-LUT with name view and skip feed through", "[write_blif]" )
{
  names_view<sequential<klut_network>> klut;

  const auto a = klut.create_pi( "a" );
  const auto b = klut.create_pi( "b" );
  const auto c = klut.create_pi( "c" );

  const auto f1 = klut.create_maj( a, b, c );
  const auto f2 = klut.create_ro(); // f2 <- f1
  const auto f3 = klut.create_ro(); // f3 <- f1
  const auto f4 = klut.create_xor( f2, f3 );
  
  klut.set_name( f1, "maj(a,b,c)" );
  klut.set_name( f2, "dff1" );
  klut.set_name( f3, "dff2" );
  klut.set_name( f4, "xor(dff1,dff2)" );

  klut.create_po( f4, "output" );
  klut.create_ri( f1 ); // f2 <- f1
  klut.create_ri( f1 ); // f3 <- f1

  CHECK( klut.num_gates() == 2 );
  CHECK( klut.num_registers() == 2 );
  CHECK( klut.num_pis() == 3 );
  CHECK( klut.num_pos() == 1 );

  std::ostringstream out;
  write_blif_params ps;
  ps.skip_feedthrough = true;
  write_blif( klut, out, ps );
  write_blif( klut, "test.blif" );

  /* additional spaces at the end of .inputs and .outputs */
  CHECK( out.str() == ".model top\n"
                      ".inputs a b c \n"
                      ".outputs output \n"
                      ".latch maj(a,b,c) dff1   3\n"
                      ".latch maj(a,b,c) dff2   3\n"
                      ".names new_n0\n"
                      "0\n"
                      ".names new_n1\n"
                      "1\n"
                      ".names dff1 dff2 xor(dff1,dff2)\n"
                      "10 1\n"
                      "01 1\n"
                      ".names a b c maj(a,b,c)\n"
                      "-11 1\n"
                      "1-1 1\n"
                      "11- 1\n"
                      ".end\n" );

  blif_read_after_write_test( klut );
}