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
  \file resubstitution.hpp
  \brief Boolean resubstitution

  \author Heinz Riener
*/

#pragma once

#include "../traits.hpp"
#include "reconv_cut2.hpp"
#include "../views/depth_view.hpp"
#include "../views/fanout_view.hpp"
#include "../networks/aig.hpp"
#include "../networks/mig.hpp"
#include "../networks/xag.hpp"
#include "../networks/xmg.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"

#include <fmt/format.h>

#include <iostream>
#include <optional>

namespace mockturtle
{

/* based on abcRefs.c */
template<typename Ntk>
class node_mffc_inside
{
public:
  using node = typename Ntk::node;

public:
  explicit node_mffc_inside( Ntk const& ntk )
    : ntk( ntk )
  {
  }

  int32_t run( node const& n, std::vector<node> const& leaves, std::vector<node>& inside )
  {
    /* increment the fanout counters for the leaves */
    for ( const auto& l : leaves )
      ntk.incr_h1( l );

    /* dereference the node */
    auto count1 = node_deref_rec( n );

    /* collect the nodes inside the MFFC */
    node_mffc_cone( n, inside );

    /* reference it back */
    auto count2 = node_ref_rec( n );
    assert( count1 == count2 );

    for ( const auto& l : leaves )
      ntk.decr_h1( l );

    return count1;
  }

private:
  /* ! \brief Dereference the node's MFFC */
  int32_t node_deref_rec( node const& n )
  {
    if ( ntk.is_ci( n ) )
      return 0;

    int32_t counter = 1;
    ntk.foreach_fanin( n, [&]( const auto& f ){
        auto const& p = ntk.get_node( f );
        // assert( ntk.fanout_size( p ) > 0 );

        ntk.decr_h1( p );
        if ( ntk.fanout_size( p ) == 0 )
          counter += node_deref_rec( p );
      });

    return counter;
  }

  /* ! \brief Reference the node's MFFC */
  int32_t node_ref_rec( node const& n )
  {
    if ( ntk.is_ci( n ) )
      return 0;

    int32_t counter = 1;
    ntk.foreach_fanin( n, [&]( const auto& f ){
        auto const& p = ntk.get_node( f );

        auto v = ntk.fanout_size( p );
        ntk.incr_h1( p );
        if ( v == 0 )
          counter += node_ref_rec( p );
      });

    return counter;
  }

  void node_mffc_cone_rec( node const& n, std::vector<node>& cone, bool top_most )
  {
    /* skip visited nodes */
    if ( ntk.visited( n ) == ntk.trav_id() )
      return;
    ntk.set_visited( n, ntk.trav_id() );
    // std::cout << "mffc computation visists " << n
    //           << "( " << ntk.fanout_size( n ) << " )" << std::endl;

    if ( !top_most && ( ntk.is_ci( n ) || ntk.fanout_size( n ) > 0 ) )
      return;

    /* recurse on children */
    ntk.foreach_fanin( n, [&]( const auto& f ){
        node_mffc_cone_rec( ntk.get_node( f ), cone, false );
      });

    /* collect the internal nodes */
    cone.emplace_back( n );
  }

  void node_mffc_cone( node const& n, std::vector<node>& cone )
  {
    cone.clear();
    ntk.incr_trav_id();
    node_mffc_cone_rec( n, cone, true );
  }

private:
  Ntk const& ntk;
};

/*! \brief Parameters for resubstitution.
 *
 * The data structure `resubstitution_params` holds configurable parameters with
 * default arguments for `resubstitution`.
 */
struct resubstitution_params
{
  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{8}; // In ABC: nCutMax == nLeavesMax

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{150};

  /*! \brief Maximum number of pair-wise divisors to consider. */
  uint32_t max_divisors2{500};

  /*! \brief Maximum number of nodes per reconvergence-driven window. */
  uint32_t max_nodes{100};

  /*! \brief Maximum number of nodes added by resubstitution. */
  uint32_t max_inserts{1};

  /*! \brief Maximum number of nodes compared during resubstitution. */
  uint32_t max_compare{20};

  /*! \brief Maximum fanout of a node to be considered as root. */
  uint32_t skip_fanout_limit_for_roots{1000};

  /*! \brief Maximum fanout of a node to be considered as divisor. */
  uint32_t skip_fanout_limit_for_divisors{100};

  /*! \brief Extend window with nodes. */
  bool extend{false};

  /*! \brief Disable majority 1-resubsitution filter rules. */
  bool disable_maj_one_resub_filter{false};

  /*! \brief Disable majority 2-resubsitution filter rules. */
  bool disable_maj_two_resub_filter{false};

  /*! \brief Enable zero-gain substitution. */
  bool zero_gain{false};

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};
}; /* resubstitution_params */

/*! \brief Statistics for resubstitution.
 *
 * The data structure `resubstitution_stats` provides data collected by running
 * `resubstitution`.
 */
struct resubstitution_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{0};

  /*! \brief Accumulated runtime for cut computation. */
  stopwatch<>::duration time_cuts{0};

  /*! \brief Accumulated runtime for mffc computation. */
  stopwatch<>::duration time_mffc{0};

  /*! \brief Accumulated runtime for divisor computation. */
  stopwatch<>::duration time_divs{0};

  /*! \brief Accumulated runtime for depth computation. */
  stopwatch<>::duration time_depth{0};

  /*! \brief Accumulated runtime for simulation. */
  stopwatch<>::duration time_simulation{0};

  /*! \brief Accumulated runtime for resubstitution. */
  stopwatch<>::duration time_resubstitution{0};

  /*! \brief Accumulated runtime for zero-resub. */
  stopwatch<>::duration time_resubC{0};

  /*! \brief Accumulated runtime for zero-resub. */
  stopwatch<>::duration time_resub0{0};

  /*! \brief Accumulated runtime for S-resub. */
  stopwatch<>::duration time_resubS{0};

  /*! \brief Accumulated runtime for one-resub. */
  stopwatch<>::duration time_resub1{0};

  /*! \brief Accumulated runtime for 12-resub. */
  stopwatch<>::duration time_resub12{0};

  /*! \brief Accumulated runtime for D-resub. */
  stopwatch<>::duration time_resubD{0};

  /*! \brief Accumulated runtime for two-resub. */
  stopwatch<>::duration time_resub2{0};

  /*! \brief Accumulated runtime for three-resub. */
  stopwatch<>::duration time_resub3{0};

  /*! \brief Accumulated runtime for R-resub. */
  stopwatch<>::duration time_resubR{0};

  /*! \brief Accumulated runtime for cut evaluation/computing a resubsitution. */
  stopwatch<>::duration time_eval{0};

  /*! \brief Accumulated runtime for updating the network. */
  stopwatch<>::duration time_replace{0};

  /*! \brief Accumulated runtime for substituting nodes. */
  stopwatch<>::duration time_substitute{0};

  /*! \brief Accumulated runtime for updating depth_view and fanout_view. */
  stopwatch<>::duration time_update{0};

  /*! \brief Initial network size (before resubstitution) */
  uint64_t initial_size{0};

  /*! \brief Total number of divisors  */
  uint64_t num_total_divisors{0};

  /*! \brief Total number of leaves  */
  uint64_t num_total_leaves{0};

  /*! \brief Total number of gain  */
  uint64_t total_gain{0};

  /*! \brief Number of accepted constant resubsitutions */
  uint64_t num_const_accepts{0};

  /*! \brief Number of accepted zero resubsitutions */
  uint64_t num_div0_accepts{0};

  /*! \brief Number of accepted one resubsitutions */
  uint64_t num_div1_accepts{0};

  /*! \brief Number of accepted single AND-resubsitutions */
  uint64_t num_div1_and_accepts{0};

  /*! \brief Number of accepted single OR-resubsitutions */
  uint64_t num_div1_or_accepts{0};

  /*! \brief Number of accepted two resubsitutions using triples of unate divisors */
  uint64_t num_div12_accepts{0};

  /*! \brief Number of accepted single 2AND-resubsitutions */
  uint64_t num_div12_2and_accepts{0};

  /*! \brief Number of accepted single 2OR-resubsitutions */
  uint64_t num_div12_2or_accepts{0};

  /*! \brief Number of accepted two resubsitutions */
  uint64_t num_div2_accepts{0};

  /*! \brief Number of accepted double AND-OR-resubsitutions */
  uint64_t num_div2_and_or_accepts{0};

  /*! \brief Number of accepted double OR-AND-resubsitutions */
  uint64_t num_div2_or_and_accepts{0};

  /*! \brief Number of accepted three resubsitutions */
  uint64_t num_div3_accepts{0};

  /*! \brief Number of accepted AND-2OR-resubsitutions */
  uint64_t num_div3_and_2or_accepts{0};

  /*! \brief Number of accepted OR-2AND-resubsitutions */
  uint64_t num_div3_or_2and_accepts{0};

  /*! \brief Number of accepted relevance-resubsitutions */
  uint64_t num_divR_accepts{0};

  /*! \brief Number of filtered one resubsitutions */
  uint64_t num_one_filter{0};

  /*! \brief Number of filtered two resubsitutions */
  uint64_t num_two_filter{0};

  void report() const
  {
    std::cout << fmt::format( "[i] total time                                                  ({:>5.2f} secs)\n", to_seconds( time_total ) );
    std::cout << fmt::format( "[i]   cut time                                                  ({:>5.2f} secs)\n", to_seconds( time_cuts ) );
    std::cout << fmt::format( "[i]   mffc time                                                 ({:>5.2f} secs)\n", to_seconds( time_mffc ) );
    std::cout << fmt::format( "[i]   divs time                                                 ({:>5.2f} secs)\n", to_seconds( time_divs ) );
    std::cout << fmt::format( "[i]   depth time                                                ({:>5.2f} secs)\n", to_seconds( time_depth ) );
    std::cout << fmt::format( "[i]   simulation time                                           ({:>5.2f} secs)\n", to_seconds( time_simulation ) );
    std::cout << fmt::format( "[i]   evaluation time                                           ({:>5.2f} secs)\n", to_seconds( time_eval ) );

      std::cout << "===== AIG =====" << std::endl;
      std::cout << fmt::format( "[i]     constant-resub {:6d}                                   ({:>5.2f} secs)\n",
                                num_const_accepts, to_seconds( time_resubC ) );
      std::cout << fmt::format( "[i]            0-resub {:6d}                                   ({:>5.2f} secs)\n",
                                num_div0_accepts, to_seconds( time_resub0 ) );
      std::cout << fmt::format( "[i]            S-resub                                          ({:>5.2f} secs)\n", to_seconds( time_resubS ) );
      std::cout << fmt::format( "[i]            1-resub {:6d} = {:6d} ORs     + {:6d} ANDs    ({:>5.2f} secs)\n",
                                num_div1_accepts, num_div1_or_accepts, num_div1_and_accepts, to_seconds( time_resub1 ) );
      std::cout << fmt::format( "[i]           12-resub {:6d} = {:6d} 2AND    + {:6d} 2OR     ({:>5.2f} secs)\n",
                                num_div12_accepts, num_div12_2and_accepts, num_div12_2or_accepts, to_seconds( time_resub12 ) );
      std::cout << fmt::format( "[i]            D-resub                                          ({:>5.2f} secs)\n", to_seconds( time_resubD ) );
      std::cout << fmt::format( "[i]            2-resub {:6d} = {:6d} AND-OR  + {:6d} OR-AND  ({:>5.2f} secs)\n",
                                num_div2_accepts, num_div2_and_or_accepts, num_div2_or_and_accepts, to_seconds( time_resub2 ) );
      std::cout << fmt::format( "[i]            3-resub {:6d} = {:6d} AND-2OR + {:6d} OR-2AND ({:>5.2f} secs)\n",
                                num_div3_accepts, num_div3_and_2or_accepts, num_div3_or_2and_accepts, to_seconds( time_resub3 ) );
      std::cout << fmt::format( "[i]            total   {:6d}\n",
                                (num_const_accepts + num_div0_accepts + num_div1_accepts + num_div12_accepts + num_div2_accepts + num_div3_accepts) );

      std::cout << "===== MIG =====" << std::endl;
      std::cout << fmt::format( "[i]     constant-resub {:6d}                                   ({:>5.2f} secs)\n",
                                num_const_accepts, to_seconds( time_resubC ) );
      std::cout << fmt::format( "[i]            0-resub {:6d}                                   ({:>5.2f} secs)\n",
                                num_div0_accepts, to_seconds( time_resub0 ) );
      std::cout << fmt::format( "[i]            S-resub                                          ({:>5.2f} secs)\n", to_seconds( time_resubS ) );
      std::cout << fmt::format( "[i]            R-resub {:6d}                                   ({:>5.2f} secs)\n",
                                num_divR_accepts, to_seconds( time_resubR ) );
      std::cout << fmt::format( "[i]            1-resub {:6d} = {:6d} MAJ                      ({:>5.2f} secs)\n",
                                num_div1_accepts, num_div1_accepts, to_seconds( time_resub1 ) );
      std::cout << fmt::format( "[i]           12-resub {:6d} = {:6d} MAJ                      ({:>5.2f} secs)\n",
                                num_div12_accepts, num_div12_accepts, to_seconds( time_resub12 ) );
      std::cout << fmt::format( "[i]            D-resub                                          ({:>5.2f} secs)\n", to_seconds( time_resubD ) );
      std::cout << fmt::format( "[i]            2-resub {:6d} = {:6d} 2MAJ                     ({:>5.2f} secs)\n",
                                 num_div2_accepts, num_div2_accepts, to_seconds( time_resub2 ) );
      std::cout << fmt::format( "[i]            total   {:6d}\n",
                                (num_const_accepts + num_div0_accepts + num_divR_accepts + num_div1_accepts + num_div12_accepts + num_div2_accepts + num_div3_accepts) );

    std::cout << fmt::format( "[i]   replace time                                              ({:>5.2f} secs)\n", to_seconds( time_replace ) );
    std::cout << fmt::format( "[i]     substitute                                              ({:>5.2f} secs)\n", to_seconds( time_substitute ) );
    std::cout << fmt::format( "[i]     update                                                  ({:>5.2f} secs)\n", to_seconds( time_update ) );
    std::cout << fmt::format( "[i] total divisors            = {:8d}\n",         ( num_total_divisors ) );
    std::cout << fmt::format( "[i] total leaves              = {:8d}\n",         ( num_total_leaves ) );
    std::cout << fmt::format( "[i] total gain                = {:8d} ({:>5.2f}\%)\n",
                              total_gain, ( (100.0 * total_gain) / initial_size ) );
    // std::cout << fmt::format( "[i] accepted resubs       = {:8d}\n",         ( num_zero_accepts + num_one_accepts + num_two_accepts ) );
    // std::cout << fmt::format( "[i]   0-resubs            = {:8d}\n",         ( num_zero_accepts ) );
    // std::cout << fmt::format( "[i]   1-resubs            = {:8d}\n",         ( num_one_accepts ) );
    // std::cout << fmt::format( "[i]   2-resubs            = {:8d}\n",         ( num_two_accepts ) );
    std::cout << fmt::format( "[i] filtered cand.            = {:8d}\n",         ( num_one_filter + num_two_filter ) );
    std::cout << fmt::format( "[i]   1-resubs                = {:8d}\n",         ( num_one_filter ) );
    std::cout << fmt::format( "[i]   2-resubs                = {:8d}\n",         ( num_two_filter ) );
  }
}; /* resubstitution_stats */

