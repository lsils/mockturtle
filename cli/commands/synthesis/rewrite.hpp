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
  \file rewrite.hpp
  \brief Rewrite command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/algorithms/rewrite.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/names_view.hpp>

namespace alice
{

/* Writes a network to file */
class rewrite_command : public alice::command
{
public:
  explicit rewrite_command( const environment::ptr& env )
      : command( env,
                 "Performs technology-independent rewriting of the logic network." )
  {
    add_flag( "--zero,-z", "Performs zero-gain rewriting [default = no]" );
    add_flag( "--verbose,-v", "Toggle verbose printout [default = no]" );
    /* TODO: add other flags */
  }

protected:
  void execute()
  {
    /* TODO: check network type: currently it works only with AIGs */
    if ( store<network_manager>().empty() )
    {
      env->err() << "Empty logic network.\n";
      return;
    }

    mockturtle::rewrite_params ps;
    ps.allow_zero_gain = is_set( "zero" );
    ps.verbose = is_set( "verbose" );

    network_manager& ntk = store<network_manager>().current();
    if ( ntk.is_type( network_manager_type::AIG ) )
    {
      using namespace mockturtle;
      exact_library<aig_network>& lib = _mockturtle_global.exact_lib_man.get_aig_library();
      network_manager::aig_names& aig = ntk.get<network_manager::aig_names>();
      rewrite( aig, lib, ps );
    }
    else if ( ntk.is_type( network_manager_type::XAG ) )
    {
      using namespace mockturtle;
      exact_library<xag_network>& lib = _mockturtle_global.exact_lib_man.get_xag_library();
      network_manager::xag_names& xag = ntk.get<network_manager::xag_names>();
      rewrite( xag, lib, ps );
    }
    else if ( ntk.is_type( network_manager_type::MIG ) )
    {
      using namespace mockturtle;
      exact_library<mig_network>& lib = _mockturtle_global.exact_lib_man.get_mig_library();
      network_manager::mig_names& mig = ntk.get<network_manager::mig_names>();
      rewrite( mig, lib, ps );
    }
    else if ( ntk.is_type( network_manager_type::XMG ) )
    {
      using namespace mockturtle;
      exact_library<xmg_network>& lib = _mockturtle_global.exact_lib_man.get_xmg_library();
      network_manager::xmg_names& xmg = ntk.get<network_manager::xmg_names>();
      rewrite( xmg, lib, ps );
    }
    else
    {
      env->err() << "[e] Network type is not supported by rewrite.\n";
    }
  }
};

ALICE_ADD_COMMAND( rewrite, "Synthesis" );

} // namespace alice