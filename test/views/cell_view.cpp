#include <catch.hpp>

#include <sstream>

#include <lorina/genlib.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/networks/block.hpp>
#include <mockturtle/utils/standard_cell.hpp>
#include <mockturtle/views/cell_view.hpp>

using namespace mockturtle;

std::string const simple_library = "GATE zero 0 O=CONST0;\n"
                                   "GATE one 0 O=CONST1;\n"
                                   "GATE inverter 1 O=!a; PIN * INV 1 999 1.0 1.0 1.0 1.0\n"
                                   "GATE buffer 2 O=a; PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                   "GATE and 5 O=a*b; PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                   "GATE or 5 O=a+b; PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                   "GATE ha 7 O=!(a*b); PIN * INV 1 999 1.0 1.0 1.0 1.0\n"
                                   "GATE ha 7 O=!a*!b+a*b; PIN * INV 1 999 2.0 1.0 2.0 1.0\n";

TEST_CASE( "Create cell view", "[cell_view]" )
{
  std::vector<gate> gates;

  std::istringstream in( simple_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );

  CHECK( result == lorina::return_code::success );

  std::vector<standard_cell> cells = get_standard_cells( gates );

  CHECK( cells.size() == 7 );

  cell_view<block_network> ntk( cells );

  auto const a = ntk.create_pi();
  auto const b = ntk.create_pi();
  auto const c = ntk.create_pi();
  auto const d = ntk.create_pi();

  auto const c0 = ntk.get_constant( false );
  auto const t1 = ntk.create_and( a, b );
  auto const t2 = ntk.create_or( c, d );
  auto const t3 = ntk.create_hai( t1, d );
  auto const f = ntk.create_and( t1, t2 );
  auto const g = ntk.create_not( a );

  ntk.create_po( t3 );
  ntk.create_po( ntk.next_output_pin( t3 ) );
  ntk.create_po( f );
  ntk.create_po( g );
  ntk.create_po( ntk.get_constant() );

  ntk.add_cell( ntk.get_node( c0 ), 0 );
  ntk.add_cell( ntk.get_node( t1 ), 4 );
  ntk.add_cell( ntk.get_node( t2 ), 5 );
  ntk.add_cell( ntk.get_node( t3 ), 6 );
  ntk.add_cell( ntk.get_node( f ), 4 );
  ntk.add_cell( ntk.get_node( g ), 2 );

  CHECK( ntk.has_cell( ntk.get_node( a ) ) == false );
  CHECK( ntk.has_cell( ntk.get_node( b ) ) == false );
  CHECK( ntk.has_cell( ntk.get_node( c ) ) == false );
  CHECK( ntk.has_cell( ntk.get_node( d ) ) == false );
  CHECK( ntk.has_cell( ntk.get_node( c0 ) ) == true );
  CHECK( ntk.has_cell( ntk.get_node( t1 ) ) == true );
  CHECK( ntk.has_cell( ntk.get_node( t2 ) ) == true );
  CHECK( ntk.has_cell( ntk.get_node( t3 ) ) == true );
  CHECK( ntk.has_cell( ntk.get_node( f ) ) == true );
  CHECK( ntk.has_cell( ntk.get_node( g ) ) == true );

  CHECK( ntk.get_cell_index( ntk.get_node( c0 ) ) == 0 );
  CHECK( ntk.get_cell_index( ntk.get_node( t1 ) ) == 4 );
  CHECK( ntk.get_cell_index( ntk.get_node( t2 ) ) == 5 );
  CHECK( ntk.get_cell_index( ntk.get_node( t3 ) ) == 6 );
  CHECK( ntk.get_cell_index( ntk.get_node( f ) ) == 4 );
  CHECK( ntk.get_cell_index( ntk.get_node( g ) ) == 2 );

  CHECK( ntk.get_cell( ntk.get_node( c0 ) ).name == "zero" );
  CHECK( ntk.get_cell( ntk.get_node( t1 ) ).name == "and" );
  CHECK( ntk.get_cell( ntk.get_node( t2 ) ).name == "or" );
  CHECK( ntk.get_cell( ntk.get_node( t3 ) ).name == "ha" );
  CHECK( ntk.get_cell( ntk.get_node( f ) ).name == "and" );
  CHECK( ntk.get_cell( ntk.get_node( g ) ).name == "inverter" );

  CHECK( ntk.get_cell( ntk.get_node( c0 ) ).gates.size() == 1 );
  CHECK( ntk.get_cell( ntk.get_node( t1 ) ).gates.size() == 1 );
  CHECK( ntk.get_cell( ntk.get_node( t2 ) ).gates.size() == 1 );
  CHECK( ntk.get_cell( ntk.get_node( t3 ) ).gates.size() == 2 );
  CHECK( ntk.get_cell( ntk.get_node( f ) ).gates.size() == 1 );
  CHECK( ntk.get_cell( ntk.get_node( g ) ).gates.size() == 1 );

  CHECK( ntk.compute_area() == 23 );
  CHECK( ntk.compute_worst_delay() == 3 );

  std::stringstream report_stats;
  ntk.report_stats( report_stats );
  CHECK( report_stats.str() == "[i] Report stats: area = 23.00; delay =  3.00;\n" );

  std::stringstream report_gates;
  ntk.report_cells_usage( report_gates );
  CHECK( report_gates.str() == "[i] Report cells usage:\n"
                               "[i] zero                     \t Instance =          1\t Area =         0.00     0.00 %\n"
                               "[i] inverter                 \t Instance =          1\t Area =         1.00     4.35 %\n"
                               "[i] and                      \t Instance =          2\t Area =        10.00    43.48 %\n"
                               "[i] or                       \t Instance =          1\t Area =         5.00    21.74 %\n"
                               "[i] ha                       \t Instance =          1\t Area =         7.00    30.43 %\n"
                               "[i] TOTAL                    \t Instance =          6\t Area =        23.00   100.00 %\n" );
}

