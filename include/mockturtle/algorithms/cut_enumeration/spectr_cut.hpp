/* mockturtle: C++ logic network library
 * Copyright (C) 2018  EPFL
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
  \file mf_cut.hpp
  \brief Cut enumeration for MF mapping (see giaMf.c)

  \author Giulia Meuli
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "../cut_enumeration.hpp"
#include "../lut_mapping.hpp"
#include <kitty/spectral.hpp>
#include <kitty/static_truth_table.hpp>
#include <kitty/constructors.hpp>
#include <mockturtle/utils/cuts.hpp>

namespace mockturtle::detail
{
  inline uint64_t odd_bits() 
  {
    uint64_t count = 2;
    while( ( (count >> 63) & 1 ) != 1 )
      count |= count << 2;  
    return count;
  }
}

namespace mockturtle
{

struct cut_enumeration_spectr_cut
{
  uint32_t delay{0};
  float flow{0};
  float cost{0};
};

template<bool ComputeTruth>
bool operator<( cut_type<ComputeTruth, cut_enumeration_spectr_cut> const& c1, cut_type<ComputeTruth, cut_enumeration_spectr_cut> const& c2 )
{
  constexpr auto eps{0.005f};
  if ( c1->data.flow < c2->data.flow - eps )
    return true;
  if ( c1->data.flow > c2->data.flow + eps )
    return false;
  if ( c1->data.delay < c2->data.delay )
    return true;
  if ( c1->data.delay > c2->data.delay )
    return false;
  return c1.size() < c2.size();
}

template<typename Ntk>
void rec_core(Ntk const& ntk, node<Ntk> const& n, std::vector<uint32_t>& l)
{
  ntk.foreach_fanin(n, [&] (auto ch)
  {
    auto node_ch = ntk.get_node(ch);
    if(ntk.is_xor(node_ch))
      rec_core(ntk, node_ch,l);

    if(!ntk.is_xor(node_ch))
      l.push_back(node_ch);
  });
}

template <typename Ntk>
std::vector<uint32_t> grow_xor_cut(Ntk const& ntk, node<Ntk> const& n)
{
  std::vector<uint32_t> leaves;
  rec_core(ntk, n, leaves);

  std::sort( leaves.begin(), leaves.end() );
  leaves.erase( unique( leaves.begin(), leaves.end() ), leaves.end() );
  return leaves;
}


template<>
struct lut_mapping_update_cuts<cut_enumeration_spectr_cut>
{
  template<typename NetworkCuts, typename Ntk>
  static void apply( NetworkCuts& cuts, Ntk const& ntk )
  {
    std::vector<node<Ntk>> reverse_topo;
    topo_view<Ntk>( ntk ).foreach_node( [&]( auto n ) {
      reverse_topo.insert(reverse_topo.begin(), n);
    } );

    for(auto n : reverse_topo) 
    {
      if(ntk.is_xor(n))
      {
        //std::cout << "selected xor node: " << n; 
        const auto index = ntk.node_to_index( n );
        auto cut_set = cuts.cuts(index);

        /* clear the cut set of the node */
        cut_set.clear();
        
        /* add an empty cut and modify its leaves */
        auto leaves = grow_xor_cut(ntk, n);
        auto my_cut = cut_set.add_cut(leaves.begin(), leaves.end());

        /* set to zero cost */        
        my_cut->data.cost = 0u;

        /* crate cut truth table */
        kitty::dynamic_truth_table tt (leaves.size());
        kitty::create_symmetric( tt, detail::odd_bits());   
        my_cut -> func_id = cuts.insert_truth_table(tt);

      }
    }
  }
}; 


template<>
struct cut_enumeration_update_cut<cut_enumeration_spectr_cut>
{
  template<typename Cut, typename NetworkCuts, typename Ntk>
  static void apply( Cut& cut, NetworkCuts const& cuts, Ntk const& ntk, node<Ntk> const& n )
  {
    uint32_t delay{0};

    auto tt = cuts.truth_table( cut );
    auto spectrum = kitty::rademacher_walsh_spectrum( tt );
    cut->data.cost = std::count_if( spectrum.begin(), spectrum.end(), []( auto s ) { return s != 0; } );

    float flow = cut.size() < 2 ? 0.0f : 1.0f;
    for ( auto leaf : cut )
    {
      const auto& best_leaf_cut = cuts.cuts( leaf )[0];
      delay = std::max( delay, best_leaf_cut->data.delay );
      flow += best_leaf_cut->data.flow;
    }

    cut->data.delay = 1 + delay;
    cut->data.flow = flow / ntk.fanout_size( n );
  }
};

template<int MaxLeaves>
std::ostream& operator<<( std::ostream& os, cut<MaxLeaves, cut_data<false, cut_enumeration_spectr_cut>> const& c )
{
  os << "{ ";
  std::copy( c.begin(), c.end(), std::ostream_iterator<uint32_t>( os, " " ) );
  os << "}, D = " << std::setw( 3 ) << c->data.delay << " A = " << c->data.flow;
  return os;
}

} // namespace mockturtle
