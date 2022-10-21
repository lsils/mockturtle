#include <catch.hpp>

#include <algorithm>
#include <sstream>
#include <vector>

#include <mockturtle/io/write_blif.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/sequential.hpp>

using namespace mockturtle;

TEST_CASE( "write a k-LUT into BLIF file", "[write_blif]" )
{
  klut_network klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();

  const auto f1 = klut.create_or( a, b );
  klut.create_po( f1 );

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
}

TEST_CASE( "write a sequential k-LUT into BLIF file", "[write_blif]" )
{
  sequential<klut_network> klut;

  const auto a = klut.create_pi();
  const auto b = klut.create_pi();
  const auto c = klut.create_pi();

  const auto f1 = klut.create_or( a, b );
  const auto f_ = klut.create_and( a, b );
  klut.create_po( f_ ); // place-holder, POs should be defined first

  klut.create_ri( f1 );
  const auto f2 = klut.create_ro(); // f2 <- f1
  const auto f3 = klut.create_or( f2, c );
  klut.substitute_node( klut.get_node( f_ ), f3 );

  std::ostringstream out;
  write_blif_params ps;
  ps.skip_feedthrough = true;
  write_blif( klut, out );
  write_blif( klut, "test.blif" );

  /* additional spaces at the end of .inputs and .outputs */
  CHECK( out.str() == ".model top\n"
                      ".inputs pi2 pi3 pi4 \n"
                      ".outputs po0 \n"
                      ".latch li0 new_n7   3\n"
                      ".names new_n0\n"
                      "0\n"
                      ".names new_n1\n"
                      "1\n"
                      ".names new_n7 pi4 new_n8\n"
                      "-1 1\n"
                      "1- 1\n"
                      ".names pi2 pi3 new_n5\n"
                      "-1 1\n"
                      "1- 1\n"
                      ".names new_n8 po0\n"
                      "1 1\n"
                      ".names new_n5 li0\n"
                      "1 1\n"
                      ".end\n" );
}
