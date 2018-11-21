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
#include "../utils/stopwatch.hpp"
#include <kitty/constructors.hpp>
#include <kitty/hash.hpp>
#include <kitty/print.hpp>
#include <kitty/spectral.hpp>
#include <mockturtle/io/write_bench.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/cut_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include "../traits.hpp"

namespace mockturtle
{

struct xag_minmc_resynthesis_params
{
  bool print_stats{true};
};

struct xag_minmc_resynthesis_stats
{
  stopwatch<>::duration time_total{0};
  stopwatch<>::duration time_parse_db{0};
  stopwatch<>::duration time_classify{0};
  stopwatch<>::duration time_cleanup{0};

  uint32_t cache_hits{0};
  uint32_t cache_misses{0};
  uint32_t classify_aborts{0};
  uint32_t unknown_function_aborts{0};

  void report() const
  {
    std::cout << fmt::format( "[i] total time    = {:>5.2f} secs\n", to_seconds( time_total ) );
    std::cout << fmt::format( "[i] parse db time = {:>5.2f} secs\n", to_seconds( time_parse_db ) );
    std::cout << fmt::format( "[i] classify time = {:>5.2f} secs\n", to_seconds( time_classify ) );
    std::cout << fmt::format( "[i] - aborts      = {:>5}\n", classify_aborts );
    std::cout << fmt::format( "[i] cleanup time  = {:>5.2f} secs\n", to_seconds( time_cleanup ) );
    std::cout << fmt::format( "[i] cache hits    = {:>5}\n", cache_hits );
    std::cout << fmt::format( "[i] cache misses  = {:>5}\n", cache_misses );
    std::cout << fmt::format( "[i] unknown func. = {:>5}\n", unknown_function_aborts );
  }
};

class xag_minmc_resynthesis
{
public:
  xag_minmc_resynthesis( std::string const& filename )
  {
    db = std::make_shared<xag_network>();
    db_pis = std::make_shared<decltype( db_pis )::element_type>();
    func_mc = std::make_shared<decltype( func_mc )::element_type>();
    classify_cache = std::make_shared<decltype( classify_cache )::element_type>();
    build_db( filename );
  }

  virtual ~xag_minmc_resynthesis()
  {
    if ( ps.print_stats )
    {
      st.report();
    }
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( xag_network& xag, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn )
  {
    stopwatch t( st.time_total );

    const auto func_ext = kitty::extend_to<6>( function );
    std::vector<kitty::detail::spectral_operation> trans_2;
    kitty::static_truth_table<6> tt_ext;

    const auto cache_it = classify_cache->find( func_ext );

    if ( cache_it != classify_cache->end() )
    {
      st.cache_hits++;
      if ( !std::get<0>( cache_it->second ) )
      {
        return; /* quit */
      }
      tt_ext = std::get<1>( cache_it->second );
      trans_2 = std::get<2>( cache_it->second );
    }
    else
    {
      st.cache_misses++;
      const auto spectral = call_with_stopwatch( st.time_classify, [&]() { return kitty::exact_spectral_canonization_limit( func_ext, 100000, [&trans_2]( auto const& ops ) {
                                                                             std::copy( ops.begin(), ops.end(),
                                                                                        std::back_inserter( trans_2 ) );
                                                                           } ); } );
      ( *classify_cache )[func_ext] = {spectral.second, spectral.first, trans_2};
      if ( !spectral.second )
      {
        st.classify_aborts++;
        return; /* quit */
      }
      tt_ext = spectral.first;
    }

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
      st.unknown_function_aborts++;
      return; /* quit */
    }

    kitty::static_truth_table<6> db_repr;
    bool out_neg = 0;
    std::vector<xag_network::signal> final_xor;
    kitty::create_from_hex_string( db_repr, original_f );
    std::vector<xag_network::signal> pis( 6, xag.get_constant( false ) );
    std::copy( begin, end, pis.begin() );

    std::vector<kitty::detail::spectral_operation> trans;
    call_with_stopwatch( st.time_classify, [&]() { return kitty::exact_spectral_canonization(
                                                       db_repr, [&trans]( auto const& ops ) {
                                                         std::copy( ops.rbegin(), ops.rend(),
                                                                    std::back_inserter( trans ) );
                                                       } ); } );

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

    xag_network::signal output;
    {
      stopwatch t( st.time_cleanup );
      cut_view topo{*db, *db_pis, db->get_node( circuit )};
      output = cleanup_dangling( topo, xag, pis.begin(), pis.end() ).front();
      if ( db->is_complemented( circuit ) )
      {
        output = !output;
      }
    }

    for ( auto const& g : final_xor )
    {
      output = xag.create_xor( output, g );
    }

    fn( out_neg ? !output : output );
  }

private:
  void build_db( std::string const& filename )
  {
    stopwatch t( st.time_parse_db );

    std::ifstream file1;
    file1.open( filename );
    std::string line;
    for ( auto h = 0; h < 6; h++ )
    {
      db_pis->push_back( db->create_pi() );
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

      for ( auto h = 0u; h < inputs; h++ )
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
      db->create_po( f );

      func_mc->insert( std::make_pair( token_f, std::make_tuple( original, mc, f ) ) );
    }
    // std::cout << db->num_pos() << std::endl;
    file1.close();
  }

private:
  xag_minmc_resynthesis_params ps;
  xag_minmc_resynthesis_stats st;

  std::shared_ptr<xag_network> db;
  std::shared_ptr<std::vector<xag_network::signal>> db_pis;
  std::shared_ptr<std::unordered_map<std::string, std::tuple<std::string, unsigned, xag_network::signal>>> func_mc;
  std::shared_ptr<std::unordered_map<kitty::static_truth_table<6>, std::tuple<bool, kitty::static_truth_table<6>, std::vector<kitty::detail::spectral_operation>>, kitty::hash<kitty::static_truth_table<6>>>> classify_cache;
};

} // namespace mockturtle