TEST_CASE( "Cell view on copy", "[cell_view]" )
{
  std::vector<gate> gates;

  std::istringstream in( simple_library );
  auto result = lorina::read_genlib( in, genlib_reader( gates ) );

  CHECK( result == lorina::return_code::success );

  std::vector<standard_cell> cells = get_standard_cells( gates );

  CHECK( cells.size() == 7 );

  cell_view<block_network> ntk( cells );

  auto const a = ntk.create_pi();
  auto const b = ntk.create_pi();
  auto const c = ntk.create_pi();
  auto const d = ntk.create_pi();

  auto const c0 = ntk.get_constant( false );
  auto const t1 = ntk.create_and( a, b );
  auto const t2 = ntk.create_or( c, d );
  auto const t3 = ntk.create_hai( t1, d );
  auto const f = ntk.create_and( t1, t2 );
  auto const g = ntk.create_not( a );

  ntk.create_po( t3 );
  ntk.create_po( ntk.next_output_pin( t3 ) );
  ntk.create_po( f );
  ntk.create_po( g );
  ntk.create_po( ntk.get_constant() );

  ntk.add_cell( ntk.get_node( c0 ), 0 );
  ntk.add_cell( ntk.get_node( t1 ), 4 );
  ntk.add_cell( ntk.get_node( t2 ), 5 );
  ntk.add_cell( ntk.get_node( t3 ), 6 );
  ntk.add_cell( ntk.get_node( f ), 4 );
  ntk.add_cell( ntk.get_node( g ), 2 );

  cell_view<block_network> ntk_copy = ntk;

  CHECK( ntk_copy.has_cell( ntk_copy.get_node( a ) ) == false );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( b ) ) == false );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( c ) ) == false );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( d ) ) == false );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( c0 ) ) == true );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( t1 ) ) == true );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( t2 ) ) == true );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( t3 ) ) == true );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( f ) ) == true );
  CHECK( ntk_copy.has_cell( ntk_copy.get_node( g ) ) == true );

  CHECK( ntk_copy.get_cell_index( ntk_copy.get_node( c0 ) ) == 0 );
  CHECK( ntk_copy.get_cell_index( ntk_copy.get_node( t1 ) ) == 4 );
  CHECK( ntk_copy.get_cell_index( ntk_copy.get_node( t2 ) ) == 5 );
  CHECK( ntk_copy.get_cell_index( ntk_copy.get_node( t3 ) ) == 6 );
  CHECK( ntk_copy.get_cell_index( ntk_copy.get_node( f ) ) == 4 );
  CHECK( ntk_copy.get_cell_index( ntk_copy.get_node( g ) ) == 2 );

  CHECK( ntk_copy.get_cell( ntk_copy.get_node( c0 ) ).name == "zero" );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( t1 ) ).name == "and" );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( t2 ) ).name == "or" );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( t3 ) ).name == "ha" );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( f ) ).name == "and" );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( g ) ).name == "inverter" );

  CHECK( ntk_copy.get_cell( ntk_copy.get_node( c0 ) ).gates.size() == 1 );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( t1 ) ).gates.size() == 1 );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( t2 ) ).gates.size() == 1 );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( t3 ) ).gates.size() == 2 );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( f ) ).gates.size() == 1 );
  CHECK( ntk_copy.get_cell( ntk_copy.get_node( g ) ).gates.size() == 1 );

  CHECK( ntk_copy.compute_area() == 23 );
  CHECK( ntk_copy.compute_worst_delay() == 3 );

  std::stringstream report_stats;
  ntk_copy.report_stats( report_stats );
  CHECK( report_stats.str() == "[i] Report stats: area = 23.00; delay =  3.00;\n" );

  std::stringstream report_gates;
  ntk_copy.report_cells_usage( report_gates );
  CHECK( report_gates.str() == "[i] Report cells usage:\n"
                               "[i] zero                     \t Instance =          1\t Area =         0.00     0.00 %\n"
                               "[i] inverter                 \t Instance =          1\t Area =         1.00     4.35 %\n"
                               "[i] and                      \t Instance =          2\t Area =        10.00    43.48 %\n"
                               "[i] or                       \t Instance =          1\t Area =         5.00    21.74 %\n"
                               "[i] ha                       \t Instance =          1\t Area =         7.00    30.43 %\n"
                               "[i] TOTAL                    \t Instance =          6\t Area =        23.00   100.00 %\n" );
}
