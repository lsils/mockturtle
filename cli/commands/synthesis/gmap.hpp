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
  \file gmap.hpp
  \brief Graph mapping command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <limits>

#include <alice/alice.hpp>

#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/utils/name_utils.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/names_view.hpp>

namespace alice
{

/* Writes a network to file */
class gmap_command : public alice::command
{
public:
  explicit gmap_command( const environment::ptr& env )
      : command( env,
                 "Performs technology-independent mapping of the logic network." )
  {
    add_flag( "--aig,-a", "Maps to an AIG" );
    add_flag( "--mig,-m", "Maps to an MIG" );
    add_flag( "--xag,-x", "Maps to an XAG" );
    add_flag( "--xmg,-g", "Maps to an XMG" );
    opts.add_option( "--cut_limit,-C", cut_limit, "Max number of priority cuts (2 <= C < 50) [default = 49]" );
    opts.add_option( "--required,-R", required, "Set the required time constraints [default = best possible]" );
    opts.add_option( "--flow,-F", flow, "Set number of area flow rounds [default = 1]" );
    opts.add_option( "--ela,-A", ela, "Set number of exact area rounds [default = 2]" );
    opts.add_option( "--slimit,-S", share_limit, "Set number of cuts to compute exact area sharing [default = 1]" );
    add_flag( "--depth,-d", "Skip minimal depth mapping [default = no]" );
    add_flag( "--size,-i", "Minimize size with unconstrained depth [default = no]" );
    add_flag( "--share,-s", "Use costing based on exact area sharing [default = no]" );
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

    mockturtle::map_params ps;
    ps.cut_enumeration_ps.cut_limit = cut_limit;
    if ( is_set( "size" ) )
    {
      ps.skip_delay_round = true;
      ps.required_time = std::numeric_limits<float>::max();
    }
    else if ( is_set( "depth" ) )
    {
      ps.skip_delay_round = true;
    }
    else if ( required > 0 )
    {
      ps.required_time = required;
    }
    ps.area_flow_rounds = flow;
    ps.ela_rounds = ela;
    ps.enable_logic_sharing = is_set( "share" );
    ps.logic_sharing_cut_limit = share_limit;
    ps.verbose = is_set( "verbose" );

    reset_default_params();

    network_manager& ntk = store<network_manager>().current();
    if ( ntk.is_type( network_manager_type::AIG ) )
    {
      using namespace mockturtle;
      network_manager::aig_names& aig = ntk.get_aig();

      if ( is_set( "xag" ) )
      {
        exact_library<xag_network>& lib = _mockturtle_global.exact_lib_man.get_xag_library();
        xag_network res = map( aig, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( aig, res_names );
        mockturtle::restore_pio_names_by_order( aig, res_names );
        ntk.load_xag( res_names );
      }
      else if ( is_set( "mig" ) )
      {
        exact_library<mig_network>& lib = _mockturtle_global.exact_lib_man.get_mig_library();
        mig_network res = map( aig, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( aig, res_names );
        mockturtle::restore_pio_names_by_order( aig, res_names );
        ntk.load_mig( res_names );
      }
      else if ( is_set( "xmg" ) )
      {
        exact_library<xmg_network>& lib = _mockturtle_global.exact_lib_man.get_xmg_library();
        xmg_network res = map( aig, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( aig, res_names );
        mockturtle::restore_pio_names_by_order( aig, res_names );
        ntk.load_xmg( res_names );
      }
      else
      {
        exact_library<aig_network>& lib = _mockturtle_global.exact_lib_man.get_aig_library();
        aig_network res = map( aig, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( aig, res_names );
        mockturtle::restore_pio_names_by_order( aig, res_names );
        ntk.load_aig( res_names );
      }
    }
    else if ( ntk.is_type( network_manager_type::XAG ) )
    {
      using namespace mockturtle;
      network_manager::xag_names& xag = ntk.get_xag();

      if ( is_set( "aig" ) )
      {
        exact_library<aig_network>& lib = _mockturtle_global.exact_lib_man.get_aig_library();
        aig_network res = map( xag, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( xag, res_names );
        mockturtle::restore_pio_names_by_order( xag, res_names );
        ntk.load_aig( res_names );
      }
      else if ( is_set( "mig" ) )
      {
        exact_library<mig_network>& lib = _mockturtle_global.exact_lib_man.get_mig_library();
        mig_network res = map( xag, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( xag, res_names );
        mockturtle::restore_pio_names_by_order( xag, res_names );
        ntk.load_mig( res_names );
      }
      else if ( is_set( "xmg" ) )
      {
        exact_library<xmg_network>& lib = _mockturtle_global.exact_lib_man.get_xmg_library();
        xmg_network res = map( xag, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( xag, res_names );
        mockturtle::restore_pio_names_by_order( xag, res_names );
        ntk.load_xmg( res_names );
      }
      else
      {
        exact_library<xag_network>& lib = _mockturtle_global.exact_lib_man.get_xag_library();
        xag_network res = map( xag, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( xag, res_names );
        mockturtle::restore_pio_names_by_order( xag, res_names );
        ntk.load_xag( res_names );
      }
    }
    else if ( ntk.is_type( network_manager_type::MIG ) )
    {
      using namespace mockturtle;
      network_manager::mig_names& mig = ntk.get_mig();

      if ( is_set( "aig" ) )
      {
        exact_library<aig_network>& lib = _mockturtle_global.exact_lib_man.get_aig_library();
        aig_network res = map( mig, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( mig, res_names );
        mockturtle::restore_pio_names_by_order( mig, res_names );
        ntk.load_aig( res_names );
      }
      else if ( is_set( "xag" ) )
      {
        exact_library<xag_network>& lib = _mockturtle_global.exact_lib_man.get_xag_library();
        xag_network res = map( mig, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( mig, res_names );
        mockturtle::restore_pio_names_by_order( mig, res_names );
        ntk.load_xag( res_names );
      }
      else if ( is_set( "xmg" ) )
      {
        exact_library<xmg_network>& lib = _mockturtle_global.exact_lib_man.get_xmg_library();
        xmg_network res = map( mig, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( mig, res_names );
        mockturtle::restore_pio_names_by_order( mig, res_names );
        ntk.load_xmg( res_names );
      }
      else
      {
        exact_library<mig_network>& lib = _mockturtle_global.exact_lib_man.get_mig_library();
        mig_network res = map( mig, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( mig, res_names );
        mockturtle::restore_pio_names_by_order( mig, res_names );
        ntk.load_mig( res_names );
      }
    }
    else if ( ntk.is_type( network_manager_type::XMG ) )
    {
      using namespace mockturtle;
      network_manager::xmg_names& xmg = ntk.get_xmg();

      if ( is_set( "aig" ) )
      {
        exact_library<aig_network>& lib = _mockturtle_global.exact_lib_man.get_aig_library();
        aig_network res = map( xmg, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( xmg, res_names );
        mockturtle::restore_pio_names_by_order( xmg, res_names );
        ntk.load_aig( res_names );
      }
      else if ( is_set( "xag" ) )
      {
        exact_library<xag_network>& lib = _mockturtle_global.exact_lib_man.get_xag_library();
        xag_network res = map( xmg, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( xmg, res_names );
        mockturtle::restore_pio_names_by_order( xmg, res_names );
        ntk.load_xag( res_names );
      }
      else if ( is_set( "mig" ) )
      {
        exact_library<mig_network>& lib = _mockturtle_global.exact_lib_man.get_mig_library();
        mig_network res = map( xmg, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( xmg, res_names );
        mockturtle::restore_pio_names_by_order( xmg, res_names );
        ntk.load_mig( res_names );
      }
      else
      {
        exact_library<xmg_network>& lib = _mockturtle_global.exact_lib_man.get_xmg_library();
        xmg_network res = map( xmg, lib, ps );
        mockturtle::names_view res_names{ res };
        mockturtle::restore_network_name( xmg, res_names );
        mockturtle::restore_pio_names_by_order( xmg, res_names );
        ntk.load_xmg( res_names );
      }
    }
    else
    {
      env->err() << "[e] Network type support is not currently implemented in gmap.\n";
    }
  }

private:
  void reset_default_params()
  {
    matching = 0;
    cut_limit = 16;
    required = 0;
    flow = 1;
    ela = 2;
    share_limit = 1;
  }

private:
  uint32_t matching{ 0 };
  uint32_t cut_limit{ 49 };
  double required{ 0 };
  uint32_t flow{ 1 };
  uint32_t ela{ 2 };
  uint32_t share_limit{ 1u };
};

ALICE_ADD_COMMAND( gmap, "Synthesis" );

} // namespace alice