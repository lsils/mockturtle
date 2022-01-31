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
  \file xag_costfn_resyn.hpp
  \brief Resynthesis by cost-aware synthesis for XAGs.

  \author Hanyu Wang
*/

#pragma once

#include "../../utils/index_list.hpp"
#include "../../utils/node_map.hpp"
#include "../../utils/stopwatch.hpp"

#include <abcresub/abcresub.hpp>
#include <fmt/format.h>
#include <kitty/kitty.hpp>

#include <algorithm>
#include <optional>
#include <type_traits>
#include <vector>

#include <queue>
#include <tuple>
#include <unordered_map>

namespace mockturtle
{

struct xag_costfn_resyn_static_params
{
  using base_type = xag_costfn_resyn_static_params;

  /*! \brief Maximum number of binate divisors to be considered. */
  static constexpr uint32_t max_binates{ 50u };

  /*! \brief Reserved capacity for divisor truth tables (number of divisors). */
  static constexpr uint32_t reserve{ 200u };

  /*! \brief Whether to consider single XOR gates (i.e., using XAGs instead of AIGs). */
  static constexpr bool use_xor{ true };

  /*! \brief Whether to copy truth tables. */
  static constexpr bool copy_tts{ false };

  /*! \brief Whether to preserve depth. */
  static constexpr bool preserve_depth{ false };

  /*! \brief Whether the divisors have uniform costs (size and depth, whenever relevant). */
  static constexpr bool uniform_div_cost{ true };

  /*! \brief Size cost of each AND gate. */
  static constexpr uint32_t size_cost_of_and{ 1u };

  /*! \brief Size cost of each XOR gate (only relevant when `use_xor = true`). */
  static constexpr uint32_t size_cost_of_xor{ 1u };

  /*! \brief Depth cost of each AND gate (only relevant when `preserve_depth = true`). */
  static constexpr uint32_t depth_cost_of_and{ 1u };

  /*! \brief Depth cost of each XOR gate (only relevant when `preserve_depth = true` and `use_xor = true`). */
  static constexpr uint32_t depth_cost_of_xor{ 1u };

  using truth_table_storage_type = void;
  using node_type = void;
};

template<class TT>
struct xag_costfn_resyn_static_params_default : public xag_costfn_resyn_static_params
{
  using truth_table_storage_type = std::vector<TT>;
  using node_type = uint32_t;
};

template<class TT>
struct aig_costfn_resyn_static_params_default : public xag_costfn_resyn_static_params_default<TT>
{
  static constexpr bool use_xor = false;
};

template<class TT>
struct xag_costfn_resyn_static_params_preserve_depth : public xag_costfn_resyn_static_params_default<TT>
{
  static constexpr bool preserve_depth = true;
  static constexpr bool uniform_div_cost = false;
  static constexpr bool collect_sols = true;
};

template<class TT>
struct aig_costfn_resyn_static_params_preserve_depth : public xag_costfn_resyn_static_params_preserve_depth<TT>
{
  static constexpr bool use_xor = false;
};

template<class Ntk>
struct xag_costfn_resyn_static_params_for_sim_resub : public xag_costfn_resyn_static_params
{
  using truth_table_storage_type = incomplete_node_map<kitty::partial_truth_table, Ntk>;
  using node_type = typename Ntk::node;
};

template<class Ntk>
struct aig_costfn_resyn_static_params_for_sim_resub : public xag_costfn_resyn_static_params_for_sim_resub<Ntk>
{
  static constexpr bool use_xor = false;
};

struct xag_costfn_resyn_stats
{
  /*! \brief Time for finding 0-resub and collecting unate literals. */
  stopwatch<>::duration time_unate{ 0 };

  /*! \brief Time for finding 1-resub. */
  stopwatch<>::duration time_resub1{ 0 };

  /*! \brief Time for finding 2-resub. */
  stopwatch<>::duration time_resub2{ 0 };

  /*! \brief Time for finding 3-resub. */
  stopwatch<>::duration time_resub3{ 0 };

