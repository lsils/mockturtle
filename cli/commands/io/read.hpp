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
private:
  std::string filename{};
  std::string ntk_name{};
  network_manager_type ntk_type = AIG;

public:
  explicit read_command( const environment::ptr& env )
      : command( env,
                 "Reads an RTL file into a specified network type." )
  {
    opts.add_option( "--filename,filename", filename,
                     "File to read in [.aig, .blif, .v]" )
        ->required();

    opts.add_option( "--ntk-name", ntk_name,
                     "Name of the network (defaults to filename)" );

    add_flag( "--aig,-a", "Stores the network as an AIG (default without flags)" );
    add_flag( "--mig,-m", "Stores the network as an MIG" );
    add_flag( "--xag,-x", "Stores the network as an XAG" );
    add_flag( "--xmg,-g", "Stores the network as an XMG" );
    add_flag( "--klut,-k", "Stores the network as a k-LUT network" );
  }

protected:
  void execute()
  {
    int ntk_type_count = 0;
    if ( is_set( "aig" ) )
    {
      ntk_type = AIG;
      ntk_type_count++;
    }
    if ( is_set( "mig" ) )
    {
      ntk_type = MIG;
      ntk_type_count++;
    }
    if ( is_set( "xag" ) )
    {
      ntk_type = XAG;
      ntk_type_count++;
    }
    if ( is_set( "xmg" ) )
    {
      ntk_type = XMG;
      ntk_type_count++;
    }
    if ( is_set( "klut" ) )
    {
      ntk_type = KLUT;
      ntk_type_count++;
    }

    if ( ntk_type_count > 1 )
    {
      env->err() << "[e] Multiple network types set. Network type flags are mututally exclusive.\n";
      return;
    }

    network_manager& man = store<network_manager>().extend();
    int retcode;

    switch ( ntk_type )
    {
    case AIG:
      retcode = handle_file<mockturtle::aig_network>( man.add_aig() );
      break;

    case MIG:
      retcode = handle_file<mockturtle::mig_network>( man.add_mig() );
      break;

    case XAG:
      retcode = handle_file<mockturtle::xag_network>( man.add_xag() );
      break;

    case XMG:
      retcode = handle_file<mockturtle::xmg_network>( man.add_xmg() );
      break;

    case KLUT:
      retcode = handle_file<mockturtle::klut_network>( man.add_klut() );
      break;

    default:
      retcode = -1;
      break;
    }

    if ( retcode != 0 )
    {
      store<network_manager>().pop_current();
    }
  }

private:
  template<class Ntk>
  int handle_file( mockturtle::names_view<Ntk>& network )
  {
    lorina::return_code result;

    if ( mockturtle::check_extension( filename, "aig" ) )
    {
      result = lorina::read_aiger( filename, mockturtle::aiger_reader( network ) );

      if ( result != lorina::return_code::success )
      {
        env->err() << "[e] Unable to read aiger file\n";
        return -1;
      }
      filename.erase( filename.end() - strlen( ".aig" ), filename.end() );
    }
    else if ( mockturtle::check_extension( filename, "v" ) )
    {
      result = lorina::read_verilog( filename, mockturtle::verilog_reader( network ) );

      if ( result != lorina::return_code::success )
      {
        env->err() << "[e] Unable to read verilog file.\n";
        return -1;
      }
      filename.erase( filename.end() - strlen( ".v" ), filename.end() );
    }
    else if ( mockturtle::check_extension( filename, "blif" ) )
    {

      if constexpr ( std::is_same_v<Ntk, mockturtle::klut_network> )
      {
        result = lorina::read_blif( filename, mockturtle::blif_reader( network ) );

        if ( result != lorina::return_code::success )
        {
          env->err() << "[e] Unable to read blif file.\n";
          return -1;
        }
        filename.erase( filename.end() - strlen( ".blif" ), filename.end() );
      }
      else
      {
        env->err() << "[e] BLIF files can only be read into klut networks. (--klut)\n";
        return -1;
      }
    }
    else
    {
      env->err() << "[e] " << filename << " is not a valid input file. Accepted file extensions are .aig, .blif, and .v\n";
      return -1;
    }

    if ( ntk_name.length() > 0 )
    {
      network.set_network_name( ntk_name );
    }
    else
    {
      network.set_network_name( filename );
    }
    return 0;
  }
};

ALICE_ADD_COMMAND( read, "I/O" );

} // namespace alice
