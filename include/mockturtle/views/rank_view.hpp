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
  \file rank_view.hpp
  \brief Implements rank orders for a network

  \author Marcel Walter
*/

#pragma once

#include "../networks/detail/foreach.hpp"
#include "../traits.hpp"
#include "../utils/node_map.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace mockturtle
{

template<class Ntk, bool has_rank_interface = false>
class rank_view
{
};

template<class Ntk>
class rank_view<Ntk, true> : public Ntk
{
public:
  rank_view( Ntk const& ntk ) : Ntk( ntk )
  {
  }
};

template<class Ntk>
class rank_view<Ntk, false> : public Ntk
{
public:
  static constexpr bool is_topologically_sorted = true;
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit rank_view()
      : Ntk()
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  }

  /*! \brief Standard constructor.
   *
   * \param ntk Base network
   */
  explicit rank_view( Ntk const& ntk )
      : Ntk( ntk )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  }

};

template<class T>
rank_view( T const& ) -> rank_view<T>;

} // namespace mockturtle