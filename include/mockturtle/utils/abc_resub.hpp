/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2020  EPFL
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
  \file abc_resub.hpp
  \brief Interface to `abc_resub`.

  \author Heinz Riener
*/

#pragma once

#include <kitty/kitty.hpp>
#include <abcresub/abcresub.hpp>

namespace mockturtle
{
#if 0
enum gate_type
{
  AND,
  XOR,
};

struct gate
{
  struct fanin
  {
    uint32_t idx;
    bool inv{false};
  };

  std::vector<fanin> fanins;
  gate_type type{AND};
};
#endif

class abc_resub
{
public:
  explicit abc_resub( uint64_t num_divisors, uint64_t num_blocks_per_truth_table, uint64_t max_num_divisors = 50ul )
    : num_divisors( num_divisors )
    , num_blocks_per_truth_table( num_blocks_per_truth_table )
    , max_num_divisors( max_num_divisors )
    , counter(0)
  {
    alloc();
  }

  virtual ~abc_resub()
  {
    release();
  }

  template<class truth_table_type>
  void add_root( truth_table_type const& tt, truth_table_type const& care )
  {
    add_divisor( ~tt & care ); /* off-set */
    add_divisor( tt & care ); /* on-set */
  }

  template<class truth_table_type>
  void add_divisor( truth_table_type const& tt )
  {
    assert( abc_tts != nullptr && "assume that memory for truth tables has been allocated" );
    assert( abc_divs != nullptr && "assume that memory for divisors has been allocated" );

    assert( tt.num_blocks() == num_blocks_per_truth_table );
    for ( uint64_t i = 0ul; i < num_blocks_per_truth_table; ++i )
    {
      Vec_WrdPush( abc_tts, tt._bits[i] );
    }
    Vec_PtrPush( abc_divs, Vec_WrdEntryP( abc_tts, counter * num_blocks_per_truth_table ) );
    ++counter;
  }

  template<class iterator_type, class truth_table_storage_type>
  void add_divisors( iterator_type begin, iterator_type end, truth_table_storage_type const& tts )
  {
    assert( abc_tts != nullptr && "assume that memory for truth tables has been allocated" );
    assert( abc_divs != nullptr && "assume that memory for divisors has been allocated" );

    while ( begin != end )
    {
      add_divisor( tts[*begin] );
      ++begin;
    }
  }

  std::optional<std::vector<uint32_t>> compute_function( uint32_t num_inserts, bool useXOR = false )
  {
    int index_list_size;
    int * index_list;
    index_list_size = abcresub::Abc_ResubComputeFunction( (void **)Vec_PtrArray( abc_divs ), Vec_PtrSize( abc_divs ), num_blocks_per_truth_table, num_inserts, /* nDivsMax */max_num_divisors, /* nChoice = */0, int(useXOR), /* debug = */0, /* verbose = */0, &index_list );

    if ( index_list_size )
    {
      std::vector<uint32_t> vec( index_list_size );
      for ( auto i = 0; i < index_list_size; ++i ) // probably could be smarter
      {
        vec[i] = index_list[i];
      }
      return vec;
    }

    return std::nullopt;
  }

#if 0 /* not used */
  std::optional<std::vector<gate>> compute_function( bool& output_negation, uint32_t num_inserts, bool useXOR = false )
  {
    int index_list_size;
    int * index_list;
    index_list_size = abcresub::Abc_ResubComputeFunction( (void **)Vec_PtrArray( abc_divs ),  Vec_PtrSize( abc_divs ), num_blocks_per_truth_table, num_inserts, /* nDivsMax */max_num_divisors, /* nChoice = */0, int(useXOR), /* debug = */0, /* verbose = */0, &index_list );

    if ( index_list_size )
    {
      uint64_t const num_gates = ( index_list_size - 1u ) / 2u;
      std::vector<gate> gates( num_gates );
      for ( auto i = 0u; i < num_gates; ++i )
      {
        gate::fanin f0{uint32_t( ( index_list[2*i] >> 1u ) - 2u ), bool( index_list[2*i] % 2 )};
        gate::fanin f1{uint32_t( ( index_list[2*i + 1u] >> 1u ) - 2u ), bool( index_list[2*i + 1u] % 2 )};
        gates[i].fanins = { f0, f1 };

        gates[i].type = f0.idx < f1.idx ? gate_type::AND : gate_type::XOR;
      }
      output_negation = index_list[index_list_size - 1u] % 2;
      return gates;
    }

    return std::nullopt;
  }
#endif

  void dump( std::string const file = "dump.txt" ) const
  {
    abcresub::Abc_ResubDumpProblem( file.c_str(), (void **)Vec_PtrArray( abc_divs ),  Vec_PtrSize( abc_divs ), num_blocks_per_truth_table );
  }

protected:
  void alloc()
  {
    assert( abc_tts == nullptr );
    assert( abc_divs == nullptr );
    abc_tts = abcresub::Vec_WrdAlloc( num_divisors * num_blocks_per_truth_table );
    abc_divs = abcresub::Vec_PtrAlloc( num_divisors );
  }

  void release()
  {
    assert( abc_divs != nullptr );
    assert( abc_tts != nullptr );
    Vec_PtrFree( abc_divs );
    Vec_WrdFree( abc_tts );
    abc_divs = nullptr;
    abc_tts = nullptr;
  }

protected:
  uint64_t num_divisors;
  uint64_t num_blocks_per_truth_table;
  uint64_t max_num_divisors;
  uint64_t counter;

  abcresub::Vec_Wrd_t * abc_tts{nullptr};
  abcresub::Vec_Ptr_t * abc_divs{nullptr};
}; /* abc_resub */

} /* namespace mockturtle */
