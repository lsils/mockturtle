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
  \file lmap.hpp
  \brief LUT mapping command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/utils/name_utils.hpp>
#include <mockturtle/views/names_view.hpp>

namespace alice
{

class lmap_command : public alice::command
{
public:
  explicit lmap_command( const environment::ptr& env )
      : command( env,
                 "Performs technology mapping to LUTs." )
  {
    opts.add_option( "--cut_size,-K", ps.cut_enumeration_ps.cut_size, "Number of LUT inputs (2 <= C < 16) [default = 6]" );
    opts.add_option( "--cut_limit,-C", ps.cut_enumeration_ps.cut_limit, "Max number of priority cuts (2 <= C < 31) [default = 8]" );
    opts.add_option( "--delay,-D", ps.required_delay, "Set the delay constraints [default = best possible]" );
    opts.add_option( "--relax,-R", ps.relax_required, "Set delay relaxation in percentage [default = 0]" );
    opts.add_option( "--share,-S", ps.area_share_rounds, "Set number of area share rounds [default = 2]" );
    opts.add_option( "--flow,-F", ps.area_flow_rounds, "Set number of area flow rounds [default = 1]" );
    opts.add_option( "--ela,-A", ps.ela_rounds, "Set number of exact area rounds [default = 2]" );
    add_flag( "--area,-a", "Toggle performing area-oriented technology mapping [default = no]" );
    add_flag( "--edge,-e", "Toggle performing edge optimization [default = yes]" );
    add_flag( "--mffc,-m", "Toggle collapse of MFFCs [default = no]" );
    add_flag( "--recompute-cuts,-c", "Toggle recomputing cuts at each round [default = yes]" );
    add_flag( "--expand,-r", "Toggle cut expansion of the best cuts [default = yes]" );
    add_flag( "--truth,-t", "Toggle functional mapping [default = no]" );
    add_flag( "--dom,-d", "Toggle removing dominated cuts [default = yes]" );
    add_flag( "--verbose,-v", "Toggle verbose printout [default = no]" );
  }

protected:
  void execute()
  {
    ps.area_oriented_mapping = is_set( "area" );
    ps.edge_optimization = !is_set( "edge" );
    ps.collapse_mffcs = is_set( "mffc" );
    ps.recompute_cuts = !is_set( "recompute-cuts" );
    ps.cut_expansion = !is_set( "expand" );
    ps.remove_dominated_cuts = is_set( "dom" );
    ps.verbose = is_set( "verbose" );

    if ( store<network_manager>().empty() )
    {
      env->err() << "Empty logic network.\n";
      ps = mockturtle::lut_map_params{};
      return;
    }
    network_manager& ntk = store<network_manager>().current();

    switch ( ntk.get_current_type() )
    {
    case AIG:
      perform_mapping( ntk.get_aig(), is_set( "truth" ) );
      break;
    case XAG:
      perform_mapping( ntk.get_xag(), is_set( "truth" ) );
      break;
    case MIG:
      perform_mapping( ntk.get_mig(), is_set( "truth" ) );
      break;
    case XMG:
      perform_mapping( ntk.get_xmg(), is_set( "truth" ) );
      break;
    case KLUT:
      if ( klut_check_lut_size( ntk.get_klut() ) )
      {
        perform_mapping( ntk.get_klut(), is_set( "truth" ) );
      }
      else
      {
        env->err() << "[e] The current KLUT network is not supported for LUT mapping (e.g., run strash).\n";
      }
      break;
    case MAPPED:
    default:
      env->err() << "[e] Network type is not supported for LUT mapping (e.g., run strash).\n";
      break;
    }

    ps = mockturtle::lut_map_params{};
  }

private:
  template<typename Ntk>
  void perform_mapping( Ntk& ntk, bool compute_truth )
  {
    mockturtle::lut_map_stats st;
    if ( compute_truth )
    {
      mockturtle::klut_network res = mockturtle::lut_map<Ntk, true>( ntk, ps, &st );
      mockturtle::names_view res_names{ res };
      mockturtle::restore_network_name( ntk, res_names );
      mockturtle::restore_pio_names_by_order( ntk, res_names );
      store<network_manager>().current().load_klut( res_names );
    }
    else
    {
      mockturtle::klut_network res = mockturtle::lut_map<Ntk, false>( ntk, ps, &st );
      mockturtle::names_view res_names{ res };
      mockturtle::restore_network_name( ntk, res_names );
      mockturtle::restore_pio_names_by_order( ntk, res_names );
      store<network_manager>().current().load_klut( res_names );
    }
  }

  bool klut_check_lut_size( network_manager::klut_names const& klut )
  {
    uint32_t max_fanin = 0;
    klut.foreach_gate( [&]( auto const& n ) {
      max_fanin = std::max( max_fanin, klut.fanin_size( n ) );
    } );

    return max_fanin <= ps.cut_enumeration_ps.cut_size;
  }

private:
  mockturtle::lut_map_params ps{};
};

ALICE_ADD_COMMAND( lmap, "Mapping" );

} // namespace alice