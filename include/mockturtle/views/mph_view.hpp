/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
  \file mph_view.hpp
  \brief View that (1) adds stage and gate type information to a klut network and (2) allows explicit buffers to be inserted

  \author Rassul Bairamkulov
*/

#pragma once

#include <iostream>
#include <mockturtle/views/binding_view.hpp>

enum GateType : uint8_t 
{
    PI_GATE = 0u,
    AA_GATE = 1u,
    AS_GATE = 2u,
    SA_GATE = 3u,
    T1_GATE = 4u
};

namespace mockturtle
{
  
constexpr uint32_t _stage_mask = 0x1FFFFFFF;
constexpr uint32_t  _type_mask = 0xE0000000;

template<typename Ntk, uint8_t NUM_PHASES>
// template<uint8_t NUM_PHASES>
// class mph_view : public mockturtle::binding_view<typename Ntk>
class mph_view : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  using Ntk::set_value;
  using Ntk::_storage;
  using Ntk::_events;

  /// @brief Generic constructor – each gate is considered clocked
  /// @param ntk 
  explicit mph_view( Ntk ntk ) 
      : Ntk(ntk)
  {
    ntk.foreach_pi([&] (node pi)
    {
      set_type(pi, PI_GATE);
    });

    ntk.foreach_gate([&](node n)
    {
      set_type(n, AS_GATE);  
    });
  }

  /// @brief Constructor based on binding view. The type is inferred from the type_map
  /// @tparam type_map - map from 
  /// @param ntk 
  /// @param map 
  template<typename type_map>
  mph_view( mockturtle::binding_view<Ntk> ntk, type_map map ) 
      : Ntk(ntk)
  {
    ntk.foreach_pi([&] (node pi)
    {
      set_type(pi, PI_GATE);
    });

    ntk.foreach_gate([&](node n)
    {
      const auto & g = ntk.get_binding( n ); //determine the gate type
      set_type(n, map.at( g.name ));  //set the appropriate gate type 
    });
  }

  uint32_t get_stage( uint32_t index ) const
  {
    if (index <= 1)
    {
      return 0;
    }
    return _storage->nodes[index].data[0].h2 & _stage_mask;
  }

  uint32_t get_epoch( uint32_t index ) const
  {    
    if (index <= 1)
    {
      return 0;
    }
    return (_storage->nodes[index].data[0].h2 & _stage_mask) / NUM_PHASES;
  }

  uint32_t get_phase( uint32_t index ) const
  {
    if (index <= 1)
    {
      return 0;
    }
    return (_storage->nodes[index].data[0].h2 & _stage_mask) % NUM_PHASES;
  }

  void set_stage( uint32_t index, uint32_t stage ) const
  {
    _storage->nodes[index].data[0].h2 &= _type_mask;              // clear old stage
    _storage->nodes[index].data[0].h2 |= ( stage & _stage_mask ); // write new stage
  }

  void set_epoch( uint32_t index, uint32_t epoch ) const
  {
    auto phase = get_phase( index );
    uint32_t new_stage = epoch * NUM_PHASES + phase;
    set_stage( index, new_stage );
  }

  void set_phase( uint32_t index, uint32_t new_phase ) const
  {
    auto old_phase = get_phase( index );
    _storage->nodes[index].data[0].h2 += new_phase - old_phase ;
  }

  uint8_t get_type( uint32_t index ) const
  {
    return _storage->nodes[index].data[0].h2 >> 29;
  }

  void set_type( uint32_t index, uint8_t type ) const
  {
    _storage->nodes[index].data[0].h2 &= _stage_mask;  // clear old type
    _storage->nodes[index].data[0].h2 |= (type << 29); // write new type
  }

  std::tuple<uint32_t, uint8_t> get_stage_type( uint32_t index ) const
  {
    return std::make_tuple( _storage->nodes[index].data[0].h2 & _stage_mask, (_storage->nodes[index].data[0].h2 & _type_mask) >> 29 );
  }

  void set_stage_type( uint32_t index, uint32_t stage, uint8_t type ) const
  {
    _storage->nodes[index].data[0].h2 = (type << 29) | ( stage & _stage_mask );
  }

#pragma region Create arbitrary functions
  // signal _create_buffer( std::vector<signal> const& children, const uint32_t ID = 0) // , bool force = false, uint32_t h2 = 0u 

  signal explicit_buffer( signal const& a, uint8_t type ) // , bool force = false, uint32_t h2 = 0u 
  {
    typename storage::element_type::node_type node;


    node.children.push_back(a);
    node.data[1].h1 = 2;
    
    const auto index = _storage->nodes.size();
    _storage->nodes.push_back( node );
    _storage->hash[node] = index;

    /* increase ref-count to children */
    _storage->nodes[a].data[0].h1++;

    set_value( index, 0 );

    set_type( index, type );

    for ( auto const& fn : _events->on_add )
    {
      ( *fn )( index );
    }

    return index;
  }

    // kitty::dynamic_truth_table tt(1); 
    // kitty::create_nth_var( tt, 0 );
    // return this->_create_node( { a }, this->_storage->data.cache.insert( tt ) );
  
#pragma endregion Create arbitrary functions


#pragma region Visited flags

  // node.data[1].h2 is used for explicit buffers and cannot be used for visited flag

  void clear_visited() = delete;
  auto visited( node const& n ) = delete;
  void set_visited( node const& n, uint32_t v ) = delete;

#pragma endregion

};

}
