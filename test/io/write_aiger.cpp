#include <catch.hpp>

#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/write_aiger.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/sequential.hpp>
#include <mockturtle/views/names_view.hpp>

#include <lorina/aiger.hpp>

#include <sstream>
#include <string>

template<
    typename T,
    typename Traits = std::char_traits<T>,
    typename Container = std::vector<T>>
struct seq_buffer : std::basic_streambuf<T, Traits>
{
  using base_type = std::basic_streambuf<T, Traits>;
  using int_type = typename base_type::int_type;
  using traits_type = typename base_type::traits_type;

  virtual int_type overflow( int_type ch )
  {
    if ( traits_type::eq_int_type( ch, traits_type::eof() ) )
    {
      return traits_type::eof();
    }
    c.push_back( traits_type::to_char_type( ch ) );
    return ch;
  }

  Container const& data() const
  {
    return c;
  }

private:
  Container c;
};

using namespace mockturtle;

TEST_CASE( "write single-gate AIG into AIGER file", "[write_aiger]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();

  const auto f1 = aig.create_or( a, b );
  aig.create_po( f1 );

  seq_buffer<char> buffer;
  std::ostream os( &buffer );
  write_aiger( aig, os );

  CHECK( buffer.data() ==
         std::vector<char>{
             0x61, 0x69, 0x67, 0x20, // aig
             0x33, 0x20,             // M=3 (I+L+A)
             0x32, 0x20,             // I=2
             0x30, 0x20,             // L=0
             0x31, 0x20,             // O=1
             0x31, 0x0a,             // A=1
             0x37, 0x0a,             // 1 PO
             0x01, 0x02,             // 1 AND gate
             0x63                    // comment
         } );
}

TEST_CASE( "write AIG for XOR into AIGERfile", "[write_aiger]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();

  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( f2, f3 );
  aig.create_po( f4 );

  seq_buffer<char> buffer;
  std::ostream os( &buffer );
  write_aiger( aig, os );

  CHECK( buffer.data() ==
         std::vector<char>{
             0x61, 0x69, 0x67, 0x20, // aig
             0x36, 0x20,             // M=6 (I+L+A)
             0x32, 0x20,             // I=2
             0x30, 0x20,             // L=0
             0x31, 0x20,             // O=1
             0x34, 0x0a,             // A=4
             0x31, 0x33, 0x0a,       // 1 PO
             0x02, 0x02,             // 4 AND gates
             0x01, 0x05,
             0x03, 0x03,
             0x01, 0x02,
             0x63 // comment
         } );
}

TEST_CASE( "write sequential AIG into AIGER file", "[write_aiger]" )
{
  sequential<aig_network> aig;

  const auto a = aig.create_pi();  /* var 1 */
  const auto ro = aig.create_ro(); /* var 2 */

  const auto f1 = aig.create_and( a, ro ); /* var 3 */
  aig.create_po( f1 );
  aig.create_ri( f1 );

  seq_buffer<char> buffer;
  std::ostream os( &buffer );
  write_aiger( aig, os );

  CHECK( buffer.data() ==
         std::vector<char>{
             0x61, 0x69, 0x67, 0x20, // aig
             0x33, 0x20,             // M=3 (I+L+A)
             0x31, 0x20,             // I=1
             0x31, 0x20,             // L=1
             0x31, 0x20,             // O=1
             0x31, 0x0a,             // A=1
             0x36, 0x20, 0x34, 0x0a, // 1 latch, reset == own literal (undefined)
             0x36, 0x0a,             // 1 PO
             0x02, 0x02,             // 1 AND gate
             0x63                    // comment
         } );
}

TEST_CASE( "write sequential AIG with register initialization values", "[write_aiger]" )
{
  for ( uint8_t const init : { uint8_t( 0 ), uint8_t( 1 ) } )
  {
    sequential<aig_network> aig;

    const auto a = aig.create_pi();
    const auto ro = aig.create_ro();

    const auto f1 = aig.create_and( a, ro );
    aig.create_po( f1 );
    aig.create_ri( f1 );

    mockturtle::register_t reg;
    reg.init = init;
    aig.set_register( 0, reg );

    seq_buffer<char> buffer;
    std::ostream os( &buffer );
    write_aiger( aig, os );

    /* the latch line carries an explicit reset value: "6 <init>\n" */
    std::vector<char> expected{
        0x61, 0x69, 0x67, 0x20, // aig
        0x33, 0x20,             // M=3
        0x31, 0x20,             // I=1
        0x31, 0x20,             // L=1
        0x31, 0x20,             // O=1
        0x31, 0x0a,             // A=1
        0x36, 0x20              // latch next state, followed by reset value
    };
    expected.push_back( static_cast<char>( '0' + init ) );
    expected.insert( expected.end(), { 0x0a, 0x36, 0x0a, 0x02, 0x02, 0x63 } );

    CHECK( buffer.data() == expected );
  }
}

