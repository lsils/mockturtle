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
#include <kitty/algorithm.hpp>
#include <kitty/bit_operations.hpp>
#include <kitty/constructors.hpp>
#include <kitty/hash.hpp>
#include <kitty/operations.hpp>
#include <kitty/print.hpp>
#include <kitty/spectral.hpp>
#include <mockturtle/io/write_bench.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/cut_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include "../traits.hpp"

namespace mockturtle
{

struct xag_minmc_resynthesis_params
{
  bool print_stats{true};
  uint32_t exhaustive_dc_limit{10u};
  bool verify_database{false};
};

struct xag_minmc_resynthesis_stats
{
  stopwatch<>::duration time_total{0};
  stopwatch<>::duration time_parse_db{0};
  stopwatch<>::duration time_classify{0};
  stopwatch<>::duration time_construct{0};

  uint32_t cache_hits{0};
  uint32_t cache_misses{0};
  uint32_t classify_aborts{0};
  uint32_t unknown_function_aborts{0};
  uint32_t dont_cares{0};

  void report() const
  {
    std::cout << fmt::format( "[i] total time     = {:>5.2f} secs\n", to_seconds( time_total ) );
    std::cout << fmt::format( "[i] parse db time  = {:>5.2f} secs\n", to_seconds( time_parse_db ) );
    std::cout << fmt::format( "[i] classify time  = {:>5.2f} secs\n", to_seconds( time_classify ) );
    std::cout << fmt::format( "[i] - aborts       = {:>5}\n", classify_aborts );
    std::cout << fmt::format( "[i] construct time = {:>5.2f} secs\n", to_seconds( time_construct ) );
    std::cout << fmt::format( "[i] cache hits     = {:>5}\n", cache_hits );
    std::cout << fmt::format( "[i] cache misses   = {:>5}\n", cache_misses );
    std::cout << fmt::format( "[i] unknown func.  = {:>5}\n", unknown_function_aborts );
    std::cout << fmt::format( "[i] don't cares    = {:>5}\n", dont_cares );
  }
};

class xag_minmc_resynthesis
{
public:
  xag_minmc_resynthesis( std::string const& filename )
      : db( std::make_shared<xag_network>() ),
        db_pis( std::make_shared<decltype( db_pis )::element_type>( 6u ) ),
        func_mc( std::make_shared<decltype( func_mc )::element_type>() ),
        classify_cache( std::make_shared<decltype( classify_cache )::element_type>() )
  {
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
  void operator()( xag_network& xag, kitty::dynamic_truth_table function, kitty::dynamic_truth_table const& dont_cares, LeavesIterator begin, LeavesIterator end, Fn&& fn )
  {
    if ( !kitty::is_const0( dont_cares ) )
    {
      const auto cnt = kitty::count_ones( dont_cares );
      st.dont_cares += cnt;

      if ( cnt <= ps.exhaustive_dc_limit )
      {
        std::vector<uint8_t> ones;
        kitty::for_each_one_bit( dont_cares, [&]( auto bit ) {
          ones.push_back( bit );
          kitty::clear_bit( function, bit );
        } );

        for ( auto i = 0u; i < ( 1u << ones.size() ); ++i )
        {
          for ( auto j = 0u; j < ones.size(); ++j )
          {
            if ( ( i >> j ) & 1 )
            {
              kitty::set_bit( function, ones[j] );
            }
            else
            {
              kitty::clear_bit( function, ones[j] );
            }
          }
          ( *this )( xag, function, begin, end, fn );
        }
      }
      else
      {
        ( *this )( xag, function, begin, end, fn );
        kitty::for_each_one_bit( dont_cares, [&]( auto bit ) {
          kitty::flip_bit( function, bit );
          ( *this )( xag, function, begin, end, fn );
          kitty::flip_bit( function, bit );
        } );
      }
    }
    else
    {
      ( *this )( xag, function, begin, end, fn );
    }
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( xag_network& xag, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn )
  {
    stopwatch t1( st.time_total );

    const auto func_ext = kitty::extend_to<6>( function );
    std::vector<kitty::detail::spectral_operation> trans;
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
      trans = std::get<2>( cache_it->second );
    }
    else
    {
      st.cache_misses++;
      const auto spectral = call_with_stopwatch( st.time_classify,
                                                 [&]() { return kitty::exact_spectral_canonization_limit( func_ext, 100000,
                                                                                                          [&trans]( auto const& ops ) {
                                                                                                            std::copy( ops.begin(), ops.end(),
                                                                                                                       std::back_inserter( trans ) );
                                                                                                          } ); } );
      classify_cache->insert( {func_ext, {spectral.second, spectral.first, trans}} );
      if ( !spectral.second )
      {
        st.classify_aborts++;
        return; /* quit */
      }
      tt_ext = spectral.first;
    }

    xag_network::signal circuit;

    auto search = func_mc->find( kitty::to_hex( tt_ext ) );
    if ( search != func_mc->end() )
    {
      unsigned int mc{0u};
      std::string original_f;

      std::tie( original_f, mc, circuit ) = search->second;

      kitty::static_truth_table<6> db_repr;
      kitty::create_from_hex_string( db_repr, original_f );

      call_with_stopwatch( st.time_classify, [&]() { return kitty::exact_spectral_canonization(
                                                         db_repr, [&trans]( auto const& ops ) {
                                                           std::copy( ops.rbegin(), ops.rend(),
                                                                      std::back_inserter( trans ) );
                                                         } ); } );
    }
    else if ( kitty::is_const0( tt_ext ) )
    {
      circuit = db->get_constant( false );
    }
    else
    {
      //std::cout << "[w] unknown " << kitty::to_hex( tt_ext ) << " from " << kitty::to_hex( func_ext ) << "\n";
      st.unknown_function_aborts++;
      return; /* quit */
    }

    bool out_neg{false};
    std::vector<xag_network::signal> final_xor;
    std::vector<xag_network::signal> pis( 6, xag.get_constant( false ) );
    std::copy( begin, end, pis.begin() );

    stopwatch t2( st.time_construct );
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
        out_neg = !out_neg;
        break;
      case kitty::detail::spectral_operation::kind::spectral_translation:
      {
        const auto v1 = log2( t._var1 );
        const auto v2 = log2( t._var2 );
        pis[v1] = xag.create_xor( pis[v1], pis[v2] );
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

    if ( db->is_constant( db->get_node( circuit ) ) )
    {
      output = xag.get_constant( false );
    }
    else
    {
      cut_view topo{*db, *db_pis, db->get_node( circuit )};
      output = cleanup_dangling( topo, xag, pis.begin(), pis.end() ).front();
    }
    if ( db->is_complemented( circuit ) )
    {
      output = !output;
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
    stopwatch t1( st.time_total );
    stopwatch t2( st.time_parse_db );

    std::generate( db_pis->begin(), db_pis->end(), [&]() { return db->create_pi(); } );

    std::ifstream file1( filename.c_str(), std::ifstream::in );
    std::string line;
    unsigned pos{0u};

    //std::ofstream db_file( "/tmp/db", std::ofstream::out );

    while ( std::getline( file1, line ) )
    {
      pos = line.find( '\t' );
      const auto name = line.substr( 0, pos++ );
      auto original = line.substr( pos, 16u );
      pos += 17u;
      const auto token_f = line.substr( pos, 16u );
      pos += 17u;
      auto mc = std::stoul( line.substr( pos, 1u ) );
      pos += 2u;
      line.erase( 0, pos );

      auto circuit = line;
      auto orig_circuit = circuit;

      const std::string delimiter = " ";
      std::string token = circuit.substr( 0, circuit.find( delimiter ) );
      circuit.erase( 0, circuit.find( delimiter ) + delimiter.length() );
      const auto inputs = std::stoul( token );

      std::vector<xag_network::signal> hashing_circ( db_pis->begin(), db_pis->begin() + inputs );

      while ( circuit.size() > 4 )
      {
        std::vector<unsigned> signals( 2, 0 );
        std::vector<xag_network::signal> ff( 2 );
        for ( auto j = 0u; j < 2u; j++ )
        {
          token = circuit.substr( 0, circuit.find( delimiter ) );
          circuit.erase( 0, circuit.find( delimiter ) + delimiter.length() );
          signals[j] = std::stoul( token );
          if ( signals[j] == 0 )
          {
            ff[j] = db->get_constant( true );
          }
          else if ( signals[j] == 1 )
          {
            ff[j] = db->get_constant( false );
          }
          else
          {
            ff[j] = hashing_circ[signals[j] / 2 - 1] ^ ( signals[j] % 2 != 0 );
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

      const auto output = std::stoul( circuit );
      const auto f = hashing_circ[output / 2 - 1] ^ ( output % 2 != 0 );
      db->create_po( f );

      /* verify */
      if (ps.verify_database)
      {
        cut_view view{*db, *db_pis, db->get_node( f )};
        kitty::static_truth_table<6> tt, tt_repr;
        kitty::create_from_hex_string( tt, original );
        kitty::create_from_hex_string( tt_repr, token_f );
        auto result = simulate<kitty::static_truth_table<6>>( view )[0];
        if ( db->is_complemented( f ) )
        {
          result = ~result;
        }
        if ( tt != result )
        {
          std::cerr << "[w] invalid circuit for " << original << ", got " << kitty::to_hex( result ) << "\n";
          original = kitty::to_hex( result );

          const auto repr = exact_spectral_canonization(tt);
          if ( repr != tt_repr )
          {
            std::cerr << "[e] representatives do not match\n";
          }
        }

        //db_file << name << "\t" << token_f << "\t" << original << "\t" << mc << "\t" << orig_circuit << "\n";
      }

      func_mc->insert( {token_f, {original, mc, f}} );
    }
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
