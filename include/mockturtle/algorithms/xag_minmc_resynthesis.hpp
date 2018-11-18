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
  \file xah_minmc_resynthesis.hpp
  \brief xag resynthesis 

  \author Eleonora Testa
*/

#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "../algorithms/cleanup.hpp"
#include <kitty/constructors.hpp>
#include <kitty/print.hpp>
#include <kitty/spectral.hpp>
#include <mockturtle/io/write_bench.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/cut_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include "../traits.hpp"

namespace mockturtle
{

class xag_minmc_resynthesis
{
public:
  xag_minmc_resynthesis( std::string filename )
  {
    db = std::make_shared<xag_network>();
    func_mc = std::make_shared<std::unordered_map<std::string, std::tuple<std::string, unsigned, xag_network::signal>>>();
    build_db( filename );
  }

  xag_minmc_resynthesis( xag_minmc_resynthesis const& ) = default;
  xag_minmc_resynthesis( xag_minmc_resynthesis&& ) = default;
  xag_minmc_resynthesis& operator=( xag_minmc_resynthesis const& ) = default;
  xag_minmc_resynthesis& operator=( xag_minmc_resynthesis&& ) = default;
  virtual ~xag_minmc_resynthesis() = default;

  template<typename LeavesIterator, typename Fn>
  void operator()( xag_network& xag, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn )
  {
    kitty::static_truth_table<6> tt_ext;
    auto spectral = kitty::exact_spectral_canonization_limit( function, 100000 );

    if ( spectral.second == 0 )
      return; /* quit */
    tt_ext = extend_to( spectral.first, 6 );

    unsigned int mc = 0u;
    std::string original_f;
    xag_network::signal circuit;

    auto search = func_mc->find( kitty::to_hex( tt_ext ) );

    if ( search != func_mc->end() )
    {
      original_f = std::get<0>( search->second );
      mc = std::get<1>( search->second );
      circuit = std::get<2>( search->second );
    }
    else
    {
      return; /* quit */
    }

    kitty::static_truth_table<6> db_repr;
    bool out_neg = 0;
    std::vector<xag_network::signal> final_xor;
    kitty::create_from_hex_string( db_repr, original_f );
    std::vector<xag_network::signal> pis( 6, xag.get_constant( false ) );
    std::copy( begin, end, pis.begin() );
    
    std::vector<kitty::detail::spectral_operation> trans;
    std::vector<kitty::detail::spectral_operation> trans_2;
    const auto r1 = kitty::exact_spectral_canonization(
        db_repr, [&trans]( auto const& ops ) {
          std::copy( ops.rbegin(), ops.rend(),
                     std::back_inserter( trans ) );
        } );
    const auto r2 = kitty::exact_spectral_canonization(
        function, [&trans_2]( auto const& ops ) {
          std::copy( ops.begin(), ops.end(),
                     std::back_inserter( trans_2 ) );
        } );

    for ( auto const& t : trans_2 )
    {
      switch ( t._kind )
      {
      default:
        assert( false );
      case kitty::detail::spectral_operation::kind::permutation:
      {
        const auto v1 = log2( t._var1 );
        const auto v2 = log2( t._var2 );
        std::swap( pis[v1], pis[v2] );
      }
      break;
      case kitty::detail::spectral_operation::kind::input_negation:
      {
        const auto v1 = log2( t._var1 );
        pis[v1] = pis[v1] ^ 1;
      }
      break;
      case kitty::detail::spectral_operation::kind::output_negation:
      {
        out_neg = out_neg ^ 1;
      }
      break;
      case kitty::detail::spectral_operation::kind::spectral_translation:
      {
        const auto v1 = log2( t._var1 );
        const auto v2 = log2( t._var2 );
        auto f = xag.create_xor( pis[v1], pis[v2] );
        pis[v1] = f;
      }
      break;
      case kitty::detail::spectral_operation::kind::disjoint_translation:
      {
        const auto v1 = log2( t._var1 );
        final_xor.push_back( pis[v1] );
      }
      break;
      }
    }

    for ( auto const& t : trans )
    {
      switch ( t._kind )
      {
      default:
        assert( false );
      case kitty::detail::spectral_operation::kind::permutation:
      {
        const auto v1 = log2( t._var1 );
        const auto v2 = log2( t._var2 );
        std::swap( pis[v1], pis[v2] );
      }
      break;
      case kitty::detail::spectral_operation::kind::input_negation:
      {
        const auto v1 = log2( t._var1 );
        pis[v1] = !pis[v1];
      }
      break;
      case kitty::detail::spectral_operation::kind::output_negation:
      {
        out_neg = out_neg ^ 1;
      }
      break;
      case kitty::detail::spectral_operation::kind::spectral_translation:
      {
        const auto v1 = log2( t._var1 );
        const auto v2 = log2( t._var2 );
        //auto f = db->create_xor( pis[v1], pis[v2] );
        auto f = xag.create_xor( pis[v1], pis[v2] );
        pis[v1] = f;
      }
      break;
      case kitty::detail::spectral_operation::kind::disjoint_translation:
      {
        const auto v1 = log2( t._var1 );
        final_xor.push_back( pis[v1] );
      }
      break;
      }
    }

    auto f = circuit;

    topo_view topo{*db, f};
    f = cleanup_dangling( topo, xag, pis.begin(), pis.end() ).front(); 

    for ( auto& g : final_xor )
    {
      f = xag.create_xor( f, g );
    }

    if ( ( out_neg ) == 1 )
      fn( !f );
    else
      fn( f );
    return;

    /*if ( !fn( f ) )
    {
      return; /* quit 
    }*/
  }

private:
  void build_db( std::string filename )
  {
    std::ifstream file1;
    file1.open( filename );
    std::string line;
    for ( auto h = 0; h < 6; h++ )
    {
      db->create_pi();
    }

    while ( std::getline( file1, line ) )
    {
      std::vector<xag_network::signal> hashing_circ;
      std::string delimiter = "\t";
      line.erase( 0, line.find( delimiter ) + delimiter.length() ); // remove the name of the function
      std::string original = line.substr( 0, line.find( delimiter ) );
      line.erase( 0, line.find( delimiter ) + delimiter.length() ); // remove the tt in Rene format
      std::string token_f = line.substr( 0, line.find( delimiter ) );
      line.erase( 0, line.find( delimiter ) + delimiter.length() );

      auto mc = std::stoul( line.substr( 0, line.find( delimiter ) ), nullptr, 10 );
      line.erase( 0, line.find( delimiter ) + delimiter.length() );
      auto circuit = line;

      delimiter = " ";
      std::string token = circuit.substr( 0, circuit.find( delimiter ) );
      circuit.erase( 0, circuit.find( delimiter ) + delimiter.length() );
      const auto inputs = std::stoul( token, nullptr, 10 );

      for ( auto h = 0; h < inputs; h++ )
      {
        hashing_circ.push_back( db->make_signal( db->index_to_node( h + 1 ) ) );
      }

      while ( circuit.size() > 4 ) //circuit.size() > 4 )
      {
        std::vector<unsigned> signals( 2, 0 );
        std::vector<xag_network::signal> ff( 2 );
        for ( auto j = 0; j < 2; j++ )
        {
          token = circuit.substr( 0, circuit.find( delimiter ) );
          circuit.erase( 0, circuit.find( delimiter ) + delimiter.length() );
          signals[j] = std::stoul( token, nullptr, 10 );
          if ( signals[j] == 0 )
            ff[j] = db->get_constant( true );
          else if ( signals[j] == 1 )
            ff[j] = db->get_constant( false );
          else
          {
            if ( signals[j] % 2 != 0 )
            {
              ff[j] = hashing_circ[signals[j] / 2 - 1] ^ 1;
            }
            else
              ff[j] = hashing_circ[signals[j] / 2 - 1];
          }
        }
        circuit.erase( 0, circuit.find( delimiter ) + delimiter.length() );

        if ( signals[0] > signals[1] )
        {
          hashing_circ.push_back( db->create_xor( ff[0], ff[1] ) );
        }
        else
        {
          hashing_circ.push_back( db->create_and( ff[0], ff[1] ) );
        }
      }

      auto output = std::stoul( circuit, nullptr, 10 );
      xag_network::signal f;
      if ( output % 2 != 0 ) // complemented output
      {
        f = !hashing_circ[output / 2 - 1];
      }
      else
      {
        f = hashing_circ[output / 2 - 1];
      }
      auto t = db->create_po( f );

      func_mc->insert( std::make_pair( token_f, std::make_tuple( original, mc, f ) ) );
    }
    // std::cout << db->num_pos() << std::endl;
    file1.close();
  }

  std::shared_ptr<xag_network> db;
  std::shared_ptr<std::unordered_map<std::string, std::tuple<std::string, unsigned, xag_network::signal>>> func_mc;
};

} // namespace mockturtle
