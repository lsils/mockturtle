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
  \file factoring.hpp
  \brief Factoring command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/algorithms/node_resynthesis/sop_factoring.hpp>
#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/views/names_view.hpp>

namespace alice
{

/* Writes a network to file */
class factoring_command : public alice::command
{
public:
  explicit factoring_command( const environment::ptr& env )
      : command( env,
                 "Performs technology-independent factoring of the logic network." )
  {
    opts.add_option( "--max_pis,-w", max_pis, "Max number of MFFC inputs [default = 6]" );
    add_flag( "--sop,-s", "Performs SOP factoring [default = yes]" );
    // add_flag( "--akers,-a", "Performs Aker's MIG factoring [default = no]" );
    // add_flag( "--bidec,-b", "Performs bidecomposition factoring [default = no]" );
    add_flag( "--zero,-z", "Performs zero-gain rewriting [default = no]" );
    add_flag( "--dc,-d", "Use don't care conditions [default = no]" );
    add_flag( "--reconv,-r", "Use reconvergence-driven cuts for large MFFCs [default = yes]" );
    add_flag( "--verbose,-v", "Toggle verbose printout [default = no]" );
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

    mockturtle::refactoring_params ps;
    ps.max_pis = max_pis;
    ps.allow_zero_gain = is_set( "zero" );
    ps.use_dont_cares = is_set( "dc" );
    ps.use_reconvergence_cut = is_set( "reconv" );
    ps.verbose = is_set( "verbose" );

    reset_default_params();

    network_manager& ntk = store<network_manager>().current();
    if ( ntk.is_type( network_manager_type::AIG ) )
    {
      using namespace mockturtle;
      sop_factoring<aig_network> resyn;
      network_manager::aig_names& aig = ntk.get_aig();
      refactoring( aig, resyn, ps );
      aig = cleanup_dangling( aig );
    }
    else if ( ntk.is_type( network_manager_type::XAG ) )
    {
      using namespace mockturtle;
      sop_factoring<xag_network> resyn;
      network_manager::xag_names& xag = ntk.get_xag();
      refactoring( xag, resyn, ps );
      xag = cleanup_dangling( xag );
    }
    else if ( ntk.is_type( network_manager_type::MIG ) )
    {
      using namespace mockturtle;
      sop_factoring<mig_network> resyn;
      network_manager::mig_names& mig = ntk.get_mig();
      refactoring( mig, resyn, ps );
      mig = cleanup_dangling( mig );
    }
    else if ( ntk.is_type( network_manager_type::XMG ) )
    {
      using namespace mockturtle;
      sop_factoring<xmg_network> resyn;
      network_manager::xmg_names& xmg = ntk.get_xmg();
      refactoring( xmg, resyn, ps );
      xmg = cleanup_dangling( xmg );
    }
    else
    {
      env->err() << "[e] Network type is not supported by factoring.\n";
    }
  }

private:
  void reset_default_params()
  {
    max_pis = 6;
  }

private:
  uint32_t max_pis{ 6 };
};

ALICE_ADD_COMMAND( factoring, "Synthesis" );

} // namespace alice