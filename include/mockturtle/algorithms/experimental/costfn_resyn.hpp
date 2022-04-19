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
  \file costfn_resyn.hpp
  \brief generic resynthesis algorithm with customized cost function
  \author Hanyu Wang
*/

#pragma once


#pragma once

#include "../../utils/index_list.hpp"
#include "../../utils/stopwatch.hpp"
#include "../../utils/node_map.hpp"

#include <kitty/kitty.hpp>
#include <fmt/format.h>
#include <abcresub/abcresub.hpp>

#include <vector>
#include <algorithm>
#include <type_traits>
#include <optional>
#include <queue>
#include <unordered_set>

namespace mockturtle
{

template<class TT>
struct xag_cost_aware_resyn_static_params
{
  /*! \brief Maximum number of binate divisors to be considered. */
  static constexpr uint32_t max_binates{50u};

  /*! \brief Reserved capacity for divisor truth tables (number of divisors). */
  static constexpr uint32_t reserve{200u};

  /*! \brief Whether to consider single XOR gates (i.e., using XAGs instead of AIGs). */
  static constexpr bool use_xor{true};

  /*! \brief Whether to copy truth tables. */
  static constexpr bool copy_tts{false};

  static constexpr uint32_t max_enqueue{1000u};

  static constexpr uint32_t max_xor{1u};

  static constexpr uint32_t max_neighbors{10u};

  using truth_table_storage_type = std::vector<TT>;
  using node_type = uint32_t;
};

struct xag_costfn_resyn_stats
{
  stopwatch<>::duration time_check_unateness{0};
  stopwatch<>::duration time_enqueue{0};
  stopwatch<>::duration time_tt_calculation{0};
  stopwatch<>::duration time_check_unate{0};
  stopwatch<>::duration time_move_tt{0};
  void report() const
  { }
};

template<class TT, class CostFn, class static_params = xag_cost_aware_resyn_static_params<TT>>
class cost_aware_engine
{
public:
  using stats = xag_costfn_resyn_stats;
  using index_list_t = large_xag_index_list;
  using truth_table_t = TT;
  using cost = typename CostFn::cost;

private:
  enum gate_type {AND, OR, XOR, NONE};
  enum lit_type {EQUAL, EQUAL_INV, POS_UNATE, NEG_UNATE, POS_UNATE_INV, NEG_UNATE_INV, BINATE, DONT_CARE};
  enum resub_type {WIRE_RS, XOR_RS, AND_RS, XOR_XOR_RS, AND_XOR_RS, XOR_AND_RS, XOR_XOR_AND_RS, AND_AND_RS, AND_AND_XOR_RS, AND_AND_AND_RS, AND_XOR_XOR_RS, NONE_RS};

  std::unordered_map<TT, uint32_t, kitty::hash<TT>> tt_to_id;
  std::vector<TT> id_to_tt;
  std::vector<uint32_t> id_to_num;
  
  uint32_t to_id ( const TT & tt )
  {
    if ( tt_to_id.find( tt ) != tt_to_id.end() ) return tt_to_id[tt];
    tt_to_id[tt] = id_to_tt.size();
    id_to_tt.emplace_back( tt );
    id_to_num.emplace_back( kitty::count_ones( tt ) );
    return tt_to_id[tt];
  }

  const auto & to_tt ( uint32_t id )
  {
    assert( id < id_to_tt.size() );
    return id_to_tt[id];
  }

  const uint32_t to_num ( uint32_t id )
  {
    assert( id < id_to_num.size() );
    return id_to_num[id];
  }

  // std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> best_cost;

  // bool check_cost ( uint32_t x, uint32_t y, uint32_t c )
  // {
  //   uint32_t _x = std::min( x, y );
  //   uint32_t _y = std::max( x, y );
  //   bool ret = false; /* if best is updated */
  //   if ( best_cost.find( _x ) == best_cost.end() )
  //   {
  //     ret = true;
  //     best_cost[ _x ] = std::unordered_map<uint32_t, uint32_t>();
  //   }
  //   if ( best_cost[ _x ].find( _y ) == best_cost[ _x ].end() )
  //   {
  //     ret = true;
  //     best_cost[ _x ][ _y ] = c;
  //   }
  //   if ( best_cost[ _x ][ _y ] > c )
  //   {
  //     ret = true;
  //     best_cost[ _x ][ _y ] = c;
  //   }
  //   return ret;
  // }

