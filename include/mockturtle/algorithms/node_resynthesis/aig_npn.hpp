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
  \file aig_npn.hpp
  \brief Replace with size-optimum AIGs from NPN (from ABC rewrite)

  \author Mathias Soeken
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include <fmt/format.h>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/npn.hpp>
#include <kitty/print.hpp>
#include <kitty/static_truth_table.hpp>

#include "../../algorithms/simulation.hpp"
#include "../../io/write_bench.hpp"
#include "../../networks/aig.hpp"
#include "../../networks/xag.hpp"
#include "../../utils/node_map.hpp"
#include "../../utils/stopwatch.hpp"

namespace mockturtle
{

struct aig_npn_resynthesis_params
{
  /*! \brief Be verbose. */
  bool verbose{false};
};

struct aig_npn_resynthesis_stats
{
  stopwatch<>::duration time_classes{0};
  stopwatch<>::duration time_db{0};

  uint32_t db_size;
  uint32_t covered_classes;

  void report() const
  {
    std::cout << fmt::format( "[i] build classes time = {:>5.2f} secs\n", to_seconds( time_classes ) );
    std::cout << fmt::format( "[i] build db time      = {:>5.2f} secs\n", to_seconds( time_db ) );
  }
};

/*! \brief Resynthesis function based on pre-computed AIGs.
 *
 *
 */
class aig_npn_resynthesis
{
public:
  aig_npn_resynthesis()
      : _classes( 1 << 16 )
  {
    _repr.reserve( 222u );
    build_classes();
    build_db();
  }

