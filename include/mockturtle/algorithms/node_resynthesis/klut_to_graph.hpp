/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
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
  \file klut_to_graph.hpp
  \brief Resynthesis of a k-LUT to AIG, XAG or MIG. Combines disjoint support decomposition, shannon decomposition and NPN mapping
  *

  \author Andrea Costamagna
*/

# pragma once
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/dsd.hpp>
#include <mockturtle/algorithms/node_resynthesis/shannon.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include "null.hpp"
#include <typeinfo>

namespace mockturtle
{
/*! Resynthesis of a k-LUT to AIG, XAG or MIG in three steps. Combines disjoint support decomposition, shannon decomposition and NPN mapping
  *
  * This resynthesis function combines three techniques for resynthesising a k-LUT network into one of the three following networks: AIG, XAG or MIG.
  * First a Disjoint Support Decomposition (DSD) is performed, mapping the original logic function into a set of subfunctions with disjoint supports
  * F(x1,x2,...,xN) = F(G1(x1,x2..),G2(xi,..),...,GN(xk,...)).
  * Each Gk function is either an elementary logic function or is no longer DSD-decomposable.
  * In the latter case a shannon decomposition is called, with a threshold set to 4 and a fallback NPN resynthesis function. 
  * This is to say that, if the size of the support of the Gk function is lower or equal than 4, the solution is taken from the NPN database.
  * Otherwise, a shannon decomposition is performed, decomposing Gk into subfunctions with reduced support. As soon as the domain of the subfunction
  * reaches 4 the NPN-based network is substituted to the subfunction
  * 
   \verbatim embed:rst

   Example
    names_view<klut_network> klut_ntk;
    if ( lorina::read_blif( testcase + ".blif", blif_reader( klut_ntk ) ) != lorina::return_code::success )
    {
      std::cout << "read <testcase>.blif failed!\n";
      return -1;
    }

    using ntk1 = aig_network;
    using ntk2 = xag_network;
    using ntk3 = mig_network;
     
    ntk1 aig;
    ntk2 xag;
    ntk3 mig;

    aig = klut_to_graph_converter( klut_ntk );    // inline version is klut_to_graph_converter(aig, klut_ntk );
    xag = klut_to_graph_converter( klut_ntk );    // inline version is klut_to_graph_converter(xag, klut_ntk );
    mig = klut_to_graph_converter( klut_ntk );    // inline version is klut_to_graph_converter(mig, klut_ntk );
   \endverbatim
 *
 */
  // define some aliases for the resynthesis functions. Useful for the make function
  using aig_npn_type  = xag_npn_resynthesis<aig_network,xag_network,xag_npn_db_kind::aig_complete>;
  using xag_npn_type  = xag_npn_resynthesis<xag_network,xag_network,xag_npn_db_kind::xag_complete>;
  using mig_npn_type  = mig_npn_resynthesis;
  using null_npn_type = null_resynthesis<aig_network>;

  template<class NtkDest>
  const auto make(uint32_t &threshold)
  // declare the type of the npn-resynthesis function depending on the desired network type
  {
    if constexpr ( std::is_same<NtkDest, aig_network>::value )
      return aig_npn_type{};
    else if constexpr (std::is_same<NtkDest, xag_network>::value )
      return xag_npn_type{};
    else if constexpr (std::is_same<NtkDest, mig_network>::value)
      return mig_npn_type{};
    else 
    {
      threshold = 0; // if the threshold is not specified the npn fallback is the null resynthesis function
      return null_npn_type{};
    }

  }


  template<class NtkDest>
  NtkDest klut_to_graph_converter( klut_network const& kLUT, node_resynthesis_params const& ps = {}, node_resynthesis_stats* pst = nullptr )
  /* function performing the kLUT to AIG/XAG/MIG mapping 
  * INPUTS : kLUT- type network and same parameters that can be passed to the node_resynthesis function
  * OUTPUT : AIG/XAG/MIG network, depending on NtkDest
  */
  {
    uint32_t threshold {4}; 
    auto fallback_npn = make<NtkDest>(threshold);
    shannon_resynthesis<NtkDest,decltype(fallback_npn)> fallback_shannon(threshold ,&fallback_npn);
    dsd_resynthesis<NtkDest, decltype( fallback_shannon )> resyn( fallback_shannon );
    return node_resynthesis<NtkDest>( kLUT, resyn, ps, pst );
  }

  template<class NtkDest>
  void klut_to_graph_converter(NtkDest& ntk_dest, klut_network const& kLUT, node_resynthesis_params const& ps = {}, node_resynthesis_stats* pst = nullptr )
  /* inline function performing the kLUT to AIG/XAG/MIG mapping 
  * INPUTS : destination network, kLUT- type network and same parameters that can be passed to the node_resynthesis function
  */
  {
    uint32_t threshold {4}; 
    auto fallback_npn = make<NtkDest>(threshold);
    shannon_resynthesis<NtkDest,decltype(fallback_npn)> fallback_shannon(threshold ,&fallback_npn);
    dsd_resynthesis<NtkDest, decltype( fallback_shannon )> resyn( fallback_shannon );
    node_resynthesis<NtkDest>(ntk_dest, kLUT, resyn, ps, pst );
  }

}