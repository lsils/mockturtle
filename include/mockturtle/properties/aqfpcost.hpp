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
  \file aqfpcost.hpp
  \brief Cost functions for AQFP networks

  \author Dewmini Marakkalage 
*/

#pragma once

#include <algorithm>
#include <limits>

#include "../utils/hash_functions.hpp"
#include "../views/fanout_view.hpp"

namespace mockturtle
{

/*! \brief Cost function for computing the best splitter and buffer cost for a fanout net with given relative levels. */
class balanced_fanout_net_cost
{
public:
  static constexpr double IMPOSSIBLE = std::numeric_limits<double>::infinity();

  balanced_fanout_net_cost( const std::unordered_map<uint32_t, double>& splitters )
      : buffer_cost( splitters.at( 1u ) ), splitters( remove_buffer( splitters ) )
  {
  }

  double operator()( const std::vector<uint32_t>& config )
  {
    return cost_for_config( config );
  }

private:
  double buffer_cost;
  std::unordered_map<uint32_t, double> splitters;
  std::unordered_map<std::vector<uint32_t>, double, hash<std::vector<uint32_t>>> cache;

  static std::unordered_map<uint32_t, double> remove_buffer( std::unordered_map<uint32_t, double> splitters )
  {
    splitters.erase( 1u );
    return splitters;
  }

  double cost_for_config( const std::vector<uint32_t> config )
  {
    if ( config.size() == 1 )
    {
      if ( config[0] >= 1 )
      {
        return ( config[0] - 1 ) * buffer_cost;
      }
      else
      {
        return IMPOSSIBLE;
      }
    }

    if ( cache.count( config ) )
    {
      return cache[config];
    }

    auto result = IMPOSSIBLE;

    for ( const auto& s : splitters )
    {
      for ( auto size = 2u; size <= std::min( s.first, uint32_t( config.size() ) ); size++ )
      {
        auto sp_lev = config[config.size() - size] - 1;
        if ( sp_lev == 0 )
        {
          continue;
        }

        auto temp = s.second;

        for ( auto i = config.size() - size; i < config.size(); i++ )
        {
          temp += ( config[i] - config[config.size() - size] ) * buffer_cost;
        }

        std::vector<uint32_t> new_config( config.begin(), config.begin() + ( config.size() - size ) );
        new_config.push_back( sp_lev );
        std::sort( new_config.begin(), new_config.end() );

        temp += cost_for_config( new_config );

        if ( temp < result )
        {
          result = temp;
        }
      }
    }

    return ( cache[config] = result );
  }
};

/*! \brief Cost function for computing the cost of a path-balanced AQFP network with a given assignment of node levels. 
  *
  * Assumes no path balancing or splitters are needed for primary inputs or register outputs. 
  */
struct aqfp_network_cost
{
  static constexpr double IMPOSSIBLE = std::numeric_limits<double>::infinity();

  aqfp_network_cost( const std::unordered_map<uint32_t, double>& gate_costs, const std::unordered_map<uint32_t, double>& splitters )
      : gate_costs( gate_costs ), fanout_cc( splitters ) {}

  template<typename Ntk, typename LevelMap>
  double operator()( const Ntk& ntk, const LevelMap& level_of_node, uint32_t critical_po_level )
  {
    fanout_view dest_fv{ ntk };
    auto gate_cost = 0.0;
    auto fanout_net_cost = 0.0;

    std::vector<node<Ntk>> internal_nodes;
    dest_fv.foreach_node( [&]( auto n ) {
      if ( dest_fv.is_constant( n ) || dest_fv.is_pi( n ) )
      {
        return;
      }

      if ( n > 0u && dest_fv.is_maj( n ) )
      {
        internal_nodes.push_back( n );
      }
    } );

    for ( auto n : internal_nodes )
    {
      gate_cost += gate_costs.at( ntk.fanin_size( n ) );

      std::vector<uint32_t> rellev;

      dest_fv.foreach_fanout( n, [&]( auto fo ) {
        assert( level_of_node.at( fo ) > level_of_node.at( n ) );
        rellev.push_back( level_of_node.at( fo ) - level_of_node.at( n ) );
      } );

      uint32_t pos = 0u;
      while ( rellev.size() < dest_fv.fanout_size( n ) )
      {
        pos++;
        rellev.push_back( critical_po_level - level_of_node.at( n ) );
      }

      if ( rellev.size() > 1u || ( rellev.size() == 1u && rellev[0] > 0 ) )
      {
        std::sort( rellev.begin(), rellev.end() );
        fanout_net_cost += fanout_cc( rellev );
      }
    }

    return gate_cost + fanout_net_cost;
  }

private:
  std::unordered_map<uint32_t, double> gate_costs;
  balanced_fanout_net_cost fanout_cc;
};

} // namespace mockturtle