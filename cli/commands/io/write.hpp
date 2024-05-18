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
    if ( mockturtle::check_extension( filename, "aig" ) )
    {
      /* TODO: check network type: currently it writes only AIGs */
      if ( ntk.is_type( network_manager_type::AIG ) )
        mockturtle::write_aiger( ntk.get_aig(), filename );
      else
        env->err() << "[e] Can write only AIGs into aiger files.\n";
    }
    else if ( mockturtle::check_extension( filename, "bench" ) )
    {
      /* TODO: add checks */
      mockturtle::write_bench( ntk.get_aig(), filename );
    }
    /* add support for BLIF file */
    else if ( mockturtle::check_extension( filename, "v" ) )
    {
      if ( ntk.is_type( network_manager_type::AIG ) )
        mockturtle::write_verilog( ntk.get_aig(), filename );
      if ( ntk.is_type( network_manager_type::MIG ) )
        mockturtle::write_verilog( ntk.get_mig(), filename );
      if ( ntk.is_type( network_manager_type::XAG ) )
        mockturtle::write_verilog( ntk.get_xag(), filename );
      if ( ntk.is_type( network_manager_type::XMG ) )
        mockturtle::write_verilog( ntk.get_xmg(), filename );
      else if ( ntk.is_type( network_manager_type::MAPPED ) )
        mockturtle::write_verilog_with_cell( ntk.get_mapped(), filename );
      else
        env->err() << "[e] Logic network type not supported.\n";
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