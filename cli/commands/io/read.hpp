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
  \file read.hpp
  \brief Read command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>
#include <lorina/aiger.hpp>
#include <lorina/blif.hpp>
#include <lorina/verilog.hpp>

#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/names_view.hpp>

#include "../../utilities.hpp"

namespace alice
{

class read_command : public alice::command
{
public:
  explicit read_command( const environment::ptr& env )
      : command( env,
                 "Reads an RTL file into a specified network type." )
  {
    opts.add_option( "--filename,filename", filename,
                     "File to read in [.aig, .blif, .v]" )
        ->required();
    add_flag( "--aig,-a", "Stores the network as an AIG (default without flags)" );
    add_flag( "--mig,-m", "Stores the network as an MIG" );
    add_flag( "--xag,-x", "Stores the network as an XAG" );
    add_flag( "--xmg,-g", "Stores the network as an XMG" );
    add_flag( "--klut,-k", "Stores the network as a k-LUT network" );
  }

protected:
  void execute()
  {
    if ( mockturtle::check_extension( filename, "aig" ) )
    {
      /* TODO: check network type: currently it loads only AIGs */
      {
        mockturtle::aig_network ntk;
        mockturtle::names_view<mockturtle::aig_network> ntk_name{ ntk };
        lorina::return_code result = lorina::read_aiger( filename,
                                                         mockturtle::aiger_reader( ntk_name ) );
        if ( result != lorina::return_code::success )
        {
          env->err() << "[e]" << "Unable to read aiger file\n";
          return;
        }

        filename.erase( filename.end() - 4, filename.end() );
        ntk_name.set_network_name( filename );
        store<aig_ntk>().extend() = std::make_shared<aig_names>( ntk_name );
      }
    }
    /* add support for BLIF file */
    else if ( mockturtle::check_extension( filename, "v" ) )
    {
      /* TODO: check network type: currently it loads only AIGs */
      {
        mockturtle::aig_network ntk;
        mockturtle::names_view<mockturtle::aig_network> ntk_name{ ntk };
        lorina::return_code result = lorina::read_verilog( filename,
                                                           mockturtle::verilog_reader( ntk_name ) );
        if ( result != lorina::return_code::success )
        {
          env->err() << "[e]" << "Unable to read verilog file.\n";
          return;
        }

        filename.erase( filename.end() - 2, filename.end() );
        ntk_name.set_network_name( filename );
        store<aig_ntk>().extend() = std::make_shared<aig_names>( ntk_name );
      }
    }
    else
    {
      env->err() << "[e]" << filename << " is not a valid input file. Accepted file extensions are .aig, .blif, and .v\n";
    }
  }

private:
  std::string filename{};
};

ALICE_ADD_COMMAND( read, "I/O" );

} // namespace alice