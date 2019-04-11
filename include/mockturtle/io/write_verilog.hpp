/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL EPFL
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
  \file write_verilog.hpp
  \brief Write networks to structural Verilog format

  \author Mathias Soeken
*/

#pragma once

#include <array>
#include <fstream>
#include <iostream>
#include <string>

#include <ez/direct_iterator.hpp>
#include <fmt/format.h>

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "../utils/string_utils.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

using namespace std::string_literals;

namespace detail
{

template<int Fanin, class Ntk>
std::pair<std::array<std::string, Fanin>, std::array<std::string, Fanin>>
format_fanin( Ntk const& ntk, node<Ntk> const& n, node_map<std::string, Ntk>& node_names )
{
  std::array<std::string, Fanin> children, inv;
  ntk.foreach_fanin( n, [&]( auto const& f, auto i ) {
    children[i] = node_names[f];
    inv[i] = ntk.is_complemented( f ) ? "~" : "";
  } );
  return {children, inv};
}

} // namespace detail

/*! \brief Writes network in structural Verilog format into output stream
 *
 * An overloaded variant exists that writes the network into a file.
 *
 * **Required network functions:**
 * - `num_pis`
 * - `num_pos`
 * - `foreach_pi`
 * - `foreach_node`
 * - `foreach_fanin`
 * - `get_node`
 * - `get_constant`
 * - `is_constant`
 * - `is_pi`
 * - `is_and`
 * - `is_or`
 * - `is_xor`
 * - `is_xor3`
 * - `is_maj`
 * - `node_to_index`
 *
 * \param ntk Network
 * \param os Output stream
 */
template<class Ntk>
void write_verilog( Ntk const& ntk, std::ostream& os )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_num_pis_v<Ntk>, "Ntk does not implement the num_pis method" );
  static_assert( has_num_pos_v<Ntk>, "Ntk does not implement the num_pos method" );
  static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_is_and_v<Ntk>, "Ntk does not implement the is_and method" );
  static_assert( has_is_or_v<Ntk>, "Ntk does not implement the is_or method" );
  static_assert( has_is_xor_v<Ntk>, "Ntk does not implement the is_xor method" );
  static_assert( has_is_xor3_v<Ntk>, "Ntk does not implement the is_xor3 method" );
  static_assert( has_is_maj_v<Ntk>, "Ntk does not implement the is_maj method" );
  static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );

  const auto xs = map_and_join( ez::make_direct_iterator<decltype( ntk.num_pis() )>( 0 ),
                                ez::make_direct_iterator( ntk.num_pis() ),
                                []( auto i ) { return fmt::format( "x{}", i ); }, ", "s );
  const auto ys = map_and_join( ez::make_direct_iterator<decltype( ntk.num_pis() )>( 0 ),
                                ez::make_direct_iterator( ntk.num_pos() ),
                                []( auto i ) { return fmt::format( "y{}", i ); }, ", "s );

  os << fmt::format( "module top({}, {});\n", xs, ys )
     << fmt::format( "  input {};\n", xs )
     << fmt::format( "  output {};\n", ys );

  node_map<std::string, Ntk> node_names( ntk );

  node_names[ntk.get_constant( false )] = "1'b0";
  if ( ntk.get_node( ntk.get_constant( false ) ) != ntk.get_node( ntk.get_constant( true ) ) )
  {
    node_names[ntk.get_constant( true )] = "1'b1";
  }

  ntk.foreach_pi( [&]( auto const& n, auto i ) {
    node_names[n] = fmt::format( "x{}", i );
  } );

  topo_view ntk_topo{ntk};

  /* declare wires */
  if ( ntk.num_gates() > 0 )
  {
    os << "  wire ";
    auto first = true;
    ntk.foreach_gate( [&]( auto const& n ) {
        if ( first )
          first = false;
        else
          os << ", ";
        os << fmt::format( "n{}", ntk.node_to_index( n ) );
      } );
    os << ";\n";
  }

  ntk_topo.foreach_node( [&]( auto const& n ) {
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      return true;

    if ( ntk.is_and( n ) )
    {
      const auto [children, inv] = detail::format_fanin<2, Ntk>( ntk, n, node_names );
      os << fmt::format( "  assign n{} = {}{} & {}{};\n", ntk.node_to_index( n ),
                         inv[0], children[0], inv[1], children[1] );
    }
    else if ( ntk.is_or( n ) )
    {
      const auto [children, inv] = detail::format_fanin<2, Ntk>( ntk, n, node_names );
      os << fmt::format( "  assign n{} = {}{} | {}{};\n", ntk.node_to_index( n ),
                         inv[0], children[0], inv[1], children[1] );
    }
    else if ( ntk.is_xor( n ) )
    {
      const auto [children, inv] = detail::format_fanin<2, Ntk>( ntk, n, node_names );
      os << fmt::format( "  assign n{} = {}{} ^ {}{};\n", ntk.node_to_index( n ),
                         inv[0], children[0], inv[1], children[1] );
    }
    else if ( ntk.is_xor3( n ) )
    {
      const auto [children, inv] = detail::format_fanin<3, Ntk>( ntk, n, node_names );
      os << fmt::format( "  assign n{} = {}{} ^ {}{} ^ {}{};\n", ntk.node_to_index( n ),
                         inv[0], children[0], inv[1], children[1], inv[2], children[2] );
    }
    else if ( ntk.is_maj( n ) )
    {
      signal<Ntk> first_child;
      ntk.foreach_fanin( n, [&]( auto const& f ) { first_child = f; return false; } );

      const auto [children, inv] = detail::format_fanin<3, Ntk>( ntk, n, node_names );
      if ( ntk.is_constant( ntk.get_node( first_child ) ) )
      {
        os << fmt::format( "  assign n{0} = {1}{3} {5} {2}{4};\n",
                           ntk.node_to_index( n ),
                           inv[1], inv[2], children[1], children[2],
                           ntk.is_complemented( first_child ) ? "|" : "&" );
      }
      else
      {
        os << fmt::format( "  assign n{0} = ({1}{4} & {2}{5}) | ({1}{4} & {3}{6}) | ({2}{5} & {3}{6});\n",
                           ntk.node_to_index( n ),
                           inv[0], inv[1], inv[2], children[0], children[1], children[2] );
      }
    }
    else
    {
      os << fmt::format( "  assign n{} = unknown gate;\n", ntk.node_to_index( n ) );
    }

    node_names[n] = fmt::format( "n{}", ntk.node_to_index( n ) );
    return true;
  } );

  ntk.foreach_po( [&]( auto const& f, auto i ) {
    os << fmt::format( "  assign y{} = {}{};\n", i, ntk.is_complemented( f ) ? "~" : "", node_names[f] );
  } );

  os << "endmodule\n"
     << std::flush;
}

/*! \brief Writes network in structural Verilog format into a file
 *
 * **Required network functions:**
 * - `num_pis`
 * - `num_pos`
 * - `foreach_pi`
 * - `foreach_node`
 * - `foreach_fanin`
 * - `get_node`
 * - `get_constant`
 * - `is_constant`
 * - `is_pi`
 * - `is_and`
 * - `is_or`
 * - `is_xor`
 * - `is_xor3`
 * - `is_maj`
 * - `node_to_index`
 *
 * \param ntk Network
 * \param filename Filename
 */
template<class Ntk>
void write_verilog( Ntk const& ntk, std::string const& filename )
{
  std::ofstream os( filename.c_str(), std::ofstream::out );
  write_verilog( ntk, os );
  os.close();
}

} /* namespace mockturtle */
