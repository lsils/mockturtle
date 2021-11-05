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
  \brief Convert a k-LUT network into AIG, XAG, MIG or XMG.
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
#include <mockturtle/algorithms/node_resynthesis/xmg_npn.hpp>
#include "null.hpp"
#include <typeinfo>

namespace mockturtle
{
  namespace detail{

  template <class T>
  constexpr bool always_false = false;

  template<class NtkDest>
  const auto set_npn_resynthesis_fn()
  // declare the npn-resynthesis function to be used depending on the desired network type.
  {
    using aig_npn_type  = xag_npn_resynthesis<aig_network,xag_network,xag_npn_db_kind::aig_complete>;
    using xag_npn_type  = xag_npn_resynthesis<xag_network,xag_network,xag_npn_db_kind::xag_complete>;
    using mig_npn_type  = mig_npn_resynthesis;
    using xmg_npn_type  = xmg_npn_resynthesis;
    using null_npn_type = null_resynthesis<aig_network>;

    if constexpr ( std::is_same<typename NtkDest::base_type, aig_network>::value )
      return aig_npn_type{};
    else if constexpr (std::is_same<typename NtkDest::base_type, xag_network>::value )
      return xag_npn_type{};
    else if constexpr (std::is_same<typename NtkDest::base_type, mig_network>::value)
      return mig_npn_type{};
    else if constexpr (std::is_same<typename NtkDest::base_type, xmg_network>::value)
      return xmg_npn_type{};
    else 
      static_assert( always_false<NtkDest>, "NtkDest is not aig, xag, xmg nor mig" );
  }


  } // namespace detail

/*! \brief Convert a k-LUT network into AIG, XAG, MIG or XMG (out-of-place)
 *
 * This function is a wrapper function for resynthesizing a k-LUT network (type `NtkSrc`) into a
 * new graph (of type `NtkDest`). The new data structure can be of type AIG, XAG, MIG or XMG.
 * First the function attempts a Disjoint Support Decomposition (DSD), branching the network into subnetworks. 
 * As soon as DSD can no longer be done, there are two possibilities depending on the dimensionality of the 
 * subnetwork to be resynthesized. On the one hand, if the size of the associated support is lower or equal 
 * than 4, the solution can be recovered by exploiting the mapping of the subnetwork to its NPN-class.
 * On the other hand, if the support size is higher than 4, A Shannon decomposition is performed, branching 
 * the network in further subnetworks with reduced support.
 * Finally, once the threshold value of 4 is reached, the NPN mapping completes the graph definition.
 *
 * \tparam NtkDest Type of the destination network. Its base type should be either `aig_network`, `xag_network`, `mig_network`, or `xmg_network`.
 * \tparam NtkSrc Type of the source network. Its base type should be `klut_network`.
 * \param ntk_src Input k-lut network
 * \return An equivalent AIG, XAG, MIG or XMG network
 */

  template<class NtkDest, class NtkSrc>
  NtkDest convert_klut_to_graph( NtkSrc const& ntk_src, node_resynthesis_params const& ps = {}, node_resynthesis_stats* pst = nullptr )
  {
    static_assert( std::is_same<typename NtkSrc::base_type, klut_network>::value, "NtkSrc is not klut_network" );
    uint32_t threshold {4}; 
    auto fallback_npn = detail::set_npn_resynthesis_fn<NtkDest>();
    shannon_resynthesis<NtkDest,decltype(fallback_npn)> fallback_shannon(threshold ,&fallback_npn);
    dsd_resynthesis<NtkDest, decltype( fallback_shannon )> resyn( fallback_shannon );
    return node_resynthesis<NtkDest>( ntk_src, resyn, ps, pst );
  }

/*! \brief Convert a k-LUT network into AIG, XAG, MIG or XMG (in-place)
 *
 * The algorithmic details are the same as the out-of-place version.
 *
 * \tparam NtkDest Type of the destination network. Its base type should be either `aig_network`, `xag_network`, `mig_network`, or `xmg_network`.
 * \tparam NtkSrc Type of the source network. Its base type should be `klut_network`.
 * \param ntk_dest An empty AIG, XAG, MIG or XMG network to be constructed in-place
 * \param ntk_src Input k-lut network
 */
  template<class NtkDest, class NtkSrc>
  void convert_klut_to_graph(NtkDest& ntk_dest, NtkSrc const& ntk_src, node_resynthesis_params const& ps = {}, node_resynthesis_stats* pst = nullptr )
  {
    static_assert( std::is_same<typename NtkSrc::base_type, klut_network>::value, "NtkSrc is not klut_network" );
    uint32_t threshold {4}; 
    auto fallback_npn = detail::set_npn_resynthesis_fn<NtkDest>();
    shannon_resynthesis<NtkDest,decltype(fallback_npn)> fallback_shannon(threshold ,&fallback_npn);
    dsd_resynthesis<NtkDest, decltype( fallback_shannon )> resyn( fallback_shannon );
    node_resynthesis<NtkDest>(ntk_dest, ntk_src, resyn, ps, pst );
  }

} // namespace mockturtle