  /*! \brief Time for sorting unate literals and unate pairs. */
  stopwatch<>::duration time_sort{ 0 };

  /*! \brief Time for collecting unate pairs. */
  stopwatch<>::duration time_collect_pairs{ 0 };

  /*! \brief Time for dividing the target and recursive call. */
  stopwatch<>::duration time_divide{ 0 };

  /*! \brief Number of solutions */
  std::array<uint32_t, 4> num_sols;

  /*! \brief Size of MFFC */
  std::array<uint32_t, 4> num_mffc;

  void report() const
  {
    fmt::print( "[i]         <xag_costfn_resyn_solver>\n" );
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
 * The algorithm find the solution based on the given cost function
 *
 * 
   \verbatim embed:rst

   Example

   .. code-block:: c++

      using TT = kitty::static_truth_table<6>;
      const std::vector<aig_network::node> divisors = ...;
      const node_map<TT, aig_network> tts = ...;
      const TT target = ..., care = ...;
      xag_costfn_resyn_stats st;
      xag_costfn_resyn_solver<TT, node_map<TT, aig_network>, false, false, aig_network::node> resyn( st );
      auto result = resyn( target, care, divisors.begin(), divisors.end(), tts, ...);
   \endverbatim
 */
template<class TT, class static_params = xag_costfn_resyn_static_params_default<TT>>
class xag_costfn_resyn_solver
{
public:
  using stats = xag_costfn_resyn_stats;
  using index_list_t = large_xag_index_list;
  using truth_table_t = TT;
  using cost_t = typename std::pair<uint32_t, uint32_t>;
  using sol_t = std::pair<cost_t, uint32_t>;

private:
  struct unate_lit
  {
    unate_lit( uint32_t l )
        : lit( l )
    {
    }

    bool operator==( unate_lit const& other ) const
    {
      return lit == other.lit;
    }

    uint32_t lit;
    uint32_t score{ 0 };
  };

  struct fanin_pair
  {
    fanin_pair( uint32_t l1, uint32_t l2 )
        : lit1( l1 < l2 ? l1 : l2 ), lit2( l1 < l2 ? l2 : l1 )
    {
    }

    fanin_pair( uint32_t l1, uint32_t l2, bool is_xor )
        : lit1( l1 > l2 ? l1 : l2 ), lit2( l1 > l2 ? l2 : l1 )
    {
      (void)is_xor;
    }

    bool operator==( fanin_pair const& other ) const
    {
      return lit1 == other.lit1 && lit2 == other.lit2;
    }

    uint32_t lit1, lit2;
    uint32_t score{ 0 };
  };

public:
  explicit xag_costfn_resyn_solver( stats& st ) noexcept
      : st( st )
  {
    static_assert( std::is_same_v<typename static_params::base_type, xag_costfn_resyn_static_params>, "Invalid static_params type" );
    static_assert( !( static_params::uniform_div_cost && static_params::preserve_depth ), "If depth is to be preserved, divisor depth cost must be provided (usually not uniform)" );
    divisors.reserve( static_params::reserve );
  }

  /*! \brief Perform XAG resynthesis.
   *
   * `tts[*begin]` must be of type `TT`.
   * Moreover, if `static_params::copy_tts = false`, `*begin` must be of type `static_params::node_type`.
   *
   * \param target Truth table of the target function.
   * \param care Truth table of the care set.
   * \param begin Begin iterator to divisor nodes.
   * \param end End iterator to divisor nodes.
   * \param tts A data structure (e.g. std::vector<TT>) that stores the truth tables of the divisor functions.
   * \param _leaf_cost_fn The Cost Function of the leave node (existing nodes)
   * \param _node_cost_fn The Cost Function of the fanout node (nodes added)
   * \param _cmp_cost_fn The compare function to compare solutions
   * \param _max_cost Initial Cost
   */
  template<class iterator_type, class LeafFn, class NodeFn, class CmpFn, class cost_type,
           bool enabled = !static_params::uniform_div_cost && static_params::preserve_depth, typename = std::enable_if_t<enabled>>
  std::optional<index_list_t> operator()( TT const& target, TT const& care, iterator_type begin, iterator_type end, typename static_params::truth_table_storage_type const& tts, LeafFn&& _leaf_cost_fn, NodeFn&& _node_cost_fn, CmpFn&& _cmp_cost_fn, cost_type _initial_cost )
  {

    static_assert( static_params::copy_tts || std::is_same_v<typename std::iterator_traits<iterator_type>::value_type, typename static_params::node_type>, "iterator_type does not dereference to static_params::node_type" );

    ptts = &tts;
    on_off_sets[0] = ~target & care;
    on_off_sets[1] = target & care;

    node_cost_fn = _node_cost_fn;
    leaf_cost_fn = _leaf_cost_fn;
    cmp_fn = _cmp_cost_fn;
    initial_cost = _initial_cost;

    // priority queue is from large to small
    auto cmp = [&]( sol_t x, sol_t y ) { return !_cmp_cost_fn( x.first, y.first ); };

    forest_sols.clear();

    divisors.resize( 1 ); /* clear previous data and reserve 1 dummy node for constant */
    forest_sols.emplace_back( std::tuple( std::pair( 0, 0 ), 0, 0 ) );

    while ( begin != end )
    {
      if constexpr ( static_params::copy_tts )
      {
        divisors.emplace_back( ( *ptts )[*begin] );
      }
      else
      {
        divisors.emplace_back( *begin );
      }
      forest_sols.emplace_back( std::tuple( std::pair( leaf_cost_fn( *begin ) ), 0, 0 ) );
      ++begin;
    }
    st.num_mffc[initial_cost.first > 3 ? 3 : initial_cost.first]++;
    search_solutions();
    return get_solution();
  }

private:
  auto get_solution_rec( uint32_t root_lit )
  {
    if ( ( root_lit >> 1 ) < divisors.size() )
    {
      /* reach the input, where root_lit == div_id */
      return root_lit;
    }
    /* back trace and find the whole structure */
    auto [cost, left, right] = forest_sols[( root_lit >> 1 )];
    auto idx_left = get_solution_rec( left );
    auto idx_right = get_solution_rec( right );

    auto idx_ret = ( left < right ) ? index_list.add_and( idx_left, idx_right )
                                    : index_list.add_xor( idx_left, idx_right );
    return idx_ret + ( root_lit & 01 );
  }

  std::optional<index_list_t> get_solution()
  {
    if ( root_sols[0].size() == 0 )
    {
      st.num_sols[0]++;
      return std::nullopt;
    }
    else
    {
      st.num_sols[root_sols[0].size() > 3 ? 3 : root_sols[0].size()]++;
      auto best_cost = initial_cost;
      std::optional<uint32_t> res;
      for ( auto sol : root_sols[0] ) {
        if (cmp_fn(sol.first, best_cost)) {
          best_cost = sol.first;
          res = sol.second;
        }
      }
      if (!res) return std::nullopt;
      index_list.clear();
      index_list.add_inputs( divisors.size() - 1 );
      index_list.add_output( get_solution_rec( *res ) );
      return index_list;
    }
  }
  uint32_t add_solution( uint32_t lit0, uint32_t lit1 = 0, uint32_t lit2 = 0, bool is_root = true, bool is_xor = false )
  {
    if ( lit1 == 0 ) /* add leaf */
    {
      auto leaf_cost = std::get<0>( forest_sols[( lit0 >> 1 )] );
      if ( is_root )
      {
        root_sols[rec_depth].emplace_back( std::pair( leaf_cost, lit0 ) ); /* cost of leaf node */
      }
      return lit0;
    }
    else /* add node */
    {
      uint32_t on_off = ( lit0 & 01 );
      auto node_cost = node_cost_fn( std::get<0>( forest_sols[( lit1 >> 1 )] ), std::get<0>( forest_sols[( lit2 >> 1 )] ), is_xor );
      assert( ( lit1 >> 1 ) != ( lit2 >> 1 ) ); /* two fanin should not be the same */
      /* lit1 < lit2 : AND; lit1 > lit2: XOR */
      forest_sols.emplace_back( ( is_xor ^ ( lit1 < lit2 ) ) ? std::tuple( node_cost, lit1, lit2 )
                                                             : std::tuple( node_cost, lit2, lit1 ) );
      if ( is_root )
      {
        root_sols[rec_depth].emplace_back( std::pair( node_cost, ( ( forest_sols.size() - 1 ) << 1 ) | on_off ) );
      }
      return ( ( forest_sols.size() - 1 ) << 1 ) | on_off;
    }
  }


  void search_solutions()
  {
    rec_depth = 0u;
    /* collect all to temp_sols */
    search_solutions_rec();
  }

  void search_solutions_rec()
  {
    pos_unate_lits.clear();
    neg_unate_lits.clear();
    binate_divs.clear();
    pos_unate_pairs.clear();
    neg_unate_pairs.clear();

    root_sols[rec_depth].clear();

    /* try 0-resub and collect unate literals */
    call_with_stopwatch( st.time_unate, [&]() {
      return find_one_unate();
    } );

    /* sort unate literals and try 1-resub */
    call_with_stopwatch( st.time_sort, [&]() {
      sort_unate_lits( pos_unate_lits, 1 );
      sort_unate_lits( neg_unate_lits, 0 );
    } );
    call_with_stopwatch( st.time_resub1, [&]() {
      return find_div_div( pos_unate_lits, 1 );
    } );
    call_with_stopwatch( st.time_resub1, [&]() {
      return find_div_div( neg_unate_lits, 0 );
    } );

    if ( binate_divs.size() > static_params::max_binates )
    {
      binate_divs.resize( static_params::max_binates );
    }
    if constexpr ( static_params::use_xor )
    {
      /* collect XOR-type unate pairs and try 1-resub with XOR */
      find_xor();
    }

    /* collect AND-type unate pairs and sort (both types), then try 2- and 3-resub */
    call_with_stopwatch( st.time_collect_pairs, [&]() {
      collect_unate_pairs();
    } );
    call_with_stopwatch( st.time_sort, [&]() {
      sort_unate_pairs( pos_unate_pairs, 1 );
      sort_unate_pairs( neg_unate_pairs, 0 );
    } );
    call_with_stopwatch( st.time_resub2, [&]() {
      return find_div_pair( pos_unate_lits, pos_unate_pairs, 1 );
    } );
    call_with_stopwatch( st.time_resub2, [&]() {
      return find_div_pair( neg_unate_lits, neg_unate_pairs, 0 );
    } );

    call_with_stopwatch( st.time_resub3, [&]() {
      return find_pair_pair( pos_unate_pairs, 1 );
    } );
    call_with_stopwatch( st.time_resub3, [&]() {
      return find_pair_pair( neg_unate_pairs, 0 );
    } );

    if (rec_depth >= max_rec_depth) return;

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
    } );

    if ( score_div > score_pair / 2 ) /* divide with a divisor */
    {
      /* if using pos_lit (on_off_div = 1), modify on-set and use an OR gate on top;
         if using neg_lit (on_off_div = 0), modify off-set and use an AND gate on top
       */
      uint32_t const lit = on_off_div ? pos_unate_lits[0].lit : neg_unate_lits[0].lit;
      call_with_stopwatch( st.time_divide, [&]() {
        on_off_sets[on_off_div] &= lit & 0x1 ? get_div( lit >> 1 ) : ~get_div( lit >> 1 );
      } );
      rec_depth += 1u;
      search_solutions_rec();
      rec_depth -= 1u;
      if ( root_sols[rec_depth+1].size() > 0 )
      {
        for ( auto sol : root_sols[rec_depth+1] ) 
        {
          if ( (sol.second >> 1) == (lit >> 1) ) // filter X = X AND X
          {}
          else if ( (sol.second >> 1) == 0 ) // filter X = 1 AND X
          {}
          else add_solution( on_off_div, lit ^ 0x1, sol.second ^ on_off_div );
        }
      }
    }
    else if ( score_pair > 0 ) /* divide with a pair */
    {
      fanin_pair const pair = on_off_pair ? pos_unate_pairs[0] : neg_unate_pairs[0];
      call_with_stopwatch( st.time_divide, [&]() {
        if constexpr ( static_params::use_xor )
        {
          if ( pair.lit1 > pair.lit2 ) /* XOR pair: ~(lit1 ^ lit2) = ~lit1 ^ lit2 */
          {
            on_off_sets[on_off_pair] &= ( pair.lit1 & 0x1 ? get_div( pair.lit1 >> 1 ) : ~get_div( pair.lit1 >> 1 ) ) ^ ( pair.lit2 & 0x1 ? ~get_div( pair.lit2 >> 1 ) : get_div( pair.lit2 >> 1 ) );
          }
          else /* AND pair: ~(lit1 & lit2) = ~lit1 | ~lit2 */
          {
            on_off_sets[on_off_pair] &= ( pair.lit1 & 0x1 ? get_div( pair.lit1 >> 1 ) : ~get_div( pair.lit1 >> 1 ) ) | ( pair.lit2 & 0x1 ? get_div( pair.lit2 >> 1 ) : ~get_div( pair.lit2 >> 1 ) );
          }
        }
        else /* AND pair: ~(lit1 & lit2) = ~lit1 | ~lit2 */
        {
          on_off_sets[on_off_pair] &= ( pair.lit1 & 0x1 ? get_div( pair.lit1 >> 1 ) : ~get_div( pair.lit1 >> 1 ) ) | ( pair.lit2 & 0x1 ? get_div( pair.lit2 >> 1 ) : ~get_div( pair.lit2 >> 1 ) );
        }
      } );
      rec_depth += 1u;
      search_solutions_rec();
      rec_depth -= 1u;
      if (root_sols[rec_depth+1].size() > 0 ) {
        uint32_t new_lit1;
        if constexpr ( static_params::use_xor )
        {
          new_lit1 = add_solution( 0, pair.lit1, pair.lit2, false, pair.lit1 > pair.lit2 ); 
        }
        else
        {
          new_lit1 = add_solution( 0, pair.lit1, pair.lit2, false );
        }
        for ( auto sol : root_sols[rec_depth+1] ) 
        {
          if ( (sol.second >> 1) == 0 ) // filter X = 1 AND X
          {}
          else add_solution( on_off_pair, new_lit1 ^ 0x1, sol.second ^ on_off_pair );
        }
      }
    }
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
      if constexpr ( static_params::collect_sols )
      {
        add_solution( 1 );
      }
      else
      {
        return 1;
      }
    }
    if ( num_bits[1] == 0 )
    {
      if constexpr ( static_params::collect_sols )
      {
        add_solution( 0 );
      }
      else
      {
        return 0;
      }
    }

