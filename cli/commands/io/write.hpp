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
public:
  explicit write_command( const environment::ptr& env )
      : command( env,
                 "Writes the current Boolean network to file." )
  {
    opts.add_option( "--filename,filename", filename,
                     "Name of the file to write [.aig, .bench, .blif, .v]" )
        ->required();
    add_flag( "--aig,-a", "Writes the AIG network (default without flags)" );
    add_flag( "--mig,-m", "Writes the MIG network" );
    add_flag( "--xag,-x", "Writes the XAG network" );
    add_flag( "--xmg,-g", "Writes the XMG network" );
    add_flag( "--klut,-k", "Writes the k-LUT network" );
  }

protected:
  void execute()
  {
    if ( mockturtle::check_extension( filename, "aig" ) )
    {
      /* TODO: check network type: currently it writes only AIGs */
      if ( !store<aig_ntk>().empty() )
      {
        mockturtle::write_aiger( *store<aig_ntk>().current(), filename );
      }
      else
      {
        env->err() << "Empty AIG network.\n";
      }
    }
    else if ( mockturtle::check_extension( filename, "bench" ) )
    {
      if ( !store<aig_ntk>().empty() )
      {
        mockturtle::write_bench( *store<aig_ntk>().current(), filename );
      }
      else
      {
        env->err() << "Empty AIG network.\n";
      }
    }
    /* add support for BLIF file */
    else if ( mockturtle::check_extension( filename, "v" ) )
    {
      /* TODO: check network type: currently it writes only AIGs */
      if ( !store<aig_ntk>().empty() )
      {
        mockturtle::write_verilog( *store<aig_ntk>().current(), filename );
      }
      else
      {
        env->err() << "Empty AIG network.\n";
      }
    }
    else
    {
      env->err() << "[e]" << filename << " file extention has not been recognized.\n";
    }
  }

private:
  std::string filename{};
};

ALICE_ADD_COMMAND( write, "I/O" );

} // namespace alice