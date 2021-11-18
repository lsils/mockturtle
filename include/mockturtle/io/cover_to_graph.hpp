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
  \file cover_to_graph.hpp
  \brief transforms a cover data structure into another network type
  \author Andrea Costamagna
*/

#pragma once

#include <iostream>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/cover.hpp>
#include <string>
#include <vector>

namespace mockturtle
{

namespace detail
{
/*! \brief signals_connector
 * This struct allows to map any signal in the new network to the output of a node in the old one.
 */
template<class Ntk>
struct signals_connector
{
  signals_connector()
  {
    signals.reserve( 10000u );
  }
  void insert( signal<Ntk> signal_ntk, uint64_t node_index )
  {
    signals[node_index] = signal_ntk;
  }

  std::unordered_map<uint64_t, signal<Ntk>> signals;
};



template<class Ntk>
class cover_to_graph_converter
{

  using type_cover_signals = std::vector<uint64_t>;

public:
  cover_to_graph_converter( Ntk& ntk, cover_network& cover_ntk )
      : _ntk( ntk ),
        _cover_ntk( cover_ntk )
  {
  }

#pragma region recursive functions
  signal<Ntk> recursive_or( const std::vector<signal<Ntk>>& signals )
  {
    if ( signals.size() == 1 )
    {
      return signals[0];
    }
    else if ( signals.size() == 2 )
    {
      signal<Ntk> signal_out = _ntk.create_or( signals[0], signals[1] );
      return signal_out;
    }
    else
    {
      std::size_t const half_size = signals.size() / 2;
      std::vector<signal<Ntk>> vector_l( signals.begin(), signals.begin() + half_size );
      std::vector<signal<Ntk>> vector_r( signals.begin() + half_size, signals.end() );

      signal<Ntk> signal_l = recursive_or( vector_l );
      signal<Ntk> signal_r = recursive_or( vector_r );
      signal<Ntk> signal_out = _ntk.create_or( signal_l, signal_r );

      return signal_out;
    }
  }

  signal<Ntk> recursive_and( const std::vector<signal<Ntk>>& signals )
  {

    if ( signals.size() == 1 )
    {
      return signals[0];
    }
    else if ( signals.size() == 2 )
    {
      signal<Ntk> signal_out = _ntk.create_and( signals[0], signals[1] );
      return signal_out;
    }
    else
    {
      std::size_t const half_size = signals.size() / 2;
      std::vector<signal<Ntk>> vector_l( signals.begin(), signals.begin() + half_size );
      std::vector<signal<Ntk>> vector_r( signals.begin() + half_size, signals.end() );

      signal<Ntk> signal_l = recursive_and( vector_l );
      signal<Ntk> signal_r = recursive_and( vector_r );
      signal<Ntk> signal_out = _ntk.create_and( signal_l, signal_r );

      return signal_out;
    }
  }
#pragma endregion

#pragma region converter functions
  signal<Ntk> convert_cube_to_graph( const mockturtle::cover_storage_node& Nde, const kitty::cube& cb, const bool& is_sop )
  {
    std::vector<signal<Ntk>> signals;

    for ( auto j = 0u; j < Nde.children.size(); j++ )
    {
      if ( cb.get_mask( j ) == 1 )
        signals.emplace_back( ( cb.get_bit( j ) == 1 ) ? _connector.signals[Nde.children[j].index] : !_connector.signals[Nde.children[j].index] );
    }
    return ( is_sop ? recursive_and( signals ) : recursive_or( signals ) );
  }

  signal<Ntk> convert_cover_to_graph( const mockturtle::cover_storage_node& Nde )
  {
    std::vector<signal<Ntk>> signals_internal;

    bool is_sop = _cover_ntk._storage->data.covers[Nde.data[1].h1].second;
    auto cbs = _cover_ntk._storage->data.covers[Nde.data[1].h1].first;

    for ( auto const& cb : cbs )
    {
      if ( cb.num_literals() == 1 && Nde.children.size() == 1 )
      {
        if ( cb.get_bit( 0 ) == 1 )
          return _ntk.create_buf( _connector.signals[Nde.children[0].index] );
        else if ( cb.get_bit( 0 ) == 0 )
          return _ntk.create_not( _connector.signals[Nde.children[0].index] );
        else
          std::cerr << "something wrong in your 1 literal node";
      }
      else
      {
        signals_internal.emplace_back( convert_cube_to_graph( Nde, cb, is_sop ) );
      }
    }
    if ( signals_internal.size() == 1 )
    {
      return signals_internal.at( 0 );
    }
    else
    {
      return ( is_sop ? recursive_or( signals_internal ) : recursive_and( signals_internal ) );
    }
  }

