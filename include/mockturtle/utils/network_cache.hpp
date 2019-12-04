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

#include <unordered_map>
#include <vector>

#include "../traits.hpp"

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

  bool has( Key const& key ) const
  {
    return _map.find( key ) != _map.end();
  }

  bool insert( Key const& key, Ntk const& ntk )
  {
    /* ntk must have one primary output and not too many primary inputs */
    if ( ntk.num_pos() != 1u || ntk.num_pis() > _pis.size() )
    {
      return false;
    }

    /* insert ntk into _db, create an output and return the index of the output */
    const auto f = cleanup_dangling( ntk, _db, _pis.begin(), _pis.begin() + ntk.num_pis() ).front();
    _map.emplace( key, f );
    return true;
  }

  signal<Ntk> get( Key const& key ) const
  {
    return _map.at( key );
  }

  auto get_view( Key const& key ) const
  {
    return topo_view<Ntk>( _db, _map.at( key ) );
  }

private:
  Ntk _db;
  std::vector<signal<Ntk>> _pis;
  std::unordered_map<Key, signal<Ntk>, Hash> _map;
};

} /* namespace mockturtle */
