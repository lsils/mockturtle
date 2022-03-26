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
  \file testcase_minimizer.hpp
  \brief Minimize test case for debugging

  \author Siang-Yun Lee
*/

#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_aiger.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <lorina/lorina.hpp>
#include <fmt/format.h>
#include <optional>

namespace mockturtle
{

struct testcase_minimizer_params
{
  enum 
  {
    verilog,
    aiger
  } file_format = verilog;

  /*! \brief Path to find the initial test case and to store the minmized test case. */
  std::string path{"."};

  /*! \brief File name of the initial test case. */
  std::string init_case{"testcase.v"};

  /*! \brief File name of the minimized test case. */
  std::string minimized_case{"minimized.v"};

  /*! \brief Target maximum size of the test case. */
  uint32_t max_size{20u};

  /*! \brief Number of iterations. nullopt = infinity */
  std::optional<uint32_t> num_iterations{std::nullopt};
}; /* testcase_minimizer_params */

/*! \brief Debugging test case minimizer
 *
 * Given a (sequence of) algorithm(s) and a test case that is
 * known to trigger a bug in the algorithm(s), this utility
 * minimizes the test case by trying to (1) remove POs, 
 * (2) replace nodes with constants, (3) replace a gate with
 * one of its fanins, and also removing dangling nodes (including
 * PIs) after each modification. Only changes after which the bug
 * is still triggered are kept; otherwise, the change is reverted.
 * 
 * The script of algorithm(s) to be run can be provided as (1) a
 * lambda function taking a network as input and returning a Boolean,
 * which is true if the bug is triggered and is false otherwise
 * (i.e. the buggy behavior is not observed); or (2) a lambda function
 * making a command string to be called, taking a filename string as
 * input. The command should return 1 if the buggy behavior is observed
 * and 0 otherwise. In this case, if the command segfaults, it is counted
 * as a buggy behavior.
 * 
 * The initial test case should be provided as a Verilog file.
 *
  \verbatim embed:rst

   Usage

   .. code-block:: c++

     auto opt = []( mig_network ntk ) -> bool {
       direct_resynthesis<mig_network> resyn;
       refactoring( ntk, resyn );
       return !network_is_acylic( ntk );
     };

     testcase_minimizer_params ps;
     ps.path = "."; // current directory
     ps.init_case = "acyclic.v";
     testcase_minimizer<mig_network> minimizer( ps );
     minimizer.run( opt );
   \endverbatim
*/
template<typename Ntk>
class testcase_minimizer
{
  using node = typename Ntk::node;

public:
  explicit testcase_minimizer( testcase_minimizer_params const ps = {} )
    : ps( ps )
  {
    //assert( ps.file_format != testcase_minimizer_params::aiger || std::is_same_v<typename Ntk::base_type, aig_network> );
  }

  void run( std::function<bool(Ntk)> const& fn )
  {
    switch ( ps.file_format )
    {
      case testcase_minimizer_params::verilog:
        if ( lorina::read_verilog( ps.path + "/" + ps.init_case, verilog_reader( ntk ) ) != lorina::return_code::success )
        {
          fmt::print( "[e] Could not read test case `{}`\n", ps.init_case );
          return;
        }
        break;
      case testcase_minimizer_params::aiger:
        if ( lorina::read_aiger( ps.path + "/" + ps.init_case, aiger_reader( ntk ) ) != lorina::return_code::success )
        {
          fmt::print( "[e] Could not read test case `{}`\n", ps.init_case );
          return;
        }
        break;
      default:
        fmt::print( "[e] Unsupported format\n" );
    }

    if ( !test( fn ) )
    {
      fmt::print( "[e] The initial test case does not trigger the buggy behavior\n" );
      return;
    }

    uint32_t counter{0};
    while ( !ps.num_iterations || counter++ < ps.num_iterations )
    {
      ntk_backup2 = cleanup_dangling( ntk );
      reduce();

      if ( test( fn ) )
      {
        fmt::print( "[i] Testcase with I/O = {}/{} gates = {} triggers the buggy behavior\n", ntk.num_pis(), ntk.num_pos(), ntk.num_gates() );
        switch ( ps.file_format )
        {
          case testcase_minimizer_params::verilog:
            write_verilog( ntk, ps.path + "/" + ps.minimized_case );
            break;
          case testcase_minimizer_params::aiger:
            write_aiger( ntk, ps.path + "/" + ps.minimized_case );
            break;
          default:
            fmt::print( "[e] Unsupported format\n" );
        }
        
        if ( ntk.size() <= ps.max_size )
        {
          break;
        }
      }
      else
      {
        ntk = ntk_backup2;
      }
    }
  }

