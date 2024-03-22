/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2023  EPFL
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
  \file rsfq_path_balancing.hpp
  \brief Path balancing util for superconducting electronics

  \author Alessandro Tempia Calvino
*/

#pragma once

#include <vector>

#include <kitty/operations.hpp>

#include "../../utils/node_map.hpp"
#include "../../views/binding_view.hpp"
#include "../../views/rsfq_view.hpp"

namespace mockturtle
{

namespace detail
{

template<class Ntk>
class rsfq_path_balancing_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using buffer_map = node_map<std::vector<signal>, Ntk>;

public:
  explicit rsfq_path_balancing_impl( Ntk const& ntk ) : _ntk( ntk )
  {}

  rsfq_view<Ntk> run()
  {
    auto [res, old2new] = initialize_copy_buf_network();

    load_dff_element();
    generate_buffered_network( res, old2new );

    return res;
  }

private:
  std::pair<rsfq_view<Ntk>, buffer_map> initialize_copy_buf_network()
  {
    buffer_map old2new( _ntk );
    Ntk res( _ntk.get_library() );

    old2new[_ntk.get_constant( false )].push_back( res.get_constant( false ) );
    if ( _ntk.get_node( _ntk.get_constant( true ) ) != _ntk.get_node( _ntk.get_constant( false ) ) )
    {
      old2new[_ntk.get_constant( true )].push_back( res.get_constant( true ) );
    }
    _ntk.foreach_pi( [&]( auto const& n ) {
      old2new[n].push_back( res.create_pi() );
    } );

    rsfq_view<Ntk> rsfq_res{ res };
    return {rsfq_res, old2new};
  }

  void load_dff_element()
  {
    for ( auto const& gate : _ntk.get_library() )
    {
      if ( gate.num_vars == 1 && kitty::is_const0( kitty::cofactor0( gate.function, 0 ) ) )
      {
        _buf_id = gate.id;
        return;
      }
    }
  }

  void generate_buffered_network( rsfq_view<Ntk>& res, buffer_map& old2new )
  {
    node_map<uint32_t, Ntk> delays( res, 0 );

    /* network is supposed to be stored in topo order */
    _ntk.foreach_gate( [&]( auto const& n ) {
      uint32_t max_delay = 0;

      /* compute arrival */
      gate const& g = _ntk.get_binding( n );
      _ntk.foreach_fanin( n, [&]( auto const& f, uint32_t i ) {
        uint32_t pin_delay = static_cast<uint32_t>( std::max( g.pins[i].rise_block_delay, g.pins[i].fall_block_delay ) );
        max_delay = std::max( max_delay, delays[old2new[f][0]] + pin_delay );
      } );

      std::vector<signal> children( _ntk.fanin_size( n ) );

      _ntk.foreach_fanin( n, [&]( auto const& f, uint32_t i ) {
        /* slack indicates the number of padding dffs */
        uint32_t pin_delay = static_cast<uint32_t>( std::max( g.pins[i].rise_block_delay, g.pins[i].fall_block_delay ) );
        uint32_t slack = max_delay - delays[old2new[f][0]] - pin_delay;
        auto& dffs = old2new[f];

        if ( dffs.size() <= slack )
        {
          /* create dffs to pad up to the slack */
          for ( auto j = dffs.size(); j <= slack; ++j )
          {
            /* create a buffer */
            auto const buf = create_dff( res, dffs[j - 1], delays );
            dffs.push_back( buf );
          }
        }

        assert( delays[dffs[slack]] + pin_delay == max_delay );
        children[i] = dffs[slack];
      } );

      uint32_t size = res.size();
      auto const new_node = res.clone_node( _ntk, n, children );
      old2new[n].push_back( new_node );
      delays.resize();
      delays[new_node] = max_delay;

      _worst_delay = std::max( _worst_delay, max_delay );

      res.add_binding( res.get_node( new_node ), _ntk.get_binding_index( n ) );
    } );

    /* pad POs based on circuit worst delay */
    _ntk.foreach_po( [&]( auto const& f ) {
      /* don't buffer constant POs */
      if ( _ntk.is_constant( _ntk.get_node( f ) ) )
      {
        res.create_po( old2new[f][0] );
        return; 
      }

      uint32_t slack = _worst_delay - delays[old2new[f][0]];
      auto& dffs = old2new[f];
      /* create dffs to pad up to the depth */
      auto i = 0u;
      for ( i = dffs.size(); i <= slack; ++i )
      {
        /* create a buffer */
        auto const buf = create_dff( res, dffs[i - 1], delays );
        dffs.push_back( buf );
      }
      res.create_po( old2new[f][i - 1] );
    } );

    assert( check_balancing( res, delays ) );
  }