namespace detail
{

struct abc_vec
{
public:
  abc_vec( uint32_t caps )
    : _caps( ( caps > 0 && caps < 8 ) ? 8 : caps )
    , _size( 0 )
    , _array( _caps ? (void**)malloc( sizeof(void*) * _caps ) : nullptr )
  {
  }

  ~abc_vec()
  {
    free( _array );
  }

  void grow( uint32_t min_caps )
  {
    if ( _caps >= min_caps )
      return;
    _array = (void**)realloc( (char*)_array, sizeof(void*) * min_caps );
    _caps = min_caps;
  }

  void push( void *entry )
  {
    if ( _size == _caps )
    {
      if ( _caps < 16 )
        grow( 16 );
      else
        grow( 2 * _caps );
    }
    _array[_size++] = entry;
  }

  void* entry( uint32_t i )
  {
    return this->operator[]( i );
  }

  void* operator[]( uint32_t i )
  {
    assert( i >= 0 && i < _size );
    return _array[i];
  }

public:
  /*! \brief reserved number of entries */
  uint32_t _caps;

  /*! \brief used number of entries */
  uint32_t _size;

  /*! \brief actual data */
  void **_array;
}; /* abc_vec */

template<typename Ntk>
struct simulation_table
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  simulation_table( Ntk const& ntk, uint32_t max_leaves, uint32_t max_divisors )
    : ntk( ntk )
    , max_leaves( max_leaves )
    , max_divisors( max_divisors )
    , num_bits( 1 << max_leaves )
    , num_words( num_bits <= 32 ? 1 : num_bits / 32 )
    , sims( max_divisors )
    , data( ntk.size(), nullptr )
    , phase( ntk.size(), false )
  {
    /* for each divisor we reserve num_words */
    uint32_t *info = new uint32_t[ num_words * ( max_divisors + 1 ) ];

    /* set the first few entries to zero */
    memset( info, 0, sizeof(uint32_t) * num_words * max_leaves );

    /* push the pointers into info to a vector */
    for ( auto i = 0u; i < max_divisors; ++i )
      sims.push( info + i * num_words );

    /* set elementary truth tables */
    for ( auto k = 0u; k < max_leaves; k++ )
    {
      uint32_t *simulation_entry = (uint32_t*)sims._array[k];
      for ( auto i = 0u; i < num_bits; ++i )
      {
        if ( i & ( 1 << k ) )
          simulation_entry[i>>5] |= ( 1 << ( i & 31 ) );
      }
    }
  }

  void resize()
  {
    if ( data.size() < ntk.size() )
      data.resize( ntk.size() );
    if ( phase.size() < ntk.size() )
      phase.resize( ntk.size() );
  }

  bool get_phase( node const& n ) const
  {
    assert( n < data.size() );
    assert( n < phase.size() );
    return phase.at( n );
  }

