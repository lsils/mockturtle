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
  \file network_cache.hpp
  \brief Network cache

  \author Mathias Soeken
*/

#pragma once

#include <type_traits>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>
#include <kitty/print.hpp>

#include "../traits.hpp"
#include "../algorithms/simulation.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

/*! \brief Network cache.
 *
 * ...
 *
   \verbatim embed:rst
  
   Example
   
   .. code-block:: c++
   
      ...
   \endverbatim
 */
template<typename Ntk, typename Key, class Hash = std::hash<Key>>
class network_cache
{
public:
  explicit network_cache( uint32_t num_vars )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi method" );

    for ( uint32_t i = 0u; i < num_vars; ++i )
    {
      _pis.emplace_back( _db.create_pi() );
    }
  }

  Ntk& network()
  {
    return _db;
  }

  std::vector<signal<Ntk>> const& pis() const
  {
    return _pis;
  }

  bool has( Key const& key ) const
  {
    return _map.find( key ) != _map.end();
  }

  template<class _Ntk>
  bool insert( Key const& key, _Ntk const& ntk )
  {
    /* ntk must have one primary output and not too many primary inputs */
    if ( ntk.num_pos() != 1u || ntk.num_pis() > _pis.size() )
    {
      return false;
    }

    /* insert ntk into _db, create an output and return the index of the output */
    const auto f = cleanup_dangling( ntk, _db, _pis.begin(), _pis.begin() + ntk.num_pis() ).front();
    insert_signal( key, f, ntk.num_pis() );
    return true;
  }

  void insert_signal( Key const& key, signal<Ntk> const& f, uint64_t support_size )
  {
    _map.emplace( key, f );
    _support.emplace( f, support_size );
    _db.create_po( f );
  }

  signal<Ntk> get( Key const& key ) const
  {
    return _map.at( key ).first;
  }

  auto get_view( Key const& key ) const
  {
    return topo_view<Ntk>( _db, _map.at( key ) );
  }

  void verify() const
  {
    if ( _map.size() != _db.num_pos() )
    {
      std::cout << "[e] size of map does not match number of POs\n";
    }
    std::cout << "[i] number of entries: " << _db.num_pos() << "\n";

    if constexpr ( std::is_same_v<Key, kitty::dynamic_truth_table> )
    {
      default_simulator<kitty::dynamic_truth_table> sim( _pis.size() );
      const auto results = simulate<kitty::dynamic_truth_table>( _db, sim );
      for ( auto i = 0u; i < results.size(); ++i )
      {
        const auto f = _db.po_at( i );
        if ( !_support.count( f ) )
        {
          std::cout << "[e] could not find support information for output " << i << "\n";
          continue;
        }

        const auto tt = kitty::shrink_to( results[i], _support.at( f ) );
        fmt::print( "[i] output {} has signal ({}, {}) and truth table {}\n",
                    i, _db.get_node( f ), _db.is_complemented( f ), kitty::to_hex( tt ) );

        const auto it = _map.find( tt );

        if ( it == _map.end() )
        {
          std::cout << "[e] could not find simulated output " << i
                    << " in map with truth table " << kitty::to_hex( tt ) << "\n";
        }
        else if ( it->second != f )
        {
          std::cout << "[e] mismatch in network cache: "
                    << "signal at output " << i << " is " << _db.get_node( f )
                    << ", expected " << _db.get_node( it->second ) << "\n";
        }
      }

      for ( const auto& [k, v] : _map )
      {
        fmt::print( "[i] maps {} to signal ({}, {})\n",
                    kitty::to_hex( k ), _db.get_node( v ), _db.is_complemented( v ) );
      }
    }
  }

private:
  Ntk _db;
  std::vector<signal<Ntk>> _pis;
  std::unordered_map<signal<Ntk>, uint32_t> _support;
  std::unordered_map<Key, signal<Ntk>, Hash> _map;
};

} /* namespace mockturtle */