  /* return if the c is acceptable */
  // bool compare_cost ( c curr, auto upper )
  // {
  //   /* for depth c */
  //   if( c.first != upper.first ) return c.first < upper.first;
  //   // if( c.second != upper.second ) return c.second < upper.second;
  //   return false;
  // }

  struct task 
  {
    std::array<uint32_t, 2> sets; /* the on-off set of each task (could be optimized) */
    uint32_t c; /* the lower bound of the cost */
    uint32_t score;
    std::size_t prev;
    bool done;
    gate_type ntype;
    uint32_t lit;
    uint32_t num_xor;
    const bool operator > ( const task & t ) const
    {
      if ( c != t.c ) return c > t.c;
      /* the most likely first */
      return score > t.score;
    }

    task( auto _done, auto _prev, auto _lit, auto _ntype, auto _cost ): done(_done), prev(_prev), lit(_lit), ntype(_ntype), c(_cost), score(0), num_xor(0) {}
  };

  struct deq_task
  {
    uint32_t c; /* the lower bound of the c */
    std::size_t prev;
    gate_type ntype;
    uint32_t lit;
    deq_task( const task & t ): prev( t.prev ), ntype( t.ntype ), lit( t.lit ), c( t.c ) {}
  };

private:
  /* the list of temp nodes */
  std::vector<deq_task> mem;

  /* the depth c function */
  std::function<uint32_t(uint32_t)> depth_fn;

  /* cost upper bound */
  uint32_t upper_bound;

  template<class Q>
  auto add_neighbors ( const task & t, Q & q )
  {
    for ( auto v = 1u; v < divisors.size(); ++v )
    {
      auto _t = call_with_stopwatch( st.time_tt_calculation, [&]() {
        return find_unate_subtask( t, v );
      } );
      if ( _t ) /* prune dont cares */
      {
        if ( _t->done == true )
        {
          upper_bound = (_t->c);
        }
        call_with_stopwatch( st.time_enqueue, [&]() {
          q.push( *_t );
        } );
      }
    }
  }

  /* */
  auto tt_move ( uint32_t off, uint32_t on, uint32_t lit, gate_type ntype )
  {
    const auto & tt = lit & 0x1? ~get_div( lit>>1 ) : get_div( lit>>1 ) ;
    uint32_t _off = 0u;
    uint32_t _on  = 0u;
    switch ( ntype )
    {
    case OR:
      _off = off;
      _on = to_id( ~tt & to_tt( on ) );
      break;
    case AND:
      _off = to_id( tt & to_tt( off ) );
      _on = on;
      break;
    case XOR:
      _off = to_id( ( ~tt & to_tt( off ) ) | ( tt & to_tt( on ) ) );
      _on = to_id( ( ~tt & to_tt( on ) ) | ( tt & to_tt( off ) ) );
      break;
    }
    return std::make_tuple( _off, _on );
  }

  using cand_t = std::pair<uint32_t, uint32_t>;
  cand_t back_trace( size_t pos )
  {
    size_t p = pos;
    std::priority_queue<cand_t, std::vector<cand_t>, std::greater<>> cand_q;
    cand_q.push( std::pair( depth_fn( mem[p].lit >> 1 ), mem[p].lit ) );
    while ( mem[p].prev != 0 )
    {
      for ( p=mem[p].prev; ; p=mem[p].prev )
      {
        cand_q.push( std::pair( depth_fn( mem[p].lit >> 1 ), mem[p].lit ) );
        if ( mem[p].ntype != mem[mem[p].prev].ntype ) break;
      }
      /* add the nodes */
      while ( cand_q.size() > 1 )
      {
        auto fanin1 = cand_q.top(); cand_q.pop();
        auto fanin2 = cand_q.top(); cand_q.pop();
        uint32_t new_lit = 0u;
        if ( mem[p].ntype == AND ) new_lit = index_list.add_and( fanin1.second, fanin2.second );
        else if ( mem[p].ntype == OR ) new_lit = index_list.add_and( fanin1.second ^ 0x1, fanin2.second ^ 0x1) ^ 0x1;
        else if ( mem[p].ntype == XOR ) new_lit = index_list.add_xor( fanin1.second, fanin2.second );
        auto new_cost = fanin2.first + 1; // TODO: change this "1" to c
        cand_q.push( std::pair( new_cost, new_lit ) );
      }
    }
    return cand_q.top();
  }

