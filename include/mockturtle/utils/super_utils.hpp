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
  \file tech_library.hpp
  \brief Implements utilities to create supergates for technology mapping

  \author Alessandro Tempia Calvino
*/

#pragma once

#include <cassert>
#include <unordered_map>
#include <vector>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/npn.hpp>
#include <kitty/print.hpp>
#include <kitty/static_truth_table.hpp>
#include <lorina/lorina.hpp>

#include "../io/genlib_reader.hpp"
#include "../io/super_reader.hpp"
#include "../traits.hpp"
#include "../utils/truth_table_cache.hpp"

namespace mockturtle
{

template<unsigned NInputs>
struct composed_gate
{
  uint32_t id;
  bool is_super{false};
  int32_t root_id{ -1 };
  kitty::dynamic_truth_table function;
  double area{ 0.0f };
  std::array<float, NInputs> tdelay{};
  std::vector<uint32_t> fanin{};
};

template<unsigned NInputs = 5u>
class supergate_utils
{
public:
  explicit supergate_utils( std::vector<gate> const& gates, super_lib const& supergates_spec = {} )
      : _gates( gates ),
        _supergates_spec( supergates_spec ),
        _supergates()
  {
    if ( _supergates_spec.supergates.size() == 0 )
    {
      compute_library_with_genlib();
    }
    else
    {
      generate_library_with_super();
    }
  }

  const std::vector<composed_gate<NInputs>>& get_super_library() const
  {
    return _supergates;
  }

public:
  void compute_library_with_genlib()
  {
    for ( const auto& g : _gates )
    {
      std::array<float, NInputs> pin_to_pin_delays{};

      auto i = 0u;
      for ( auto const& pin : g.pins )
      {
        /* use worst pin delay */
        pin_to_pin_delays[i] = std::max( pin.rise_block_delay, pin.fall_block_delay );
      }

      composed_gate<NInputs> s = {_supergates.size(),
                                  false,
                                  g.id,
                                  g.function,
                                  g.area,
                                  pin_to_pin_delays,
                                  {}};

      _supergates.emplace_back( s );
    }
  }

