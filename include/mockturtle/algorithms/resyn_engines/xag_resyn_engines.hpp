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
  \file xag_resyn_engines.hpp
  \brief Resynthesis by recursive decomposition for AIGs or XAGs.
  (based on ABC's implementation in `giaResub.c` by Alan Mishchenko)

  \author Siang-Yun Lee
*/

#pragma once

#include "../../utils/index_list.hpp"
#include "../../utils/stopwatch.hpp"

#include <kitty/kitty.hpp>
#include <fmt/format.h>

#include <vector>
#include <algorithm>
#include <type_traits>

namespace mockturtle
{

struct xag_resyn_engine_params
{
  /*! \brief Maximum size (number of gates) of the dependency circuit. */
  uint32_t max_size{0u};

  /*! \brief Maximum number of binate divisors to be considered. */
  uint32_t max_binates{50u};

  /*! \brief Reserved capacity for divisor truth tables (number of divisors). */
  uint32_t reserve{200u};
};

struct xag_resyn_engine_stats
{
  /*! \brief Time for adding divisor truth tables. */
  //stopwatch<>::duration time_add_divisor{0};

  /*! \brief Time for finding 0-resub and collecting unate literals. */
  stopwatch<>::duration time_unate{0};

  /*! \brief Time for finding 1-resub. */
  stopwatch<>::duration time_resub1{0};

  /*! \brief Time for finding 2-resub. */
  stopwatch<>::duration time_resub2{0};

  /*! \brief Time for finding 3-resub. */
  stopwatch<>::duration time_resub3{0};

  /*! \brief Time for sorting unate literals and unate pairs. */
  stopwatch<>::duration time_sort{0};

  /*! \brief Time for collecting unate pairs. */
  stopwatch<>::duration time_collect_pairs{0};

  /*! \brief Time for dividing the target and recursive call. */
  stopwatch<>::duration time_divide{0};

  void report() const
  {
    fmt::print( "[i]         <xag_resyn_engine>\n" );
    //fmt::print( "[i]             add divisors : {:>5.2f} secs\n", to_seconds( time_add_divisor ) );
    fmt::print( "[i]             0-resub      : {:>5.2f} secs\n", to_seconds( time_unate ) );
    fmt::print( "[i]             1-resub      : {:>5.2f} secs\n", to_seconds( time_resub1 ) );
    fmt::print( "[i]             2-resub      : {:>5.2f} secs\n", to_seconds( time_resub2 ) );
    fmt::print( "[i]             3-resub      : {:>5.2f} secs\n", to_seconds( time_resub3 ) );
    fmt::print( "[i]             sort         : {:>5.2f} secs\n", to_seconds( time_sort ) );
    fmt::print( "[i]             collect pairs: {:>5.2f} secs\n", to_seconds( time_collect_pairs ) );
    fmt::print( "[i]             dividing     : {:>5.2f} secs\n", to_seconds( time_divide ) );
  }
};

/*! \brief Logic resynthesis engine for AIGs or XAGs.
 *
 * The algorithm is based on ABC's implementation in `giaResub.c` by Alan Mishchenko.
 *
 * Divisors are classified as positive unate (not overlapping with target offset),
 * negative unate (not overlapping with target onset), or binate (overlapping with
 * both onset and offset). Furthermore, pairs of binate divisors are combined with
 * an AND operation and considering all possible input polarities and again classified
 * as positive unate, negative unate or binate. Simple solutions of zero cost 
 * (one unate divisor), one node (two unate divisors), two nodes (one unate divisor + 
 * one unate pair), and three nodes (two unate pairs) are exhaustively examined.
 * When no simple solutions can be found, the algorithm heuristically chooses an unate
 * divisor or an unate pair to divide the target function with and recursively calls
 * itself to decompose the remainder function.
 *
 * \param use_xor Whether to consider XOR gates as having the same cost as AND gates (i.e., using XAGs).
 */
template<class TT, typename node_type, typename truth_table_storage_type, bool copy_tts = true, bool use_xor = false>
class xag_resyn_engine
{
public:
  using stats = xag_resyn_engine_stats;
  using params = xag_resyn_engine_params;
  using index_list_t = xag_index_list<true>;
  using truth_table_t = TT;

private:
  struct unate_lit
  {
    unate_lit( uint32_t l )
      : lit( l )
    { }

    bool operator==( unate_lit const& other ) const
    {
      return lit == other.lit;
    }

    uint32_t lit;
    uint32_t score{0};
  };

  struct fanin_pair
  {
    fanin_pair( uint32_t l1, uint32_t l2 )
      : lit1( l1 < l2 ? l1 : l2 ), lit2( l1 < l2 ? l2 : l1 )
    { }

    fanin_pair( uint32_t l1, uint32_t l2, bool is_xor )
      : lit1( l1 > l2 ? l1 : l2 ), lit2( l1 > l2 ? l2 : l1 )
    { (void)is_xor; }

    bool operator==( fanin_pair const& other ) const
    {
      return lit1 == other.lit1 && lit2 == other.lit2;
    }

    uint32_t lit1, lit2;
    uint32_t score{0};
  };

public:
  explicit xag_resyn_engine( TT const& target, TT const& care, truth_table_storage_type const& tts, stats& st, params const& ps = {} )
    : on_off_sets( { ~target & care, target & care } ), tts( tts ), st( st ), ps( ps )
  {
    divisors.reserve( ps.reserve );
    divisors.resize( 1 ); // reserve 1 dummy node for constant
  }

  void add_divisor( node_type const& n )
  {
    if constexpr ( copy_tts )
    {
      divisors.emplace_back( tts[n] );
    }
    else
    {
      divisors.emplace_back( n );
    }
  }

  template<class iterator_type>
  void add_divisors( iterator_type begin, iterator_type end )
  { 
    while ( begin != end )
    {
      add_divisor( *begin );
      ++begin;
    }
  }

  std::optional<index_list_t> operator()()
  {
    return compute_function();
  }

  template<class iterator_type>
  std::optional<index_list_t> operator()( iterator_type begin, iterator_type end )
  {
    add_divisors( begin, end );
    return compute_function();
  }

private:
  std::optional<index_list_t> compute_function()
  {
    index_list.add_inputs( divisors.size() - 1 );
    auto const lit = compute_function_rec( ps.max_size );
    if ( lit )
    {
      assert( index_list.num_gates() <= ps.max_size );
      index_list.add_output( *lit );
      return index_list;
    }
    return std::nullopt;
  }

  std::optional<uint32_t> compute_function_rec( uint32_t num_inserts )
  {
    pos_unate_lits.clear();
    neg_unate_lits.clear();
    binate_divs.clear();
    pos_unate_pairs.clear();
    neg_unate_pairs.clear();

    /* try 0-resub and collect unate literals */
    auto const res0 = call_with_stopwatch( st.time_unate, [&]() {
      return find_one_unate();
    });
    if ( res0 )
    {
      return *res0;
    }
    if ( num_inserts == 0u )
    {
      return std::nullopt;
    }

    /* sort unate literals and try 1-resub */
    call_with_stopwatch( st.time_sort, [&]() {
      sort_unate_lits( pos_unate_lits, 1 );
      sort_unate_lits( neg_unate_lits, 0 );
    });
    auto const res1or = call_with_stopwatch( st.time_resub1, [&]() {
      return find_div_div( pos_unate_lits, 1 );
    });
    if ( res1or )
    {
      return *res1or;
    }
    auto const res1and = call_with_stopwatch( st.time_resub1, [&]() {
      return find_div_div( neg_unate_lits, 0 );
    });
    if ( res1and )
    {
      return *res1and;
    }

    if ( binate_divs.size() > ps.max_binates )
    {
      binate_divs.resize( ps.max_binates );
    }

    if constexpr ( use_xor )
    {
      /* collect XOR-type unate pairs and try 1-resub with XOR */
      auto const res1xor = find_xor();
      if ( res1xor )
      {
        return *res1xor;
      }
    }
    if ( num_inserts == 1u )
    {
      return std::nullopt;
    }

    /* collect AND-type unate pairs and sort (both types), then try 2- and 3-resub */
    call_with_stopwatch( st.time_collect_pairs, [&]() {
      collect_unate_pairs();
    });
    call_with_stopwatch( st.time_sort, [&]() {
      sort_unate_pairs( pos_unate_pairs, 1 );
      sort_unate_pairs( neg_unate_pairs, 0 );
    });
    auto const res2or = call_with_stopwatch( st.time_resub2, [&]() {
      return find_div_pair( pos_unate_lits, pos_unate_pairs, 1 );
    });
    if ( res2or )
    {
      return *res2or;
    }
    auto const res2and = call_with_stopwatch( st.time_resub2, [&]() {
      return find_div_pair( neg_unate_lits, neg_unate_pairs, 0 );
    });
    if ( res2and )
    {
      return *res2and;
    }

    if ( num_inserts >= 3u )
    {
      auto const res3or = call_with_stopwatch( st.time_resub3, [&]() {
        return find_pair_pair( pos_unate_pairs, 1 );
      });
      if ( res3or )
      {
        return *res3or;
      }
      auto const res3and = call_with_stopwatch( st.time_resub3, [&]() {
        return find_pair_pair( neg_unate_pairs, 0 );
      });
      if ( res3and )
      {
        return *res3and;
      }
    }

    /* choose something to divide and recursive call on the remainder */
    /* Note: dividing = AND the on-set (if using positive unate) or the off-set (if using negative unate)
                        with the *negation* of the divisor/pair (subtracting) */
    uint32_t on_off_div, on_off_pair;
    uint32_t score_div = 0, score_pair = 0;

    call_with_stopwatch( st.time_divide, [&]() {
      if ( pos_unate_lits.size() > 0 )
      {
        on_off_div = 1; /* use pos_lit */
        score_div = pos_unate_lits[0].score;
        if ( neg_unate_lits.size() > 0 && neg_unate_lits[0].score > pos_unate_lits[0].score )
        {
          on_off_div = 0; /* use neg_lit */
          score_div = neg_unate_lits[0].score;
        }
      }
      else if ( neg_unate_lits.size() > 0 )
      {
        on_off_div = 0; /* use neg_lit */
        score_div = neg_unate_lits[0].score;
      }

      if ( num_inserts >= 3u )
      {
        if ( pos_unate_pairs.size() > 0 )
        {
          on_off_pair = 1; /* use pos_pair */
          score_pair = pos_unate_pairs[0].score;
          if ( neg_unate_pairs.size() > 0 && neg_unate_pairs[0].score > pos_unate_pairs[0].score )
          {
            on_off_pair = 0; /* use neg_pair */
            score_pair = neg_unate_pairs[0].score;
          }
        }
        else if ( neg_unate_pairs.size() > 0 )
        {
          on_off_pair = 0; /* use neg_pair */
          score_pair = neg_unate_pairs[0].score;
        }
      }
    });

    if ( score_div > score_pair / 2 ) /* divide with a divisor */
    {
      /* if using pos_lit (on_off_div = 1), modify on-set and use an OR gate on top;
         if using neg_lit (on_off_div = 0), modify off-set and use an AND gate on top
       */
      uint32_t const lit = on_off_div ? pos_unate_lits[0].lit : neg_unate_lits[0].lit;
      call_with_stopwatch( st.time_divide, [&]() {
        on_off_sets[on_off_div] &= lit & 0x1 ? get_div( lit >> 1 ) : ~get_div( lit >> 1 );
      });

      auto const res_remain_div = compute_function_rec( num_inserts - 1 );
      if ( res_remain_div )
      {
        auto const new_lit = index_list.add_and( ( lit ^ 0x1 ), *res_remain_div ^ on_off_div );
        return new_lit + on_off_div;
      }
    }
    else if ( score_pair > 0 ) /* divide with a pair */
    {
      fanin_pair const pair = on_off_pair ? pos_unate_pairs[0] : neg_unate_pairs[0];
      call_with_stopwatch( st.time_divide, [&]() {
        if constexpr ( use_xor )
        {
          if ( pair.lit1 > pair.lit2 ) /* XOR pair: ~(lit1 ^ lit2) = ~lit1 ^ lit2 */
          {
            on_off_sets[on_off_pair] &= ( pair.lit1 & 0x1 ? get_div( pair.lit1 >> 1 ) : ~get_div( pair.lit1 >> 1 ) )
                                  ^ ( pair.lit2 & 0x1 ? ~get_div( pair.lit2 >> 1 ) : get_div( pair.lit2 >> 1 ) );
          }
          else /* AND pair: ~(lit1 & lit2) = ~lit1 | ~lit2 */
          {
            on_off_sets[on_off_pair] &= ( pair.lit1 & 0x1 ? get_div( pair.lit1 >> 1 ) : ~get_div( pair.lit1 >> 1 ) )
                                  | ( pair.lit2 & 0x1 ? get_div( pair.lit2 >> 1 ) : ~get_div( pair.lit2 >> 1 ) );
          }
        }
        else /* AND pair: ~(lit1 & lit2) = ~lit1 | ~lit2 */
        {
          on_off_sets[on_off_pair] &= ( pair.lit1 & 0x1 ? get_div( pair.lit1 >> 1 ) : ~get_div( pair.lit1 >> 1 ) )
                                | ( pair.lit2 & 0x1 ? get_div( pair.lit2 >> 1 ) : ~get_div( pair.lit2 >> 1 ) );
        }
      });

      auto const res_remain_pair = compute_function_rec( num_inserts - 2 );
      if ( res_remain_pair )
      {
        auto const new_lit1 = index_list.add_and( pair.lit1, pair.lit2 );
        auto const new_lit2 = index_list.add_and( new_lit1 ^ 0x1, *res_remain_pair ^ on_off_pair );
        return new_lit2 + on_off_pair;
      }
    }

    return std::nullopt;
  }

  /* See if there is a constant or divisor covering all on-set bits or all off-set bits.
     1. Check constant-resub
     2. Collect unate literals
     3. Find 0-resub (both positive unate and negative unate) and collect binate (neither pos nor neg unate) divisors
   */
  std::optional<uint32_t> find_one_unate()
  {
    num_bits[0] = kitty::count_ones( on_off_sets[0] ); /* off-set */
    num_bits[1] = kitty::count_ones( on_off_sets[1] ); /* on-set */
    if ( num_bits[0] == 0 )
    {
      return 1;
    }
    if ( num_bits[1] == 0 )
    {
      return 0;
    }

    for ( auto v = 1u; v < divisors.size(); ++v )
    {
      bool unateness[4] = {false, false, false, false};
      /* check intersection with off-set */
      if ( intersection_is_empty<false, false>( get_div(v), on_off_sets[0] ) )
      {
        pos_unate_lits.emplace_back( v << 1 );
        unateness[0] = true;
      }
      else if ( intersection_is_empty<true, false>( get_div(v), on_off_sets[0] ) )
      {
        pos_unate_lits.emplace_back( v << 1 | 0x1 );
        unateness[1] = true;
      }

      /* check intersection with on-set */
      if ( intersection_is_empty<false, false>( get_div(v), on_off_sets[1] ) )
      {
        neg_unate_lits.emplace_back( v << 1 );
        unateness[2] = true;
      }
      else if ( intersection_is_empty<true, false>( get_div(v), on_off_sets[1] ) )
      {
        neg_unate_lits.emplace_back( v << 1 | 0x1 );
        unateness[3] = true;
      }

      /* 0-resub */
      if ( unateness[0] && unateness[3] )
      {
        return ( v << 1 );
      }
      if ( unateness[1] && unateness[2] )
      {
        return ( v << 1 ) + 1;
      }
      /* useless unate literal */
      if ( ( unateness[0] && unateness[2] ) || ( unateness[1] && unateness[3] ) )
      {
        pos_unate_lits.pop_back();
        neg_unate_lits.pop_back();
      }
      /* binate divisor */
      else if ( !unateness[0] && !unateness[1] && !unateness[2] && !unateness[3] )
      {
        binate_divs.emplace_back( v );
      }
    }
    return std::nullopt;
  }

  /* Sort the unate literals by the number of minterms in the intersection.
     - For `pos_unate_lits`, `on_off` = 1, sort by intersection with on-set;
     - For `neg_unate_lits`, `on_off` = 0, sort by intersection with off-set
   */
  void sort_unate_lits( std::vector<unate_lit>& unate_lits, uint32_t on_off )
  {
    for ( auto& l : unate_lits )
    {
      l.score = kitty::count_ones( ( l.lit & 0x1 ? ~get_div( l.lit >> 1 ) : get_div( l.lit >> 1 ) ) & on_off_sets[on_off] );
    }
    std::sort( unate_lits.begin(), unate_lits.end(), [&]( unate_lit const& l1, unate_lit const& l2 ) {
        return l1.score > l2.score; // descending order
    });
  }

  void sort_unate_pairs( std::vector<fanin_pair>& unate_pairs, uint32_t on_off )
  {
    for ( auto& p : unate_pairs )
    {
      if constexpr ( use_xor )
      {
        p.score = ( p.lit1 > p.lit2 ) ?
                    kitty::count_ones( ( ( p.lit1 & 0x1 ? ~get_div( p.lit1 >> 1 ) : get_div( p.lit1 >> 1 ) )
                                     ^ ( p.lit2 & 0x1 ? ~get_div( p.lit2 >> 1 ) : get_div( p.lit2 >> 1 ) ) )
                                     & on_off_sets[on_off] )
                  : kitty::count_ones( ( p.lit1 & 0x1 ? ~get_div( p.lit1 >> 1 ) : get_div( p.lit1 >> 1 ) )
                                     & ( p.lit2 & 0x1 ? ~get_div( p.lit2 >> 1 ) : get_div( p.lit2 >> 1 ) )
                                     & on_off_sets[on_off] );
      }
      else
      {
        p.score = kitty::count_ones( ( p.lit1 & 0x1 ? ~get_div( p.lit1 >> 1 ) : get_div( p.lit1 >> 1 ) )
                                   & ( p.lit2 & 0x1 ? ~get_div( p.lit2 >> 1 ) : get_div( p.lit2 >> 1 ) )
                                   & on_off_sets[on_off] );
      }
    }
    std::sort( unate_pairs.begin(), unate_pairs.end(), [&]( fanin_pair const& p1, fanin_pair const& p2 ) {
        return p1.score > p2.score; // descending order
    });
  }

  /* See if there are two unate divisors covering all on-set bits or all off-set bits.
     - For `pos_unate_lits`, `on_off` = 1, try covering all on-set bits by combining two with an OR gate;
     - For `neg_unate_lits`, `on_off` = 0, try covering all off-set bits by combining two with an AND gate
   */
  std::optional<uint32_t> find_div_div( std::vector<unate_lit>& unate_lits, uint32_t on_off )
  {
    for ( auto i = 0u; i < unate_lits.size(); ++i )
    {
      uint32_t const& lit1 = unate_lits[i].lit;
      if ( unate_lits[i].score * 2 < num_bits[on_off] )
      {
        break;
      }
      for ( auto j = i + 1; j < unate_lits.size(); ++j )
      {
        uint32_t const& lit2 = unate_lits[j].lit;
        if ( unate_lits[i].score + unate_lits[j].score < num_bits[on_off] )
        {
          break;
        }
        auto const ntt1 = lit1 & 0x1 ? get_div( lit1 >> 1 ) : ~get_div( lit1 >> 1 );
        auto const ntt2 = lit2 & 0x1 ? get_div( lit2 >> 1 ) : ~get_div( lit2 >> 1 );
        if ( intersection_is_empty<false, false>( ntt1, ntt2, on_off_sets[on_off] ) )
        {
          auto const new_lit = index_list.add_and( ( lit1 ^ 0x1 ), ( lit2 ^ 0x1 ) );
          return new_lit + on_off;
        }
      }
    }
    return std::nullopt;
  }

  std::optional<uint32_t> find_div_pair( std::vector<unate_lit>& unate_lits, std::vector<fanin_pair>& unate_pairs, uint32_t on_off )
  {
    for ( auto i = 0u; i < unate_lits.size(); ++i )
    {
      uint32_t const& lit1 = unate_lits[i].lit;
      for ( auto j = 0u; j < unate_pairs.size(); ++j )
      {
        fanin_pair const& pair2 = unate_pairs[j];
        if ( unate_lits[i].score + pair2.score < num_bits[on_off] )
        {
          break;
        }
        auto const ntt1 = lit1 & 0x1 ? get_div( lit1 >> 1 ) : ~get_div( lit1 >> 1 );
        TT ntt2;
        if constexpr ( use_xor )
        {
          if ( pair2.lit1 > pair2.lit2 )
          {
            ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) )
                 ^ ( pair2.lit2 & 0x1 ? ~get_div( pair2.lit2 >> 1 ) : get_div( pair2.lit2 >> 1 ) );
          }
          else
          {
            ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) )
                 | ( pair2.lit2 & 0x1 ? get_div( pair2.lit2 >> 1 ) : ~get_div( pair2.lit2 >> 1 ) );
          }
        }
        else
        {
          ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) )
               | ( pair2.lit2 & 0x1 ? get_div( pair2.lit2 >> 1 ) : ~get_div( pair2.lit2 >> 1 ) );
        }
        
        if ( intersection_is_empty<false, false>( ntt1, ntt2, on_off_sets[on_off] ) )
        {
          uint32_t new_lit1;
          if constexpr ( use_xor )
          {
            if ( pair2.lit1 > pair2.lit2 )
            {
              new_lit1 = index_list.add_xor( pair2.lit1, pair2.lit2 );
            }
            else
            {
              new_lit1 = index_list.add_and( pair2.lit1, pair2.lit2 );
            }
          }
          else
          {
            new_lit1 = index_list.add_and( pair2.lit1, pair2.lit2 );
          }
          auto const new_lit2 = index_list.add_and( ( lit1 ^ 0x1 ), new_lit1 ^ 0x1 );
          return new_lit2 + on_off;
        }
      }
    }
    return std::nullopt;
  }

  std::optional<uint32_t> find_pair_pair( std::vector<fanin_pair>& unate_pairs, uint32_t on_off )
  {
    for ( auto i = 0u; i < unate_pairs.size(); ++i )
    {
      fanin_pair const& pair1 = unate_pairs[i];
      if ( pair1.score * 2 < num_bits[on_off] )
      {
        break;
      }
      for ( auto j = i + 1; j < unate_pairs.size(); ++j )
      {
        fanin_pair const& pair2 = unate_pairs[j];
        if ( pair1.score + pair2.score < num_bits[on_off] )
        {
          break;
        }
        TT ntt1, ntt2;
        if constexpr ( use_xor )
        {
          if ( pair1.lit1 > pair1.lit2 )
          {
            ntt1 = ( pair1.lit1 & 0x1 ? get_div( pair1.lit1 >> 1 ) : ~get_div( pair1.lit1 >> 1 ) )
                 ^ ( pair1.lit2 & 0x1 ? ~get_div( pair1.lit2 >> 1 ) : get_div( pair1.lit2 >> 1 ) );
          }
          else
          {
            ntt1 = ( pair1.lit1 & 0x1 ? get_div( pair1.lit1 >> 1 ) : ~get_div( pair1.lit1 >> 1 ) )
                 | ( pair1.lit2 & 0x1 ? get_div( pair1.lit2 >> 1 ) : ~get_div( pair1.lit2 >> 1 ) );
          }
          if ( pair2.lit1 > pair2.lit2 )
          {
            ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) )
                 ^ ( pair2.lit2 & 0x1 ? ~get_div( pair2.lit2 >> 1 ) : get_div( pair2.lit2 >> 1 ) );
          }
          else
          {
            ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) )
                 | ( pair2.lit2 & 0x1 ? get_div( pair2.lit2 >> 1 ) : ~get_div( pair2.lit2 >> 1 ) );
          }
        }
        else
        {
          ntt1 = ( pair1.lit1 & 0x1 ? get_div( pair1.lit1 >> 1 ) : ~get_div( pair1.lit1 >> 1 ) )
               | ( pair1.lit2 & 0x1 ? get_div( pair1.lit2 >> 1 ) : ~get_div( pair1.lit2 >> 1 ) );
          ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) )
               | ( pair2.lit2 & 0x1 ? get_div( pair2.lit2 >> 1 ) : ~get_div( pair2.lit2 >> 1 ) );
        }

        if ( intersection_is_empty<false, false>( ntt1, ntt2, on_off_sets[on_off] ) )
        {
          uint32_t fanin_lit1, fanin_lit2;
          if constexpr ( use_xor )
          {
            if ( pair1.lit1 > pair1.lit2 )
            {
              fanin_lit1 = index_list.add_xor( pair1.lit1, pair1.lit2 );
            }
            else
            {
              fanin_lit1 = index_list.add_and( pair1.lit1, pair1.lit2 );
            }
            if ( pair2.lit1 > pair2.lit2 )
            {
              fanin_lit2 = index_list.add_xor( pair2.lit1, pair2.lit2 );
            }
            else
            {
              fanin_lit2 = index_list.add_and( pair2.lit1, pair2.lit2 );
            }
          }
          else
          {
            fanin_lit1 = index_list.add_and( pair1.lit1, pair1.lit2 );
            fanin_lit2 = index_list.add_and( pair2.lit1, pair2.lit2 );
          }
          uint32_t const output_lit = index_list.add_and( fanin_lit1 ^ 0x1, fanin_lit2 ^ 0x1 );
          return output_lit + on_off;
        }
      }
    }
    return std::nullopt;
  }

  std::optional<uint32_t> find_xor()
  {
    /* collect XOR-type pairs (d1 ^ d2) & off = 0 or ~(d1 ^ d2) & on = 0, selecting d1, d2 from binate_divs */
    for ( auto i = 0u; i < binate_divs.size(); ++i )
    {
      for ( auto j = i + 1; j < binate_divs.size(); ++j )
      {
        auto const tt_xor = get_div( binate_divs[i] ) ^ get_div( binate_divs[j] );
        bool unateness[4] = {false, false, false, false};
        /* check intersection with off-set; additionally check intersection with on-set is not empty (otherwise it's useless) */
        if ( intersection_is_empty<false, false>( tt_xor, on_off_sets[0] ) && !intersection_is_empty<false, false>( tt_xor, on_off_sets[1] ) )
        {
          pos_unate_pairs.emplace_back( binate_divs[i] << 1, binate_divs[j] << 1, true );
          unateness[0] = true;
        }
        if ( intersection_is_empty<true, false>( tt_xor, on_off_sets[0] ) && !intersection_is_empty<true, false>( tt_xor, on_off_sets[1] ) )
        {
          pos_unate_pairs.emplace_back( ( binate_divs[i] << 1 ) + 1, binate_divs[j] << 1, true );
          unateness[1] = true;
        }

        /* check intersection with on-set; additionally check intersection with off-set is not empty (otherwise it's useless) */
        if ( intersection_is_empty<false, false>( tt_xor, on_off_sets[1] ) && !intersection_is_empty<false, false>( tt_xor, on_off_sets[0] ) )
        {
          neg_unate_pairs.emplace_back( binate_divs[i] << 1, binate_divs[j] << 1, true );
          unateness[2] = true;
        }
        if ( intersection_is_empty<true, false>( tt_xor, on_off_sets[1] ) && !intersection_is_empty<true, false>( tt_xor, on_off_sets[0] ) )
        {
          neg_unate_pairs.emplace_back( ( binate_divs[i] << 1 ) + 1, binate_divs[j] << 1, true );
          unateness[3] = true;
        }

        if ( unateness[0] && unateness[2] )
        {
          return index_list.add_xor( ( binate_divs[i] << 1 ), ( binate_divs[j] << 1 ) );
        }
        if ( unateness[1] && unateness[3] )
        {
          return index_list.add_xor( ( binate_divs[i] << 1 ) + 1, ( binate_divs[j] << 1 ) );
        }
      }
    }

    return std::nullopt;
  }

  /* collect AND-type pairs (d1 & d2) & off = 0 or ~(d1 & d2) & on = 0, selecting d1, d2 from binate_divs */
  void collect_unate_pairs()
  {
    for ( auto i = 0u; i < binate_divs.size(); ++i )
    {
      for ( auto j = i + 1; j < binate_divs.size(); ++j )
      {
        collect_unate_pairs_detail<false, false>( binate_divs[i], binate_divs[j] );
        collect_unate_pairs_detail<false, true>( binate_divs[i], binate_divs[j] );
        collect_unate_pairs_detail<true, false>( binate_divs[i], binate_divs[j] );
        collect_unate_pairs_detail<true, true>( binate_divs[i], binate_divs[j] );
      }
    }
  }

  template<bool neg1 = false, bool neg2 = false>
  void collect_unate_pairs_detail( uint32_t div1, uint32_t div2 )
  {
    /* check intersection with off-set; additionally check intersection with on-set is not empty (otherwise it's useless) */
    if ( intersection_is_empty<neg1, neg2>( get_div( div1 ), get_div( div2 ), on_off_sets[0] ) && !intersection_is_empty<neg1, neg2>( get_div( div1 ), get_div( div2 ), on_off_sets[1] ) )
    {
      pos_unate_pairs.emplace_back( ( div1 << 1 ) + (uint32_t)neg1, ( div2 << 1 ) + (uint32_t)neg2 );
    }
    /* check intersection with on-set; additionally check intersection with off-set is not empty (otherwise it's useless) */
    else if ( intersection_is_empty<neg1, neg2>( get_div( div1 ), get_div( div2 ), on_off_sets[1] ) && !intersection_is_empty<neg1, neg2>( get_div( div1 ), get_div( div2 ), on_off_sets[0] ) )
    {
      neg_unate_pairs.emplace_back( ( div1 << 1 ) + (uint32_t)neg1, ( div2 << 1 ) + (uint32_t)neg2 );
    }
  }