  inline signal create_dff( rsfq_view<Ntk>& res, signal const& fanin, node_map<uint32_t, Ntk>& delays )
  {
    /* create dff */
    signal dff = res._create_node( { fanin }, 0x2 );
    res.add_binding( res.get_node( dff ), _buf_id );
    res.set_dff( res.get_node( dff ) );

    /* compute delay */
    delays.resize();
    gate const& g = res.get_binding( dff );
    delays[dff] = delays[fanin] + static_cast<uint32_t>( std::max( g.pins[0].rise_block_delay, g.pins[0].fall_block_delay ) );

    return dff;
  }

  bool check_balancing( rsfq_view<Ntk>& res, node_map<uint32_t, Ntk> const& delays )
  {
    /* check fanin balancing */
    bool correct = true;
    res.foreach_gate( [&]( auto const& n ) {
      gate const& g = res.get_binding( n );
      res.foreach_fanin( n, [&]( auto const& f, uint32_t i ) {
        uint32_t pin_delay = static_cast<uint32_t>( std::max( g.pins[i].rise_block_delay, g.pins[i].fall_block_delay ) );
        uint32_t fanin_delay = delays[f] + pin_delay;

        if ( delays[n] != fanin_delay )
        {
          correct = false;
        }
        return correct;
      } );
      return correct;
    } );

    /* check balanced POs */
    if ( correct )
    {
      res.foreach_po( [&]( auto const& f ) {
        if ( res.is_constant( res.get_node( f ) ) )
          return true;
        if ( delays[f] != _worst_delay )
        {
          correct = false;
        }
        return correct;
      } );
    }

    return correct;
  }

private:
  uint32_t _buf_id{ 0 };
  uint32_t _worst_delay{ 0 };
  
  Ntk const& _ntk;
};

} // namespace detail

/*! \brief Path balancing for RSFQ.
 *
 * This function does path balancing according
 * to the RSFQ technology constraints:
 * - Inserts padding DFFs to balance nodes' fanin
 * - Insert padding DFFs to balance POs
 *
 * \param ntk Mapped network
 */
template<class Ntk>
rsfq_view<Ntk> rsfq_path_balancing( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_has_binding_v<Ntk>, "Ntk does not implement the has_binding method" );

  detail::rsfq_path_balancing_impl p( ntk );
  return p.run();
}

/*! \brief Check path balancing for RSFQ.
 *
 * This function checks path balancing according
 * to the RSFQ technology constraints:
 * - Checks nodes are balanced
 * - Checks POs are balanced
 *
 * \param ntk Network
 */
template<class Ntk>
bool rsfq_check_buffering( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_has_binding_v<Ntk>, "Ntk does not implement the has_binding method" );

  bool result = true;
  uint32_t worst_delay = 0;
  node_map<uint32_t, Ntk> delays( ntk, 0 );

  /* check path balancing */
  ntk.foreach_gate( [&]( auto const& n ) {
    gate const& g = ntk.get_binding( n );

    /* compute delay */
    ntk.foreach_fanin( n, [&]( auto const& f, uint32_t i ) {
      uint32_t pin_delay = static_cast<uint32_t>( std::max( g.pins[i].rise_block_delay, g.pins[i].fall_block_delay ) );
      delays[n] = std::max( delays[n], delays[f] + pin_delay );
    } );

    /* verify path balancing */
    ntk.foreach_fanin( n, [&]( auto const& f, uint32_t i ) {
      uint32_t pin_delay = static_cast<uint32_t>( std::max( g.pins[i].rise_block_delay, g.pins[i].fall_block_delay ) );
      uint32_t fanin_delay = delays[f] + pin_delay;

      if ( delays[n] != fanin_delay )
        result = false;

      return result;
    } );

    worst_delay = std::max( worst_delay, delays[n] );
    return result;
  } );

  /* check balanced POs */
  if ( result )
  {
    ntk.foreach_po( [&]( auto const& f ) {
      /* don't check constant POs */
      if ( ntk.is_constant( ntk.get_node( f ) ) )
        return true;
      if ( delays[f] != worst_delay )
      {
        result = false;
      }
      return result;
    } );
  }

  return result;
}

} // namespace mockturtle