    for ( auto v = 1u; v < divisors.size(); ++v )
    {
      bool unateness[4] = { false, false, false, false };
      /* check intersection with off-set */
      if ( kitty::intersection_is_empty<TT, 1, 1>( get_div( v ), on_off_sets[0] ) )
      {
        pos_unate_lits.emplace_back( v << 1 );
        unateness[0] = true;
      }
      else if ( kitty::intersection_is_empty<TT, 0, 1>( get_div( v ), on_off_sets[0] ) )
      {
        pos_unate_lits.emplace_back( v << 1 | 0x1 );
        unateness[1] = true;
      }

      /* check intersection with on-set */
      if ( kitty::intersection_is_empty<TT, 1, 1>( get_div( v ), on_off_sets[1] ) )
      {
        neg_unate_lits.emplace_back( v << 1 );
        unateness[2] = true;
      }
      else if ( kitty::intersection_is_empty<TT, 0, 1>( get_div( v ), on_off_sets[1] ) )
      {
        neg_unate_lits.emplace_back( v << 1 | 0x1 );
        unateness[3] = true;
      }

      /* 0-resub */
      if ( unateness[0] && unateness[3] )
      {
        if constexpr ( static_params::collect_sols )
        {
          add_solution( ( v << 1 ) );
        }
        else
        {
          return ( v << 1 );
        }
      }
      if ( unateness[1] && unateness[2] )
      {
        if constexpr ( static_params::collect_sols )
        {
          add_solution( ( v << 1 ) + 1 );
        }
        else
        {
          return ( v << 1 ) + 1;
        }
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
    } );
  }

