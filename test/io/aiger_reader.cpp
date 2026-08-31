#include <catch.hpp>

#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/sequential.hpp>
#include <mockturtle/views/names_view.hpp>

#include <lorina/aiger.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace mockturtle;

TEST_CASE( "read and write names", "[aiger_reader]" )
{
  sequential<aig_network> aig;
  names_view<sequential<aig_network>> named_aig{ aig };

  std::string file{ "aag 7 2 1 2 4\n"
                    "2\n"
                    "4\n"
                    "6 8\n"
                    "6\n"
                    "7\n"
                    "8 2 6\n"
                    "10 3 7\n"
                    "12 9 11\n"
                    "14 4 12\n"
                    "i0 x0\n"
                    "i1 x1\n"
                    "l0 s0\n"
                    "o0 y0\n"
                    "o1 y1\n" };

  std::istringstream in( file );
  auto const result = lorina::read_ascii_aiger( in, aiger_reader( named_aig ) );
  CHECK( result == lorina::return_code::success );

  CHECK( named_aig.get_name( aig.make_signal( aig.pi_at( 0 ) ) ) == "x0" );
  CHECK( named_aig.get_name( aig.make_signal( aig.pi_at( 1 ) ) ) == "x1" );
  CHECK( named_aig.get_name( aig.make_signal( aig.ro_at( 0 ) ) ) == "s0" );
  CHECK( named_aig.get_name( aig.ri_at( 0 ) ) == "s0_next" );
  CHECK( named_aig.get_output_name( 0 ) == "y0" );
  CHECK( named_aig.get_output_name( 1 ) == "y1" );
}

TEST_CASE( "out-of-bounds index", "[aiger_reader]" )
{
  aig_network aig;

  std::string file{ "aag 3 2 0 1 1\n"
                    "2\n"
                    "33\n"
                    "7\n"
                    "6 3 5\n" };

  std::istringstream in( file );
  auto const result = lorina::read_ascii_aiger( in, aiger_reader( aig ) );
  CHECK( result == lorina::return_code::parse_error );
}

TEST_CASE( "negative index", "[aiger_reader]" )
{
  aig_network aig;

  std::string file{ "aag 3 2 0 1 1\n"
                    "-1\n"
                    "3\n"
                    "7\n"
                    "6 3 5\n" };

  std::istringstream in( file );
  auto const result = lorina::read_ascii_aiger( in, aiger_reader( aig ) );
  CHECK( result == lorina::return_code::parse_error );
}

TEST_CASE( "read an ASCII Aiger file into an AIG network and store input-output names", "[aiger_reader]" )
{
  aig_network aig;
  names_view<aig_network> named_aig{ aig };

  std::string file{ "aag 6 2 0 1 4\n"
                    "2\n"
                    "4\n"
                    "13\n"
                    "6 2 4\n"
                    "8 2 7\n"
                    "10 4 7\n"
                    "12 9 11\n"
                    "i0 foo\n"
                    "i1 bar\n"
                    "o0 foobar\n" };

  std::istringstream in( file );
  auto const result = lorina::read_ascii_aiger( in, aiger_reader( named_aig ) );
  CHECK( result == lorina::return_code::success );
  CHECK( named_aig.size() == 7 );
  CHECK( named_aig.num_pis() == 2 );
  CHECK( named_aig.num_pos() == 1 );
  CHECK( named_aig.num_gates() == 4 );

  CHECK( named_aig.get_name( aig.make_signal( aig.pi_at( 0 ) ) ) == "foo" );
  CHECK( named_aig.get_name( aig.make_signal( aig.pi_at( 1 ) ) ) == "bar" );
  CHECK( named_aig.get_output_name( 0 ) == "foobar" );
}

TEST_CASE( "read a sequential ASCII Aiger file into an AIG network", "[aiger_reader]" )
{
  sequential<aig_network> aig;
  names_view<sequential<aig_network>> named_aig{ aig };

  std::string file{ "aag 7 2 1 2 4\n"
                    "2\n"
                    "4\n"
                    "6 8\n"
                    "6\n"
                    "7\n"
                    "8 2 6\n"
                    "10 3 7\n"
                    "12 9 11\n"
                    "14 4 12\n"
                    "i0 foo\n"
                    "i1 bar\n"
                    "l0 barfoo\n"
                    "o0 foobar\n"
                    "o1 barbar\n" };

  lorina::text_diagnostics consumer;
  lorina::diagnostic_engine diag( &consumer );
  std::istringstream in( file );
  auto const result = lorina::read_ascii_aiger( in, aiger_reader( named_aig ), &diag );
  CHECK( result == lorina::return_code::success );
  CHECK( named_aig.size() == 8 );
  CHECK( named_aig.num_cis() == 3 );
  CHECK( named_aig.num_cos() == 3 );
  CHECK( named_aig.num_pis() == 2 );
  CHECK( named_aig.num_pos() == 2 );
  CHECK( named_aig.num_gates() == 4 );
  CHECK( named_aig.num_registers() == 1 );

  CHECK( named_aig.get_name( aig.make_signal( aig.pi_at( 0 ) ) ) == "foo" );
  CHECK( named_aig.get_name( aig.make_signal( aig.pi_at( 1 ) ) ) == "bar" );
  CHECK( named_aig.get_name( aig.make_signal( aig.ro_at( 0 ) ) ) == "barfoo" );
  CHECK( named_aig.get_name( aig.ri_at( 0 ) ) == "barfoo_next" );
  CHECK( named_aig.get_output_name( 0 ) == "foobar" );
  CHECK( named_aig.get_output_name( 1 ) == "barbar" );
}

