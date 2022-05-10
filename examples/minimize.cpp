/* This example code demonstrates how to use mockturtle's
 * testcase minimizer to minimize bug-triggering testcases.
 */

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/algorithms/testcase_minimizer.hpp>
#include <mockturtle/utils/debugging_utils.hpp>
#include <mockturtle/views/color_view.hpp>

using namespace mockturtle;

int main( int argc, char* argv[] )
{
  if( argc != 2 )
  {
    std::cout << "Please give exactly one argument, which is the filename of the initial testcase (without extension)\n";
    std::cout << "For example: ./minimize fuzz\n";
    return -1;
  }

  /* Use this lambda function for debugging mockturtle algorithms */
  auto opt = []( aig_network aig ) -> bool {
    resubstitution_params ps;
    ps.max_pis = 100;
    ps.max_inserts = 20u;

    sim_resubstitution( aig, ps );
    return !network_is_acyclic( color_view{aig} ); // return true if buggy
  };

  /* Use this lambda function for debugging external tools or algorithms that segfaults */
  /* Note that this version is not supported on Windows platform */
  auto make_command = []( std::string const& filename ) -> std::string {
    return "abc -c \"read " + filename + "; rewrite\"";
  };

  testcase_minimizer_params ps;
  ps.file_format = testcase_minimizer_params::aiger;
  ps.path = "."; // current directory
  ps.init_case = std::string( argv[1] );
  ps.minimized_case = ps.init_case + "_minimized";
  ps.max_size = 0;
  ps.verbose = true;
  testcase_minimizer<aig_network> minimizer( ps );
  minimizer.run( opt );
  //minimizer.run( make_command );

  return 0;
}