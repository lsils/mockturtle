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
  \file balance.hpp
  \brief Balance command

  \author Philippe Sauter
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/algorithms/balancing.hpp>
#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/views/names_view.hpp>

namespace alice
{

class balance_command : public alice::command
{
private:
  mockturtle::lut_map_params ps;
  enum rebalance_type
  {
    SOP,
    ESOP
  } rebalance;

public:
  explicit balance_command( const environment::ptr& env )
      : command( env,
                 "Performs logic network balancing using dynamic-programming and cut-enumeration." )
  {
    add_flag( "--esop", "Use ESOP rebalancing function [default = yes]" );
    add_flag( "--sop", "Use SOP rebalancing function  [default = no]" );
    add_option( "--cut-size", ps.cut_enumeration_ps.cut_size, "Maximum number of leaves for a cut.  [default =  6]" );
    add_option( "--cut-limit", ps.cut_enumeration_ps.cut_limit, "Maximum number of cuts for a node.   [default = 8]" );
    add_option( "--fanin-limit", ps.cut_enumeration_ps.fanin_limit, "Maximum number of fanins for a node. [default = 10]" );
    add_option( "--use-dont-care", ps.cut_enumeration_ps.minimize_truth_table, "Prune cuts by removing don't cares.  [default = yes]" );
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

    if ( is_set( "sop" ) )
    {
      rebalance = SOP;
    }
    else
    {
      rebalance = ESOP;
    }

    ps.verbose = is_set( "verbose" ) || is_set( "w" );
    ps.cut_enumeration_ps.verbose = is_set( "verbose" ) || is_set( "w" );

    network_manager& ntk = store<network_manager>().current();
    switch ( ntk.get_current_type() )
    {
    case AIG:
    {
      network_manager::aig_names aig = balance<network_manager::aig_names>( ntk.get_aig() );
      ntk.load_aig( aig );
    }
    break;

    case XAG:
    {
      network_manager::xag_names xag = balance<network_manager::xag_names>( ntk.get_xag() );
      ntk.load_xag( xag );
    }
    break;

    case MIG:
    {
      network_manager::mig_names mig = balance<network_manager::mig_names>( ntk.get_mig() );
      ntk.load_mig( mig );
    }
    break;

    case XMG:
    {
      network_manager::xmg_names xmg = balance<network_manager::xmg_names>( ntk.get_xmg() );
      ntk.load_xmg( xmg );
    }
    break;

    case KLUT:
    {
      network_manager::klut_names klut = balance<network_manager::klut_names>( ntk.get_klut() );
      ntk.load_klut( klut );
    }
    break;

    default:
      env->err() << "[e] Network type is not supported by balance.\n";
      break;
    }
  }

private:
  template<typename Ntk>
  Ntk balance( Ntk& ntk )
  {
    Ntk res;

    if ( rebalance == SOP )
    {
      res = mockturtle::sop_balancing( ntk, ps );
    }
    else
    {
      res = mockturtle::esop_balancing( ntk, ps );
    }

    mockturtle::restore_network_name( ntk, res );
    mockturtle::restore_pio_names_by_order( ntk, res );

    return res;
  }
};

ALICE_ADD_COMMAND( balance, "Synthesis" );

} // namespace alice