/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file network_fuzz_tester.hpp
  \brief Network fuzz tester

  \author Heinz Riener
*/

#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <lorina/lorina.hpp>
#include <fmt/format.h>
#include <optional>

namespace mockturtle
{

struct fuzz_tester_params
{
  uint64_t num_pis{4u};
  uint64_t num_gates{10u};
  enum 
  {
    verilog,
    aiger
  } file_format = verilog;
  std::string filename{"fuzz_test.v"};

  /* number of networks to test: nullopt means infinity */
  std::optional<uint64_t> num_iterations{std::nullopt};

  uint64_t num_pis_step{1u};
  uint64_t num_gates_step{10u};
  uint64_t num_pis_max{10u};
  uint64_t num_gates_max{100u};
  uint64_t num_iterations_step{100u};
}; /* fuzz_tester_params */

/*! \brief Network fuzz tester
 *
 * Runs an algorithm on many small ramdon logic networks.  Fuzz
 * testing is often useful to detect potential segmentation faults in
 * new implementations.  The generated benchmarks are saved first in a
 * file.  If a segmentation fault occurs, the file can be used to
 * reproduce and debug the problem.
 *
  \verbatim embed:rst

   Usage

   .. code-block:: c++

     #include <mockturtle/algorithms/aig_resub.hpp>
     #include <mockturtle/algorithms/cleanup.hpp>
     #include <mockturtle/algorithms/network_fuzz_tester.hpp>
     #include <mockturtle/algorithms/resubstitution.hpp>
     #include <mockturtle/generators/random_logic_generator.hpp>

     auto opt = [&]( aig_network aig ) -> bool {
       resubstitution_params ps;
       resubstitution_stats st;
       aig_resubstitution( aig, ps, &st );
       aig = cleanup_dangling( aig );
       return true;
     };

     fuzz_tester_params ps;
     ps.num_iterations = 100;
     auto gen = default_random_aig_generator();
     network_fuzz_tester fuzzer( gen, ps );
     fuzzer.run( opt );
   \endverbatim
*/
template<class NetworkGenerator>
class network_fuzz_tester
{
public:
  explicit network_fuzz_tester( NetworkGenerator& gen, fuzz_tester_params const ps = {} )
    : gen( gen )
    , ps( ps )
  {}

  template<typename Fn>
  void run_incremental( Fn&& fn )
  {
    uint64_t counter{0};
    uint64_t num_pis = ps.num_pis;
    uint64_t num_gates = ps.num_gates;
    uint64_t counter_step{0};
    while ( ( !ps.num_iterations || counter < ps.num_iterations ) && num_pis <= ps.num_pis_max && num_gates <= ps.num_gates_max )
    {
      auto ntk = gen.generate( num_pis, num_gates, std::random_device{}() );
      fmt::print( "[i] create network #{}: I/O = {}/{} gates = {} nodes = {}\n",
                  counter++, ntk.num_pis(), ntk.num_pos(), ntk.num_gates(), ntk.size() );

      fmt::print( "[i] write network `{}`\n", ps.filename );

      switch ( ps.file_format )
      {
        case fuzz_tester_params::verilog:
          write_verilog( ntk, ps.filename );
          break;
        case fuzz_tester_params::aiger:
          write_aiger( ntk, ps.filename );
          break;
        default:
          fmt::print( "[w] unsupported format\n" );
      }

      /* run optimization algorithm */
      if ( !fn( ntk ) )
      {
        return;
      }

      if ( ++counter_step >= ps.num_iterations_step )
      {
        counter_step = 0;
        num_gates += ps.num_gates_step;
        if ( num_gates > ps.num_gates_max )
        {
          num_gates = ps.num_gates;
          num_pis += ps.num_pis_step;
        }
      }
    }
  }

  template<typename Fn>
  void run( Fn&& fn )
  {
    uint64_t counter{0};
    while ( !ps.num_iterations || counter < ps.num_iterations )
    {
      auto ntk = gen.generate( ps.num_pis, ps.num_gates, std::random_device{}() );
      fmt::print( "[i] create network #{}: I/O = {}/{} gates = {} nodes = {}\n",
                  counter++, ntk.num_pis(), ntk.num_pos(), ntk.num_gates(), ntk.size() );

      fmt::print( "[i] write network `{}`\n", ps.filename );

      switch ( ps.file_format )
      {
        case fuzz_tester_params::verilog:
          write_verilog( ntk, ps.filename );
          break;
        case fuzz_tester_params::aiger:
          write_aiger( ntk, ps.filename );
          break;
        default:
          fmt::print( "[w] unsupported format\n" );
      }

      /* run optimization algorithm */
      if ( !fn( ntk ) )
      {
        return;
      }
    }
  }

  template<typename Ntk, typename Fn>
  void rerun_on_benchmark( Fn&& fn )
  {
    /* read benchmark from a file */
    Ntk ntk;
    fmt::print( "[i] read network `{}`\n", ps.filename );
    if ( lorina::read_verilog( ps.filename, verilog_reader( ntk ) ) != lorina::return_code::success )
    {
      fmt::print( "[e] could not read benchmark `{}`\n", ps.filename );
      return;
    }

    fmt::print( "[i] network: I/O = {}/{} gates = {} nodes = {}\n",
                ntk.num_pis(), ntk.num_pos(), ntk.num_gates(), ntk.size() );

    /* run optimization algorithm */
    fn( ntk );
  }

private:
  NetworkGenerator& gen;
  fuzz_tester_params const ps;
}; /* network_fuzz_tester */

} /* namespace mockturtle */