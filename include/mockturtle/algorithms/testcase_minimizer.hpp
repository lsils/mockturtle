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
  \brief Minimize testcase for debugging

  \author Siang-Yun Lee
*/

#include "../io/write_verilog.hpp"
#include "../io/verilog_reader.hpp"
#include "../io/write_aiger.hpp"
#include "../io/aiger_reader.hpp"
#include "../utils/debugging_utils.hpp"
#include "../networks/aig.hpp"

#include <lorina/lorina.hpp>
#include <fmt/format.h>
#include <optional>
#include <utility>

namespace mockturtle
{

/*! \brief Parameters for testcase_minimizer. */
struct testcase_minimizer_params
{
  /*! \brief File format of the testcase. */
  enum 
  {
    verilog,
    aiger
  } file_format = verilog;

  /*! \brief Path to find the initial test case and to store the minmized test case. */
  std::string path{"."};

  /*! \brief File name of the initial test case (excluding extension). */
  std::string init_case{"testcase"};

  /*! \brief File name of the minimized test case (excluding extension). */
  std::string minimized_case{"minimized"};

  /*! \brief Target maximum size of the test case. */
  uint32_t max_size{20u};

  /*! \brief Maximum number of iterations in total. nullopt = infinity */
  std::optional<uint32_t> num_iterations{std::nullopt};

  /*! \brief Step into the next stage if nothing works for this number of iterations. */
  uint32_t num_iterations_stage{100u};

  /*! \brief Be verbose. */
  bool verbose{false};
}; /* testcase_minimizer_params */

/*! \brief Debugging testcase minimizer
 *
 * Given a (sequence of) algorithm(s) and a testcase that is
 * known to trigger a bug in the algorithm(s), this utility
 * minimizes the testcase by trying to (1) remove POs, 
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
 *
  \verbatim embed:rst

   Usage

   .. code-block:: c++

     auto opt = []( mig_network ntk ) -> bool {
       direct_resynthesis<mig_network> resyn;
       refactoring( ntk, resyn );
       return !network_is_acyclic( ntk );
     };

     testcase_minimizer_params ps;
     ps.path = "."; // current directory
     ps.init_case = "acyclic";
     testcase_minimizer<mig_network> minimizer( ps );
     minimizer.run( opt );
   \endverbatim
*/
template<typename Ntk>
class testcase_minimizer
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  explicit testcase_minimizer( testcase_minimizer_params const ps = {} )
    : ps( ps )
  {
    //assert( ps.file_format != testcase_minimizer_params::aiger || std::is_same_v<typename Ntk::base_type, aig_network> );
    switch ( ps.file_format )
    {
      case testcase_minimizer_params::verilog:
        file_extension = ".v";
        break;
      case testcase_minimizer_params::aiger:
        file_extension = ".aig";
        break;
      default:
        fmt::print( "[e] Unsupported format\n" );
    }
  }

  void run( std::function<bool(Ntk)> const& fn )
  {
    if ( !read_initial_testcase() )
    {
      return;
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
      if ( !reduce() )
      {
        break;
      }
      if ( ntk.num_gates() == 0 )
      {
        ++stage_counter;
        ntk = ntk_backup2;
        continue;
      }

      if ( test( fn ) )
      {
        fmt::print( "[i] Testcase with I/O = {}/{} gates = {} triggers the buggy behavior\n", ntk.num_pis(), ntk.num_pos(), ntk.num_gates() );
        write_testcase( ps.minimized_case );
        stage_counter = 0;
        
        if ( ntk.size() <= ps.max_size )
        {
          break;
        }
      }
      else
      {
        ++stage_counter;
        ntk = ntk_backup2;
      }
    }
  }

  void run( std::function<std::string(std::string const&)> const& make_command )
  {
    if ( !read_initial_testcase() )
    {
      return;
    }

    if ( !test( make_command, ps.path + "/" + ps.init_case + file_extension ) )
    {
      fmt::print( "[e] The initial test case does not trigger the buggy behavior\n" );
      return;
    }

    uint32_t counter{0};
    while ( !ps.num_iterations || counter++ < ps.num_iterations )
    {
      ntk_backup2 = cleanup_dangling( ntk );
      if ( !reduce() )
      {
        break;
      }
      if ( ntk.num_gates() == 0 )
      {
        ++stage_counter;
        ntk = ntk_backup2;
        continue;
      }

      write_testcase( "tmp" );

      if ( test( make_command, ps.path + "/tmp" + file_extension ) )
      {
        fmt::print( "[i] Testcase with I/O = {}/{} gates = {} triggers the buggy behavior\n", ntk.num_pis(), ntk.num_pos(), ntk.num_gates() );
        write_testcase( ps.minimized_case );
        stage_counter = 0;
        
        if ( ntk.size() <= ps.max_size )
        {
          break;
        }
      }
      else
      {
        ++stage_counter;
        ntk = ntk_backup2;
      }
    }
  }

private:
  bool read_initial_testcase()
  {
    switch ( ps.file_format )
    {
      case testcase_minimizer_params::verilog:
        if ( lorina::read_verilog( ps.path + "/" + ps.init_case + file_extension, verilog_reader( ntk ) ) != lorina::return_code::success )
        {
          fmt::print( "[e] Could not read test case `{}{}`\n", ps.init_case, file_extension );
          return false;
        }
        break;
      case testcase_minimizer_params::aiger:
        if ( lorina::read_aiger( ps.path + "/" + ps.init_case + file_extension, aiger_reader( ntk ) ) != lorina::return_code::success )
        {
          fmt::print( "[e] Could not read test case `{}{}`\n", ps.init_case, file_extension );
          return false;
        }
        break;
      default:
        fmt::print( "[e] Unsupported format\n" );
        return false;
    }
    return true;
  }

