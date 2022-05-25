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
  \file names_view.hpp
  \brief Implements methods to declare names for network signals

  \author Heinz Riener
  \author Mathias Soeken
*/

#pragma once

#include "../traits.hpp"

#include <map>

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
  template<typename StrType = const char*>
  names_view( Ntk const& ntk = Ntk(), StrType name = "" )
      : Ntk( ntk ), _network_name{ name }
  {
  }

  names_view( names_view<Ntk> const& named_ntk )
      : Ntk( named_ntk ), _network_name( named_ntk._network_name ), _signal_names( named_ntk._signal_names ), _output_names( named_ntk._output_names )
  {
  }

  names_view<Ntk>& operator=( names_view<Ntk> const& named_ntk )
  {

      Ntk::operator=(named_ntk);
      _signal_names = named_ntk._signal_names;
      _network_name = named_ntk._network_name;
      _output_names = named_ntk._output_names;
      return *this;

  }

  signal create_pi( std::string const& name = {} )
  {
    const auto s = Ntk::create_pi( name );
    if ( !name.empty() )
    {
      set_name( s, name );
    }
    return s;
  }

  uint32_t create_po( signal const& s, std::string const& name = {} )
  {
    const auto index = Ntk::num_pos();
    auto id = Ntk::create_po( s, name );
    if ( !name.empty() )
    {
      set_output_name( index, name );
    }
    return id;
  }

  signal create_ro( std::string const& name = std::string() )
  {
    const auto s = Ntk::create_ro( name );
    if ( !name.empty() )
    {
      set_name( s, name );
    }
    return s;
  }

  uint32_t create_ri( signal const& f, int8_t reset = 0, std::string const& name = std::string() )
  {
    const auto index = Ntk::num_pos();
    auto id = Ntk::create_po( s, name );
    if ( !name.empty() )
    {
      set_output_name( index, name );
    }
    return id;
  }

  signal create_ro( std::string const& name = std::string() )
  {
    const auto s = Ntk::create_ro( name );
    if ( !name.empty() )
    {
      set_name( s, name );
    }
    return s;
  }

  uint32_t create_ri( signal const& f, int8_t reset = 0, std::string const& name = std::string() )
  {
    const auto index = Ntk::num_pos();
    auto id = Ntk::create_ri( f, reset, name );
    if ( !name.empty() )
    {
      set_output_name( index, name );
    }
    return id;
  }

  template<class Other>
  void copy_network_metadata(Other &other) {
    if constexpr ( has_get_network_name_v<Other> && has_set_network_name_v<Other> ) {
      set_network_name( other.get_network_name() );
    }
    Ntk::copy_network_metadata( other );
  }

  template<class Other>
  void copy_node_metadata(node dest, Other &other, typename Other::node source) {
    (void) dest;
    (void) source;
    Ntk::copy_node_metadata( dest, other, source );
  }

  template<class Other>
  void copy_signal_metadata(signal dest, Other &other, typename Other::signal source) {
    if constexpr ( has_has_name_v<Other> && has_get_name_v<Other>)
    {
      if ( other.has_name( source ) )
      {
        this->set_name( dest, other.get_name( source ));
      }
    }
    Ntk::copy_signal_metadata( dest, other, source );
  }

  template<class Other>
  void copy_output_metadata(uint32_t dest, Other &other, uint32_t source) {
    if constexpr ( has_has_output_name_v<Other>
                   && has_get_output_name_v<Other>
                   && has_set_output_name_v<Other> )
    {
      if ( other.has_output_name( source ) )
      {
        set_output_name( dest, other.get_name( source ));
      }
    }
    Ntk::copy_output_metadata( dest, other, source );
  }

  template<typename StrType = const char*>
  void set_network_name( StrType name ) noexcept
  {
    _network_name = name;
  }

  std::string get_network_name() const noexcept
  {
    return _network_name;
  }

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

  bool has_output_name( uint32_t index ) const
  {
    return ( _output_names.find( index ) != _output_names.end() );
  }

  void set_output_name( uint32_t index, std::string const& name )
  {
    _output_names[index] = name;
  }

  std::string get_output_name( uint32_t index ) const
  {
    return _output_names.at( index );
  }

private:
  std::string _network_name;
  std::map<signal, std::string> _signal_names;
  std::map<uint32_t, std::string> _output_names;
}; /* names_view */

template<class T>
names_view( T const& ) -> names_view<T>;

template<class T>
names_view( T const&, typename T::signal const& ) -> names_view<T>;

} // namespace mockturtle
