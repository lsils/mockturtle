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
  \file arewrite.hpp
  \brief Algebraic depth rewrite command

  \author Philippe Sauter
*/

#pragma once

#include <alice/alice.hpp>

#include <mockturtle/views/names_view.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting.hpp>
#include <mockturtle/algorithms/xag_algebraic_rewriting.hpp>
#include <mockturtle/algorithms/xmg_algebraic_rewriting.hpp>

namespace alice
{

class arewrite_command : public alice::command
{
public:
  explicit arewrite_command( const environment::ptr& env )
      : command( env,
                 "Performs algebraic depth rewriting (depth optimization)." )
  { 
    add_flag( "--prevent-increase", "Prevent area increase while optimizing depth. [default = no]" );
  }

protected:
  void execute()
  {
    if ( store<network_manager>().empty() )
    {
      env->err() << "Empty logic network.\n";
      return;
    }
    
    network_manager& ntk = store<network_manager>().current();
    switch (ntk.get_current_type())
    {
      case AIG: {
        mockturtle::xag_algebraic_depth_rewriting_params ps;
        ps.allow_area_increase = !is_set("prevent-increase");
        mockturtle::depth_view depth_xag{ ntk.get_xag() };
        mockturtle::xag_algebraic_depth_rewriting( depth_xag );
        } break;
      
      case XAG: {
        mockturtle::xag_algebraic_depth_rewriting_params ps;
        ps.allow_area_increase = !is_set("prevent-increase");
        mockturtle::depth_view depth_xag{ ntk.get_xag() };
        mockturtle::xag_algebraic_depth_rewriting( depth_xag );
        } break;

      case MIG: {
        mockturtle::mig_algebraic_depth_rewriting_params ps;
        ps.allow_area_increase = !is_set("prevent-increase");
        mockturtle::depth_view depth_mig{ ntk.get_mig() };
        mockturtle::mig_algebraic_depth_rewriting( depth_mig, ps );
        } break;

      case XMG: {
        mockturtle::xmg_algebraic_depth_rewriting_params ps;
        ps.allow_area_increase = !is_set("prevent-increase");
        mockturtle::depth_view depth_xmg{ ntk.get_xmg() };
        mockturtle::xmg_algebraic_depth_rewriting( depth_xmg, ps );
        } break;

      case KLUT:
      case MAPPED:
      default: 
        env->err() << "[e] Network type is not supported by balance.\n";
        break;
    }
  }
};

ALICE_ADD_COMMAND( arewrite, "Synthesis" );

} // namespace alice