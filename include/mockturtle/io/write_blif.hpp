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
  \file write_blif.hpp
  \brief Write networks to BLIF format

  \author Heinz Riener
*/

#pragma once

#include <fstream>
#include <iostream>
#include <string>

#include <fmt/format.h>
#include <kitty/operations.hpp>
#include <kitty/print.hpp>
#include <kitty/isop.hpp>

#include "../traits.hpp"

namespace mockturtle
{

/*! \brief Writes network in BLIF format into output stream
 *
 * An overloaded variant exists that writes the network into a file.
 *
 * **Required network functions:**
 * - `fanin_size`
 * - `foreach_fanin`
 * - `foreach_pi`
 * - `foreach_po`
 * - `get_node`
 * - `is_constant`
 * - `is_pi`
 * - `node_function`
 * - `node_to_index`
 * - `num_pis`
 * - `num_pos`
 *
 * \param ntk Network
 * \param os Output stream
 */
template<class Ntk>
void write_blif( Ntk const& ntk, std::ostream& os )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_fanin_size_v<Ntk>, "Ntk does not implement the fanin_size method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_num_pis_v<Ntk>, "Ntk does not implement the num_pis method" );
  static_assert( has_num_pos_v<Ntk>, "Ntk does not implement the num_pos method" );
  static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );
  static_assert( has_node_function_v<Ntk>, "Ntk does not implement the node_function method" );

  topo_view topo_ntk{ntk};

  os << ".model netlist\n";

  if ( topo_ntk.num_pis() > 0u )
  {
    os << ".inputs ";
    topo_ntk.foreach_pi( [&]( auto const& n ) {
        os << fmt::format( "n{} ", topo_ntk.node_to_index( n ) );
      } );
    os << "\n";
  }

  if ( topo_ntk.num_pos() > 0u )
  {
    os << ".outputs ";
    topo_ntk.foreach_po( [&]( auto const& n, auto index ) {
        os << fmt::format( "po{} ", index );
      } );
    os << "\n";
  }

  os << ".names n0\n";
  os << "0\n";

  os << ".names n1\n";
  os << "1\n";

  topo_ntk.foreach_node( [&]( auto const& n ) {
    if ( topo_ntk.is_constant( n ) || topo_ntk.is_pi( n ) )
      return; /* continue */

    os << fmt::format( ".names " );
    topo_ntk.foreach_fanin( n, [&]( auto const& c ) {
        os << fmt::format( "n{} ", topo_ntk.node_to_index( c ) );
      });
    os << fmt::format( " n{}\n", topo_ntk.node_to_index( n ) );

    auto const func = topo_ntk.node_function( n );
    for ( const auto& cube : isop( func ) )
    {
      cube.print( topo_ntk.fanin_size( n ), os );
      os << " 1\n";
    }
    });

  if ( topo_ntk.num_pos() > 0u )
  {
    topo_ntk.foreach_po( [&]( auto const& n, auto index ) {
        os << fmt::format( ".names n{} po{}\n1 1\n", topo_ntk.node_to_index( n ), index );
      } );
  }

  os << ".end\n";
  os << std::flush;
}

/*! \brief Writes network in BENCH format into a file
 *
 * **Required network functions:**
 * - `fanin_size`
 * - `foreach_fanin`
 * - `foreach_pi`
 * - `foreach_po`
 * - `get_node`
 * - `is_constant`
 * - `is_pi`
 * - `node_function`
 * - `node_to_index`
 * - `num_pis`
 * - `num_pos`
 *
 * \param ntk Network
 * \param filename Filename
 */
template<class Ntk>
void write_blif( Ntk const& ntk, std::string const& filename )
{
  std::ofstream os( filename.c_str(), std::ofstream::out );
  write_blif( ntk, os );
  os.close();
}

} /* namespace mockturtle */