private:
  /* equivalent to kitty::is_const0( tt1 & tt2 ), but faster when num_blocks is a lot */
  template<bool neg1 = false, bool neg2 = false>
  bool intersection_is_empty( TT const& tt1, TT const& tt2 )
  {
    if constexpr ( !neg1 && !neg2 )
    {
      return kitty::binary_predicate( tt1, tt2, []( auto a, auto b ) { return !(a & b); } );
    }
    else if constexpr ( neg1 && !neg2 )
    {
      return kitty::binary_predicate( tt1, tt2, []( auto a, auto b ) { return !(~a & b); } );
    }
    else if constexpr ( !neg1 && neg2 )
    {
      return kitty::binary_predicate( tt1, tt2, []( auto a, auto b ) { return !(a & ~b); } );
    }
    else // ( neg1 && neg2 )
    {
      return kitty::binary_predicate( tt1, tt2, []( auto a, auto b ) { return !(~a & ~b); } );
    }
  }
  
  /* equivalent to kitty::is_const0( tt1 & tt2 & tt3 ), but faster when num_blocks is a lot */
  template<bool neg1 = false, bool neg2 = false>
  bool intersection_is_empty( TT const& tt1, TT const& tt2, TT const& tt3 )
  {
    if constexpr ( !neg1 && !neg2 )
    {
      return kitty::ternary_predicate( tt1, tt2, tt3, []( auto a, auto b, auto c ) { return !(a & b & c); } );
    }
    else if constexpr ( neg1 && !neg2 )
    {
      return kitty::ternary_predicate( tt1, tt2, tt3, []( auto a, auto b, auto c ) { return !(~a & b & c); } );
    }
    else if constexpr ( !neg1 && neg2 )
    {
      return kitty::ternary_predicate( tt1, tt2, tt3, []( auto a, auto b, auto c ) { return !(a & ~b & c); } );
    }
    else // ( neg1 && neg2 )
    {
      return kitty::ternary_predicate( tt1, tt2, tt3, []( auto a, auto b, auto c ) { return !(~a & ~b & c); } );
    }
  }

  inline TT const& get_div( uint32_t idx ) const
  {
    if constexpr ( copy_tts )
    {
      return divisors[idx];
    }
    else
    {
      return tts[divisors[idx]];
    }
  }

private:
  std::array<TT, 2> on_off_sets;
  std::array<uint32_t, 2> num_bits; /* number of bits in on-set and off-set */

  truth_table_storage_type const& tts;
  std::vector<std::conditional_t<copy_tts, TT, node_type>> divisors;

  index_list_t index_list;

  /* positive unate: not overlapping with off-set
     negative unate: not overlapping with on-set */
  std::vector<unate_lit> pos_unate_lits, neg_unate_lits;
  std::vector<uint32_t> binate_divs;
  std::vector<fanin_pair> pos_unate_pairs, neg_unate_pairs;

  stats& st;
  params const& ps;
}; /* xag_resyn_engine */

} /* namespace mockturtle */