  std::optional<uint32_t> get_cost( size_t pos, uint32_t lit, gate_type _ntype, bool balancing = false ) const 
  {
    uint32_t c;

    if ( balancing ) /* a better estimation of depth c */
    {
      if ( pos == 0 ) /* only one lit */
      {
        // depth_cost = depth_fn( lit >> 1 );
      }
      else
      {
        std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<>> cost_q;
        cost_q.push( depth_fn( lit >> 1 ) );
        int p = -1;
        while ( p == -1 || mem[p].prev != 0 )
        {
          for ( p = ( p==-1? pos : mem[p].prev ); ; p = mem[p].prev ) /* get all the same node type */
          {
            cost_q.push( depth_fn( mem[p].lit >> 1 ) );
            if ( mem[p].ntype != mem[mem[p].prev].ntype ) break;
          }
          while ( cost_q.size() > 1 ) /* add node while maintaining the depth order */
          {
            cost_q.pop();
            cost_q.push( cost_q.top() + 1 );
            cost_q.pop();
          }
        }
        while ( cost_q.size() > 1 ) /* add node while maintaining the depth order */
        {
          cost_q.pop();
          cost_q.push( cost_q.top() + 1 );
          cost_q.pop();
        }    
        // depth_cost = cost_q.top();
      }
    }
    else
    {
      // depth_cost = std::max( mem[pos].c.second, (uint32_t)depth_fn( lit>>1 ) ) + 1;
    }
    return 0;
  };

  lit_type check_unateness ( const TT & off_set, const TT & on_set, const TT & tt )
  {
    bool unateness[4] = {
      kitty::intersection_is_empty<TT, 1, 1>( tt, off_set ),
      kitty::intersection_is_empty<TT, 0, 1>( tt, off_set ),
      kitty::intersection_is_empty<TT, 1, 1>( tt, on_set ),
      kitty::intersection_is_empty<TT, 0, 1>( tt, on_set ),
    };
    if ( ( unateness[0] && unateness[2] ) || ( unateness[1] && unateness[3] ) ) return DONT_CARE;
    if ( unateness[0] && unateness[3] ) return EQUAL;
    if ( unateness[1] && unateness[2] ) return EQUAL_INV;
    if ( unateness[0] ) return POS_UNATE;
    if ( unateness[1] ) return POS_UNATE_INV;
    if ( unateness[2] ) return NEG_UNATE_INV;
    if ( unateness[3] ) return NEG_UNATE;
    return BINATE;
  }
  std::optional<task> find_unate_subtask ( const task & _t, uint32_t v )
  {
    auto const & tt = get_div(v);
    auto off = _t.sets[0];
    auto on  = _t.sets[1];
    auto ltype = call_with_stopwatch( st.time_check_unate, [&] () {
      return check_unateness( to_tt( off ), to_tt( on ), tt );
    } );
    gate_type _ntype = NONE;
    bool done = false;
    uint32_t lit = v << 1;
    switch ( ltype )
    {
    case DONT_CARE: return std::nullopt;
    case EQUAL:     done = true; _ntype = NONE;         break;
    case EQUAL_INV: done = true; _ntype = NONE; lit++ ; break;
    case POS_UNATE:              _ntype = OR;           break;
    case POS_UNATE_INV:          _ntype = OR;   lit++;  break;
    case NEG_UNATE:              _ntype = AND;          break;
    case NEG_UNATE_INV:          _ntype = AND;  lit++;  break;
    case BINATE:                 _ntype = XOR;          break;
    }

    if constexpr ( static_params::use_xor == false )
    {
      if ( _ntype == XOR ) return std::nullopt;
    }

    if ( _ntype != NONE && _ntype == _t.ntype && ( lit >> 1 ) <= ( _t.lit >> 1 ) ) 
    {
      return std::nullopt; /* commutativity */
    } 
    auto c = get_cost( mem.size() - 1, lit, _ntype, false ); // TODO: assume prev task always at back()
    if ( !c || *c < upper_bound )
    {
      return std::nullopt; /* task is pruned */
    }
    task t( done, mem.size() - 1, lit, _ntype, *c );
    if ( _ntype == XOR ) 
    {
      if ( _t.num_xor >= static_params::max_xor ) return std::nullopt;
      t.num_xor = _t.num_xor + 1;
    }
    if ( done == false )
    {
      auto [ _off, _on ] = call_with_stopwatch( st.time_move_tt, [&] () {
        return tt_move( off, on, lit, _ntype );
      } );
      // if ( check_cost( _off, _on, (*c).first ) == false )
      // {
      //   return std::nullopt;
      // }
      t.sets[0] = _off;
      t.sets[1] = _on;
      t.score = to_num( _off ) + to_num( _on );
    }
    return t;
  }

