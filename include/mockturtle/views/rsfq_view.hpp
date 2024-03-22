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
  \file rsfq_view.hpp
  \brief Implements methods to mark balancing DFFs

  \author Alessandro Tempia Calvino
*/

#pragma once

#include "../utils/node_map.hpp"

#include <map>
#include <iostream>

namespace mockturtle
{

/*! \brief Adds methods to mark balancing DFFs.
 *
 * This view adds methods to manage a mapped RSFQ network that
 * implements balancing DFFs. This view can be used to mark
 * and report balancing DFFs. It always adds the functions
 * `set_dff`, `is_dff`, `remove_dff`, `num_dffs`.
 * 
 */
template<class Ntk>
class rsfq_view : public Ntk
{
public:
  using node = typename Ntk::node;

public:
  explicit rsfq_view()
      : Ntk()
      , _dffs( *this )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  }

  explicit rsfq_view( Ntk const& ntk )
      : Ntk( ntk )
      , _dffs( *this )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  }

  rsfq_view<Ntk>& operator=( rsfq_view<Ntk> const& _rsfq_ntk )
  {
    Ntk::operator=( _rsfq_ntk );
    _dffs = _rsfq_ntk._dffs;
    return *this;
  }

  void set_dff( node const& n )
  {
    _dffs[n] = true;
  }

  bool is_dff( node const& n ) const
  {
    return _dffs.has( n );
  }

  void remove_dff( node const& n )
  {
    _dffs.erase( n );
  }

  uint32_t num_dffs() const
  {
    return _dffs.size();
  }

private:
  node_map<bool, Ntk, std::unordered_map<node, bool>> _dffs;
}; /* rsfq_view */

template<class T>
rsfq_view( T const& ) -> rsfq_view<T>;

} // namespace mockturtle