TEST_CASE( "read latches with omitted reset values", "[aiger_reader]" )
{
  /* An omitted reset value means 0. The original AIGER format initialized every
     latch to zero and the reset field added in 1.9 is optional, so a writer that
     leaves it out -- ABC does -- must not be read as an undefined value. */
  sequential<aig_network> aig;

  std::string file{ "aag 3 1 1 1 1\n"
                    "2\n"
                    "4 6\n"
                    "6\n"
                    "6 2 4\n" };

  std::istringstream in( file );
  CHECK( lorina::read_ascii_aiger( in, aiger_reader( aig ) ) == lorina::return_code::success );

  CHECK( aig.num_registers() == 1u );
  CHECK( aig.register_at( 0 ).init == 0u );
}

TEST_CASE( "read latches with explicit reset values", "[aiger_reader]" )
{
  for ( auto const& [token, expected] : std::vector<std::pair<std::string, uint8_t>>{ { "0", 0u }, { "1", 1u } } )
  {
    sequential<aig_network> aig;

    std::string file{ "aag 3 1 1 1 1\n"
                      "2\n"
                      "4 6 " +
                      token + "\n"
                              "6\n"
                              "6 2 4\n" };

    std::istringstream in( file );
    CHECK( lorina::read_ascii_aiger( in, aiger_reader( aig ) ) == lorina::return_code::success );

    CHECK( aig.num_registers() == 1u );
    CHECK( aig.register_at( 0 ).init == expected );
  }
}

TEST_CASE( "read a latch with an undefined reset value", "[aiger_reader]" )
{
  /* a reset value that is neither 0 nor 1 -- by convention the latch's own
     literal -- denotes an undefined initial value */
  sequential<aig_network> aig;

  std::string file{ "aag 3 1 1 1 1\n"
                    "2\n"
                    "4 6 4\n"
                    "6\n"
                    "6 2 4\n" };

  std::istringstream in( file );
  CHECK( lorina::read_ascii_aiger( in, aiger_reader( aig ) ) == lorina::return_code::success );

  CHECK( aig.num_registers() == 1u );
  CHECK( aig.register_at( 0 ).init != 0u );
  CHECK( aig.register_at( 0 ).init != 1u );
}

TEST_CASE( "read bad state properties as primary outputs", "[aiger_reader]" )
{
  /* Writers emitting the AIGER 1.9 extended header move the primary outputs into
     the bad-state section; ABC does so for any design with a non-zero latch
     initialization. Dropping them would silently discard logic. */
  sequential<aig_network> aig;

  std::string file{ "aag 3 1 1 0 1 1\n"
                    "2\n"
                    "4 6 1\n"
                    "6\n"
                    "6 2 4\n" };

  std::istringstream in( file );
  CHECK( lorina::read_ascii_aiger( in, aiger_reader( aig ) ) == lorina::return_code::success );

  CHECK( aig.num_pis() == 1u );
  CHECK( aig.num_registers() == 1u );
  CHECK( aig.num_pos() == 1u );
  CHECK( aig.register_at( 0 ).init == 1u );
}

TEST_CASE( "read output names when only some outputs are named", "[aiger_reader]" )
{
  /* the symbol table is sparse, so a name must land on the output it belongs to */
  names_view<aig_network> aig;

  std::string file{ "aag 3 2 0 2 1\n"
                    "2\n"
                    "4\n"
                    "2\n"
                    "6\n"
                    "6 2 4\n"
                    "o1 second\n" };

  std::istringstream in( file );
  CHECK( lorina::read_ascii_aiger( in, aiger_reader( aig ) ) == lorina::return_code::success );

  CHECK( aig.num_pos() == 2u );
  CHECK( aig.get_output_name( 1 ) == "second" );
}