  void initialization()
  {
    index_list.clear();
    index_list.add_inputs( divisors.size() - 1 );
    mem.clear();
    id_to_num.clear();
    id_to_tt.clear();
    tt_to_id.clear();
    // best_cost.clear();
  }

public:
  explicit cost_aware_engine( stats& st ) noexcept
    : st( st )
  {
    divisors.reserve( static_params::reserve );
  }

  template<class iterator_type>
  std::optional<index_list_t> operator()( TT const& target, TT const& care, iterator_type begin, iterator_type end, typename static_params::truth_table_storage_type const& tts, uint32_t max_cost = std::numeric_limits<uint32_t>::max() )
  {
    static_assert( static_params::copy_tts || std::is_same_v<typename std::iterator_traits<iterator_type>::value_type, typename static_params::node_type>, "iterator_type does not dereference to static_params::node_type" );

    ptts = &tts;
    on_off_sets[0] = ~target & care;
    on_off_sets[1] = target & care;

    divisors.resize( 1 ); /* clear previous data and reserve 1 dummy node for constant */
    while ( begin != end )
    {
      if constexpr ( static_params::copy_tts )
      {
        divisors.emplace_back( (*ptts)[*begin] );
      }
      else
      {
        divisors.emplace_back( *begin );
      }
      ++begin;
    }

    upper_bound = max_cost;
    initialization();

    task init_task( false, 0, 0, NONE, 0 );
    init_task.sets[0] = to_id( ~target & care );
    init_task.sets[1] = to_id( target & care );

    std::priority_queue<task, std::vector<task>, std::greater<>> q;
    /* prepare the initial task */
    call_with_stopwatch( st.time_enqueue, [&]() {
      q.push( init_task );
    } );

    while ( !q.empty() )
    {
      /* get the current lower bound */
      auto t = q.top(); q.pop(); 
      mem.emplace_back( deq_task( t ) );
      /* back trace succeed tasks */
      if ( t.done == true )
      {
        index_list.clear();
        index_list.add_inputs( divisors.size() - 1 );
        index_list.add_output( back_trace( mem.size() - 1 ).second );
        return index_list;
      }
      // if ( compare_cost( t.c, upper_bound ) == false ) break;
      if ( q.size() >= static_params::max_enqueue ) break;
      add_neighbors ( t, q );
    }
    return std::nullopt;
  }

  inline TT const& get_div( uint32_t idx ) const
  {
    if constexpr ( static_params::copy_tts )
    {
      return divisors[idx];
    }
    else
    {
      return (*ptts)[divisors[idx]];
    }
  }

private:
  uint32_t num_vars;

  std::array<TT, 2> on_off_sets;
  std::array<uint32_t, 2> num_bits; /* number of bits in on-set and off-set */

  const typename static_params::truth_table_storage_type* ptts;
  std::vector<std::conditional_t<static_params::copy_tts, TT, typename static_params::node_type>> divisors;

  index_list_t index_list;

  CostFn costfn{};

  stats& st;
}; /* cost_aware_engine */
}