  void write_testcase( std::string const& filename )
  {
    switch ( ps.file_format )
    {
      case testcase_minimizer_params::verilog:
        write_verilog( ntk, ps.path + "/" + filename + file_extension );
        break;
      case testcase_minimizer_params::aiger:
        write_aiger( ntk, ps.path + "/" + filename + file_extension );
        break;
      default:
        fmt::print( "[e] Unsupported format\n" );
    }
  }

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

  bool reduce()
  {
    if ( stage_counter >= ps.num_iterations_stage || ( reducing_stage == po && ntk.num_pos() == 1 ) )
    {
      reducing_stage = static_cast<stages>( static_cast<int>( reducing_stage ) + 1 );
      stage_counter = 0;
    }

    switch ( reducing_stage )
    {
      case po:
      {
        uint32_t const ith_po = std::rand() % ntk.num_pos();
        const node n = ntk.get_node( ntk.po_at( ith_po ) );
        if ( ps.verbose )
          fmt::print( "[i] Remove {}-th PO (node {})\n", ith_po, n );
        ntk.substitute_node( n, ntk.get_constant( false ) );
        break;
      }
      case pi:
      {
        const node n = ntk.index_to_node( std::rand() % ntk.num_pis() + 1 );
        if ( ps.verbose )
          fmt::print( "[i] Remove PI {}\n", n );
        ntk.substitute_node( n, ntk.get_constant( false ) );
        break;
      }
      case testcase_minimizer::gate:
      {
        node const& n = get_random_gate();
        if ( ps.verbose )
          fmt::print( "[i] Substitute gate {} with const0\n", n );
        ntk.substitute_node( n, ntk.get_constant( false ) );
        break;
      }
      case testcase_minimizer::fanout:
      {
        auto const& [n, ni] = get_random_gate_with_gate_fanin();
        if ( ps.verbose )
          fmt::print( "[i] Create PO at gate {} and substitute its fanout {} with const0\n", ni, n );
        ntk.create_po( ntk.make_signal( ni ) );
        ntk.substitute_node( n, ntk.get_constant( false ) );
        break;
      }
      case testcase_minimizer::fanin:
      {
        node const& n = get_random_gate();
        signal fi;
        uint32_t const ith_fanin = std::rand() % ntk.fanin_size( n );
        ntk.foreach_fanin( n, [&]( auto const& f, auto i ){
          if ( i == ith_fanin )
            fi = f;
        });
        if ( ps.verbose )
          fmt::print( "[i] Substitute gate {} with its {}-th fanin {}{}\n", n, ith_fanin, ntk.is_complemented( fi ) ? "!" : "", ntk.get_node( fi ) );
        ntk.substitute_node( n, fi );
        assert( network_is_acyclic( ntk ) );
        break;
      }
      default:
        return false; // all stages done, nothing to reduce
    }

    ntk = cleanup_dangling( ntk, true, true );
    return true;
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

  std::pair<node, node> get_random_gate_with_gate_fanin()
  {
    while ( true )
    {
      node const& n = get_random_gate();
      node ni;
      bool has_gate_fanin = false;
      ntk.foreach_fanin( n, [&]( auto const& f ){
        ni = ntk.get_node( f );
        if ( !ntk.is_pi( ni ) && !ntk.is_constant( ni ) )
        {
          has_gate_fanin = true;
          return false; // break
        }
        return true; // next fanin
      });
      if ( has_gate_fanin )
      {
        return std::make_pair( n, ni );
      }
    }
  }

private:
  testcase_minimizer_params const ps;
  std::string file_extension;
  Ntk ntk, ntk_backup, ntk_backup2;

  enum stages : int {
    po = 1, // remove a PO (substitute with const0)
    pi, // remove a PI (substitute with const0)
    gate, // remove a gate (substitute with const0)
    fanout, // remove TFO of a gate (create PO at a non-PI fanin, then substitute with const0)
    fanin // substitute a node with its first fanin
  } reducing_stage{po};
  uint32_t stage_counter{0u};
}; /* testcase_minimizer */

} /* namespace mockturtle */