  void run()
  {
    for ( auto const& inpt : _cover_ntk._storage->inputs )
    {
      _connector.insert( _ntk.create_pi(), inpt );
    }

    for ( auto const& nde : _cover_ntk._storage->nodes )
    {
      uint64_t index = _cover_ntk._storage->hash[nde];
      bool condition1 = ( std::find( _cover_ntk._storage->inputs.begin(), _cover_ntk._storage->inputs.end(), index ) != _cover_ntk._storage->inputs.end() );
      bool condition2 = nde.data[1].h1 == 0 || nde.data[1].h1 == 1;

      if ( !condition1 && !condition2 )
      {
        _connector.insert( convert_cover_to_graph( nde ), _cover_ntk._storage->hash[nde] );
      }
    }

    for ( auto const& outpt : _cover_ntk._storage->outputs )
    {
      const auto o = _connector.signals[outpt.index];
      _ntk.create_po( o );
    }
  }

public:
  Ntk& _ntk;

private:
  signals_connector<Ntk> _connector;
  cover_network& _cover_ntk;
};

} /* namespace detail */

/*! \brief convert_covers_to_graph function
 *
 * This function verifies if the destination network has the basic requirements for the creation of a SOP or a POS, i.e. the 
 * create_and and the create_or functions. Afterward, it initializes the cover_network_to_graph_converter and it performs the conversion.
 *
 * \param cover_ntk Input network of type `cover_network`. 
 * \param ntk Output network of type `Ntk`.  
 * 
  \verbatim embed:rst

  Example

  .. code-block:: c++

    const cover_network cover = ...;

    aig_network aig;
    xag_network xag;
    mig_network mig;
    xmg_network xmg;

    convert_covers_to_graph( cover_ntk, aig );
    convert_covers_to_graph( cover_ntk, xag ); 
    convert_covers_to_graph( cover_ntk, mig );
    convert_covers_to_graph( cover_ntk, xmg );

  \endverbatim
 */
template<class Ntk>
void convert_covers_to_graph( cover_network& cover_ntk, Ntk& ntk )
{
  static_assert( has_create_and_v<Ntk>, "NtkDest does not implement the create_not method" );
  static_assert( has_create_or_v<Ntk>, "NtkDest does not implement the create_po method" );

  detail::cover_to_graph_converter<Ntk> converter( ntk, cover_ntk );
  converter.run();
}

/*! \brief convert_covers_to_graph function
 *
 * This function verifies if the destination network has the basic requirements for the creation of a SOP or a POS, i.e. the 
 * create_and and the create_or functions. Afterward, it initializes the cover_network_to_graph_converter and it performs the conversion.
 *
 * \param cover_ntk Input network of type `cover_network`. 
 * \return ntk Output network of type `Ntk`.  
 * 
  \verbatim embed:rst

  Example

  .. code-block:: c++

    const klut_network klut = ...;

    aig_network aig;
    xag_network xag;
    mig_network mig;
    xmg_network xmg;

    aig = convert_covers_to_graph( klut_ntk );
    xag = convert_covers_to_graph( klut_ntk ); 
    mig = convert_covers_to_graph( klut_ntk );
    xmg = convert_covers_to_graph( klut_ntk );

  \endverbatim
 */

template<class Ntk>
Ntk convert_covers_to_graph( cover_network& cover_ntk )
{

  static_assert( has_create_and_v<Ntk>, "NtkDest does not implement the create_not method" );
  static_assert( has_create_or_v<Ntk>, "NtkDest does not implement the create_po method" );

  Ntk ntk;
  detail::cover_to_graph_converter<Ntk> converter( ntk, cover_ntk );
  converter.run();
  return converter._ntk;
}

} /* namespace mockturtle */
