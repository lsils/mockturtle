/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
  \author Siang-Yun Lee
*/

#include "../io/write_verilog.hpp"
#include "../io/write_aiger.hpp"
#include "../io/verilog_reader.hpp"
#include "../io/aiger_reader.hpp"

#include <lorina/lorina.hpp>
#include <fmt/format.h>
#include <optional>
#include <array>
#include <cstdio>

namespace mockturtle
{

/*! \brief Parameters for testcase_minimizer. */
struct fuzz_tester_params
{
  /*! \brief File format to be generated. */
  enum
  {
    verilog,
    aiger
  } file_format = verilog;

  /*! \brief Name of the generated testcase file. */
  std::string filename{"fuzz_test.v"};

  /*! \brief Filename written out by the command (to do CEC with the input testcase). */
  std::optional<std::string> outputfile{std::nullopt};

  /*! \brief Max number of networks to test: nullopt means infinity. */
  std::optional<uint64_t> num_iterations{std::nullopt};

  /*! \brief Number of networks to test before increasing size. */
  uint64_t num_iterations_step{100u};

  /*! \brief Number of PIs to start with. */
  uint64_t num_pis{4u};

  /*! \brief Number of gates to start with. */
  uint64_t num_gates{10u};

  /*! \brief Number of PIs to increment at each step. */
  uint64_t num_pis_step{1u};

  /*! \brief Number of gates to increment at each step. */
  uint64_t num_gates_step{10u};

  /*! \brief Max number of PIs. */
  uint64_t num_pis_max{10u};

  /*! \brief Max number of gates. */
  uint64_t num_gates_max{100u};
}; /* fuzz_tester_params */

/*! \brief Network fuzz tester
 *
 * Runs an algorithm on many small random logic networks.  Fuzz
 * testing is often useful to detect potential segmentation faults in
 * new implementations.  The generated benchmarks are saved first in a
 * file.  If a segmentation fault occurs, the file can be used to
 * reproduce and debug the problem.
 *
 * The entry function `run` generates different networks with the same
 * number of PIs and gates. The function `run_incremental`, on the other
 * hand, generates networks of increasing sizes.
 *
 * The script of algorithm(s) to be tested can be provided as (1) a
 * lambda function taking a network as input and returning a Boolean,
 * which is true if the algorithm behaves as expected; or (2) a lambda
 * function making a command string to be called, taking a filename string
 * as input (not supported on Windows platform).
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
template<typename Ntk, class NetworkGenerator>
class network_fuzz_tester
{
public:
  explicit network_fuzz_tester( NetworkGenerator& gen, fuzz_tester_params const ps = {} )
    : gen( gen )
    , ps( ps )
  {}

#ifndef _MSC_VER
  void run_incremental( std::function<std::string(std::string const&)>&& make_command )
  {
    run_incremental( make_callback( make_command ) );
  }

  void run( std::function<std::string(std::string const&)>&& make_command )
  {
    run( make_callback( make_command ) );
  }
#endif

  void run_incremental( std::function<bool(Ntk)>&& fn )
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

      if ( ps.outputfile )
      {
        if ( !abc_cec() )
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

  void run( std::function<bool(Ntk)>&& fn )
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

      if ( ps.outputfile )
      {
        if ( !abc_cec() )
          return;
      }
    }
  }

  template<typename Fn>
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
#ifndef _MSC_VER
  inline std::function<bool(Ntk)> make_callback( std::function<std::string(std::string const&)>& make_command )
  {
    std::function<bool(Ntk)> fn = [&]( Ntk ntk ) -> bool {
      (void) ntk;
      int status = std::system( make_command( ps.filename ).c_str() );
      if ( status < 0 )
      {
        std::cout << "[e] Unexpected error when calling command: " << strerror( errno ) << '\n';
        return false;
      }
      else
      {
        if ( WIFEXITED( status ) )
        {
          if ( WEXITSTATUS( status ) == 0 ) // normal
          {
            if ( ps.outputfile )
              return abc_cec();
            return true;
          }
          else if ( WEXITSTATUS( status ) == 1 ) // buggy
            return false;
          else
          {
            std::cout << "[e] Unexpected return value: " << WEXITSTATUS( status ) << '\n';
            return false;
          }
        }
        else // segfault
        {
          return false;
        }
      }
    };
    return fn;
  }
#endif

  inline bool abc_cec()
  {
    std::string command = fmt::format( "abc -q \"cec -n {} {}\"", ps.filename, *ps.outputfile );

    std::array<char, 128> buffer;
    std::string result;
    #ifdef _MSC_VER
    std::unique_ptr<FILE, decltype( &_pclose )> pipe( _popen( command.c_str(), "r" ), _pclose );
    #else
    std::unique_ptr<FILE, decltype( &pclose )> pipe( popen( command.c_str(), "r" ), pclose );
    #endif
    if ( !pipe )
    {
      throw std::runtime_error( "popen() failed" );
    }
    while ( fgets( buffer.data(), buffer.size(), pipe.get() ) != nullptr )
    {
      result += buffer.data();
    }

    /* search for one line which says "Networks are equivalent" and ignore all other debug output from ABC */
    std::stringstream ss( result );
    std::string line;
    while ( std::getline( ss, line, '\n' ) )
    {
      if ( line.size() >= 23u && line.substr( 0u, 23u ) == "Networks are equivalent" )
      {
        return true;
      }
    }

    fmt::print( "[e] Files are not equivalent\n" );
    return false;
  }

private:
  NetworkGenerator& gen;
  fuzz_tester_params const ps;
}; /* network_fuzz_tester */

} /* namespace mockturtle */