  void set_phase( node const& n )
  {
    if ( n == 0 ) return;
    assert( n < data.size() );
    assert( n < phase.size() );
    uint32_t *data_n = (uint32_t*)data[n];
    auto const p = data_n[0] & 1;
    if ( p )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ~data_n[k];
    }
    phase[n] = p;
  }

  void set_ith_var( node const& n, uint32_t i )
  {
    assert( n < data.size() );
    assert( n < phase.size() );
    data[n] = sims[i];
  }

  void compute_and( node const& n, signal const& s0, signal const& s1 )
  {
    auto const n0 = ntk.get_node( s0 );
    auto const n1 = ntk.get_node( s1 );

    uint32_t *data_n = (uint32_t*)data[n];
    uint32_t *data_0 = (uint32_t*)data[n0];
    uint32_t *data_1 = (uint32_t*)data[n1];

    if ( ntk.is_complemented( s0 ) && ntk.is_complemented( s1 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ~data_0[k] & ~data_1[k];
    }
    else if ( ntk.is_complemented( s0 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ~data_0[k] & data_1[k];
    }
    else if ( ntk.is_complemented( s1 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = data_0[k] & ~data_1[k];
    }
    else
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = data_0[k] & data_1[k];
    }
  }

  void compute_xor( node const& n, signal const& s0, signal const& s1 )
  {
    assert( n != 0 );
    auto const n0 = ntk.get_node( s0 );
    auto const n1 = ntk.get_node( s1 );

    uint32_t zero[num_words];
    for ( auto i = 0u; i < num_words; ++i )
      zero[i] = 0;

    uint32_t *data_n = (uint32_t*)data[n];
    uint32_t *data_0 = ( ntk.is_constant( n0 ) ? (uint32_t*)(&zero) : (uint32_t*)data[n0] );
    uint32_t *data_1 = ( ntk.is_constant( n1 ) ? (uint32_t*)(&zero) : (uint32_t*)data[n1] );

    if ( ntk.is_complemented( s0 ) && ntk.is_complemented( s1 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ~data_0[k] ^ ~data_1[k];
    }
    else if ( ntk.is_complemented( s0 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ~data_0[k] ^ data_1[k];
    }
    else if ( ntk.is_complemented( s1 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = data_0[k] ^ ~data_1[k];
    }
    else
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = data_0[k] ^ data_1[k];
    }
  }

  void compute_maj3( node const& n, signal const& s0, signal const& s1, signal const& s2 )
  {
    assert( n != 0 );

    auto const n0 = ntk.get_node( s0 );
    auto const n1 = ntk.get_node( s1 );
    auto const n2 = ntk.get_node( s2 );

    uint32_t zero[num_words];
    for ( auto i = 0u; i < num_words; ++i )
      zero[i] = 0;

    uint32_t *data_n = (uint32_t*)data[n];
    uint32_t *data_0 = ( ntk.is_constant( n0 ) ? (uint32_t*)(&zero) : (uint32_t*)data[n0] );
    uint32_t *data_1 = ( ntk.is_constant( n1 ) ? (uint32_t*)(&zero) : (uint32_t*)data[n1] );
    uint32_t *data_2 = ( ntk.is_constant( n2 ) ? (uint32_t*)(&zero) : (uint32_t*)data[n2] );

    if ( ntk.is_complemented( s0 ) && ntk.is_complemented( s1 ) && ntk.is_complemented( s2 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ( ~data_0[k] & ~data_1[k] ) | ( ~data_1[k] & ~data_2[k] ) | ( ~data_0[k] & ~data_2[k] );
    }
    else if ( ntk.is_complemented( s0 ) && ntk.is_complemented( s1 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ( ~data_0[k] & ~data_1[k] ) | ( ~data_1[k] & data_2[k] ) | ( ~data_0[k] & data_2[k] );
    }
    else if ( ntk.is_complemented( s1 ) && ntk.is_complemented( s2 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ( data_0[k] & ~data_1[k] ) | ( ~data_1[k] & ~data_2[k] ) | ( data_0[k] & ~data_2[k] );
    }
    else if ( ntk.is_complemented( s0 ) && ntk.is_complemented( s2 ) )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ( ~data_0[k] & data_1[k] ) | ( data_1[k] & ~data_2[k] ) | ( ~data_0[k] & ~data_2[k] );
    }
    else if ( ntk.is_complemented( s0 )  )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ( ~data_0[k] & data_1[k] ) | ( data_1[k] & data_2[k] ) | ( ~data_0[k] & data_2[k] );
    }
    else if ( ntk.is_complemented( s1 )  )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ( data_0[k] & ~data_1[k] ) | ( ~data_1[k] & data_2[k] ) | ( data_0[k] & data_2[k] );
    }
    else if ( ntk.is_complemented( s2 )  )
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ( data_0[k] & data_1[k] ) | ( data_1[k] & ~data_2[k] ) | ( data_0[k] & ~data_2[k] );
    }
    else
    {
      for ( auto k = 0u; k < num_words; ++k )
        data_n[k] = ( data_0[k] & data_1[k] ) | ( data_1[k] & data_2[k] ) | ( data_0[k] & data_2[k] );
    }
  }

  bool implies( node const& a, node const& b ) const
  {
    auto data_a = (uint32_t*)data[a];
    auto data_b = (uint32_t*)data[b];
    for ( auto k = 0u; k < num_words; ++k )
    {
      if ( data_a[k] & ~data_b[k] )
        return false; /* early termination */
    }
    return true;
  }

  bool and_implies( signal const& a, signal const& b, node const& r ) const
  {
    auto data_r = (uint32_t*)data[r];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_r = data_r[k];
      if ( tt_a & tt_b & ~tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool and_equal( signal const& a, signal const& b, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( tt_a & tt_b ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool and3_equal( signal const& a, signal const& b, signal const& c, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( tt_a & tt_b & tt_c ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool or_and_equal( signal const& a, signal const& b, signal const& c, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( tt_a | ( tt_b & tt_c ) ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool and_or_equal( signal const& a, signal const& b, signal const& c, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( tt_a & ( tt_b | tt_c ) ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool and_2or_equal( signal const& a, signal const& b, signal const& c, signal const& d, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    auto data_d = (uint32_t*)data[ntk.get_node( d )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_d = ntk.is_complemented( d ) ? ~data_d[k] : data_d[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( ( tt_a | tt_b ) & ( tt_c | tt_d ) ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool or_2and_equal( signal const& a, signal const& b, signal const& c, signal const& d, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    auto data_d = (uint32_t*)data[ntk.get_node( d )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_d = ntk.is_complemented( d ) ? ~data_d[k] : data_d[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( ( tt_a & tt_b ) | ( tt_c & tt_d ) ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool or_equal( signal const& a, signal const& b, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( tt_a | tt_b ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool or3_equal( signal const& a, signal const& b, signal const& c, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( tt_a | tt_b | tt_c ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool maj3_equal( signal const& a, signal const& b, signal const& c, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( ( tt_a & tt_b ) | ( tt_a & tt_c ) | ( tt_b & tt_c ) ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool maj2_equal( signal const& a, signal const& b, signal const& c, signal const& d, signal const& e, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    auto data_d = (uint32_t*)data[ntk.get_node( d )];
    auto data_e = (uint32_t*)data[ntk.get_node( e )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_d = ntk.is_complemented( d ) ? ~data_d[k] : data_d[k];
      auto const& tt_e = ntk.is_complemented( e ) ? ~data_e[k] : data_e[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      auto const& tt_t = ( tt_a & tt_b ) | ( tt_a & tt_c ) | ( tt_b & tt_c );
      if ( ( ( tt_t & tt_d ) | ( tt_t & tt_e ) | ( tt_d & tt_e ) ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool maj2_compl_equal( signal const& a, signal const& b, signal const& c, signal const& d, signal const& e, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    auto data_d = (uint32_t*)data[ntk.get_node( d )];
    auto data_e = (uint32_t*)data[ntk.get_node( e )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_d = ntk.is_complemented( d ) ? ~data_d[k] : data_d[k];
      auto const& tt_e = ntk.is_complemented( e ) ? ~data_e[k] : data_e[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      auto const& tt_t = ~( ( tt_a & tt_b ) | ( tt_a & tt_c ) | ( tt_b & tt_c ) );
      if ( ( ( tt_t & tt_d ) | ( tt_t & tt_e ) | ( tt_d & tt_e ) ) != tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool maj3_implies( signal const& a, signal const& b, signal const& c, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( ( ( tt_a & tt_b ) | ( tt_a & tt_c ) | ( tt_b & tt_c ) ) & ~tt_r ) != 0 )
        return false; /* early termination */
    }
    return true;
  }

  bool maj3_implies_reverse( signal const& a, signal const& b, signal const& c, signal const& r ) const
  {
    auto data_r = (uint32_t*)data[ntk.get_node( r )];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    auto data_c = (uint32_t*)data[ntk.get_node( c )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_c = ntk.is_complemented( c ) ? ~data_c[k] : data_c[k];
      auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
      if ( ( ~( ( tt_a & tt_b ) | ( tt_a & tt_c ) | ( tt_b & tt_c ) ) & tt_r ) != 0 )
        return false; /* early termination */
    }
    return true;
  }

  bool and_implies_reverse( node const& r, signal const& a, signal const& b ) const
  {
    auto data_r = (uint32_t*)data[r];
    auto data_a = (uint32_t*)data[ntk.get_node( a )];
    auto data_b = (uint32_t*)data[ntk.get_node( b )];
    for ( auto k = 0u; k < num_words; ++k )
    {
      auto const& tt_a = ntk.is_complemented( a ) ? ~data_a[k] : data_a[k];
      auto const& tt_b = ntk.is_complemented( b ) ? ~data_b[k] : data_b[k];
      auto const& tt_r = data_r[k];
      if ( ~(tt_a & tt_b) & tt_r )
        return false; /* early termination */
    }
    return true;
  }

  bool nequal( node const& a, node const& b ) const
  {
    auto data_a = (uint32_t*)data[a];
    auto data_b = (uint32_t*)data[b];
    for ( auto k = 0u; k < num_words; ++k )
    {
      if ( data_a[k] != data_b[k] )
        return true; /* early termination */
    }
    return false;
  }

  bool equal( node const& a, node const& b ) const
  {
    auto data_a = (uint32_t*)data[a];
    auto data_b = (uint32_t*)data[b];
    for ( auto k = 0u; k < num_words; ++k )
      if ( data_a[k] != data_b[k] )
        return false; /* early termination */
    return true;
  }

  bool is_false( node const& a ) const
  {
    auto data_a = (uint32_t*)data[a];
    for ( auto k = 0u; k < num_words; ++k )
      if ( data_a[k] != 0 )
        return false; /* early termination */
    return true;
  }

  bool is_true( node const& a ) const
  {
    auto data_a = (uint32_t*)data[a];
    for ( auto k = 0u; k < num_words; ++k )
      if ( data_a[k] != 0xffffffff )
        return false; /* early termination */
    return true;
  }

  bool maj3_relevance( signal const& x, signal const& y, signal const& z, signal const& r ) const
  {
    if ( ntk.get_node( x ) == 0 )
    {
      assert( ntk.get_node( y ) != 0 );
      assert( ntk.get_node( z ) != 0 );
      assert( ntk.get_node( r ) != 0 );
      auto data_r = (uint32_t*)data[ntk.get_node( r )];
      auto data_y = (uint32_t*)data[ntk.get_node( y )];
      auto data_z = (uint32_t*)data[ntk.get_node( z )];
      for ( auto k = 0u; k < num_words; ++k )
      {
        auto const& tt_x = ntk.is_complemented( x ) ? 0xffffffff : 0;
        auto const& tt_y = ntk.is_complemented( y ) ? ~data_y[k] : data_y[k];
        auto const& tt_z = ntk.is_complemented( z ) ? ~data_z[k] : data_z[k];
        auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
        if ( ( ( tt_x ^ tt_r ) & ( tt_y ^ tt_z ) ) != 0 )
          return false; /* early termination */
      }
      return true;
    }
    else if ( ntk.get_node( y ) == 0 )
    {
      assert( ntk.get_node( x ) != 0 );
      assert( ntk.get_node( z ) != 0 );
      assert( ntk.get_node( r ) != 0 );
      auto data_r = (uint32_t*)data[ntk.get_node( r )];
      auto data_x = (uint32_t*)data[ntk.get_node( x )];
      auto data_z = (uint32_t*)data[ntk.get_node( z )];
      for ( auto k = 0u; k < num_words; ++k )
      {
        auto const& tt_x = ntk.is_complemented( x ) ? ~data_x[k] : data_x[k];
        auto const& tt_y = ntk.is_complemented( y ) ? 0xffffffff : 0;
        auto const& tt_z = ntk.is_complemented( z ) ? ~data_z[k] : data_z[k];
        auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
        if ( ( ( tt_x ^ tt_r ) & ( tt_y ^ tt_z ) ) != 0 )
          return false; /* early termination */
      }
      return true;
    }
    else
    {
      assert( ntk.get_node( x ) != 0 );
      assert( ntk.get_node( y ) != 0 );
      assert( ntk.get_node( z ) != 0 );
      assert( ntk.get_node( r ) != 0 );
      auto data_r = (uint32_t*)data[ntk.get_node( r )];
      auto data_x = (uint32_t*)data[ntk.get_node( x )];
      auto data_y = (uint32_t*)data[ntk.get_node( y )];
      auto data_z = (uint32_t*)data[ntk.get_node( z )];
      for ( auto k = 0u; k < num_words; ++k )
      {
        auto const& tt_x = ntk.is_complemented( x ) ? ~data_x[k] : data_x[k];
        auto const& tt_y = ntk.is_complemented( y ) ? ~data_y[k] : data_y[k];
        auto const& tt_z = ntk.is_complemented( z ) ? ~data_z[k] : data_z[k];
        auto const& tt_r = ntk.is_complemented( r ) ? ~data_r[k] : data_r[k];
        if ( ( ( tt_x ^ tt_r ) & ( tt_y ^ tt_z ) ) != 0 )
          return false; /* early termination */
      }
      return true;
    }
  }

public:
  uint32_t size() const
  {
    return max_divisors;
  }

protected:
  Ntk const& ntk;
  uint32_t max_leaves;
  uint32_t max_divisors;
  uint32_t num_bits;
  uint32_t num_words;

  abc_vec sims;
  std::vector<void*> data;
  std::vector<bool> phase;
}; /* simulation_table */

template<class Ntk>
class resubstitution_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  void update_node_level( node const& n, bool top_most = true )
  {
    int32_t curr_level = ntk.level( n );

    int32_t max_level = 0;
    ntk.foreach_fanin( n, [&]( const auto& f ){
        auto const p = ntk.get_node( f );
        auto const fanin_level = ntk.level( p );
        if ( fanin_level > max_level )
        {
          max_level = fanin_level;
        }
      });
    ++max_level;

    if ( curr_level != max_level )
    {
      ntk.set_level( n, max_level );

      /* update only one more level */
      if ( top_most )
      {
        ntk.foreach_fanout( n, [&]( const auto& p ){
            update_node_level( p, false );
          });
      }
    }
  }

  explicit resubstitution_impl( Ntk& ntk, resubstitution_params const& ps, resubstitution_stats& st )
    : ntk( ntk ), ps( ps ), st( st ), sims( ntk, ps.max_pis, ps.max_divisors )
  {
    st.initial_size = ntk.num_gates();

    if constexpr ( std::is_same<typename Ntk::base_type, aig_network>::value ||
                   std::is_same<typename Ntk::base_type, mig_network>::value ||
                   std::is_same<typename Ntk::base_type, xag_network>::value )
    {
      auto const update_level_of_new_node = [&]( const auto& n ){
        ntk.resize_levels();
        update_node_level( n );
      };

      auto const update_level_of_existing_node = [&]( node const& n, const auto& old_children ){
        (void)old_children;
        update_node_level( n );
      };

      auto const update_fanout_of_new_node = [&]( const auto& n ){
        assert( ntk.fanin_size( n ) > 0 );
        ntk.resize_fanouts();

        /* update fanout */
        ntk.foreach_fanin( n, [&]( const auto& f ){
            auto const p = ntk.get_node( f );
            ntk.add_node( p, n );
          });
      };

      auto const update_fanout_of_existing_node = [&]( const auto& n, const auto& old_children ){
        /* update fanout */
        for ( const auto& c : old_children )
          ntk.remove_node( ntk.get_node( c ), n );

        ntk.foreach_fanin( n, [&]( const auto& f ){
            auto const p = ntk.get_node( f );
            ntk.add_node( p, n );
          });
      };

      auto const update_fanout_of_deleted_node = [&]( const auto& n ){
        /* update fanout */
        ntk.set_fanout( n, {} );
        ntk.foreach_fanin( n, [&]( const auto& f ){
            auto const p = ntk.get_node( f );
            ntk.remove_node( p, n );
          });
      };

      auto const update_level_of_deleted_node = [&]( const auto& n ){
        /* update fanout */
        ntk.set_level( n, -1 );
      };

      ntk._events->on_add.emplace_back( update_fanout_of_new_node );
      ntk._events->on_add.emplace_back( update_level_of_new_node );

      ntk._events->on_modified.emplace_back( update_fanout_of_existing_node );
      ntk._events->on_modified.emplace_back( update_level_of_existing_node );

      ntk._events->on_delete.emplace_back( update_fanout_of_deleted_node );
      ntk._events->on_delete.emplace_back( update_level_of_deleted_node );
    }
  }

  void run()
  {
    stopwatch t( st.time_total );

    /* start the managers */
    cut_manager<Ntk> mgr( ps.max_pis );

    /* resynthesize each node once */
    const auto size = ntk.size();
    ntk.foreach_gate( [&]( auto const& n, auto i ) {
        /* skip if all nodes have been tried */
        if ( i >= size )
          return false; /* terminate */

        if ( ntk.fanout_size( n ) == 0 )
          return true; /* next */

        /* skip nodes with many fanouts */
        if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
          return true; /* next */

        /* compute a reconvergence-driven cut */
        auto const leaves = call_with_stopwatch( st.time_cuts, [&]() {
            return reconv_driven_cut( mgr, ntk, n );
          });

        /* evaluate this cut */
        auto const g = call_with_stopwatch( st.time_eval, [&]() {
            return eval( n, leaves, ps.max_inserts, update_level );
          });
        if ( !g )
          return true; /* next */

        /* update gain */
        st.total_gain += last_gain;

        /* update network */
        call_with_stopwatch( st.time_replace, [&]() {
            replace_node( n, *g );
          });

        return true; /* next */
      });
  }

private:
  void replace_node( node const& old_node, signal const& new_signal, bool update_level = true )
  {
    call_with_stopwatch( st.time_substitute, [&]() { ntk.substitute_node( old_node, new_signal ); } );
  }

  void collect_divisors_rec( node const& n, std::vector<node>& internal )
  {
    /* skip visited nodes */
    if ( ntk.visited( n ) == ntk.trav_id() )
      return;
    ntk.set_visited( n, ntk.trav_id() );

    ntk.foreach_fanin( n, [&]( const auto& f ){
        // assert( ntk.get_node( f ) != 0 );
        collect_divisors_rec( ntk.get_node( f ), internal );
      });

    /* collect the internal nodes */
    if ( ntk.value( n ) == 0 && n != 0 ) /* ntk.fanout_size( n ) */
    {
      internal.emplace_back( n );
    }
  }

  bool collect_divisors( node const& root, std::vector<node> const& leaves, uint32_t required )
  {
    /* clear candidate divisors */
    divs_1up.clear();
    divs_1un.clear();
    divs_1b.clear();

    /* add the leaves of the cuts to the divisors */
    divs.clear();
    ntk.incr_trav_id();
    for ( const auto& l : leaves )
    {
      // if ( ntk.fanout_size( l ) != 0 )
      divs.emplace_back( l );
      ntk.set_visited( l, ntk.trav_id() );
    }

    /* mark nodes in the MFFC */
    for ( const auto& t : temp )
    {
      ntk.set_value( t, 1 );
    }

    /* collect the cone (without MFFC) */
    collect_divisors_rec( root, divs );

    /* unmark the current MFFC */
    for ( const auto& t : temp )
    {
      ntk.set_value( t, 0 );
    }

    /* check if the number of divisors is not exceeded */
    if ( divs.size() - leaves.size() + temp.size() >= sims.size() - ps.max_pis )
      return false;

    /* get the number of divisors to collect */
    int32_t limit = sims.size() - ps.max_pis - ( divs.size() - leaves.size() + temp.size() );

    /* explore the fanouts, which are not in the MFFC */
    int32_t counter = 0;
    bool quit = false;

    /* NOTE: this is tricky and cannot be converted to a range-based loop */
    auto size = divs.size();
    for ( auto i = 0u; i < size; ++i )
    {
      auto const d = divs[i];

      // if ( ntk.fanout_size( d ) == 0 )
      //   continue;

      if ( ntk.fanout_size( d ) > ps.skip_fanout_limit_for_divisors )
      {
        continue;
      }

      /* if the fanout has all fanins in the set, add it */
      ntk.foreach_fanout( d, [&]( node const& p ){
          // if ( ntk.fanout_size( p ) == 0 )
          //   return true;

          if ( ntk.visited( p ) == ntk.trav_id() || ntk.level( p ) > required )
          {
            return true; /* next fanout */
          }

          bool all_fanins_visited = true;
          ntk.foreach_fanin( p, [&]( const auto& g ){
              if ( ntk.visited( ntk.get_node( g ) ) != ntk.trav_id() )
              {
                all_fanins_visited = false;
                return false; /* terminate fanin-loop */
              }
              return true; /* next fanin */
            });

          if ( !all_fanins_visited )
            return true; /* next fanout */

          bool has_root_as_child = false;
          ntk.foreach_fanin( p, [&]( const auto& g ){
              if ( ntk.get_node( g ) == root )
              {
                has_root_as_child = true;
                return false; /* terminate fanin-loop */
              }
              return true; /* next fanin */
            });

          if ( has_root_as_child )
          {
            return true; /* next fanout */
          }

          divs.emplace_back( p );
          ++size;
          ntk.set_visited( p, ntk.trav_id() );

          /* quit computing divisors if there are too many of them */
          if ( ++counter == limit )
          {
            quit = true;
            return false; /* terminate fanout-loop */
          }

          return true; /* next fanout */
        });

      if ( quit )
        break;
    }

    /* get the number of divisors */
    num_divs = divs.size();

    /* add the nodes in the MFFC */
    for ( const auto& t : temp )
    {
      // if ( ntk.fanout_size( t ) == 0 )
      //   continue;
      divs.emplace_back( t );
    }

    assert( root == divs.at( divs.size()-1u ) );
    assert( divs.size() - leaves.size() <= sims.size() - ps.max_pis );

    return true;
  }

  void simulate( std::vector<node> const &leaves )
  {
    sims.resize();
    assert( divs.size() - leaves.size() <= sims.size() - ps.max_pis );

    auto i = 0u;
    for ( const auto& d : divs )
    {
      // assert( d != 0 );
      if ( i < leaves.size() )
      {
        /* initialize the leaf */
        sims.set_ith_var( d, i );
        i++;
        continue;
      }

      /* set storage for the node's simulation info */
      sims.set_ith_var( d, i - leaves.size() + ps.max_pis );

      if constexpr ( std::is_same<typename Ntk::base_type, aig_network>::value )
      {
        std::array<signal, 2u> fanins;
        ntk.foreach_fanin( d, [&]( const auto& s, auto i ){
            fanins[i] = s;
          });

        /* simulate the AND-node */
        sims.compute_and( d, fanins[0u], fanins[1u] );
        i++;
        continue;
      }

      if constexpr ( std::is_same<typename Ntk::base_type, mig_network>::value )
      {
        std::array<signal, 3u> fanins;
        ntk.foreach_fanin( d, [&]( const auto& s, auto i ){
            fanins[i] = s;
          });

        /* simulate the MAJ3-node */
        sims.compute_maj3( d, fanins[0u], fanins[1u], fanins[2u] );
        i++;
        continue;
      }

      if constexpr ( std::is_same<typename Ntk::base_type, xag_network>::value )
      {
        if ( ntk.is_and( d ) )
        {
          std::array<signal, 2u> fanins;
          ntk.foreach_fanin( d, [&]( const auto& s, auto i ){
              fanins[i] = s;
            });

          /* simulate the AND-node */
          sims.compute_and( d, fanins[0u], fanins[1u] );
          i++;
          continue;
        }
        else if ( ntk.is_xor( d ) )
        {
          std::array<signal, 2u> fanins;
          ntk.foreach_fanin( d, [&]( const auto& s, auto i ){
              fanins[i] = s;
            });

          /* simulate the XOR-node */
          sims.compute_xor( d, fanins[0u], fanins[1u] );
          i++;
          continue;
        }
        else
        {
          assert( false );
        }
      }

      if constexpr ( std::is_same<typename Ntk::base_type, xmg_network>::value )
      {
        if ( ntk.is_xor( d ) )
        {
          std::array<signal, 2u> fanins;
          ntk.foreach_fanin( d, [&]( const auto& s, auto i ){
              fanins[i] = s;
            });

          /* simulate the XOR-node */
          sims.compute_xor( d, fanins[0u], fanins[1u] );
          i++;
          continue;
        }
        else if ( ntk.is_maj( d ) )
        {
          std::array<signal, 3u> fanins;
          ntk.foreach_fanin( d, [&]( const auto& s, auto i ){
              fanins[i] = s;
            });

          /* simulate the MAJ3-node */
          sims.compute_maj3( d, fanins[0u], fanins[1u], fanins[2u] );
          i++;
          continue;
        }
        else
        {
          assert( false );
        }
      }

      assert( false && "unreachable" );
    }

    /* normalize */
    for ( const auto& d : divs )
    {
      sims.set_phase( d );
    }
  }

  std::optional<signal> resub_const( node const& root, uint32_t required )
  {
    (void)required;

    if ( sims.is_false( root ) )
    {
      return sims.get_phase( root ) ? ntk.get_constant( true ) : ntk.get_constant( false );
    }
    return std::optional<signal>();
  }

  std::optional<signal> resub_div0( node const& root, uint32_t required )
  {
    (void)required;

    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const& d = divs.at( i );
      if ( sims.nequal( root, d ) )
        continue; /* next */

      return ( sims.get_phase( d ) ^ sims.get_phase( root ) ) ? !ntk.make_signal( d ) : ntk.make_signal( d );
    }

    return std::optional<signal>();
  }

  void resub_divS( node const& root, uint32_t required )
  {
    /* clear candidate divisors */
    divs_1up.clear();
    divs_1un.clear();
    divs_1b.clear();

    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const& d = divs.at( i );
      if ( ntk.level( d ) > required - 1 )
        continue;

      /* check positive containment */
      if ( sims.implies( d, root ) )
      {
        divs_1up.emplace_back( d );
        continue;
      }

      /* check negative containment */
      if ( sims.implies( root, d ) )
      {
        divs_1un.emplace_back( d );
        continue;
      }

      /* add the node to binates */
      divs_1b.emplace_back( d );
    }
  }

  void resub_divD( node const& root, uint32_t required )
  {
    /* clear candidate divisors */
    divs_2up0.clear();
    divs_2up1.clear();
    divs_2un0.clear();
    divs_2un1.clear();

    for ( auto i = 0u; i < divs_1b.size(); ++i )
    {
      auto const& d0 = divs_1b.at( i );
      if ( ntk.level( d0 ) > required - 2 )
        continue;

      for ( auto j = i + 1; j < divs_1b.size(); ++j )
      {
        auto const& d1 = divs_1b.at( j );
        if ( ntk.level( d1 ) > required - 2 )
          continue;

        auto const s0 = ntk.make_signal( d0 );
        auto const s1 = ntk.make_signal( d1 );

        if ( divs_2up0.size() < ps.max_divisors2 )
        {
          /* ( l & r ) --> root */
          if ( sims.and_implies( s0, s1, root ) )
          {
            divs_2up0.emplace_back( s0 );
            divs_2up1.emplace_back( s1 );
          }

          /* ( ~l & r ) --> root */
          if ( sims.and_implies( !s0, s1, root ) )
          {
            divs_2up0.emplace_back( !s0 );
            divs_2up1.emplace_back(  s1 );
          }

          /* ( l & ~r ) --> root */
          if ( sims.and_implies( s0, !s1, root ) )
          {
            divs_2up0.emplace_back(  s0 );
            divs_2up1.emplace_back( !s1 );
          }

          /* ( ~l & ~r ) --> root */
          if ( sims.and_implies( !s0, !s1, root ) )
          {
            divs_2up0.emplace_back( !s0 );
            divs_2up1.emplace_back( !s1 );
          }
        }

        if ( divs_2un0.size() < ps.max_divisors2 )
        {
          /* root --> ( l & r ) */
          if ( sims.and_implies_reverse( root, s0, s1 ) )
          {
            divs_2un0.emplace_back(  s0 );
            divs_2un1.emplace_back(  s1 );
          }

          /* root --> ( ~l & r ) */
          if ( sims.and_implies_reverse( root, s0, s1 ) )
          {
            divs_2un0.emplace_back( !s0 );
            divs_2un1.emplace_back(  s1 );
          }

          /* root --> ( l & ~r ) */
          if ( sims.and_implies_reverse( root, s0, s1 ) )
          {
            divs_2un0.emplace_back(  s0 );
            divs_2un1.emplace_back( !s1 );
          }

          /* root --> ( ~l & ~r ) */
          if ( sims.and_implies_reverse( root, s0, s1 ) )
          {
            divs_2un0.emplace_back( !s0 );
            divs_2un1.emplace_back( !s1 );
          }
        }
      }
    }
  }

  std::optional<signal> resub_div1( node const& root, uint32_t required )
  {
    (void)required;

    auto const s = ntk.make_signal( root );

    /* check for positive unate divisors */
    for ( auto i = 0u; i < divs_1up.size(); ++i )
    {
      auto const& d0 = divs_1up.at( i );

      for ( auto j = i + 1; j < divs_1up.size(); ++j )
      {
        auto const& d1 = divs_1up.at( j );

        auto const s0 = ntk.make_signal( d0 );
        auto const s1 = ntk.make_signal( d1 );

        /* ( l | r ) <-> root */
        if ( sims.or_equal( s0, s1, s ) )
        {
          ++st.num_div1_or_accepts;
          auto const l = sims.get_phase( d0 ) ? !s0 : s0;
          auto const r = sims.get_phase( d1 ) ? !s1 : s1;
          return sims.get_phase( root ) ? !ntk.create_or( l, r ) : ntk.create_or( l, r );
        }
      }
    }

    /* check for negative unate divisors */
    for ( auto i = 0u; i < divs_1un.size(); ++i )
    {
      auto const& d0 = divs_1un.at( i );

      for ( auto j = i + 1; j < divs_1un.size(); ++j )
      {
        auto const& d1 = divs_1un.at( j );

        auto const s0 = ntk.make_signal( d0 );
        auto const s1 = ntk.make_signal( d1 );

        /* ( l & r ) <-> root */
        if ( sims.and_equal( s0, s1, s ) )
        {
          ++st.num_div1_and_accepts;
          auto const l = sims.get_phase( d0 ) ? !s0 : s0;
          auto const r = sims.get_phase( d1 ) ? !s1 : s1;
          return sims.get_phase( root ) ? !ntk.create_and( l, r ) : ntk.create_and( l, r );
        }
      }
    }

    return std::optional<signal>();
  }

  std::optional<signal> resub_div12( node const& root, uint32_t required )
  {
    auto const s = ntk.make_signal( root );

    /* check positive unate divisors */
    for ( auto i = 0u; i < divs_1up.size(); ++i )
    {
      for ( auto j = i + 1; j < divs_1up.size(); ++j )
      {
        for ( auto k = j + 1; k < divs_1up.size(); ++k )
        {
          auto const d0 = divs_1up.at( i );
          auto const d1 = divs_1up.at( j );
          auto const d2 = divs_1up.at( k );

          auto const s0 = ntk.make_signal( d0 );
          auto const s1 = ntk.make_signal( d1 );
          auto const s2 = ntk.make_signal( d2 );

          if ( sims.or3_equal( s0, s1, s2, s ) )
          {
            auto const max_level = std::max({ ntk.level( d0 ), ntk.level( d1 ), ntk.level( d2 ) });
            assert( max_level <= required - 1 );

            node max = d0;
            node min0 = d1;
            node min1 = d2;
            if ( ntk.level( d1 ) == max_level )
            {
              max = d1;
              min0 = d0;
              min1 = d2;
            }
            else
            {
              max = d2;
              min0 = d0;
              min1 = d1;
            }

            auto const a = sims.get_phase( max ) ? !ntk.make_signal( max ) : ntk.make_signal( max );
            auto const b = sims.get_phase( min0 ) ? !ntk.make_signal( min0 ) : ntk.make_signal( min0 );
            auto const c = sims.get_phase( min1 ) ? !ntk.make_signal( min1 ) : ntk.make_signal( min1 );
            ++st.num_div12_2or_accepts;
            return sims.get_phase( root ) ? !ntk.create_or( a, ntk.create_or( b, c ) ) : ntk.create_or( a, ntk.create_or( b, c ) );
          }
        }
      }
    }

    /* check negative unate divisors */
    for ( auto i = 0u; i < divs_1un.size(); ++i )
    {
      for ( auto j = i + 1; j < divs_1un.size(); ++j )
      {
        for ( auto k = j + 1; k < divs_1un.size(); ++k )
        {
          auto const d0 = divs_1un.at( i );
          auto const d1 = divs_1un.at( j );
          auto const d2 = divs_1un.at( k );

          auto const s0 = ntk.make_signal( d0 );
          auto const s1 = ntk.make_signal( d1 );
          auto const s2 = ntk.make_signal( d2 );

          if ( sims.and3_equal( s0, s1, s2, s ) )
          {
            auto const max_level = std::max({ ntk.level( d0 ), ntk.level( d1 ), ntk.level( d2 ) });
            assert( max_level <= required - 1 );

            node max = d0;
            node min0 = d1;
            node min1 = d2;
            if ( ntk.level( d1 ) == max_level )
            {
              max = d1;
              min0 = d0;
              min1 = d2;
            }
            else
            {
              max = d2;
              min0 = d0;
              min1 = d1;
            }

            auto const a = sims.get_phase( max ) ? !ntk.make_signal( max ) : ntk.make_signal( max );
            auto const b = sims.get_phase( min0 ) ? !ntk.make_signal( min0 ) : ntk.make_signal( min0 );
            auto const c = sims.get_phase( min1 ) ? !ntk.make_signal( min1 ) : ntk.make_signal( min1 );
            ++st.num_div12_2and_accepts;
            return sims.get_phase( root ) ? !ntk.create_and( a, ntk.create_and( b, c ) ) : ntk.create_and( a, ntk.create_and( b, c ) );
          }
        }
      }
    }

    return std::optional<signal>();
  }

  std::optional<signal> resub_div2( node const& root, uint32_t required )
  {
    (void)required;

    auto const s = ntk.make_signal( root );

    /* check positive unate divisors */
    for ( const auto& d0 : divs_1up )
    {
      auto const s0 = ntk.make_signal( d0 );

      for ( auto j = 0u; j < divs_2up0.size(); ++j )
      {
        auto const s1 = divs_2up0.at( j );
        auto const s2 = divs_2up1.at( j );

        auto const a = sims.get_phase( d0 ) ? !s0 : s0;
        auto const b = sims.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
        auto const c = sims.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;

        if ( sims.or_and_equal( s0, s1, s2, s ) )
        {
          ++st.num_div2_or_and_accepts;
          return sims.get_phase( root ) ?
            !ntk.create_or( a, ntk.create_and( b, c ) ) :
             ntk.create_or( a, ntk.create_and( b, c ) );
        }
      }
    }

    /* check negative unate divisors */
    for ( const auto& d0 : divs_1un )
    {
      auto const s0 = ntk.make_signal( d0 );

      for ( auto j = 0u; j < divs_2un0.size(); ++j )
      {
        auto const s1 = divs_2un0.at( j );
        auto const s2 = divs_2un1.at( j );

        auto const a = sims.get_phase( d0 ) ? !s0 : s0;
        auto const b = sims.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
        auto const c = sims.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;

        if ( sims.and_or_equal( s0, s1, s2, s ) )
        {
          ++st.num_div2_and_or_accepts;
          return sims.get_phase( root ) ?
            !ntk.create_and( a, ntk.create_or( b, c ) ) :
            ntk.create_and( a, ntk.create_or( b, c ) );
        }
      }
    }

    return std::optional<signal>();
  }

  std::optional<signal> resub_div3( node const& root, uint32_t required )
  {
    (void)required;

    auto const s = ntk.make_signal( root );

    for ( auto i = 0u; i < divs_2up0.size(); ++i )
    {
      auto const s0 = divs_2up0.at( i );
      auto const s1 = divs_2up1.at( i );

      for ( auto j = i + 1; j < divs_2up0.size(); ++j )
      {
        auto const s2 = divs_2up0.at( j );
        auto const s3 = divs_2up1.at( j );

        if ( sims.and_2or_equal( s0, s1, s2, s3, s ) )
        {
          auto const a = sims.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
          auto const b = sims.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
          auto const c = sims.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;
          auto const d = sims.get_phase( ntk.get_node( s3 ) ) ? !s3 : s3;

          ++st.num_div3_and_2or_accepts;
          return sims.get_phase( root ) ?
            !ntk.create_and( ntk.create_or( a, b ) , ntk.create_or( c, d ) ) :
             ntk.create_and( ntk.create_or( a, b ) , ntk.create_or( c, d ) );
        }
      }
    }

    for ( auto i = 0u; i < divs_2un0.size(); ++i )
    {
      auto const s0 = divs_2un0.at( i );
      auto const s1 = divs_2un1.at( i );

      for ( auto j = i + 1; j < divs_2un0.size(); ++j )
      {
        auto const s2 = divs_2un0.at( j );
        auto const s3 = divs_2un1.at( j );

        if ( sims.or_2and_equal( s0, s1, s2, s3, s ) )
        {
          auto const a = sims.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
          auto const b = sims.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
          auto const c = sims.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;
          auto const d = sims.get_phase( ntk.get_node( s3 ) ) ? !s3 : s3;

          ++st.num_div3_or_2and_accepts;
          return sims.get_phase( root ) ?
            !ntk.create_or( ntk.create_and( a, b ) , ntk.create_and( c, d ) ) :
             ntk.create_or( ntk.create_and( a, b ) , ntk.create_and( c, d ) );
        }
      }
    }

    return std::optional<signal>();
  }

  void resub_div_majS( node const& root, uint32_t required )
  {
    divs_1mp0.clear();
    divs_1mp1.clear();
    divs_1mn0.clear();
    divs_1mn1.clear();
    divs_1mb.clear();

    auto const s = ntk.make_signal( root );
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const& d0 = divs.at( i );
      if ( ntk.level( d0 ) > required - 1 )
        continue;

      for ( auto j = i + 1; j < num_divs; ++j )
      {
        auto const& d1 = divs.at( j );
        if ( ntk.level( d1 ) > required - 1 )
          continue;

        auto const s0 = ntk.make_signal( d0 );
        auto const s1 = ntk.make_signal( d1 );

        /* Boolean filtering rule for MAJ-3 */
        if ( sims.maj3_equal( s0, s1, s, s ) )
        {
          divs_1mp0.emplace_back( d0 );
          divs_1mp1.emplace_back( d1 );
          continue;
        }

        if ( sims.maj3_equal( !s0, s1, s, s ) )
        {
          divs_1mn0.emplace_back( d0 );
          divs_1mn1.emplace_back( d1 );
          continue;
        }

        if ( std::find( divs_1mb.begin(), divs_1mb.end(), d1 ) == divs_1mb.end() )
          divs_1mb.emplace_back( d1 );
      }

      if ( std::find( divs_1mb.begin(), divs_1mb.end(), d0 ) == divs_1mb.end() )
        divs_1mb.emplace_back( d0 );
    }
  }

  void resub_div_majD( node const& root, uint32_t required )
  {
    divs_2mp0.clear();
    divs_2mp1.clear();
    divs_2mp2.clear();
    divs_2mn0.clear();
    divs_2mn1.clear();
    divs_2mn2.clear();

    auto const s = ntk.make_signal( root );
    for ( auto i = 0u; i < divs_1mb.size(); ++i )
    {
      auto const& d0 = divs_1mb.at( i );
      if ( ntk.level( d0 ) > required - 2 )
        continue;

      auto const s0 = ntk.make_signal( d0 );

      for ( auto j = i + 1; j < divs_1mb.size(); ++j )
      {
        auto const& d1 = divs_1mb.at( j );
        if ( ntk.level( d1 ) > required - 2 )
          continue;

        auto const s1 = ntk.make_signal( d1 );

        for ( auto k = j + 1; k < divs_1mb.size(); ++k )
        {
          auto const& d2 = divs_1mb.at( k );
          if ( ntk.level( d2 ) > required - 2 )
            continue;

          auto const s2 = ntk.make_signal( d2 );

          if ( sims.maj3_implies( s0, s1, s2, s ) )
          {
            divs_2mp0.emplace_back( s0 );
            divs_2mp1.emplace_back( s1 );
            divs_2mp2.emplace_back( s2 );
            continue;
          }

          if ( sims.maj3_implies( !s0, s1, s2, s ) )
          {
            divs_2mp0.emplace_back( !s0 );
            divs_2mp1.emplace_back(  s1 );
            divs_2mp2.emplace_back(  s2 );
            continue;
          }

          if ( sims.maj3_implies_reverse( s, s0, s1, s2 ) )
          {
            divs_2mn0.emplace_back( s0 );
            divs_2mn1.emplace_back( s1 );
            divs_2mn2.emplace_back( s2 );
            continue;
          }

          if ( sims.maj3_implies_reverse( s, !s0, s1, s2 ) )
          {
            divs_2mn0.emplace_back( !s0 );
            divs_2mn1.emplace_back(  s1 );
            divs_2mn2.emplace_back(  s2 );
            continue;
          }
        }
      }
    }
  }

  std::optional<signal> resub_div_maj1( node const& root, uint32_t required )
  {
    (void)required;

    auto s = ntk.make_signal( root );

    /* check positive unate divisors */
    for ( auto i = 0u; i < divs_1mp0.size(); ++i )
    {
      auto d0 = divs_1mp0.at( i );
      auto d1 = divs_1mp1.at( i );

      auto s0 = ntk.make_signal( d0 );
      auto s1 = ntk.make_signal( d1 );

      for ( auto j = i + 1; j < divs_1mp0.size(); ++j )
      {
        auto d2 = divs_1mp0.at( j );
        auto s2 = ntk.make_signal( d2 );

        if ( sims.maj3_equal( s0, s1, s2, s ) )
        {
          auto const a = sims.get_phase( d0 ) ? !s0 : s0;
          auto const b = sims.get_phase( d1 ) ? !s1 : s1;
          auto const c = sims.get_phase( d2 ) ? !s2 : s2;
          return sims.get_phase( root ) ? !ntk.create_maj( a, b, c ) : ntk.create_maj( a, b, c );
        }

        d2 = divs_1mp1.at( j );
        s2 = ntk.make_signal( d2 );

        if ( sims.maj3_equal( s0, s1, s2, s ) )
        {
          auto const a = sims.get_phase( d0 ) ? !s0 : s0;
          auto const b = sims.get_phase( d1 ) ? !s1 : s1;
          auto const c = sims.get_phase( d2 ) ? !s2 : s2;
          return sims.get_phase( root ) ? !ntk.create_maj( a, b, c ) : ntk.create_maj( a, b, c );
        }
      }
    }

    /* check negative unate divisors */
    for ( auto i = 0u; i < divs_1mn0.size(); ++i )
    {
      auto d0 = divs_1mn0.at( i );
      auto d1 = divs_1mn1.at( i );

      auto s0 = ntk.make_signal( d0 );
      auto s1 = ntk.make_signal( d1 );

      for ( auto j = i + 1; j < divs_1mn0.size(); ++j )
      {
        auto d2 = divs_1mn0.at( j );
        auto s2 = ntk.make_signal( d2 );

        if ( sims.maj3_equal( !s0, s1, s2, s ) )
        {
          auto const a = sims.get_phase( d0 ) ? !s0 : s0;
          auto const b = sims.get_phase( d1 ) ? !s1 : s1;
          auto const c = sims.get_phase( d2 ) ? !s2 : s2;
          return sims.get_phase( root ) ? !ntk.create_maj( !a, b, c ) : ntk.create_maj( !a, b, c );
        }

        d2 = divs_1mn1.at( j );
        s2 = ntk.make_signal( d2 );

        if ( sims.maj3_equal( !s0, s1, s2, s ) )
        {
          auto const a = sims.get_phase( d0 ) ? !s0 : s0;
          auto const b = sims.get_phase( d1 ) ? !s1 : s1;
          auto const c = sims.get_phase( d2 ) ? !s2 : s2;
          return sims.get_phase( root ) ? !ntk.create_maj( !a, b, c ) : ntk.create_maj( !a, b, c );
        }
      }
    }

    return std::optional<signal>();
  }

  std::optional<signal> resub_div_maj12( node const& root, uint32_t required )
  {
    (void)required;

    auto s = ntk.make_signal( root );

    /* check positive unate divisors */
    for ( auto i = 0u; i < divs_1mp0.size(); ++i )
    {
      auto d0 = divs_1mp0.at( i );
      auto d1 = divs_1mp1.at( i );

      auto s0 = ntk.make_signal( d0 );
      auto s1 = ntk.make_signal( d1 );

      for ( auto j = i + 1; j < divs_1mb.size(); ++j )
      {
        auto d2 = divs_1mb.at( j );
        auto s2 = ntk.make_signal( d2 );

        if ( sims.maj3_equal( s0, s1, s2, s ) )
        {
          auto const a = sims.get_phase( d0 ) ? !s0 : s0;
          auto const b = sims.get_phase( d1 ) ? !s1 : s1;
          auto const c = sims.get_phase( d2 ) ? !s2 : s2;
          return sims.get_phase( root ) ? !ntk.create_maj( a, b, c ) : ntk.create_maj( a, b, c );
        }
      }
    }

    /* check negative unate divisors */
    for ( auto i = 0u; i < divs_1mn0.size(); ++i )
    {
      auto d0 = divs_1mn0.at( i );
      auto d1 = divs_1mn1.at( i );

      auto s0 = ntk.make_signal( d0 );
      auto s1 = ntk.make_signal( d1 );

      for ( auto j = i + 1; j < divs_1mb.size(); ++j )
      {
        auto d2 = divs_1mb.at( j );
        auto s2 = ntk.make_signal( d2 );

        if ( sims.maj3_equal( !s0, s1, s2, s ) )
        {
          auto const a = sims.get_phase( d0 ) ? !s0 : s0;
          auto const b = sims.get_phase( d1 ) ? !s1 : s1;
          auto const c = sims.get_phase( d2 ) ? !s2 : s2;
          return sims.get_phase( root ) ? !ntk.create_maj( !a, b, c ) : ntk.create_maj( !a, b, c );
        }
      }
    }

    return std::optional<signal>();
  }

  std::optional<signal> resub_div_majR( node const& root, uint32_t required )
  {
    (void)required;

    std::vector<signal> fs;
    ntk.foreach_fanin( root, [&]( const auto& f ){
        fs.emplace_back( f );
      });

    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const& d0 = divs.at( i );
      auto s = ntk.make_signal( d0 );

      if ( ntk.get_node( fs[0] ) != d0 && ntk.fanout_size( ntk.get_node( fs[0] ) ) == 1 && sims.maj3_relevance( fs[0], fs[1], fs[2], s ) )
      {
        auto const a = sims.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const b = sims.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];
        auto const c = sims.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sims.get_phase( root ) ?
          !ntk.create_maj( sims.get_phase( d0 ) ? !s : s, b, c ) :
          ntk.create_maj( sims.get_phase( d0 ) ? !s : s, b, c );
      }
      else if ( ntk.get_node( fs[1] ) != d0 && ntk.fanout_size( ntk.get_node( fs[1] ) ) == 1 && sims.maj3_relevance( fs[1], fs[0], fs[2], s ) )
      {
        auto const q = sims.get_phase( d0 ) ? !s : s;
        auto const a = sims.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const b = sims.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];
        auto const c = sims.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sims.get_phase( root ) ?
          !ntk.create_maj( q, a, c ) :
           ntk.create_maj( q, a, c );
      }
      else if ( ntk.get_node( fs[2] ) != d0 && ntk.fanout_size( ntk.get_node( fs[2] ) ) == 1 && sims.maj3_relevance( fs[2], fs[0], fs[1], s ) )
      {
        auto const a = sims.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const b = sims.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];
        auto const c = sims.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sims.get_phase( root ) ?
          !ntk.create_maj( sims.get_phase( d0 ) ? !s : s, a, b ) :
          ntk.create_maj( sims.get_phase( d0 ) ? !s : s, a, b );
      }
      else if ( ntk.get_node( fs[0] ) != d0 && ntk.fanout_size( ntk.get_node( fs[0] ) ) == 1 && sims.maj3_relevance( !fs[0], fs[1], fs[2], s ) )
      {
        auto const a = sims.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const b = sims.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];
        auto const c = sims.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sims.get_phase( root ) ?
          !ntk.create_maj( sims.get_phase( d0 ) ? s : !s, b, c ) :
           ntk.create_maj( sims.get_phase( d0 ) ? s : !s, b, c );
      }
      else if ( ntk.get_node( fs[1] ) != d0 && ntk.fanout_size( ntk.get_node( fs[1] ) ) == 1 && sims.maj3_relevance( !fs[1], fs[0], fs[2], s ) )
      {
        auto const a = sims.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const b = sims.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];
        auto const c = sims.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sims.get_phase( root ) ?
          !ntk.create_maj( sims.get_phase( d0 ) ? s : !s, a, c ) :
           ntk.create_maj( sims.get_phase( d0 ) ? s : !s, a, c );
      }
      else if ( ntk.get_node( fs[2] ) != d0 && ntk.fanout_size( ntk.get_node( fs[2] ) ) == 1 && sims.maj3_relevance( !fs[2], fs[0], fs[1], s ) )
      {
        auto const a = sims.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const b = sims.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];
        auto const c = sims.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sims.get_phase( root ) ?
          !ntk.create_maj( sims.get_phase( d0 ) ? s : !s, a, b ) :
           ntk.create_maj( sims.get_phase( d0 ) ? s : !s, a, b );
      }
    }

    return std::optional<signal>();
  }

  std::optional<signal> resub_div_maj2( node const& root, uint32_t required )
  {
    (void)required;

    auto s = ntk.make_signal( root );

    /* check positive unate divisors */
    for ( auto i = 0u; i < divs_1mp0.size(); ++i )
    {
      auto const d0 = divs_1mp0.at( i );
      auto const s0 = ntk.make_signal( d0 );

      auto const d1 = divs_1mp1.at( i );
      auto const s1 = ntk.make_signal( d1 );

      for ( auto j = 0u; j < divs_2mp0.size(); ++j )
      {
        auto const s2 = divs_2mp0.at( j );
        auto const s3 = divs_2mp1.at( j );
        auto const s4 = divs_2mp2.at( j );

        auto const a = sims.get_phase( d0 ) ? !s0 : s0;
        auto const b = sims.get_phase( d1 ) ? !s1 : s1;
        auto const c = sims.get_phase( ntk.get_node( s2 ) ) ? !s1 : s2;
        auto const d = sims.get_phase( ntk.get_node( s3 ) ) ? !s2 : s3;
        auto const e = sims.get_phase( ntk.get_node( s4 ) ) ? !s3 : s4;

        if ( sims.maj2_equal( s0, s1, s2, s3, s4, s ) )
        {
          return sims.get_phase( root ) ?
            !ntk.create_maj( a, b, ntk.create_maj( c, d, e ) ) :
             ntk.create_maj( a, b, ntk.create_maj( c, d, e ) );
        }
      }
    }

    /* check positive unate divisors */
    for ( auto i = 0u; i < divs_1mn0.size(); ++i )
    {
      auto const d0 = divs_1mn0.at( i );
      auto const s0 = ntk.make_signal( d0 );

      auto const d1 = divs_1mn1.at( i );
      auto const s1 = ntk.make_signal( d1 );

      for ( auto j = 0u; j < divs_2mn0.size(); ++j )
      {
        auto const s2 = divs_2mn0.at( j );
        auto const s3 = divs_2mn1.at( j );
        auto const s4 = divs_2mn2.at( j );

        auto const a = sims.get_phase( d0 ) ? !s0 : s0;
        auto const b = sims.get_phase( d1 ) ? !s1 : s1;
        auto const c = sims.get_phase( ntk.get_node( s2 ) ) ? !s1 : s2;
        auto const d = sims.get_phase( ntk.get_node( s3 ) ) ? !s2 : s3;
        auto const e = sims.get_phase( ntk.get_node( s4 ) ) ? !s3 : s4;

        if ( sims.maj2_compl_equal( s0, s1, s2, s3, s4, s ) )
        {
          return sims.get_phase( root ) ?
            !ntk.create_maj( a, b, !ntk.create_maj( c, d, e ) ) :
             ntk.create_maj( a, b, !ntk.create_maj( c, d, e ) );
        }
      }
    }

    return std::optional<signal>();
  }

  std::optional<signal> eval( node const& root, std::vector<node> const &leaves, uint32_t num_steps, bool update_level )
  {
    uint32_t const required = update_level ? 0 : std::numeric_limits<uint32_t>::max();

    assert( num_steps >= 0 );
    assert( num_steps <= 3 );

    last_gain = -1;

    /* collect the MFFC */
    int32_t num_mffc = call_with_stopwatch( st.time_mffc, [&]() {
        node_mffc_inside collector( ntk );
        auto num_mffc = collector.run( root, leaves, temp );
        assert( num_mffc > 0 );
        return num_mffc;
      });

    /* collect the divisor nodes */
    bool div_comp_success = call_with_stopwatch( st.time_divs, [&]() {
        return collect_divisors( root, leaves, required );
      });

    if ( !div_comp_success )
      return std::optional<signal>();

    /* update statistics */
    st.num_total_divisors += num_divs;
    st.num_total_leaves += leaves.size();

    /* simulate the nodes */
    call_with_stopwatch( st.time_simulation, [&]() { simulate( leaves ); });

    if constexpr ( std::is_same<typename Ntk::base_type, aig_network>::value )
    {
      /* consider constants */
      auto g = call_with_stopwatch( st.time_resubC, [&]() {
          return resub_const( root, required ); } );
      if ( g )
      {
        ++st.num_const_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      /* consider equal nodes */
      g = call_with_stopwatch( st.time_resub0, [&]() {
          return resub_div0( root, required ); });
      if ( g )
      {
        ++st.num_div0_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      if ( ps.max_inserts == 0 || num_mffc == 1 )
        return std::optional<signal>();

      /* get the one level divisors */
      call_with_stopwatch( st.time_resubS, [&]() { resub_divS( root, required ); });

      /* consider one node */
      g = call_with_stopwatch( st.time_resub1, [&]() {
          return resub_div1( root, required ); });
      if ( g )
      {
        ++st.num_div1_accepts;
        last_gain = num_mffc - 1;
        return g; /* accepted resub */
      }

      if ( ps.max_inserts == 1 || num_mffc == 2 )
        return std::optional<signal>();

      /* consider triples */
      g = call_with_stopwatch( st.time_resub12, [&]() {
          return resub_div12( root, required ); });
      if ( g )
      {
        ++st.num_div12_accepts;
        last_gain = num_mffc - 2;
        return g; /* accepted resub */
      }

      /* get the two level divisors */
      call_with_stopwatch( st.time_resubD, [&]() { resub_divD( root, required ); });

      /* consider two nodes */
      g = call_with_stopwatch( st.time_resub2, [&]() {
          return resub_div2( root, required ); });
      if ( g )
      {
        ++st.num_div2_accepts;
        last_gain = num_mffc - 2;
        return g; /* accepted resub */
      }

      if ( ps.max_inserts == 2 || num_mffc == 3 )
        return std::optional<signal>();

      /* consider three nodes */
      g = call_with_stopwatch( st.time_resub3, [&]() {
          return resub_div3( root, required ); });
      if ( g )
      {
        ++st.num_div3_accepts;
        last_gain = num_mffc - 3;
        return g; /* accepted resub */
      }

      return std::optional<signal>();
    }

    if constexpr ( std::is_same<typename Ntk::base_type, mig_network>::value )
    {
      /* consider constants */
      auto g = call_with_stopwatch( st.time_resubC, [&]() {
          return resub_const( root, required ); } );
      if ( g )
      {
        ++st.num_const_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      /* consider equal nodes */
      g = call_with_stopwatch( st.time_resub0, [&]() {
          return resub_div0( root, required ); });
      if ( g )
      {
        ++st.num_div0_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      /* relevance */
      g = call_with_stopwatch( st.time_resubR, [&]() {
          return resub_div_majR( root, required ); });
      if ( g )
      {
        ++st.num_divR_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      if ( ps.max_inserts == 0 || num_mffc == 1 )
        return std::optional<signal>();

      /* get the one level divisors */
      call_with_stopwatch( st.time_resubS, [&]() { resub_div_majS( root, required ); });

      /* consider one node for majority */
      g = call_with_stopwatch( st.time_resub1, [&]() {
          return resub_div_maj1( root, required ); });
      if ( g )
      {
        ++st.num_div1_accepts;
        last_gain = num_mffc - 1;
        return g; /* accepted resub */
      }

      /* consider */
      g = call_with_stopwatch( st.time_resub12, [&]() {
          return resub_div_maj12( root, required ); });
      if ( g )
      {
        ++st.num_div12_accepts;
        last_gain = num_mffc - 1;
        return g; /* accepted resub */
      }

      if ( ps.max_inserts == 1 || num_mffc == 2 )
        return std::optional<signal>();

      /* get the two level divisors */
      call_with_stopwatch( st.time_resubD, [&]() { resub_div_majD( root, required ); });      

      /* consider two node for majority */
      g = call_with_stopwatch( st.time_resub2, [&]() {
          return resub_div_maj2( root, required ); });
      if ( g )
      {
        ++st.num_div2_accepts;
        last_gain = num_mffc - 2;
        return g; /* accepted resub */
      }
    }

    if constexpr ( std::is_same<typename Ntk::base_type, xag_network>::value )
    {
      /* consider constants */
      auto g = call_with_stopwatch( st.time_resubC, [&]() {
          return resub_const( root, required ); } );
      if ( g )
      {
        ++st.num_const_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      /* consider equal nodes */
      g = call_with_stopwatch( st.time_resub0, [&]() {
          return resub_div0( root, required ); });
      if ( g )
      {
        ++st.num_div0_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      if ( ps.max_inserts == 0 || num_mffc == 1 )
        return std::optional<signal>();
    }

    if constexpr ( std::is_same<typename Ntk::base_type, xmg_network>::value )
    {
      /* consider constants */
      auto g = call_with_stopwatch( st.time_resubC, [&]() {
          return resub_const( root, required ); } );
      if ( g )
      {
        ++st.num_const_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      /* consider equal nodes */
      g = call_with_stopwatch( st.time_resub0, [&]() {
          return resub_div0( root, required ); });
      if ( g )
      {
        ++st.num_div0_accepts;
        last_gain = num_mffc;
        return g; /* accepted resub */
      }

      if ( ps.max_inserts == 0 || num_mffc == 1 )
        return std::optional<signal>();
    }

    return std::optional<signal>();
  }

private:
  Ntk& ntk;

  resubstitution_params const& ps;
  resubstitution_stats& st;

  bool update_level = false;
  bool verbose = false;

  std::vector<node> temp;
  std::vector<node> divs;
  uint32_t num_divs{0};
  int32_t last_gain{-1};

  simulation_table<Ntk> sims;

  std::vector<node> divs_1up; // unate positive candidates
  std::vector<node> divs_1un; // unate negative candidates
  std::vector<node> divs_1b; // binate candidates
  std::vector<signal> divs_2up0;
  std::vector<signal> divs_2up1;
  std::vector<signal> divs_2un0;
  std::vector<signal> divs_2un1;

  std::vector<node> divs_1mp0;
  std::vector<node> divs_1mp1;
  std::vector<node> divs_1mn0;
  std::vector<node> divs_1mn1;
  std::vector<node> divs_1mb;
  std::vector<signal> divs_2mp0;
  std::vector<signal> divs_2mp1;
  std::vector<signal> divs_2mp2;
  std::vector<signal> divs_2mn0;
  std::vector<signal> divs_2mn1;
  std::vector<signal> divs_2mn2;
}; /* resubstitution_impl */

} /* namespace detail */

/*! \brief Boolean resubstitution.
 *
 * **Required network functions:**
 * - `clear_values`
 * - `clear_visited`
 * - `fanout_size`
 * - `foreach_gate`
 * - `foreach_node`
 * - `get_node`
 * - `make_signal`
 * - `set_value`
 * - `size`
 * - `substitute_node_of_parents`
 *
 * \param ntk Input network (will be changed in-place)
 * \param ps Resubstitution params
 * \param pst Resubstitution statistics
 */
template<class Ntk>
void resubstitution( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_is_ci_v<Ntk>, "Ntk does not implement the is_ci method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
  static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the has_size method" );
  static_assert( has_value_v<Ntk>, "Ntk does not implement the has_value method" );
  static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );
  // static_assert( make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  // static_assert( substitude_node_of_parents_v<Ntk>, "Ntk does not implement the substitute_node_of_parents method" );

  /* FIXME */
  using view_t = depth_view<fanout_view<Ntk>>;
  ::mockturtle::fanout_view<Ntk> fanout_view( ntk );
  ::mockturtle::depth_view<::mockturtle::fanout_view<Ntk>> resub_view( fanout_view );

  /* fanout_view */
  // static_assert( fanout_v<Ntk>, "Ntk does not implement the fanout method" );
  // static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
  // static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method" );

  /* depth_view */
  // static_assert( has_level_v<Ntk>, "Ntk does not implement the level method" );

#if 0
  static_assert( has_trav_id_v<Ntk>, "Ntk does not implement the trav_id method" );
  static_assert( has_incr_h1_v<Ntk>, "Ntk does not implement the incr_h1 method" );
  static_assert( has_decr_h1_v<Ntk>, "Ntk does not implement the decr_h1 method" );
  static_assert( update_v<Ntk>, "Ntk does not implement the update method" );
#endif

  resubstitution_stats st;
  detail::resubstitution_impl<view_t> p( resub_view, ps, st );
  p.run();
  if ( ps.verbose )
  {
    st.report();
  }
  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */
