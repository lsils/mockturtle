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
  \file crossing_view.hpp
  \brief Extends a network with representation of crossing cells

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

namespace mockturtle
{

template<class Ntk>
class crossing_view : public Ntk
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  /* construct with an empty network */
  explicit crossing_view()
      : Ntk()
  {
    static_assert( is_buffered_network_type_v<Ntk>, "Ntk is not a buffered network type" );
  }

  /* construct with a network */
  explicit crossing_view( Ntk const& ntk )
      : Ntk( ntk )
  {
    static_assert( is_buffered_network_type_v<Ntk>, "Ntk is not a buffered network type" );
  }

  /* copy constructor */
  explicit crossing_view( crossing_view<Ntk> const& other )
      : Ntk( other )
  {}

  /* copy assignment */
  crossing_view<Ntk>& operator=( crossing_view<Ntk> const& other )
  {
    Ntk::operator=( other );
    return *this;
  }

  /* destructor */
  ~crossing_view()
  {}

public:
  std::pair<signal, signal> create_crossing( signal const& in1, signal const& in2 )
  {

  }

  void insert_crossing( signal const& in1, signal const& in2, node const& out1, node const& out2 )
  {

  }

private:

}; /* crossing_view */

template<class T>
crossing_view( T const& ) -> crossing_view<T>;

} // namespace mockturtle