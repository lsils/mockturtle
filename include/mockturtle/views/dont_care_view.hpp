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
  \file dont_care_view.hpp
  \brief Implements methods to store external don't-cares

  \author Siang-Yun Lee
*/

#pragma once

#include "../traits.hpp"
#include "../algorithms/simulation.hpp"

#include <vector>

namespace mockturtle
{

template<class Ntk>
class dont_care_view : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  dont_care_view( Ntk const& ntk, Ntk const& cdc_ntk )
    : Ntk( ntk ), _excdc( cdc_ntk )
  {
    assert( ntk.num_pis() == cdc_ntk.num_pis() );
    assert( cdc_ntk.num_pos() == 1 );
  }

  dont_care_view( dont_care_view<Ntk> const& ntk )
    : Ntk( ntk ), _excdc( ntk._excdc )
  {
  }

  dont_care_view<Ntk>& operator=( dont_care_view<Ntk> const& other )
  {
    Ntk::operator=( other );
    _excdc = other._excdc;
    return *this;
  }

  bool pattern_is_EXCDC( std::vector<bool> const& pattern ) const
  {
    assert( pattern.size() == this->num_pis() );

    default_simulator<bool> sim( pattern );
    auto const vals = simulate<bool>( _excdc, sim );
    return vals[0];
  }

private:
  Ntk _excdc;
}; /* dont_care_view */

template<class T>
dont_care_view( T const&, T const& ) -> dont_care_view<T>;

} // namespace mockturtle