  void run( std::function<std::string(std::string const&)> const& make_command )
  {
    if ( !test( make_command, ps.init_case ) )
    {
      fmt::print( "[e] The initial test case does not trigger the buggy behavior\n" );
      return;
    }

    switch ( ps.file_format )
    {
      case testcase_minimizer_params::verilog:
        if ( lorina::read_verilog( ps.path + "/" + ps.init_case, verilog_reader( ntk ) ) != lorina::return_code::success )
        {
          fmt::print( "[e] Could not read test case `{}`\n", ps.init_case );
          return;
        }
        break;
      case testcase_minimizer_params::aiger:
        if ( lorina::read_aiger( ps.path + "/" + ps.init_case, aiger_reader( ntk ) ) != lorina::return_code::success )
        {
          fmt::print( "[e] Could not read test case `{}`\n", ps.init_case );
          return;
        }
        break;
      default:
        fmt::print( "[e] Unsupported format\n" );
    }

    uint32_t counter{0};
    while ( !ps.num_iterations || counter++ < ps.num_iterations )
    {
      ntk_backup2 = cleanup_dangling( ntk );
      reduce();
      if ( ntk.num_gates() == 0 )
      {
        ntk = ntk_backup2;
        continue;
      }

      switch ( ps.file_format )
      {
        case testcase_minimizer_params::verilog:
          write_verilog( ntk, ps.path + "/" + "tmp.v" );
          break;
        case testcase_minimizer_params::aiger:
          write_aiger( ntk, ps.path + "/" + "tmp.aig" );
          break;
        default:
          fmt::print( "[e] Unsupported format\n" );
      }

      if ( test( make_command, "tmp.aig" ) )
      {
        fmt::print( "[i] Testcase with I/O = {}/{} gates = {} triggers the buggy behavior\n", ntk.num_pis(), ntk.num_pos(), ntk.num_gates() );
        switch ( ps.file_format )
        {
          case testcase_minimizer_params::verilog:
            write_verilog( ntk, ps.path + "/" + ps.minimized_case );
            break;
          case testcase_minimizer_params::aiger:
            write_aiger( ntk, ps.path + "/" + ps.minimized_case );
            break;
          default:
            fmt::print( "[e] Unsupported format\n" );
        }
        
        if ( ntk.size() <= ps.max_size )
        {
          break;
        }
      }
      else
      {
        ntk = ntk_backup2;
      }
    }
  }

private:
  bool test( std::function<bool(Ntk)> const& fn )
  {
    ntk_backup = cleanup_dangling( ntk );
    bool res = fn( ntk );
    ntk = ntk_backup;
    return res;
  }

  bool test( std::function<std::string(std::string const&)> const& make_command, std::string const& filename )
  {
    std::string const command = make_command( ps.path + "/" + filename );
    int status = std::system( command.c_str() );
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
          return false;
        else if ( WEXITSTATUS( status ) == 1 ) // buggy
          return true;
        else
        {
          std::cout << "[e] Unexpected return value: " << WEXITSTATUS( status ) << '\n';
          return false;
        }
      }
      else // segfault
      {
        return true;
      }
    }
  }

  void reduce()
  {
    if ( po_counter < ntk.num_pos() - 1 )
    {
      std::cout << "[i] substitute PO " << ntk.get_node( ntk.po_at( po_counter ) ) << "\n";
      ntk.substitute_node( ntk.get_node( ntk.po_at( po_counter ) ), ntk.get_constant( false ) );
      ++po_counter;
      ntk = cleanup_dangling( ntk, true, true );
    }
    else
    {
      auto const& n = get_random_gate();
      std::cout << "[i] substitute node " << n << "\n";
      ntk.substitute_node( n, ntk.get_constant( false ) );
      ntk = cleanup_dangling( ntk );
    }
  }

  node get_random_gate()
  {
    while ( true )
    {
      node n = ntk.index_to_node( ( std::rand() % ( ntk.size() - ntk.num_pis() - 1 ) ) + ntk.num_pis() + 1 );
      if ( !ntk.is_dead( n ) && !ntk.is_pi( n ) )
      {
        return n;
      }
    }
  }

private:
  testcase_minimizer_params const ps;
  Ntk ntk, ntk_backup, ntk_backup2;
  uint32_t po_counter{0u};
}; /* testcase_minimizer */

} /* namespace mockturtle */