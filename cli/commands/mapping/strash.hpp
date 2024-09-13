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
  \file strash.hpp
  \brief Structural hashing command

  \author Philippe Sauter
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/names_view.hpp>
#include <mockturtle/views/mapping_view.hpp>
#include <mockturtle/algorithms/klut_to_graph.hpp>
#include <mockturtle/algorithms/collapse_mapped.hpp>

namespace alice
{

/* Writes a network to file */
class strash_command : public alice::command
{
private:
  network_manager_type ntk_type = AIG;

public:
  explicit strash_command( const environment::ptr& env )
      : command( env,
                 "Convert any network into a homogeneous graph network." )
  {
    add_flag( "--aig,-a", "Stores the network as an AIG (default without flags)" );
    add_flag( "--mig,-m", "Stores the network as an MIG" );
    add_flag( "--xag,-x", "Stores the network as an XAG" );
    add_flag( "--xmg,-g", "Stores the network as an XMG" );
  }

protected:
  void execute()
  {
    int ntk_type_count = 0;
    if( is_set( "aig" ) )  { ntk_type = AIG;  ntk_type_count++; }
    if( is_set( "mig" ) )  { ntk_type = MIG;  ntk_type_count++; }
    if( is_set( "xag" ) )  { ntk_type = XAG;  ntk_type_count++; }
    if( is_set( "xmg" ) )  { ntk_type = XMG;  ntk_type_count++; }
    if( is_set( "klut" ) ) { ntk_type = KLUT; ntk_type_count++; }

    if (ntk_type_count > 1) {
        env->err() << "[e] Multiple network types set. Network type flags are mututally exclusive.\n";
        return;
    }

    network_manager& ntk = store<network_manager>().current();

    // convert mapped networks first to k-LUT
    if ( ntk.is_type( MAPPED ) ) {
      auto const& mapped = ntk.get_mapped();
      mockturtle::mapping_view mapped_cell { mapped };
      auto const klut_optional = mockturtle::collapse_mapped_network<mockturtle::klut_network>( mapped_cell );
      
      if ( klut_optional ) {
          mockturtle::names_view klut_names { *klut_optional };
          ntk.load_klut( klut_names );
      } else {
          std::cerr << "[ERROR] Unable to collapse mapped network to k-LUT.\n";
      }
    }

    if ( ntk.is_type( KLUT ) ) {
      auto const& klut = ntk.get_klut();

      switch (ntk_type)
      {
        case AIG: {
          env->out() << "[i] convert to AIG.\n";
          mockturtle::aig_network res = mockturtle::convert_klut_to_graph<mockturtle::aig_network>( klut );
          mockturtle::names_view res_names { res };
          mockturtle::restore_network_name( klut, res_names );
          mockturtle::restore_pio_names_by_order( klut, res_names );
          ntk.load_aig( res_names );
          } break;

        case MIG: {
          env->out() << "[i] convert to MIG.\n";
          mockturtle::mig_network res = mockturtle::convert_klut_to_graph<mockturtle::mig_network>( klut );
          mockturtle::names_view res_names { res };
          mockturtle::restore_network_name( klut, res_names );
          mockturtle::restore_pio_names_by_order( klut, res_names );
          ntk.load_mig( res_names );
          } break;

        case XAG: {
          env->out() << "[i] convert to XAG.\n";
          mockturtle::xag_network res = mockturtle::convert_klut_to_graph<mockturtle::xag_network>( klut );
          mockturtle::names_view res_names { res };
          mockturtle::restore_network_name( klut, res_names );
          mockturtle::restore_pio_names_by_order( klut, res_names );
          ntk.load_xag( res_names );
          } break;

        case XMG: {
          env->out() << "[i] convert to XMG.\n";
          mockturtle::xmg_network res = mockturtle::convert_klut_to_graph<mockturtle::xmg_network>( klut );
          mockturtle::names_view res_names { res };
          mockturtle::restore_network_name( klut, res_names );
          mockturtle::restore_pio_names_by_order( klut, res_names );
          ntk.load_xmg( res_names );
          } break;

        default: break;
      }
    } else {
      env->err() << "[e] Network type support is not currently implemented in strash.\n";
    }
  }
};

ALICE_ADD_COMMAND( strash, "Mapping" );

} // namespace alice