  virtual ~aig_npn_resynthesis()
  {
    if ( ps.verbose )
    {
      st.report();
    }
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( aig_network& aig, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn )
  {
    kitty::static_truth_table<4> tt = kitty::extend_to<4>( function );

    /* get representative of function */
    const auto repr = _repr[_classes[*tt.cbegin()]];

    /* check if representative has circuits */
    const auto it = _repr_to_signal.find( repr );
    if ( it == _repr_to_signal.end() )
    {
      return;
    }

    const auto config = kitty::exact_npn_canonization( tt );

    assert( repr == std::get<0>( config ) );

    std::vector<aig_network::signal> pis( 4, aig.get_constant( false ) );
    std::copy( begin, end, pis.begin() );

    std::vector<aig_network::signal> pis_perm( 4 );
    auto perm = std::get<2>( config );
    for ( auto i = 0; i < 4; ++i )
    {
      pis_perm[i] = pis[perm[i]];
    }

    const auto& phase = std::get<1>( config );
    for ( auto i = 0; i < 4; ++i )
    {
      if ( ( phase >> perm[i] ) & 1 )
      {
        pis_perm[i] = !pis_perm[i];
      }
    }

    for ( auto const& cand : it->second )
    {
      std::unordered_map<xag_network::node, aig_network::signal> db_to_ntk;
      db_to_ntk[0] = aig.get_constant( false );
      for ( auto i = 0; i < 4; ++i )
      {
        db_to_ntk[i + 1] = pis_perm[i];
      }
      const auto f = copy_db_entry( aig, _db.get_node( cand ), db_to_ntk ) ^ _db.is_complemented( cand );
      if ( !fn( ( ( phase >> 4 ) & 1 ) ? !f : f ) )
      {
        return;
      }
      //break;
    }
  }

private:
  aig_network::signal
  copy_db_entry( aig_network& aig, xag_network::node const& n, std::unordered_map<xag_network::node, aig_network::signal>& db_to_ntk ) const
  {
    if ( const auto it = db_to_ntk.find( n ); it != db_to_ntk.end() )
    {
      return it->second;
    }

    std::array<aig_network::signal, 2> fanin;
    _db.foreach_fanin( n, [&]( auto const& f, auto i ) {
      fanin[i] = copy_db_entry( aig, _db.get_node( f ), db_to_ntk ) ^ _db.is_complemented( f );
    } );

    const auto f = _db.is_xor( n ) ? aig.create_xor( fanin[0], fanin[1] ) : aig.create_and( fanin[0], fanin[1] );
    return db_to_ntk[n] = f;
  }

  void build_classes()
  {
    stopwatch t( st.time_classes );

    kitty::dynamic_truth_table map( 16u );
    std::transform( map.cbegin(), map.cend(), map.begin(), []( auto word ) { return ~word; } );

    int64_t index = 0;
    kitty::static_truth_table<4> tt;
    while ( index != -1 )
    {
      kitty::create_from_words( tt, &index, &index + 1 );
      const auto res = kitty::exact_npn_canonization( tt, [&]( const auto& tt ) {
        _classes[*tt.cbegin()] = _repr.size();
        kitty::clear_bit( map, *tt.cbegin() );
      } );
      _repr.push_back( std::get<0>( res ) );

      /* find next non-classified truth table */
      index = find_first_one_bit( map );
    }
  }

  void build_db()
  {
    stopwatch t( st.time_db );

    /* four primary inputs */
    _db.create_pi();
    _db.create_pi();
    _db.create_pi();
    _db.create_pi();

    auto* p = subgraphs;
    while ( true )
    {
      auto entry0 = *p++;
      auto entry1 = *p++;

      if ( entry0 == 0 && entry1 == 0 )
        break;

      auto is_xor = entry0 & 1;
      entry0 >>= 1;

      const auto child0 = _db.make_signal( entry0 >> 1 ) ^ ( entry0 & 1 );
      const auto child1 = _db.make_signal( entry1 >> 1 ) ^ ( entry1 & 1 );

      if ( is_xor )
      {
        _db.create_xor( child0, child1 );
      }
      else
      {
        _db.create_and( child0, child1 );
      }
    }

    const auto sim_res = simulate_nodes<kitty::static_truth_table<4>>( _db );

    _db.foreach_node( [&]( auto n ) {
      if ( _repr[_classes[*sim_res[n].cbegin()]] == sim_res[n] )
      {
        if ( _repr_to_signal.count( sim_res[n] ) == 0 )
        {
          _repr_to_signal.insert( {sim_res[n], {_db.make_signal( n )}} );
        }
        else
        {
          _repr_to_signal[sim_res[n]].push_back( _db.make_signal( n ) );
        }
        //std::cout << fmt::format( "[i] signal {} realizes class {}\n", n, kitty::to_hex( sim_res[n] ) );
      }
      else
      {
        const auto f = ~sim_res[n];
        if ( _repr[_classes[*f.cbegin()]] == f )
        {
          if ( _repr_to_signal.count( f ) == 0 )
          {
            _repr_to_signal.insert( {f, {!_db.make_signal( n )}} );
          }
          else
          {
            _repr_to_signal[f].push_back( !_db.make_signal( n ) );
          }
          //std::cout << fmt::format( "[i] signal !{} realizes class {}\n", n, kitty::to_hex( f ) );
        }
      }
    } );

    st.db_size = _db.size();
    st.covered_classes = _repr_to_signal.size();
  }

  aig_npn_resynthesis_params ps;
  aig_npn_resynthesis_stats st;

  std::vector<kitty::static_truth_table<4>> _repr;
  std::vector<uint8_t> _classes;
  std::unordered_map<kitty::static_truth_table<4>, std::vector<xag_network::signal>, kitty::hash<kitty::static_truth_table<4>>> _repr_to_signal;

  xag_network _db;

  // clang-format off
  inline static const uint16_t subgraphs[]
  {
    0x0008,0x0002, 0x000a,0x0002, 0x0008,0x0003, 0x000a,0x0003, 0x0009,0x0002,
    0x000c,0x0002, 0x000e,0x0002, 0x000c,0x0003, 0x000e,0x0003, 0x000d,0x0002,
    0x000c,0x0004, 0x000e,0x0004, 0x000c,0x0005, 0x000e,0x0005, 0x000d,0x0004,
    0x0010,0x0002, 0x0012,0x0002, 0x0010,0x0003, 0x0012,0x0003, 0x0011,0x0002,
    0x0010,0x0004, 0x0012,0x0004, 0x0010,0x0005, 0x0012,0x0005, 0x0011,0x0004,
    0x0010,0x0006, 0x0012,0x0006, 0x0010,0x0007, 0x0012,0x0007, 0x0011,0x0006,
    0x0016,0x0005, 0x0014,0x0006, 0x0016,0x0006, 0x0014,0x0007, 0x0016,0x0007,
    0x0015,0x0006, 0x0014,0x0008, 0x0016,0x0008, 0x0014,0x0009, 0x0016,0x0009,
    0x0015,0x0008, 0x0018,0x0006, 0x001a,0x0006, 0x0018,0x0007, 0x001a,0x0007,
    0x0019,0x0006, 0x0018,0x0009, 0x001a,0x0009, 0x0019,0x0008, 0x001e,0x0005,
    0x001c,0x0006, 0x001e,0x0006, 0x001c,0x0007, 0x001e,0x0007, 0x001d,0x0006,
    0x001c,0x0008, 0x001e,0x0008, 0x001c,0x0009, 0x001e,0x0009, 0x001d,0x0008,
    0x0020,0x0006, 0x0022,0x0006, 0x0020,0x0007, 0x0022,0x0007, 0x0021,0x0006,
    0x0020,0x0008, 0x0022,0x0008, 0x0020,0x0009, 0x0022,0x0009, 0x0021,0x0008,
    0x0024,0x0006, 0x0026,0x0006, 0x0024,0x0007, 0x0026,0x0007, 0x0025,0x0006,
    0x0026,0x0008, 0x0024,0x0009, 0x0026,0x0009, 0x0025,0x0008, 0x0028,0x0004,
    0x002a,0x0004, 0x0028,0x0005, 0x002a,0x0007, 0x0028,0x0008, 0x002a,0x0009,
    0x0029,0x0008, 0x002a,0x000b, 0x0029,0x000a, 0x002a,0x000f, 0x0029,0x000e,
    0x002a,0x0011, 0x002a,0x0013, 0x002c,0x0004, 0x002e,0x0004, 0x002c,0x0005,
    0x002c,0x0009, 0x002e,0x0009, 0x002d,0x0008, 0x002d,0x000c, 0x002e,0x000f,
    0x002e,0x0011, 0x002e,0x0012, 0x0030,0x0004, 0x0032,0x0007, 0x0032,0x0009,
    0x0031,0x0008, 0x0032,0x000b, 0x0032,0x000d, 0x0032,0x000f, 0x0031,0x000e,
    0x0032,0x0013, 0x0034,0x0004, 0x0036,0x0004, 0x0034,0x0005, 0x0036,0x0005,
    0x0035,0x0004, 0x0036,0x0008, 0x0034,0x0009, 0x0036,0x0009, 0x0035,0x0008,
    0x0036,0x000b, 0x0036,0x000d, 0x0036,0x0011, 0x0035,0x0010, 0x0036,0x0013,
    0x0038,0x0004, 0x0039,0x0004, 0x0038,0x0009, 0x003a,0x0009, 0x0039,0x0008,
    0x0038,0x000b, 0x003a,0x000b, 0x003a,0x000d, 0x003a,0x0011, 0x003a,0x0012,
    0x0038,0x0013, 0x003a,0x0013, 0x003c,0x0002, 0x003e,0x0002, 0x003c,0x0003,
    0x003e,0x0005, 0x003e,0x0007, 0x003c,0x0008, 0x003e,0x0008, 0x003c,0x0009,
    0x003e,0x0009, 0x003d,0x0008, 0x003e,0x000d, 0x003e,0x0011, 0x003e,0x0013,
    0x003e,0x0017, 0x003e,0x001b, 0x003e,0x001d, 0x0040,0x0002, 0x0042,0x0002,
    0x0042,0x0005, 0x0041,0x0006, 0x0042,0x0008, 0x0041,0x0008, 0x0042,0x000d,
    0x0042,0x0011, 0x0042,0x0015, 0x0042,0x0019, 0x0042,0x001b, 0x0042,0x001c,
    0x0041,0x001c, 0x0044,0x0002, 0x0046,0x0003, 0x0045,0x0004, 0x0046,0x0007,
    0x0045,0x0008, 0x0046,0x000b, 0x0046,0x000f, 0x0046,0x0013, 0x0045,0x0012,
    0x0046,0x0017, 0x0046,0x001b, 0x0046,0x0021, 0x0048,0x0002, 0x004a,0x0002,
    0x0048,0x0003, 0x004a,0x0003, 0x0049,0x0002, 0x0048,0x0008, 0x004a,0x0008,
    0x0048,0x0009, 0x004a,0x0009, 0x0049,0x0008, 0x004a,0x000b, 0x004a,0x000f,
    0x004a,0x0011, 0x004a,0x0012, 0x004a,0x0013, 0x004a,0x0015, 0x004a,0x0019,
    0x004a,0x001b, 0x004a,0x001d, 0x004c,0x0002, 0x004c,0x0003, 0x004d,0x0002,
    0x004c,0x0008, 0x004e,0x0008, 0x004c,0x0009, 0x004e,0x0009, 0x004d,0x0008,
    0x004c,0x000b, 0x004e,0x000b, 0x004c,0x000f, 0x004e,0x000f, 0x004e,0x0011,
    0x004c,0x0012, 0x004c,0x0013, 0x004e,0x0013, 0x004e,0x0015, 0x004c,0x0017,
    0x004e,0x0019, 0x004c,0x001b, 0x004e,0x001b, 0x004c,0x001c, 0x004c,0x001d,
    0x004e,0x001d, 0x0050,0x0004, 0x0052,0x0004, 0x0050,0x0006, 0x0052,0x0009,
    0x0052,0x000d, 0x0052,0x000f, 0x0052,0x0013, 0x0052,0x0017, 0x0052,0x0019,
    0x0052,0x001d, 0x0052,0x001f, 0x0052,0x0021, 0x0052,0x0023, 0x0052,0x0024,
    0x0052,0x0025, 0x0051,0x0024, 0x0052,0x0027, 0x0054,0x0004, 0x0056,0x0004,
    0x0054,0x0005, 0x0056,0x0006, 0x0054,0x0007, 0x0056,0x0011, 0x0056,0x001b,
    0x0056,0x001e, 0x0054,0x001f, 0x0056,0x001f, 0x0056,0x0020, 0x0054,0x0021,
    0x0055,0x0020, 0x0056,0x0024, 0x0054,0x0025, 0x0056,0x0025, 0x0055,0x0024,
    0x0054,0x0027, 0x0056,0x0027, 0x0055,0x0026, 0x005a,0x0007, 0x005a,0x0009,
    0x005a,0x000b, 0x005a,0x0015, 0x005a,0x001f, 0x0059,0x0020, 0x0058,0x0024,
    0x005a,0x0024, 0x005a,0x0027, 0x0059,0x0026, 0x005c,0x0004, 0x005e,0x0004,
    0x005c,0x0005, 0x005e,0x0006, 0x005c,0x0007, 0x005d,0x0006, 0x005e,0x000d,
    0x005e,0x0013, 0x005e,0x0017, 0x005c,0x001f, 0x005d,0x001e, 0x005e,0x0020,
    0x005e,0x0021, 0x005e,0x0022, 0x005e,0x0023, 0x005c,0x0024, 0x005e,0x0024,
    0x005c,0x0025, 0x005e,0x0025, 0x005d,0x0024, 0x005e,0x0026, 0x005e,0x0027,
    0x0062,0x0004, 0x0061,0x0004, 0x0062,0x0006, 0x0061,0x0006, 0x0060,0x000f,
    0x0060,0x0013, 0x0062,0x0013, 0x0060,0x0019, 0x0062,0x001c, 0x0060,0x001d,
    0x0062,0x001d, 0x0062,0x001f, 0x0060,0x0021, 0x0060,0x0023, 0x0062,0x0024,
    0x0060,0x0027, 0x0061,0x0026, 0x0064,0x0002, 0x0066,0x0002, 0x0064,0x0006,
    0x0066,0x0007, 0x0066,0x0009, 0x0066,0x000d, 0x0066,0x0013, 0x0066,0x0015,
    0x0066,0x0017, 0x0066,0x0019, 0x0066,0x001a, 0x0065,0x001a, 0x0066,0x001f,
    0x0066,0x0023, 0x0066,0x0027, 0x0066,0x002f, 0x0066,0x0030, 0x006a,0x0002,
    0x0068,0x0003, 0x0068,0x0006, 0x006a,0x0006, 0x006a,0x0011, 0x0068,0x0016,
    0x0068,0x0017, 0x006a,0x0017, 0x006a,0x001a, 0x006a,0x001b, 0x006a,0x0025,
    0x006a,0x002d, 0x006e,0x0003, 0x006e,0x0007, 0x006e,0x0009, 0x006e,0x000b,
    0x006e,0x0015, 0x006e,0x0016, 0x006e,0x0017, 0x006c,0x001a, 0x006e,0x001a,
    0x006e,0x001f, 0x006e,0x002b, 0x006e,0x0035, 0x0070,0x0002, 0x0070,0x0003,
    0x0072,0x0006, 0x0070,0x0007, 0x0071,0x0006, 0x0072,0x000b, 0x0072,0x000f,
    0x0072,0x0013, 0x0070,0x0015, 0x0071,0x0014, 0x0072,0x0017, 0x0072,0x0018,
    0x0070,0x0019, 0x0072,0x0019, 0x0070,0x001a, 0x0070,0x001b, 0x0072,0x001b,
    0x0071,0x001a, 0x0072,0x0021, 0x0072,0x0029, 0x0076,0x0002, 0x0076,0x0003,
    0x0075,0x0002, 0x0076,0x0006, 0x0074,0x0007, 0x0076,0x0007, 0x0075,0x0006,
    0x0076,0x000d, 0x0076,0x0011, 0x0076,0x0013, 0x0075,0x0014, 0x0076,0x0019,
    0x0076,0x001a, 0x0076,0x001b, 0x0075,0x001c, 0x0074,0x0023, 0x0075,0x0022,
    0x0074,0x0026, 0x0076,0x0026, 0x0074,0x0027, 0x0076,0x002b, 0x0076,0x002f,
    0x0078,0x0002, 0x0078,0x0004, 0x007a,0x0004, 0x007a,0x0005, 0x0079,0x0004,
    0x007a,0x0009, 0x007a,0x000a, 0x007a,0x000b, 0x007a,0x000d, 0x007a,0x000f,
    0x007a,0x0010, 0x007a,0x0011, 0x007a,0x0012, 0x007a,0x0013, 0x007a,0x0017,
    0x007a,0x001b, 0x007a,0x0021, 0x007a,0x0027, 0x007a,0x002b, 0x007a,0x002f,
    0x007a,0x0030, 0x0079,0x0034, 0x007a,0x0039, 0x007a,0x003a, 0x007e,0x0002,
    0x007c,0x0004, 0x007e,0x0004, 0x007e,0x000c, 0x007c,0x000d, 0x007e,0x0011,
    0x007e,0x0013, 0x007e,0x001b, 0x007e,0x0025, 0x007e,0x002d, 0x007e,0x0037,
    0x0082,0x0003, 0x0082,0x0005, 0x0082,0x0009, 0x0082,0x000b, 0x0080,0x0010,
    0x0082,0x0010, 0x0082,0x0012, 0x0082,0x0015, 0x0082,0x001f, 0x0082,0x002b,
    0x0082,0x0035, 0x0082,0x0039, 0x0082,0x003f, 0x0084,0x0002, 0x0086,0x0002,
    0x0084,0x0003, 0x0086,0x0003, 0x0085,0x0002, 0x0086,0x0004, 0x0084,0x0005,
    0x0085,0x0004, 0x0086,0x000a, 0x0084,0x000b, 0x0085,0x000a, 0x0086,0x000d,
    0x0086,0x000e, 0x0086,0x000f, 0x0084,0x0010, 0x0084,0x0011, 0x0086,0x0011,
    0x0085,0x0010, 0x0084,0x0012, 0x0084,0x0013, 0x0086,0x0013, 0x0085,0x0012,
    0x0086,0x0019, 0x0086,0x0023, 0x0086,0x0029, 0x0086,0x0033, 0x0086,0x0039,
    0x008a,0x0003, 0x0089,0x0002, 0x0088,0x0004, 0x008a,0x0004, 0x0088,0x0005,
    0x0089,0x0004, 0x008a,0x000b, 0x008a,0x0010, 0x0088,0x0011, 0x008a,0x0011,
    0x0089,0x0010, 0x0088,0x0012, 0x008a,0x0012, 0x0089,0x0012, 0x008a,0x0017,
    0x008a,0x001b, 0x0089,0x0020, 0x008a,0x0025, 0x0088,0x0027, 0x008a,0x002b,
    0x008a,0x002f, 0x008a,0x0039, 0x0088,0x003a, 0x008d,0x0044, 0x0092,0x0009,
    0x0092,0x0025, 0x0092,0x0029, 0x0092,0x002d, 0x0092,0x0033, 0x0092,0x0037,
    0x0092,0x003d, 0x0092,0x0041, 0x0095,0x0002, 0x0095,0x0004, 0x0095,0x0010,
    0x0095,0x0012, 0x0096,0x0021, 0x0096,0x0029, 0x0095,0x002e, 0x0096,0x0030,
    0x0096,0x0033, 0x0096,0x003a, 0x0096,0x0043, 0x009a,0x0008, 0x009a,0x0009,
    0x0099,0x0008, 0x009a,0x0011, 0x009a,0x0023, 0x009a,0x0033, 0x009a,0x003d,
    0x009a,0x0044, 0x009a,0x0045, 0x0099,0x0044, 0x009d,0x0002, 0x009e,0x0008,
    0x009c,0x0009, 0x009e,0x0009, 0x009d,0x0008, 0x009e,0x0011, 0x009d,0x0010,
    0x009e,0x001f, 0x009e,0x003f, 0x00a0,0x0009, 0x00a0,0x0011, 0x00a2,0x0030,
    0x00a2,0x0033, 0x00a6,0x0006, 0x00a6,0x0007, 0x00a6,0x0011, 0x00a6,0x0044,
    0x00a6,0x004b, 0x00aa,0x0007, 0x00aa,0x0015, 0x00ae,0x0006, 0x00ae,0x0011,
    0x00ae,0x001b, 0x00ae,0x0025, 0x00ae,0x003d, 0x00ae,0x0041, 0x00ae,0x0043,
    0x00ae,0x0045, 0x00b2,0x0006, 0x00b0,0x0007, 0x00b1,0x0006, 0x00b2,0x0017,
    0x00b1,0x0016, 0x00b0,0x0019, 0x00b2,0x0021, 0x00b2,0x003d, 0x00b5,0x004a,
    0x00ba,0x0009, 0x00ba,0x000f, 0x00bc,0x0009, 0x00be,0x0009, 0x00be,0x000f,
    0x00bd,0x000e, 0x00be,0x0017, 0x00c2,0x0009, 0x00c2,0x0019, 0x00c2,0x001f,
    0x00c2,0x0033, 0x00c6,0x0009, 0x00c5,0x000e, 0x00c6,0x0015, 0x00c6,0x0023,
    0x00c4,0x002d, 0x00c6,0x002f, 0x00c5,0x002e, 0x00c6,0x0045, 0x00ce,0x0007,
    0x00ce,0x0021, 0x00ce,0x0023, 0x00ce,0x0025, 0x00ce,0x0027, 0x00ce,0x0033,
    0x00ce,0x003d, 0x00d2,0x0006, 0x00d0,0x0015, 0x00d0,0x001b, 0x00d2,0x001b,
    0x00d1,0x001a, 0x00d0,0x001f, 0x00d2,0x0025, 0x00d1,0x0024, 0x00d2,0x0037,
    0x00d2,0x0041, 0x00d2,0x0045, 0x00d9,0x0044, 0x00e1,0x0004, 0x00e2,0x000d,
    0x00e2,0x0021, 0x00e0,0x003a, 0x00e6,0x003d, 0x00e6,0x0061, 0x00e6,0x0067,
    0x00e9,0x0004, 0x00ea,0x0008, 0x00ea,0x0009, 0x00ea,0x0039, 0x00e9,0x0038,
    0x00ea,0x003f, 0x00ec,0x000d, 0x00ee,0x000d, 0x00ee,0x0037, 0x00f2,0x003d,
    0x00f2,0x0062, 0x00f5,0x0002, 0x00fa,0x0017, 0x00fa,0x003d, 0x00fe,0x0006,
    0x00fd,0x0006, 0x00fc,0x0015, 0x00fe,0x001b, 0x00fc,0x0025, 0x00fe,0x0025,
    0x00fd,0x0024, 0x00fe,0x0041, 0x00fe,0x004d, 0x00fd,0x004e, 0x0101,0x0014,
    0x0106,0x004d, 0x010a,0x0009, 0x010a,0x000b, 0x0109,0x000a, 0x010a,0x004f,
    0x010a,0x0058, 0x010e,0x0008, 0x010c,0x0009, 0x010e,0x0009, 0x010d,0x0008,
    0x010e,0x000b, 0x010e,0x002b, 0x010d,0x002a, 0x010e,0x0035, 0x010e,0x003d,
    0x010e,0x003f, 0x010e,0x0049, 0x010e,0x0057, 0x010d,0x0056, 0x010d,0x0058,
    0x0111,0x0004, 0x0111,0x0006, 0x0110,0x0009, 0x0112,0x0009, 0x0111,0x0008,
    0x0112,0x002f, 0x0110,0x0035, 0x0110,0x0037, 0x0112,0x0039, 0x0112,0x003d,
    0x0112,0x003f, 0x0112,0x0045, 0x0111,0x0044, 0x0112,0x004b, 0x0112,0x0059,
    0x0112,0x0069, 0x0112,0x007f, 0x0116,0x0009, 0x0115,0x0008, 0x0114,0x000b,
    0x0116,0x000b, 0x0116,0x0058, 0x011a,0x0015, 0x011a,0x001f, 0x011a,0x002b,
    0x011a,0x003f, 0x011a,0x0049, 0x011a,0x0085, 0x011e,0x0007, 0x011e,0x0019,
    0x011e,0x001b, 0x011e,0x0023, 0x011e,0x0027, 0x011e,0x002f, 0x011e,0x0043,
    0x011e,0x004b, 0x011e,0x004e, 0x011e,0x004f, 0x011e,0x005f, 0x011e,0x0061,
    0x011e,0x0065, 0x011e,0x0083, 0x0122,0x0006, 0x0120,0x0007, 0x0122,0x0007,
    0x0121,0x0006, 0x0122,0x0049, 0x0121,0x004e, 0x0122,0x008f, 0x0125,0x0004,
    0x0124,0x0007, 0x0125,0x0006, 0x0124,0x001b, 0x0126,0x001b, 0x0126,0x0045,
    0x0126,0x0087, 0x0128,0x0007, 0x0129,0x0006, 0x012a,0x0019, 0x012a,0x003d,
    0x012a,0x0051, 0x012a,0x0065, 0x012a,0x0083, 0x012d,0x005a, 0x0132,0x0009,
    0x0132,0x008f, 0x0134,0x0009, 0x0135,0x003e, 0x013a,0x003d, 0x013a,0x0044,
    0x0139,0x0044, 0x013e,0x0009, 0x013d,0x0008, 0x013c,0x003d, 0x013c,0x0044,
    0x013c,0x0053, 0x013e,0x008f, 0x013e,0x0095, 0x0142,0x0044, 0x0142,0x0097,
    0x0142,0x009e, 0x0144,0x0007, 0x0148,0x0015, 0x0148,0x001c, 0x0148,0x001f,
    0x0148,0x0026, 0x0149,0x0086, 0x014d,0x0006, 0x014e,0x0044, 0x014d,0x0048,
    0x014e,0x009e, 0x0152,0x0009, 0x0151,0x00a6, 0x0155,0x0030, 0x015d,0x003a,
    0x0162,0x009e, 0x0164,0x000f, 0x0164,0x0013, 0x0169,0x000e, 0x0174,0x0009,
    0x0179,0x0008, 0x0180,0x0009, 0x0181,0x0044, 0x0186,0x0044, 0x0185,0x0044,
    0x018a,0x0068, 0x0195,0x004e, 0x01a6,0x0009, 0x01a5,0x0008, 0x01b1,0x003a,
    0x01c4,0x0029, 0x01c4,0x0030, 0x01ca,0x008f, 0x01ca,0x0095, 0x01cc,0x0029,
    0x01cc,0x0033, 0x01ce,0x003d, 0x01d6,0x00b2, 0x01d8,0x0009, 0x01d9,0x002a,
    0x01d9,0x0056, 0x01d9,0x00a4, 0x01dd,0x003a, 0x01e2,0x00b2, 0x01e6,0x0013,
    0x01e6,0x009f, 0x01e6,0x00ba, 0x01e6,0x00c0, 0x01e6,0x00d3, 0x01e6,0x00d5,
    0x01e6,0x00e5, 0x01e8,0x0005, 0x01f2,0x0013, 0x01f2,0x0095, 0x01f2,0x009f,
    0x01f2,0x00ba, 0x01f2,0x00c0, 0x01f2,0x00d3, 0x0202,0x008f, 0x0202,0x0095,
    0x0202,0x00f3, 0x0202,0x00f9, 0x020a,0x0044, 0x0209,0x00b4, 0x020e,0x0009,
    0x020d,0x0008, 0x020c,0x003d, 0x020c,0x0044, 0x020c,0x0053, 0x020e,0x008f,
    0x020e,0x0095, 0x020c,0x00b1, 0x020e,0x00f3, 0x020e,0x00f9, 0x0210,0x0013,
    0x0211,0x0024, 0x0210,0x0026, 0x0219,0x0004, 0x021e,0x008f, 0x021e,0x0095,
    0x0221,0x003a, 0x0230,0x0009, 0x0236,0x0009, 0x0234,0x0029, 0x0234,0x0030,
    0x0234,0x0033, 0x0234,0x003a, 0x0234,0x003d, 0x0234,0x0044, 0x0235,0x00a6,
    0x023a,0x0009, 0x023d,0x003a, 0x0245,0x0044, 0x0249,0x003a, 0x024e,0x009e,
    0x024e,0x0106, 0x0251,0x0026, 0x0258,0x0013, 0x0259,0x0024, 0x0258,0x0061,
    0x0259,0x0086, 0x0258,0x00c7, 0x0258,0x00df, 0x0259,0x00ec, 0x0258,0x00fc,
    0x025d,0x0024, 0x025d,0x00de, 0x0260,0x00f6, 0x0268,0x0009, 0x0269,0x0044,
    0x0268,0x00f3, 0x0268,0x00f9, 0x026d,0x003a, 0x0270,0x0068, 0x0275,0x003a,
    0x027a,0x0044, 0x0279,0x0044, 0x027e,0x007e, 0x0281,0x0044, 0x0285,0x0008,
    0x028d,0x0006, 0x028d,0x00d2, 0x0295,0x00cc, 0x0296,0x00f6, 0x0295,0x00f8,
    0x0299,0x0030, 0x029e,0x007e, 0x029d,0x0080, 0x02a6,0x008f, 0x02a6,0x0095,
    0x02aa,0x0029, 0x02aa,0x0030, 0x02b5,0x0008, 0x02b9,0x003a, 0x02bd,0x0004,
    0x02bd,0x00fc, 0x02c2,0x00b2, 0x02c1,0x00b4, 0x02c4,0x0029, 0x02c8,0x0029,
    0x02c8,0x0033, 0x02ca,0x003d, 0x02ce,0x0029, 0x02ce,0x0030, 0x02d2,0x0068,
    0x02d1,0x006a, 0x02d5,0x006a, 0x02d9,0x0008, 0x02de,0x012c, 0x02e2,0x012c,
    0x02e4,0x0009, 0x02e5,0x002a, 0x02e5,0x0056, 0x02e5,0x012c, 0x02ea,0x0029,
    0x02ea,0x0030, 0x02e9,0x0030, 0x02ec,0x0029, 0x02ec,0x0030, 0x02ee,0x012c,
    0x02f1,0x0068, 0x02f1,0x00b2, 0x02f1,0x0108, 0x02f1,0x012c, 0x02f6,0x0013,
    0x02f6,0x0015, 0x02f6,0x001f, 0x02f6,0x0030, 0x02f6,0x0065, 0x02f6,0x0067,
    0x02f6,0x009f, 0x02f6,0x00b6, 0x02f6,0x00b9, 0x02f6,0x00c0, 0x02f6,0x00cf,
    0x02f6,0x0107, 0x02f6,0x010b, 0x02f6,0x010f, 0x02f6,0x0115, 0x02f6,0x012d,
    0x02f6,0x0134, 0x02f6,0x0153, 0x02f6,0x0171, 0x02f6,0x0176, 0x02f8,0x0003,
    0x02fa,0x017b, 0x02fc,0x00ba, 0x02fc,0x00d3, 0x0302,0x0013, 0x0302,0x001f,
    0x0302,0x0030, 0x0302,0x005d, 0x0302,0x0065, 0x0302,0x0067, 0x0302,0x0099,
    0x0302,0x009f, 0x0302,0x00ad, 0x0302,0x00b9, 0x0302,0x00c0, 0x0302,0x00cf,
    0x0301,0x00d2, 0x0301,0x00fe, 0x0302,0x0107, 0x0302,0x010b, 0x0302,0x010f,
    0x0302,0x0117, 0x0302,0x0134, 0x0302,0x0153, 0x0302,0x0157, 0x0302,0x0176,
    0x0306,0x0029, 0x0308,0x00b2, 0x0309,0x00dc, 0x030d,0x00f8, 0x0312,0x00f3,
    0x0318,0x007e, 0x031d,0x0080, 0x0321,0x0008, 0x0321,0x0094, 0x0326,0x017b,
    0x0326,0x0181, 0x0329,0x012e, 0x032a,0x017b, 0x032a,0x0181, 0x032e,0x008f,
    0x032e,0x0095, 0x032e,0x00f3, 0x032e,0x00f9, 0x0332,0x0009, 0x0331,0x0008,
    0x0330,0x003d, 0x0330,0x0044, 0x0330,0x0053, 0x0332,0x008f, 0x0332,0x0095,
    0x0330,0x00b1, 0x0332,0x00f3, 0x0332,0x00f9, 0x0330,0x0127, 0x0332,0x017b,
    0x0332,0x0181, 0x033c,0x0013, 0x033c,0x001c, 0x033d,0x0086, 0x033d,0x00ec,
    0x033d,0x0172, 0x033e,0x019d, 0x0345,0x0002, 0x0344,0x008f, 0x0344,0x00f3,
    0x034d,0x0030, 0x0352,0x0033, 0x0354,0x0029, 0x0354,0x0030, 0x035a,0x0009,
    0x035a,0x017b, 0x035a,0x019b, 0x035a,0x01a2, 0x035e,0x0181, 0x0360,0x0009,
    0x0366,0x0009, 0x0364,0x0029, 0x0364,0x0030, 0x0364,0x0033, 0x0364,0x003a,
    0x0364,0x003d, 0x0364,0x0044, 0x0369,0x0030, 0x0370,0x0029, 0x0370,0x0030,
    0x0376,0x0033, 0x037a,0x0009, 0x037a,0x019b, 0x037a,0x01a2, 0x037c,0x0009,
    0x0382,0x0181, 0x0386,0x0009, 0x0384,0x0029, 0x0384,0x0030, 0x0384,0x0033,
    0x0384,0x003a, 0x0384,0x003d, 0x0384,0x0044, 0x038a,0x0044, 0x038a,0x009e,
    0x038a,0x0106, 0x038a,0x0198, 0x038d,0x010e, 0x038d,0x0152, 0x038d,0x0158,
    0x0392,0x009e, 0x0392,0x0106, 0x0392,0x0198, 0x0395,0x0086, 0x0395,0x009a,
    0x0395,0x00ec, 0x0395,0x0172, 0x0398,0x014e, 0x0398,0x0175, 0x0398,0x018d,
    0x039c,0x0023, 0x039c,0x0027, 0x039c,0x00ef, 0x039c,0x0139, 0x039c,0x0168,
    0x03a0,0x0019, 0x03a0,0x001d, 0x03a0,0x0023, 0x03a0,0x0027, 0x03a1,0x004e,
    0x03a4,0x0162, 0x03a4,0x0183, 0x03a8,0x0013, 0x03a8,0x0027, 0x03a8,0x0133,
    0x03a8,0x0148, 0x03a8,0x0181, 0x03ac,0x0013, 0x03ac,0x0027, 0x03b0,0x017b,
    0x03b0,0x0181, 0x03b4,0x004b, 0x03b4,0x00e0, 0x03b4,0x00fb, 0x03b8,0x000f,
    0x03b8,0x0013, 0x03b8,0x00ab, 0x03b8,0x00bf, 0x03b8,0x00d0, 0x03bd,0x00da,
    0x03bd,0x012c, 0x03c8,0x000f, 0x03c8,0x0013, 0x03c8,0x0019, 0x03c8,0x001d,
    0x03cd,0x0086, 0x03cd,0x00ec, 0x03cd,0x0172, 0x03d2,0x00e0, 0x03d2,0x00ef,
    0x03d2,0x0112, 0x03d2,0x0139, 0x03d2,0x0168, 0x03d6,0x017b, 0x03d6,0x0181,
    0x03da,0x0133, 0x03da,0x0148, 0x03e2,0x0023, 0x03e2,0x0027, 0x03e6,0x0027,
    0x03e6,0x0181, 0x03ee,0x017b, 0x03ee,0x0181, 0x03fe,0x003d, 0x0401,0x012a,
    0x0401,0x019e, 0x0405,0x01a0, 0x040a,0x000d, 0x040a,0x011f, 0x040a,0x016f,
    0x040d,0x012a, 0x0412,0x017b, 0x041a,0x0033, 0x041a,0x003d, 0x041a,0x0181,
    0x0421,0x0086, 0x0421,0x009a, 0x0421,0x00ec, 0x0421,0x0172, 0x042e,0x0205,
    0x043a,0x0205, 0x043e,0x017b, 0x0442,0x01f5, 0x044c,0x0007, 0x0452,0x0033,
    0x0452,0x01ce, 0x0452,0x01d0, 0x0452,0x01f1, 0x0452,0x01fb, 0x0452,0x0225,
    0x0454,0x0005, 0x045a,0x0033, 0x045a,0x0181, 0x045a,0x01ce, 0x045a,0x01d0,
    0x045a,0x01f1, 0x0469,0x01de, 0x046e,0x0181, 0x047a,0x01ce, 0x047a,0x01f1,
    0x0485,0x012c, 0x0489,0x012c, 0x0490,0x01d8, 0x0496,0x0033, 0x0496,0x003d,
    0x0498,0x008f, 0x0498,0x00f3, 0x049e,0x0044, 0x049e,0x0221, 0x04a1,0x0006,
    0x04a2,0x0044, 0x04a6,0x0221, 0x04a9,0x0004, 0x04ac,0x0027, 0x04b1,0x009a,
    0x04b6,0x0097, 0x04b8,0x0027, 0x04c6,0x0219, 0x04ca,0x017b, 0x04cc,0x004b,
    0x04d0,0x00ab, 0x04d6,0x017b, 0x04d8,0x000f, 0x04d8,0x0019, 0x04d8,0x0033,
    0x04d8,0x003d, 0x04de,0x003d, 0x04de,0x0103, 0x04de,0x018b, 0x04de,0x0231,
    0x04e2,0x0044, 0x04e2,0x009e, 0x04e2,0x0106, 0x04e2,0x0198, 0x04e5,0x01a4,
    0x04e5,0x01b6, 0x04ea,0x009e, 0x04ea,0x0106, 0x04ea,0x0198, 0x04ed,0x002e,
    0x04ed,0x0038, 0x04ed,0x00a2, 0x04f1,0x0086, 0x04f1,0x009a, 0x04f1,0x00ec,
    0x04f1,0x0172, 0x04f9,0x004e, 0x04f8,0x0229, 0x04f8,0x022d, 0x0500,0x023e,
    0x0504,0x0217, 0x0510,0x00f3, 0x0514,0x0043, 0x0514,0x004d, 0x0514,0x00c3,
    0x0514,0x013d, 0x0514,0x0215, 0x0514,0x0232, 0x0515,0x0260, 0x0519,0x002a,
    0x0518,0x0030, 0x0518,0x0067, 0x0518,0x00c9, 0x0518,0x01eb, 0x0518,0x01ef,
    0x051c,0x0139, 0x051c,0x0168, 0x0520,0x0027, 0x0526,0x014e, 0x0526,0x0175,
    0x0526,0x018d, 0x052d,0x0200, 0x0532,0x0021, 0x0532,0x00bf, 0x0532,0x00d0,
    0x0532,0x0239, 0x0532,0x0266, 0x053d,0x0024, 0x053d,0x00da, 0x054a,0x000f,
    0x054a,0x00ab, 0x054a,0x023a, 0x054e,0x0043, 0x054e,0x004d, 0x054e,0x00c3,
    0x054e,0x013d, 0x054e,0x0215, 0x054e,0x0232, 0x054e,0x029d, 0x0552,0x014e,
    0x0552,0x018d, 0x0556,0x00f3, 0x0556,0x01e4, 0x055a,0x0299, 0x055d,0x0086,
    0x055d,0x009a, 0x055d,0x00ec, 0x055d,0x0172, 0x0566,0x01dc, 0x0566,0x02a5,
    0x056d,0x020a, 0x057a,0x003d, 0x057a,0x01d4, 0x057a,0x01f3, 0x0579,0x025e,
    0x057e,0x0139, 0x057e,0x0168, 0x0581,0x0006, 0x0586,0x017b, 0x0586,0x0181,
    0x0586,0x028c, 0x0588,0x0007, 0x058e,0x0033, 0x058e,0x008f, 0x058e,0x01d0,
    0x058e,0x027c, 0x0590,0x0003, 0x0596,0x0033, 0x0596,0x008f, 0x0596,0x0095,
    0x0596,0x01d0, 0x0596,0x027c, 0x05a2,0x026f, 0x05a5,0x0284, 0x05aa,0x017b,
    0x05ac,0x0205, 0x05b2,0x008f, 0x05b6,0x017b, 0x05b8,0x01da, 0x05c1,0x0276,
    0x05c6,0x0248, 0x05c8,0x0247, 0x05c8,0x027e, 0x05cc,0x003d, 0x05cc,0x01d4,
    0x05cc,0x01f3, 0x05d0,0x014e, 0x05d0,0x018d, 0x05da,0x00f9, 0x05dd,0x0006,
    0x05de,0x0044, 0x05e5,0x002e, 0x05e6,0x02f1, 0x05ea,0x01d4, 0x05ea,0x01f3,
    0x05ea,0x022d, 0x05ed,0x0002, 0x05f6,0x0027, 0x05fa,0x0097, 0x05fc,0x003d,
    0x0602,0x003d, 0x0606,0x00f3, 0x060a,0x0027, 0x060e,0x003d, 0x060e,0x0103,
    0x060e,0x018b, 0x060e,0x0231, 0x060e,0x02d1, 0x0611,0x01fc, 0x0611,0x0234,
    0x061a,0x0287, 0x061d,0x0214, 0x0621,0x01d4, 0x062a,0x0027, 0x062a,0x022d,
    0x062e,0x009e, 0x062e,0x0106, 0x062e,0x0198, 0x0632,0x009e, 0x0632,0x0106,
    0x0632,0x0198, 0x0639,0x0042, 0x0639,0x00b2, 0x0639,0x0108, 0x063d,0x01f8,
    0x0641,0x0086, 0x0641,0x009a, 0x0641,0x00ec, 0x0641,0x0172, 0x0645,0x0044,
    0x0649,0x0042, 0x0648,0x0087, 0x0648,0x00ed, 0x0648,0x0173, 0x0649,0x01a0,
    0x0648,0x0241, 0x0648,0x026f, 0x0648,0x02df, 0x0648,0x0307, 0x064c,0x023a,
    0x064c,0x02b3, 0x0651,0x0062, 0x0650,0x0217, 0x0651,0x02ac, 0x0650,0x02d6,
    0x0655,0x0042, 0x065d,0x0042, 0x0664,0x02b1, 0x0664,0x02ce, 0x0669,0x0238,
    0x066d,0x002a, 0x066c,0x0039, 0x066d,0x01f6, 0x066c,0x0213, 0x066c,0x022e,
    0x066d,0x02a2, 0x066c,0x02e1, 0x0671,0x002a, 0x0670,0x0030, 0x0670,0x0067,
    0x0670,0x00c9, 0x0670,0x01eb, 0x0670,0x01ef, 0x0670,0x02c3, 0x0675,0x0020,
    0x0678,0x0133, 0x0678,0x0148, 0x067c,0x0027, 0x0681,0x023a, 0x0684,0x0021,
    0x0684,0x00bf, 0x0684,0x00d0, 0x0689,0x01fc, 0x068e,0x0162, 0x068e,0x0183,
    0x0691,0x0200, 0x0696,0x0023, 0x0696,0x00e0, 0x0696,0x00fb, 0x0696,0x0268,
    0x069a,0x0282, 0x069d,0x007e, 0x06a2,0x004b, 0x06a2,0x023e, 0x06a2,0x02dc,
    0x06a6,0x0097, 0x06aa,0x02b1, 0x06aa,0x02ce, 0x06ae,0x0039, 0x06ae,0x0213,
    0x06ae,0x022e, 0x06ae,0x02e1, 0x06b2,0x0162, 0x06b2,0x0183, 0x06b6,0x0023,
    0x06b6,0x00e0, 0x06b6,0x00fb, 0x06ba,0x008f, 0x06ba,0x01e4, 0x06be,0x034b,
    0x06c1,0x0086, 0x06c1,0x009a, 0x06c1,0x00ec, 0x06c1,0x0172, 0x06c6,0x01da,
    0x06c6,0x0280, 0x06c6,0x0351, 0x06ce,0x008f, 0x06d2,0x01e3, 0x06d2,0x0287,
    0x06d2,0x0353, 0x06d6,0x027a, 0x06d6,0x029b, 0x06da,0x0033, 0x06da,0x01ce,
    0x06da,0x01f1, 0x06de,0x0133, 0x06de,0x0148, 0x06e2,0x0021, 0x06e2,0x00bf,
    0x06e2,0x00d0, 0x06e5,0x023a, 0x06e9,0x0004, 0x06ee,0x028c, 0x06ee,0x0338,
    0x06f2,0x0328, 0x06f2,0x0330, 0x06f4,0x0005, 0x06f9,0x01e0, 0x06fe,0x0328,
    0x06fe,0x0330, 0x0702,0x003d, 0x0702,0x00f3, 0x0702,0x0330, 0x0704,0x0003,
    0x070a,0x003d, 0x070a,0x00f3, 0x070a,0x01d4, 0x070a,0x01f3, 0x070a,0x0330,
    0x0711,0x032a, 0x0711,0x032e, 0x0716,0x003d, 0x0718,0x0205, 0x0718,0x0282,
    0x071e,0x00f3, 0x0720,0x01dc, 0x0720,0x02a5, 0x0726,0x0324, 0x072a,0x028a,
    0x072a,0x02a7, 0x0729,0x031c, 0x0729,0x032a, 0x072e,0x003d, 0x072e,0x00f9,
    0x072e,0x022d, 0x072e,0x0248, 0x072e,0x02e4, 0x0730,0x003d, 0x0730,0x0247,
    0x0730,0x02e3, 0x0730,0x0324, 0x0732,0x0324, 0x0739,0x032e, 0x073e,0x003d,
    0x0740,0x003d, 0x0744,0x027a, 0x0744,0x029b, 0x0748,0x0033, 0x0748,0x01ce,
    0x0748,0x01f1, 0x074c,0x0162, 0x074c,0x0183, 0x0750,0x0023, 0x0750,0x00e0,
    0x0750,0x00fb, 0x0755,0x0246, 0x075a,0x0095, 0x075a,0x0397, 0x075d,0x0004,
    0x076a,0x03b3, 0x076d,0x0002, 0x0772,0x02fb, 0x0772,0x0301, 0x0772,0x0315,
    0x0772,0x0397, 0x0776,0x008f, 0x077e,0x0027, 0x078a,0x00a1, 0x0792,0x009d,
    0x0792,0x00c3, 0x0792,0x02fb, 0x0792,0x0301, 0x0792,0x0315, 0x0792,0x03bd,
    0x0796,0x0027, 0x0796,0x024f, 0x079e,0x009d, 0x07a6,0x009d, 0x07a6,0x02fb,
    0x07a6,0x0301, 0x07a6,0x0315, 0x07a6,0x03bd, 0x07aa,0x0027, 0x07aa,0x024f,
    0x07ae,0x009d, 0x07b9,0x004e, 0x07b8,0x0087, 0x07b8,0x00ed, 0x07b8,0x0173,
    0x07b8,0x0197, 0x07b9,0x021a, 0x07b9,0x02b8, 0x07b9,0x0364, 0x07be,0x0029,
    0x07be,0x0030, 0x07c0,0x017b, 0x07c6,0x017b, 0x07c8,0x00f3, 0x07ce,0x00f3,
    0x07d0,0x008f, 0x07d6,0x008f, 0x07d9,0x01e8, 0x07dd,0x0292, 0x07e2,0x0053,
    0x07e6,0x008f, 0x07e6,0x00f3, 0x07e6,0x017b, 0x07e8,0x0029, 0x07e8,0x0030,
    0x07ec,0x0021, 0x07ec,0x02ad, 0x07f2,0x0181, 0x07f2,0x0315, 0x07f4,0x0021,
    0x07f8,0x020f, 0x07fd,0x002e, 0x0800,0x008f, 0x0805,0x0006, 0x0809,0x03c2,
    0x080d,0x0084, 0x0812,0x0009, 0x0811,0x0008, 0x0812,0x00f3, 0x0812,0x00f9,
    0x0812,0x017b, 0x0812,0x0181, 0x0814,0x0033, 0x0818,0x0023, 0x081c,0x0285,
    0x0826,0x03bd, 0x082c,0x008f, 0x082c,0x017b, 0x0832,0x0043, 0x0832,0x011b,
    0x0832,0x01b3, 0x0832,0x01c3, 0x0835,0x032a, 0x0838,0x0085, 0x0839,0x032a,
    0x083e,0x0049, 0x083d,0x0084, 0x083e,0x02fb, 0x083e,0x0301, 0x083e,0x0315,
    0x083e,0x0397, 0x0842,0x0009, 0x0841,0x0008, 0x0844,0x0009, 0x0846,0x008f,
    0x084a,0x0033, 0x084e,0x0285, 0x0851,0x009a, 0x0856,0x00a1, 0x0859,0x031c,
    0x085d,0x00b2, 0x0861,0x0012, 0x0861,0x02cc, 0x0865,0x0058, 0x0865,0x007e,
    0x0869,0x004a, 0x0871,0x0010, 0x0876,0x003d, 0x0879,0x032c, 0x087e,0x0089,
    0x0882,0x0229, 0x0882,0x022d, 0x0882,0x02c7, 0x0882,0x02cb, 0x0886,0x0021,
    0x0886,0x02ad, 0x0885,0x0356, 0x088a,0x0017, 0x088a,0x020f, 0x0889,0x0354,
    0x088d,0x009c, 0x0892,0x0089, 0x0895,0x0246, 0x089a,0x03bd, 0x089e,0x008f,
    0x089e,0x02f9, 0x089e,0x0313, 0x08a1,0x032a, 0x08a6,0x0053, 0x08a6,0x0095,
    0x08a6,0x0397, 0x08a8,0x017b, 0x08ad,0x031a, 0x08b2,0x017b, 0x08b4,0x00f3,
    0x08b5,0x02a0, 0x08b8,0x0089, 0x08c1,0x0024, 0x08c4,0x00f3, 0x08c9,0x007e,
    0x08cd,0x007c, 0x08cd,0x0222, 0x08cd,0x0294, 0x08d1,0x003a, 0x08d6,0x0009,
    0x08d9,0x003a, 0x08dc,0x001f, 0x08e0,0x008f, 0x08e0,0x017b, 0x08e4,0x0009,
    0x08e8,0x01ed, 0x08ed,0x031c, 0x08f2,0x003d, 0x08f6,0x008f, 0x08f6,0x017b,
    0x08fa,0x0009, 0x08fe,0x003d, 0x0902,0x01e9, 0x0904,0x01e9, 0x0904,0x0381,
    0x090a,0x03b1, 0x090d,0x031a, 0x0910,0x0299, 0x0914,0x034b, 0x0919,0x0008,
    0x091c,0x0033, 0x091c,0x003d, 0x0920,0x0027, 0x0924,0x0027, 0x0924,0x01fb,
    0x092a,0x01ce, 0x092a,0x01f1, 0x092d,0x031c, 0x0930,0x001f, 0x0936,0x00c5,
    0x0938,0x00c5, 0x0938,0x0381, 0x093c,0x001b, 0x0942,0x017d, 0x094a,0x0027,
    0x094e,0x0027, 0x094e,0x01fb, 0x0952,0x03b1, 0x095a,0x0029, 0x095a,0x0030,
    0x095d,0x0030, 0x0961,0x0030, 0x0966,0x02f9, 0x0966,0x0313, 0x0968,0x02eb,
    0x096d,0x0008, 0x0970,0x017b, 0x0974,0x0033, 0x0979,0x0150, 0x097d,0x009a,
    0x0982,0x0293, 0x0984,0x0293, 0x0984,0x0379, 0x098a,0x02eb, 0x098e,0x0009,
    0x0992,0x003d, 0x0996,0x003d, 0x0999,0x0062, 0x099e,0x003d, 0x09a0,0x0027,
    0x09a5,0x0144, 0x09a8,0x02b5, 0x09ae,0x008f, 0x09ae,0x009d, 0x09b2,0x004d,
    0x09b2,0x0053, 0x09b2,0x00c3, 0x09b2,0x013d, 0x09b2,0x01c5, 0x09b2,0x0271,
    0x09b4,0x0025, 0x09ba,0x0033, 0x09ba,0x0079, 0x09bc,0x0015, 0x09c2,0x013f,
    0x09c4,0x013f, 0x09c4,0x0379, 0x09ca,0x02b5, 0x09cd,0x0006, 0x09da,0x0009,
    0x09d9,0x0008, 0x09dc,0x000b, 0x09dc,0x004f, 0x09dd,0x0086, 0x09e0,0x0009,
    0x09e6,0x00a1, 0x09e8,0x0009, 0x09ed,0x0086, 0x09f2,0x001f, 0x09f2,0x002f,
    0x09f2,0x0049, 0x09f2,0x006f, 0x09f2,0x0085, 0x09f2,0x0091, 0x09f2,0x00a9,
    0x09f2,0x00d3, 0x09f2,0x00d7, 0x09f2,0x011d, 0x09f2,0x0121, 0x09f2,0x0235,
    0x09f2,0x0393, 0x09f6,0x0324, 0x09f8,0x0049, 0x09f8,0x00a9, 0x09f8,0x011d,
    0x09fe,0x001f, 0x09fe,0x0029, 0x09fe,0x0033, 0x09fe,0x003d, 0x09fe,0x0085,
    0x09fe,0x008f, 0x09fe,0x00d3, 0x0a00,0x003d, 0x0a06,0x012d, 0x0a0e,0x00b3,
    0x0a10,0x000b, 0x0a10,0x0387, 0x0a16,0x0059, 0x0a18,0x0009, 0x0a1e,0x0043,
    0x0a24,0x0085, 0x0a2a,0x0009, 0x0a2d,0x0008, 0x0a32,0x028a, 0x0a32,0x02a7,
    0x0a31,0x031c, 0x0a35,0x032e, 0x0a39,0x0006, 0x0a3a,0x0105, 0x0a3a,0x024f,
    0x0a3c,0x0299, 0x0a42,0x01ed, 0x0a46,0x0299, 0x0a48,0x01ed, 0x0a4c,0x0059,
    0x0a52,0x000b, 0x0a52,0x0387, 0x0a56,0x000b, 0x0a5e,0x0009, 0x0a60,0x003d,
    0x0a66,0x0105, 0x0a6a,0x0195, 0x0a6c,0x000b, 0x0a76,0x0053, 0x0a78,0x0009,
    0x0a7a,0x008f, 0x0a82,0x0299, 0x0a86,0x01ed, 0x0a8a,0x0027, 0x0a8e,0x004b,
    0x0a92,0x003d, 0x0a95,0x0322, 0x0a99,0x0038, 0x0a99,0x0090, 0x0a9c,0x0061,
    0x0a9c,0x00c7, 0x0a9c,0x012d, 0x0a9c,0x016f, 0x0a9c,0x017d, 0x0a9c,0x02c9,
    0x0a9c,0x0383, 0x0aa1,0x0010, 0x0aa4,0x00b3, 0x0aa8,0x002f, 0x0aac,0x0027,
    0x0ab0,0x004b, 0x0ab4,0x0043, 0x0ab9,0x0090, 0x0abd,0x0010, 0x0ac4,0x0019,
    0x0acc,0x00f5, 0x0acc,0x022b, 0x0acc,0x037b, 0x0ad2,0x008f, 0x0ad2,0x01f1,
    0x0ad6,0x0324, 0x0ad9,0x0330, 0x0ade,0x008f, 0x0ade,0x01f1, 0x0ae0,0x017b,
    0x0ae4,0x008f, 0x0ae9,0x004e, 0x0aee,0x0027, 0x0af2,0x028a, 0x0af2,0x02a7,
    0x0af1,0x031c, 0x0af6,0x0027, 0x0af9,0x031c, 0x0afe,0x00e9, 0x0afe,0x02bb,
    0x0b02,0x000b, 0x0b06,0x00f5, 0x0b06,0x022b, 0x0b06,0x037b, 0x0b0a,0x003d,
    0x0000,0x0000
  };
  // clang-format on
}; // namespace mockturtle

} /* namespace mockturtle */