TEST_CASE( "register initialization stays valid across formats", "[aiger_reader]" )
{
  /* `write_blif` emits the initialization verbatim, and the BLIF `.latch`
     statement accepts only the four values of `register_init`. A reader that
     produced anything else would silently write a malformed BLIF file. */
  for ( auto const& [latch_line, expected] :
        std::vector<std::pair<std::string, uint8_t>>{
            { "4 6", register_init::zero }, /* omitted reset means 0 */
            { "4 6 0", register_init::zero },
            { "4 6 1", register_init::one },
            { "4 6 4", register_init::dont_care } /* self-literal: undefined */ } )
  {
    sequential<aig_network> aig;

    std::string const file = "aag 3 1 1 1 1\n2\n" + latch_line + "\n6\n6 2 4\n";
    std::istringstream in( file );
    CHECK( lorina::read_ascii_aiger( in, aiger_reader( aig ) ) == lorina::return_code::success );

    REQUIRE( aig.num_registers() == 1u );
    CHECK( aig.register_at( 0 ).init == expected );
    CHECK( aig.register_at( 0 ).init <= register_init::unknown );
    CHECK( register_init::is_defined( aig.register_at( 0 ).init ) == ( expected <= register_init::one ) );
  }
}

TEST_CASE( "read a latched Aiger file into a combinational network", "[aiger_reader]" )
{
  /* a network that cannot hold registers flattens one timeframe of the design:
     the latch outputs become primary inputs and their next-state functions
     become primary outputs.  The latch outputs used to be skipped entirely,
     which left the reader's signal vector short of every literal above the
     primary inputs and made `on_and` read past its end. */
  aig_network combinational;
  sequential<aig_network> seq;

  std::string const file{ "aag 7 2 1 2 4\n"
                          "2\n"
                          "4\n"
                          "6 8\n"
                          "6\n"
                          "7\n"
                          "8 2 6\n"
                          "10 3 7\n"
                          "12 9 11\n"
                          "14 4 12\n" };

  std::stringstream err;
  auto* old_err = std::cerr.rdbuf( err.rdbuf() );

  std::istringstream in( file );
  CHECK( lorina::read_ascii_aiger( in, aiger_reader( combinational ) ) == lorina::return_code::success );

  std::cerr.rdbuf( old_err );

  std::istringstream seq_in( file );
  CHECK( lorina::read_ascii_aiger( seq_in, aiger_reader( seq ) ) == lorina::return_code::success );

  /* the latch output is an input and its next-state function an output */
  CHECK( combinational.num_pis() == 3 );
  CHECK( combinational.num_pos() == 3 );

  /* no logic is gained or lost by flattening */
  CHECK( combinational.num_gates() == seq.num_gates() );
  CHECK( combinational.size() == seq.size() );
  CHECK( combinational.num_cis() == seq.num_cis() );
  CHECK( combinational.num_cos() == seq.num_cos() );
  CHECK( err.str().find( "applying comb: ROs become PIs, RIs become POs" ) != std::string::npos );
}

TEST_CASE( "read a latched Aiger file with no primary inputs", "[aiger_reader]" )
{
  /* a design driven only by its registers -- an LFSR, say -- reaches every one
     of its literals through a latch output, so it flattens into a network whose
     inputs are all former latch outputs */
  aig_network combinational;

  std::string const file{ "aag 3 0 2 1 1\n"
                          "2 6\n"
                          "4 2\n"
                          "6\n"
                          "6 2 4\n" };

  std::stringstream err;
  auto* old_err = std::cerr.rdbuf( err.rdbuf() );

  std::istringstream in( file );
  CHECK( lorina::read_ascii_aiger( in, aiger_reader( combinational ) ) == lorina::return_code::success );

  std::cerr.rdbuf( old_err );

  CHECK( combinational.num_pis() == 2 );
  CHECK( combinational.num_pos() == 3 );
  CHECK( combinational.num_gates() == 1 );
  CHECK( err.str().find( "applying comb: ROs become PIs, RIs become POs" ) != std::string::npos );
}

TEST_CASE( "name a flattened latch output", "[aiger_reader]" )
{
  /* the symbol table still applies: a latch name belongs to the input that
     stands in for it, and its next-state function keeps the `_next` suffix */
  names_view<aig_network> aig;

  std::string const file{ "aag 7 2 1 2 4\n"
                          "2\n"
                          "4\n"
                          "6 8\n"
                          "6\n"
                          "7\n"
                          "8 2 6\n"
                          "10 3 7\n"
                          "12 9 11\n"
                          "14 4 12\n"
                          "i0 x0\n"
                          "l0 s0\n" };

  std::stringstream err;
  auto* old_err = std::cerr.rdbuf( err.rdbuf() );

  std::istringstream in( file );
  CHECK( lorina::read_ascii_aiger( in, aiger_reader( aig ) ) == lorina::return_code::success );

  std::cerr.rdbuf( old_err );

  CHECK( aig.get_name( aig.make_signal( aig.pi_at( 0 ) ) ) == "x0" );
  CHECK( aig.get_name( aig.make_signal( aig.pi_at( 2 ) ) ) == "s0" );
  CHECK( err.str().find( "applying comb: ROs become PIs, RIs become POs" ) != std::string::npos );
}
