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
  \file emap.hpp
  \brief Emap technology mapping command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/algorithms/emap.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/block.hpp>
#include <mockturtle/utils/name_utils.hpp>
#include <mockturtle/views/names_view.hpp>

namespace alice
{

/* Writes a network to file */
class emap_command : public alice::command
{
public:
  explicit emap_command( const environment::ptr& env )
      : command( env,
                 "Performs technology mapping to standard cells." )
  {
    opts.add_option( "--match,-M", matching, "Type of matching (0: hybrid, 1: Boolean, 2: structural) [default = hybrid]" );
    opts.add_option( "--cut_limit,-C", cut_limit, "Max number of priority cuts (2 <= C < 20) [default = 16]" );
    opts.add_option( "--delay,-D", delay, "Set the delay constraints [default = best possible]" );
    opts.add_option( "--relax,-R", relax, "Set delay relaxation in percentage [default = 0]" );
    opts.add_option( "--flow,-F", flow, "Set number of area flow rounds [default = 3]" );
    opts.add_option( "--ela,-A", ela, "Set number of exact area rounds [default = 2]" );
    opts.add_option( "--switch,-S", eswp, "Set number of exact switching power rounds [default = 0]" );
    opts.add_option( "--patterns,-P", eswp, "Set number of patterns for switching activity [default = 2048]" );
    add_flag( "--area,-a", "Toggle performing area-oriented technology mapping [default = no]" );
    add_flag( "--multi,-m", "Toggle using multi-output cells [default = no]" );
    add_flag( "--alternatives,-l", "Toggle using alternative matches [default = yes]" );
    add_flag( "--dom,-d", "Toggle removing dominated cuts [default = no]" );
    add_flag( "--verbose,-v", "Toggle verbose printout [default = no]" );
    /* TODO: add other flags */
  }

protected:
  void execute()
  {
    mockturtle::emap_params ps;
    ps.cut_enumeration_ps.cut_limit = cut_limit;
    ps.area_oriented_mapping = is_set( "area" );
    ps.map_multioutput = is_set( "multi" );
    ps.required_time = delay;
    ps.relax_required = relax;
    ps.area_flow_rounds = flow;
    ps.ela_rounds = ela;
    ps.eswp_rounds = eswp;
    ps.switching_activity_patterns = patterns;
    ps.use_match_alternatives = !is_set( "alternatives" );
    ps.remove_dominated_cuts = is_set( "dom" );
    ps.verbose = is_set( "verbose" );

    if ( matching == 2 )
      ps.matching_mode = mockturtle::emap_params::structural;
    else if ( matching == 1 )
      ps.matching_mode = mockturtle::emap_params::boolean;
    else
      ps.matching_mode = mockturtle::emap_params::hybrid;

    reset_default_params();

    if ( store<tech_library_store>().empty() )
    {
      env->err() << "Empty technology library.\n";
      return;
    }
    if ( store<network_manager>().empty() )
    {
      env->err() << "Empty logic network.\n";
      return;
    }
    network_manager& ntk = store<network_manager>().current();
    if ( !ntk.is_type( network_manager_type::AIG ) )
    {
      env->err() << "[e] Only AIGs are supported for technology mapping.\n";
      return;
    }

    auto const& aig = ntk.get_aig();

    /* perform mapping */
    mockturtle::emap_stats st;

    /* optimize memory for Boolean matching */
    if ( ps.matching_mode == mockturtle::emap_params::boolean )
    {
      mockturtle::cell_view<mockturtle::block_network> res = mockturtle::emap<6u>( aig, *store<tech_library_store>().current(), ps, &st );

      if ( st.mapping_error )
      {
        env->err() << "[e] ABORT: an error occurred during mapping.\n";
        return;
      }

      /* restore names */
      mockturtle::names_view res_names{ res };
      mockturtle::restore_network_name( ntk.get_aig(), res_names );
      mockturtle::restore_pio_names_by_order( aig, res_names );

      /* store new network */
      ntk.load_mapped( res_names );
      return;
    }

    mockturtle::cell_view<mockturtle::block_network> res = mockturtle::emap<9u>( aig, *store<tech_library_store>().current(), ps, &st );

    if ( st.mapping_error )
    {
      env->err() << "[e] ABORT: an error occurred during mapping.\n";
      return;
    }

    /* restore names */
    mockturtle::names_view res_names{ res };
    mockturtle::restore_network_name( ntk.get_aig(), res_names );
    mockturtle::restore_pio_names_by_order( aig, res_names );

    /* store new network */
    ntk.load_mapped( res_names );
  }

private:
  void reset_default_params()
  {
    matching = 0;
    cut_limit = 16;
    delay = 0;
    relax = 0;
    flow = 3;
    ela = 2;
    eswp = 0;
    patterns = 2048u;
  }

private:
  uint32_t matching{ 0 };
  uint32_t cut_limit{ 16 };
  double delay{ 0 };
  double relax{ 0 };
  uint32_t flow{ 3 };
  uint32_t ela{ 2 };
  uint32_t eswp{ 0 };
  uint32_t patterns{ 2048u };
};

ALICE_ADD_COMMAND( emap, "Mapping" );

} // namespace alice