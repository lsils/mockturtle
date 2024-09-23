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
  \file sim_resub.hpp
  \brief Simulation-guided resubstitution command

  \author Alessandro Tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/views/names_view.hpp>

namespace alice
{

class sim_resub_command : public alice::command
{
private:
  mockturtle::resubstitution_params ps;

public:
  explicit sim_resub_command( const environment::ptr& env )
      : command( env,
                 "Performs simulation-guided resubstitution." )
  {
    add_option( "--pis,-K", ps.max_pis, "Max number of PIs of reconvergence-driven cuts [default = 8]" );
    add_option( "--divisors,-D", ps.max_divisors, "Max number of divisors to consider [default = 150]" );
    add_option( "--inserts,-N", ps.max_inserts, "Max number of nodes added by resubstitution [default = 2]" );
    add_option( "--root-fanout,-M", ps.skip_fanout_limit_for_roots, "Max fanout of a node to be considered as root. [default = 1000]" );
    add_option( "--divisor-fanout,-G", ps.skip_fanout_limit_for_divisors, "Max fanout of a node to be considered as divisor. [default = 100]" );
    add_option( "--use-dont-care,-w", ps.use_dont_cares, "Use don't care for optimizations. [default = no]" );
    add_option( "--window-size,-W", ps.window_size, "Window size for don't care calculation [default = 12]" );
    add_option( "--odc-levels,-F", ps.window_size, "Number of fanout levels for ODC computation [default = 0]" );
    add_option( "--preserve-depth,-l", ps.preserve_depth, "Prevent from increasing depth.   [default = no]" );
    add_option( "--max-clauses,-C", ps.max_clauses, "Maximum number of clauses for SAT solving [default = 1000]" );
    add_option( "--conflit,-X", ps.conflict_limit, "Conflit limit for the SAT solver [default = 1000]" );
    add_option( "--max-trials,-T", ps.max_trials, "Maximum number of trials for resubstitution [default = 100]" );
    add_flag( "--verbose,-v", "Toggle verbose printout [default = no]" );
  }

protected:
  void execute()
  {
    if ( store<network_manager>().empty() )
    {
      env->err() << "Empty logic network.\n";
      return;
    }

    ps.verbose = is_set( "verbose" );

    network_manager& ntk = store<network_manager>().current();
    switch ( ntk.get_current_type() )
    {
    case AIG:
    {
      mockturtle::sim_resubstitution( ntk.get_aig(), ps );
      ntk.get_aig() = mockturtle::cleanup_dangling( ntk.get_aig() );
      break;
    }

    case XAG:
    {
      mockturtle::sim_resubstitution( ntk.get_xag(), ps );
      ntk.get_xag() = mockturtle::cleanup_dangling( ntk.get_xag() );
      break;
    }

    case MIG:
    {
      mockturtle::sim_resubstitution( ntk.get_mig(), ps );
      ntk.get_mig() = mockturtle::cleanup_dangling( ntk.get_mig() );
      break;
    }

    case XMG:
    case KLUT:
    case MAPPED:
    default:
      env->err() << "[e] Network type is not supported by sim_resub.\n";
      break;
    }

    /* reset params */
    ps = mockturtle::resubstitution_params{};
  }
};

ALICE_ADD_COMMAND( sim_resub, "Synthesis" );

} // namespace alice