  void sort_unate_pairs( std::vector<fanin_pair>& unate_pairs, uint32_t on_off )
  {
    for ( auto& p : unate_pairs )
    {
      if constexpr ( static_params::use_xor )
      {
        p.score = ( p.lit1 > p.lit2 ) ? kitty::count_ones( ( ( p.lit1 & 0x1 ? ~get_div( p.lit1 >> 1 ) : get_div( p.lit1 >> 1 ) ) ^ ( p.lit2 & 0x1 ? ~get_div( p.lit2 >> 1 ) : get_div( p.lit2 >> 1 ) ) ) & on_off_sets[on_off] )
                                      : kitty::count_ones( ( p.lit1 & 0x1 ? ~get_div( p.lit1 >> 1 ) : get_div( p.lit1 >> 1 ) ) & ( p.lit2 & 0x1 ? ~get_div( p.lit2 >> 1 ) : get_div( p.lit2 >> 1 ) ) & on_off_sets[on_off] );
      }
      else
      {
        p.score = kitty::count_ones( ( p.lit1 & 0x1 ? ~get_div( p.lit1 >> 1 ) : get_div( p.lit1 >> 1 ) ) & ( p.lit2 & 0x1 ? ~get_div( p.lit2 >> 1 ) : get_div( p.lit2 >> 1 ) ) & on_off_sets[on_off] );
      }
    }
    std::sort( unate_pairs.begin(), unate_pairs.end(), [&]( fanin_pair const& p1, fanin_pair const& p2 ) {
      return p1.score > p2.score; // descending order
    } );
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
        if ( kitty::intersection_is_empty( ntt1, ntt2, on_off_sets[on_off] ) )
        {
          if constexpr ( static_params::collect_sols )
          {
            add_solution( on_off, ( lit1 ^ 0x1 ), ( lit2 ^ 0x1 ) );
          }
          else
          {
            auto const new_lit = index_list.add_and( ( lit1 ^ 0x1 ), ( lit2 ^ 0x1 ) );
            return new_lit + on_off;
          }
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
        if constexpr ( static_params::use_xor )
        {
          if ( pair2.lit1 > pair2.lit2 )
          {
            ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) ) ^ ( pair2.lit2 & 0x1 ? ~get_div( pair2.lit2 >> 1 ) : get_div( pair2.lit2 >> 1 ) );
          }
          else
          {
            ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) ) | ( pair2.lit2 & 0x1 ? get_div( pair2.lit2 >> 1 ) : ~get_div( pair2.lit2 >> 1 ) );
          }
        }
        else
        {
          ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) ) | ( pair2.lit2 & 0x1 ? get_div( pair2.lit2 >> 1 ) : ~get_div( pair2.lit2 >> 1 ) );
        }

        if ( kitty::intersection_is_empty( ntt1, ntt2, on_off_sets[on_off] ) )
        {
          uint32_t new_lit1;
          if constexpr ( static_params::use_xor )
          {
            new_lit1 = add_solution( 0, pair2.lit1, pair2.lit2, false, pair2.lit1 > pair2.lit2 );
          }
          else
          {
            new_lit1 = add_solution( 0, pair2.lit1, pair2.lit2, false );
          }
          add_solution( on_off, lit1 ^ 0x1, new_lit1 ^ 0x1 );
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
        if constexpr ( static_params::use_xor )
        {
          if ( pair1.lit1 > pair1.lit2 )
          {
            ntt1 = ( pair1.lit1 & 0x1 ? get_div( pair1.lit1 >> 1 ) : ~get_div( pair1.lit1 >> 1 ) ) ^ ( pair1.lit2 & 0x1 ? ~get_div( pair1.lit2 >> 1 ) : get_div( pair1.lit2 >> 1 ) );
          }
          else
          {
            ntt1 = ( pair1.lit1 & 0x1 ? get_div( pair1.lit1 >> 1 ) : ~get_div( pair1.lit1 >> 1 ) ) | ( pair1.lit2 & 0x1 ? get_div( pair1.lit2 >> 1 ) : ~get_div( pair1.lit2 >> 1 ) );
          }
          if ( pair2.lit1 > pair2.lit2 )
          {
            ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) ) ^ ( pair2.lit2 & 0x1 ? ~get_div( pair2.lit2 >> 1 ) : get_div( pair2.lit2 >> 1 ) );
          }
          else
          {
            ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) ) | ( pair2.lit2 & 0x1 ? get_div( pair2.lit2 >> 1 ) : ~get_div( pair2.lit2 >> 1 ) );
          }
        }
        else
        {
          ntt1 = ( pair1.lit1 & 0x1 ? get_div( pair1.lit1 >> 1 ) : ~get_div( pair1.lit1 >> 1 ) ) | ( pair1.lit2 & 0x1 ? get_div( pair1.lit2 >> 1 ) : ~get_div( pair1.lit2 >> 1 ) );
          ntt2 = ( pair2.lit1 & 0x1 ? get_div( pair2.lit1 >> 1 ) : ~get_div( pair2.lit1 >> 1 ) ) | ( pair2.lit2 & 0x1 ? get_div( pair2.lit2 >> 1 ) : ~get_div( pair2.lit2 >> 1 ) );
        }

        if ( kitty::intersection_is_empty( ntt1, ntt2, on_off_sets[on_off] ) )
        {
          uint32_t fanin_lit1, fanin_lit2;
          if constexpr ( static_params::use_xor )
          {
            fanin_lit1 = add_solution( 0, pair1.lit1, pair1.lit2, false, pair1.lit1 > pair1.lit2 );
            fanin_lit2 = add_solution( 0, pair2.lit1, pair2.lit2, false, pair2.lit1 > pair2.lit2 );
          }
          else
          {
            fanin_lit1 = add_solution( 0, pair1.lit1, pair1.lit2, false );
            fanin_lit2 = add_solution( 0, pair2.lit1, pair2.lit2, false );
          }
          add_solution( on_off, fanin_lit1 ^ 0x1, fanin_lit2 ^ 0x1 );
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
        bool unateness[4] = { false, false, false, false };
        /* check intersection with off-set; additionally check intersection with on-set is not empty (otherwise it's useless) */
        if ( kitty::intersection_is_empty<TT, 1, 1>( tt_xor, on_off_sets[0] ) && !kitty::intersection_is_empty<TT, 1, 1>( tt_xor, on_off_sets[1] ) )
        {
          pos_unate_pairs.emplace_back( binate_divs[i] << 1, binate_divs[j] << 1, true );
          unateness[0] = true;
        }
        if ( kitty::intersection_is_empty<TT, 0, 1>( tt_xor, on_off_sets[0] ) && !kitty::intersection_is_empty<TT, 0, 1>( tt_xor, on_off_sets[1] ) )
        {
          pos_unate_pairs.emplace_back( ( binate_divs[i] << 1 ) + 1, binate_divs[j] << 1, true );
          unateness[1] = true;
        }

        /* check intersection with on-set; additionally check intersection with off-set is not empty (otherwise it's useless) */
        if ( kitty::intersection_is_empty<TT, 1, 1>( tt_xor, on_off_sets[1] ) && !kitty::intersection_is_empty<TT, 1, 1>( tt_xor, on_off_sets[0] ) )
        {
          neg_unate_pairs.emplace_back( binate_divs[i] << 1, binate_divs[j] << 1, true );
          unateness[2] = true;
        }
        if ( kitty::intersection_is_empty<TT, 0, 1>( tt_xor, on_off_sets[1] ) && !kitty::intersection_is_empty<TT, 0, 1>( tt_xor, on_off_sets[0] ) )
        {
          neg_unate_pairs.emplace_back( ( binate_divs[i] << 1 ) + 1, binate_divs[j] << 1, true );
          unateness[3] = true;
        }

        if ( unateness[0] && unateness[2] )
        {
          add_solution( ( binate_divs[i] << 1 ), ( binate_divs[j] << 1 ), true, true );
        }
        if ( unateness[1] && unateness[3] )
        {
          add_solution( ( binate_divs[i] << 1 ) | 0x1, ( binate_divs[j] << 1 ) );
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
        collect_unate_pairs_detail<1, 1>( binate_divs[i], binate_divs[j] );
        collect_unate_pairs_detail<0, 1>( binate_divs[i], binate_divs[j] );
        collect_unate_pairs_detail<1, 0>( binate_divs[i], binate_divs[j] );
        collect_unate_pairs_detail<0, 0>( binate_divs[i], binate_divs[j] );
      }
    }
  }

  template<bool pol1, bool pol2>
  void collect_unate_pairs_detail( uint32_t div1, uint32_t div2 )
  {
    /* check intersection with off-set; additionally check intersection with on-set is not empty (otherwise it's useless) */
    if ( kitty::intersection_is_empty<TT, pol1, pol2>( get_div( div1 ), get_div( div2 ), on_off_sets[0] ) && !kitty::intersection_is_empty<TT, pol1, pol2>( get_div( div1 ), get_div( div2 ), on_off_sets[1] ) )
    {
      pos_unate_pairs.emplace_back( ( div1 << 1 ) + ( uint32_t )( !pol1 ), ( div2 << 1 ) + ( uint32_t )( !pol2 ) );
    }
    /* check intersection with on-set; additionally check intersection with off-set is not empty (otherwise it's useless) */
    else if ( kitty::intersection_is_empty<TT, pol1, pol2>( get_div( div1 ), get_div( div2 ), on_off_sets[1] ) && !kitty::intersection_is_empty<TT, pol1, pol2>( get_div( div1 ), get_div( div2 ), on_off_sets[0] ) )
    {
      neg_unate_pairs.emplace_back( ( div1 << 1 ) + ( uint32_t )( !pol1 ), ( div2 << 1 ) + ( uint32_t )( !pol2 ) );
    }
  }

  inline TT const& get_div( uint32_t idx ) const
  {
    if constexpr ( static_params::copy_tts )
    {
      return divisors[idx];
    }
    else
    {
      return ( *ptts )[divisors[idx]];
    }
  }

