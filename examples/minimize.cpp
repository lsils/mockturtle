/* This example code demonstrates how to use mockturtle's
 * testcase minimizer to minimize bug-triggering testcases.
 */

#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/utils/debugging_utils.hpp>
#include <mockturtle/views/color_view.hpp>

// algorithm under test
#include <mockturtle/algorithms/aig_resub.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>

// minimizer
#include <mockturtle/algorithms/testcase_minimizer.hpp>

using namespace mockturtle;

int main( int argc, char* argv[] )
{
  if ( argc != 2 )
  {
    std::cout << "Please give exactly one argument, which is the filename of the initial testcase (without extension)\n";
    std::cout << "For example: ./minimize fuzz\n";
    return -1;
  }

  /* Use this lambda function for debugging mockturtle algorithms */
  auto opt = [&]( aig_network aig ) -> bool {
    aig_network const aig_copy = aig.clone();

    resubstitution_params ps;
    ps.max_pis = 8u;
    ps.max_inserts = 5u;
    aig_resubstitution( aig, ps );
    aig = cleanup_dangling( aig );

    return *equivalence_checking( *miter<aig_network>( aig_copy, aig ) ); // true: normal (or not the expected bug); false: buggy
  };

  /* Uncomment to use the following lambda function for debugging external tools or algorithms that segfaults */
  /* Note that this version is not supported on Windows platform */
  // auto make_command = []( std::string const& filename ) -> std::string {
  //   return "abc -c \"read " + filename + "; rewrite\"";
  // };

  testcase_minimizer_params ps;
  ps.file_format = testcase_minimizer_params::aiger;
  ps.path = "."; // current directory
  ps.init_case = std::string( argv[1] );
  ps.minimized_case = ps.init_case + "_minimized";
  ps.verbose = true;
  testcase_minimizer<aig_network> minimizer( ps );
  minimizer.run( opt );
  // minimizer.run( make_command );

  return 0;
}