TEST_CASE( "write and read back a sequential AIG in AIGER format", "[write_aiger]" )
{
  sequential<aig_network> aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();
  const auto ro0 = aig.create_ro();
  const auto ro1 = aig.create_ro();

  const auto f1 = aig.create_and( a, ro0 );
  const auto f2 = aig.create_and( b, ro1 );
  const auto f3 = aig.create_and( f1, f2 );

  aig.create_po( f3 );
  aig.create_ri( f1 );
  aig.create_ri( f2 );

  mockturtle::register_t reg0;
  reg0.init = 0u;
  aig.set_register( 0, reg0 );
  mockturtle::register_t reg1;
  reg1.init = 1u;
  aig.set_register( 1, reg1 );

  std::ostringstream out;
  write_aiger( aig, out );

  sequential<aig_network> read_aig;
  std::istringstream in( out.str() );
  CHECK( lorina::read_aiger( in, aiger_reader( read_aig ) ) == lorina::return_code::success );

  CHECK( read_aig.num_pis() == aig.num_pis() );
  CHECK( read_aig.num_pos() == aig.num_pos() );
  CHECK( read_aig.num_registers() == aig.num_registers() );
  CHECK( read_aig.num_gates() == aig.num_gates() );
  CHECK( read_aig.register_at( 0 ).init == 0u );
  CHECK( read_aig.register_at( 1 ).init == 1u );
}

TEST_CASE( "write latch names into the AIGER symbol table", "[write_aiger]" )
{
  names_view<sequential<aig_network>> aig;

  const auto a = aig.create_pi();
  const auto ro = aig.create_ro();

  const auto f1 = aig.create_and( a, ro );
  aig.create_po( f1 );
  aig.create_ri( f1 );

  aig.set_name( a, "x0" );
  aig.set_name( ro, "state" );
  aig.set_output_name( 0, "y0" );

  std::ostringstream out;
  write_aiger( aig, out );

  const auto written = out.str();
  CHECK( written.find( "i0 x0\n" ) != std::string::npos );
  CHECK( written.find( "l0 state\n" ) != std::string::npos );
  CHECK( written.find( "o0 y0\n" ) != std::string::npos );

  /* the names must survive a round trip through the reader */
  names_view<sequential<aig_network>> read_aig;
  std::istringstream in( written );
  CHECK( lorina::read_aiger( in, aiger_reader( read_aig ) ) == lorina::return_code::success );

  CHECK( read_aig.get_name( read_aig.make_signal( read_aig.pi_at( 0 ) ) ) == "x0" );
  CHECK( read_aig.get_name( read_aig.make_signal( read_aig.ro_at( 0 ) ) ) == "state" );
  CHECK( read_aig.get_output_name( 0 ) == "y0" );
}

TEST_CASE( "write sequential AIG with an undefined register reset", "[write_aiger]" )
{
  sequential<aig_network> aig;

  const auto a = aig.create_pi();
  const auto ro = aig.create_ro();

  const auto f1 = aig.create_and( a, ro );
  aig.create_po( f1 );
  aig.create_ri( f1 );

  /* the register keeps its default (undefined) reset value */
  CHECK( aig.register_at( 0 ).init != 0u );
  CHECK( aig.register_at( 0 ).init != 1u );

  std::ostringstream out;
  write_aiger( aig, out );

  /* an undefined reset is encoded by repeating the latch's own literal, not by
     omitting the field -- an omitted field means 0 in the AIGER format */
  CHECK( out.str().find( "6 4\n" ) != std::string::npos );

  sequential<aig_network> read_aig;
  std::istringstream in( out.str() );
  CHECK( lorina::read_aiger( in, aiger_reader( read_aig ) ) == lorina::return_code::success );

  CHECK( read_aig.num_registers() == 1u );
  CHECK( read_aig.register_at( 0 ).init != 0u );
  CHECK( read_aig.register_at( 0 ).init != 1u );
}