private:
  std::array<TT, 2> on_off_sets;
  std::array<uint32_t, 2> num_bits; /* number of bits in on-set and off-set */

  const typename static_params::truth_table_storage_type* ptts;
  std::vector<std::conditional_t<static_params::copy_tts, TT, typename static_params::node_type>> divisors;

  index_list_t index_list;

  /* positive unate: not overlapping with off-set
     negative unate: not overlapping with on-set */
  std::vector<unate_lit> pos_unate_lits, neg_unate_lits;
  std::vector<uint32_t> binate_divs;
  std::vector<fanin_pair> pos_unate_pairs, neg_unate_pairs;

  /* root_sols: maintain the solutions ordered by cost function
     forest_sols: maintain the topo structure of each solution */
  uint32_t max_rec_depth{5u};
  std::array<std::vector<sol_t>, 6> root_sols;
  uint32_t rec_depth{0u};

  std::vector<std::tuple<cost_t, uint32_t, uint32_t>> forest_sols;

  std::function<cost_t( cost_t, cost_t, bool )> node_cost_fn;
  std::function<cost_t( uint32_t )> leaf_cost_fn;
  std::function<bool( cost_t, cost_t )> cmp_fn;

  cost_t initial_cost;

  stats& st;
}; /* xag_costfn_resyn_solver */


} /* namespace mockturtle */
