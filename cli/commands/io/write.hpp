/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2024  EPFL
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
  \file write.hpp
  \brief Write command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/io/write_aiger.hpp>
#include <mockturtle/io/write_bench.hpp>
#include <mockturtle/io/write_blif.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/names_view.hpp>

#include "../../utilities.hpp"

namespace alice
{

/* Writes a network to file */
class write_command : public alice::command
{
private:
  std::string filename{};

public:
  explicit write_command( const environment::ptr& env )
      : command( env,
                 "Writes the current Boolean network to file." )
  {
    opts.add_option( "--filename,filename", filename,
                     "Name of the file to write [.aig, .bench, .blif, .v]" )
        ->required();
  }

protected:
  void execute()
  {
    if ( store<network_manager>().empty() )
    {
      env->err() << "[e] Empty logic network.\n";
      return;
    }

    network_manager& ntk = store<network_manager>().current();

    switch ( ntk.get_current_type() )
    {
    case AIG:
      env->out() << "[i] writing AIG network to " << filename << std::endl;
      write_file( ntk.get_aig() );
      break;

    case MIG:
      env->out() << "[i] writing MIG network to " << filename << std::endl;
      write_file( ntk.get_mig() );
      break;

    case XAG:
      env->out() << "[i] writing XAG network to " << filename << std::endl;
      write_file( ntk.get_xag() );
      break;

    case XMG:
      env->out() << "[i] writing XMG network to " << filename << std::endl;
      write_file( ntk.get_xmg() );
      break;

    case KLUT:
      env->out() << "[i] writing k-LUT network to " << filename << std::endl;
      write_file( ntk.get_klut() );
      break;

    case MAPPED:
      env->out() << "[i] writing MAPPED network to " << filename << std::endl;
      write_file( ntk.get_mapped() );
      break;

    default:
      break;
    }
  }

private:
  // most network types can be written to any format
  template<class Ntk_View>
  void write_file( Ntk_View& network )
  {
    if ( mockturtle::check_extension( filename, "aig" ) )
    {
      mockturtle::write_aiger( network, filename );
    }
    else if ( mockturtle::check_extension( filename, "bench" ) )
    {
      mockturtle::write_bench( network, filename );
    }
    else if ( mockturtle::check_extension( filename, "blif" ) )
    {
      mockturtle::write_blif( network, filename );
    }
    else if ( mockturtle::check_extension( filename, "v" ) )
    {
      if constexpr ( std::is_same_v<Ntk_View, network_manager::klut_names> )
      {
        env->err() << "[e] k-LUT networks cannot be written to Verilog.\n";
      }
      else
      {
        mockturtle::write_verilog( network, filename );
      }
    }
    else
    {
      env->err() << "[e] " << filename << " file extention has not been recognized.\n";
    }
  }

  // mapped networks require a special writer, only exists for Verilog
  void write_file( network_manager::mapped_names& network )
  {
    if ( mockturtle::check_extension( filename, "v" ) )
    {
      mockturtle::write_verilog_with_cell( network, filename );
    }
    else
    {
      env->err() << "[e]" << filename << " mapped networks currently only support verilog writeout.\n";
    }
  }
};

ALICE_ADD_COMMAND( write, "I/O" );

} // namespace alice