  void generate_library_with_super()
  {
    if ( _supergates_spec.max_num_vars > NInputs )
    {
      std::cerr << fmt::format(
        "ERROR: NInputs ({}) should be greater or equal than the max number of variables ({}) in the super file.\n", NInputs, _supergates_spec.max_num_vars
        );
      std::cerr << "WARNING: ignoring supergates, proceeding with standard library." << std::endl;
      compute_library_with_genlib();
      return;
    }

    /* create a map for the gates IDs */
    std::unordered_map<std::string, uint32_t> gates_map;

    for ( auto const& g : _gates )
    {
      if ( gates_map.find( g.name ) != gates_map.end() )
      {
        std::cerr << fmt::format( "WARNING: ignoring genlib gate {}, duplicated name entry.", g.name ) << std::endl;
      }
      else
      {
        gates_map[g.name] = g.id;
      }
    }

    /* creating input variables */
    for ( uint8_t i = 0; i < _supergates_spec.max_num_vars; ++i )
    {
      kitty::dynamic_truth_table tt{ NInputs };
      kitty::create_nth_var( tt, i );

      composed_gate<NInputs> s = {i,
                                  false,
                                  -1,
                                  tt,
                                  0.0f,
                                  {},
                                  {}};

      _supergates.emplace_back( s );
    }

    /* add supergates */
    for ( auto const g : _supergates_spec.supergates )
    {
      uint32_t root_match_id;
      if ( auto it = gates_map.find( g.name ); it != gates_map.end() )
      {
        root_match_id = it->second;
      }
      else
      {
        std::cerr << fmt::format( "WARNING: ignoring supergate {}, no reference in genlib.", g.id ) << std::endl;
        continue;
      }

      uint32_t num_vars = _gates[root_match_id].num_vars;

      if ( num_vars != g.fanins_id.size() )
      {
        std::cerr << fmt::format( "WARNING: ignoring supergate {}, wrong number of fanins.", g.id ) << std::endl;
        continue;
      }
      if ( num_vars > _supergates_spec.max_num_vars )
      {
        std::cerr << fmt::format( "WARNING: ignoring supergate {}, too many variables for the library settings.", g.id ) << std::endl;
        continue;
      }

      std::vector<uint32_t> sub_gates;

      bool error = false;
      for ( uint32_t f : g.fanins_id )
      {
        if ( f >= g.id + _supergates_spec.max_num_vars )
        {
          error = true;
          std::cerr << fmt::format( "WARNING: ignoring supergate {}, wrong fanins.", g.id ) << std::endl;
        }
        sub_gates.emplace_back( _supergates[f].id );
      }

      if ( error )
      {
        continue;
      }

      float area = compute_area( root_match_id, sub_gates );
      const auto tt = compute_truth_table( root_match_id, sub_gates );

      composed_gate<NInputs> s = {_supergates.size(),
                                  g.is_super,
                                  root_match_id,
                                  tt,
                                  area,
                                  {},
                                  sub_gates};

      compute_delay_parameters( s );

      _supergates.emplace_back( s );
    }

    /* add constants and single input gates which are not represented in SUPER */
    for ( auto& gate : _gates )
    {
      if ( gate.function.num_vars() == 0 )
      {
        /* constants */
        composed_gate<NInputs> s = {_supergates.size(),
                                    false,
                                    gate.id,
                                    gate.function,
                                    gate.area,
                                    {},
                                    {}};
        _supergates.emplace_back( s );
      }
      else if ( gate.function.num_vars() == 1 )
      {
        /* inverter or buffer */
        composed_gate<NInputs> s = {_supergates.size(),
                                    false,
                                    gate.id,
                                    gate.function,
                                    gate.area,
                                    {},
                                    {}};
        s.tdelay[0] = std::max( gate.pins[0].rise_block_delay, gate.pins[0].fall_block_delay );
        _supergates.emplace_back( s );
      }
    }
  }

private:
  inline float compute_area( uint32_t root_id, std::vector<uint32_t> const& sub_gates )
  {
    float area = _gates[root_id].area;
    for ( auto const& id : sub_gates )
    {
      area += _supergates[id].area;
    }

    return area;
  }

  inline kitty::dynamic_truth_table compute_truth_table( uint32_t root_id, std::vector<uint32_t> const& sub_gates )
  {
    std::vector<kitty::dynamic_truth_table> ttv;

    for ( auto const& id : sub_gates )
    {
      ttv.emplace_back( _supergates[id].function );
    }

    return kitty::compose_truth_table( _gates[root_id].function, ttv );
  }

  inline void compute_delay_parameters( composed_gate<NInputs>& s )
  {
    const auto& root = _gates[s.root_id];

    auto i = 0u;
    for ( auto const& pin : root.pins )
    {
      float worst_delay = std::max( pin.rise_block_delay, pin.fall_block_delay );

      compute_delay_pin_rec( s, _supergates[s.fanin[i++]], worst_delay );
    }
  }

  void compute_delay_pin_rec( composed_gate<NInputs>& root, composed_gate<NInputs>& s, float delay )
  {
    /* termination: input variable */
    if ( s.root_id == -1 )
    {
      root.tdelay[s.id] = std::max( root.tdelay[s.id], delay );
      return;
    }

    auto i = 0u;
    for ( auto const& pin : _gates[s.root_id].pins )
    {
      float worst_delay = delay + std::max( pin.rise_block_delay, pin.fall_block_delay );

      compute_delay_pin_rec( root, _supergates[s.fanin[i++]], worst_delay );
    }
  }

protected:
  std::vector<gate> const& _gates;
  super_lib const& _supergates_spec;
  std::vector<composed_gate<NInputs>> _supergates;
}; /* Class supergate_utils */

} /* namespace mockturtle */
