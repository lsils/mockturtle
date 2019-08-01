/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
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
  \file names_view.hpp
  \brief Implements methods to declare names for network signals

  \author Heinz Riener
*/

#pragma once

#include "../traits.hpp"
#include <unordered_map>

namespace mockturtle
{

template<class Ntk>
class names_view : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  names_view( Ntk const& ntk )
    : Ntk( ntk )
  {
  }

  names_view( names_view<Ntk> const& named_ntk )
    : Ntk( named_ntk )
    , _signal_names( named_ntk._signal_names )
  {
  }

private:
  names_view<Ntk> operator=( names_view<Ntk> const& named_ntk );

public:
  bool has_name( signal const& s ) const
  {
    return ( _signal_names.find( s ) != _signal_names.end() );
  }

  void set_name( signal const& s, std::string const& name )
  {
    _signal_names[s] = name;
  }

  std::string get_name( signal const& s ) const
  {
    return _signal_names.at( s );
  }

private:
  std::map<signal, std::string> _signal_names;
}; /* names_view */

template<class T>
names_view(T const&) -> names_view<T>;

template<class T>
names_view(T const&, typename T::signal const&) -> names_view<T>;

} // namespace mockturtle
