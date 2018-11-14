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

#include "../networks/mig.hpp"
#include "../traits.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/depth_view.hpp"
#include "../views/fanout_view.hpp"
#include "../views/window_view.hpp"
#include "detail/mffc_utils.hpp"
#include "reconv_cut.hpp"
#include "simulation.hpp"
#include "reconv_cut2.hpp"

#include <fmt/format.h>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/kitty.hpp>

#include <iostream>
#include <optional>

namespace mockturtle
{

namespace detail
{
  /*** observability don't cares based on abcOdc.c ***/
  struct odc_parameters
  {
    int vars_max{5};
    int levels{8};
    int perc_cutoff{10};
  };

  struct odc_statistics
  {
    int num_wins{0};
    int num_empty_wins{0};
    int num_sim_cutoffs{0};
  };

  template<typename Ntk, typename WindowNtk>
  class odc_manager
  {
  public:
    using node = typename Ntk::node;
    using signal = typename Ntk::signal;

  public:
    odc_manager( Ntk const& ntk, odc_statistics& st, odc_parameters const& ps )
      : ntk( ntk )
      , st( st )
      , ps( ps )
    {
      assert( ps.vars_max > 4 && ps.vars_max <= 16 );
      assert( ps.levels > 0 && ps.levels < 10 );
    }

    void sweep_leaf_tfo_rec( node const& l, uint32_t level_limit, node const& n )
    {
      /* TODO: also skip COs by returning if `l` is a computational output */
      if ( ntk.level( l ) > level_limit || l == n )
        return;

      if ( ntk.visited( l ) == trav_id )
        return;
      ntk.set_visited( l, trav_id );

      /* skip nodes with large fanouts to reduce runtime */
      if ( ntk.fanout_size( l ) > 100 )
        return;

      ntk.foreach_fanout( l, [&]( const auto& o ){
          sweep_leaf_tfo_rec( o, level_limit, n );
        });
    }

    /* \brief Marks the TFO of the collected nodes up to a given level. */
    void sweep_leaf_tfo( node const& n, std::vector<node> const& leaves, uint32_t level_limit )
    {
      ++trav_id;
      for ( const auto& l : leaves )
      {
        sweep_leaf_tfo_rec( l, ntk.level( l ) + level_limit, n );
      }
    }

    void collect_roots_rec( std::vector<node>& roots, node const& n )
    {
      assert( ntk.visited( n ) == trav_id );

      /* check if the node has all fanouts marked */
      bool all_fanouts_marked = true;
      ntk.foreach_fanout( n, [&]( const auto& o ){
          if ( ntk.visited( o ) != trav_id )
          {
            all_fanouts_marked = false;
            return false;
          }
          return true;
        });

      /* if some of the fanouts are unmarked, add the node to the root */
      if ( !all_fanouts_marked )
      {
        roots.push_back( n );
        return;
      }

      /* otherwise, call recursively */
      ntk.foreach_fanout( n, [&]( auto const& p ){
          collect_roots_rec( roots, p );
        });
    }

    /* \brief Collect the roots of the window
     *
     * Roots of the window are the nodes that have at least one fanout
     * that is not in the TFO of the leaves.
     */
    std::vector<node> collect_roots( node const& n )
    {
      assert( ntk.visited( n ) != trav_id );

      /* mark the node with old traversal ID */
      ntk.set_visited( n, trav_id );

      /* collect the roots */
      std::vector<node> roots;
      collect_roots_rec( roots, n );
      return roots;
    }

    bool add_missing_rec( node const& n )
    {
      /* skip the already collected leaves and branches */
      if ( ntk.visited( n ) == trav_id )
        return true;

      /* if this is not an internal node, make it a new branch */
      if ( ntk.visited( n ) != prev_trav_id || ntk.is_pi( n ) ) // TODO: ntk.is_ci
      {
        ntk.set_visited( n, trav_id );
        branches.push_back( n );
        return ( branches.size() <= 32 );
      }

      /* visit the fanins of the node */
      auto result = true;
      ntk.foreach_fanin( n, [&]( const auto& i ){
          if ( !add_missing_rec( ntk.get_node( i ) ) )
          {
            result = false;
            return false;
          }

          return true;
        });

      return result;
    }

    /* \brief Adds to the window nodes and leaves in the TFI of the roots. */
    bool add_missing( std::vector<node> const& leaves, std::vector<node> const& roots )
    {
      /* set the leaves */
      prev_trav_id = trav_id;
      ++trav_id;

      for ( const auto& l : leaves )
        ntk.set_visited( l, trav_id );

      /* explore from the roots */
      branches.clear();
      for ( const auto& r : roots )
      {
        if ( !add_missing_rec( r ) )
          return false;
      }

      return true;
    }

    bool dont_care_window( node const& n, std::vector<node> const& leaves, std::vector<node>& roots )
    {
      /* makr the TFO of the collected nodes up to the given level */
      sweep_leaf_tfo( n, leaves, ps.levels );

      /* find the roots of the window */
      roots = collect_roots( n );

      /* TODO: computation failed */
      if ( roots.size() == 0 )
      {
        return false;
      }

      if ( roots.size() == 1 && roots[0u] == n )
      {
        /* empty window */
        return false;
      }

      /* add the nodes in the TFI of the roots that are not yet in the window */
      if ( !add_missing( leaves, roots ) )
      {
        /* too many branches */
        return false;
      }

      return true;
    }

    void reset()
    {
    }

    struct window
    {
      /* allocate a new network (with a new storage) */
      WindowNtk ntk;

      std::unordered_map<node, signal> lo;
      std::unordered_map<node, signal> hi;

      signal root{ntk.get_constant( false )};

      /* ODC simulation */
      std::map<node, kitty::dynamic_truth_table> tt0;
      std::map<node, kitty::dynamic_truth_table> tt1;
    }; /* window */

    uint32_t estimate_odcs( window& win )
    {
      return 0;
    }

    std::pair<signal,signal>
    construct_window_rec( window& win, node const& n, node const& pivot )
    {
      /* skip visisted nodes */
      if ( ntk.visited( n ) == trav_id )
      {
        return {win.lo.at( n ), win.hi.at( n )};
      }
      ntk.set_visited( n, trav_id );

      /* consider the case when the node is the pivot */
      if ( n == pivot )
      {
        auto const s0 = win.ntk.get_constant( false );
        auto const s1 = win.ntk.get_constant( true );
        win.lo.emplace( n, s0 );
        win.hi.emplace( n, s1 );
        win.ntk.set_value( win.ntk.get_node( s0 ), 0 ); // mask
        win.ntk.set_value( win.ntk.get_node( s1 ), 0 ); // mask
        return {s0, s1};
      }

      std::vector<signal> fs0;
      std::vector<signal> fs1;
      uint32_t mask0 = 0;
      uint32_t mask1 = 0;
      ntk.foreach_fanin( n, [&]( const auto& f ){
          auto const& p = ntk.get_node( f );
          auto const ss = construct_window_rec( win, p, pivot );
          mask0 |= win.ntk.get_node( ss.first );
          mask1 |= win.ntk.get_node( ss.second );
          fs0.emplace_back( ss.first );
          fs1.emplace_back( ss.second );
        });

      auto const s0 = win.ntk.clone_node( ntk, n, fs0 );
      auto const s1 = win.ntk.clone_node( ntk, n, fs1 );
      win.lo.emplace( n, s0 );
      win.hi.emplace( n, s1 );
      win.ntk.set_value( win.ntk.get_node( s0 ), mask0 ); // mask
      win.ntk.set_value( win.ntk.get_node( s1 ), mask1 ); // mask
      return { s0, s1 };
    }

    uint32_t construct_window( window& win, node const& pivot, std::vector<node> const& leaves, std::vector<node> const& roots )
    {
      ++trav_id;

      /* constant */
      auto const f = ntk.get_node( ntk.get_constant( false ) );
      ntk.set_visited( f, trav_id );

      win.lo.emplace( f, win.ntk.get_constant( false ) );
      win.hi.emplace( f, win.ntk.get_constant( false ) );
      win.ntk.set_value( f, 0 ); // mask

      /* primary inputs */
      std::vector<signal> win_input_signals( 32 );
      for ( auto i = 0; i < 32; ++i )
        win_input_signals[i] = win.ntk.create_pi();

      assert( win.ntk.num_pis() == 32 );
      assert( win_input_signals.size() == 32 );
      assert( leaves.size() <= uint64_t( ps.vars_max ) );

      auto counter = 0;
      for ( const auto& l : leaves )
      {
        auto const s = win_input_signals.at( counter );
        ntk.set_visited( l, trav_id );
        win.lo.emplace( l, s );
        win.hi.emplace( l, s );
        ++counter;
        win.ntk.set_value( win.ntk.get_node( s ), 1 << counter ); // mask
      }

      for ( const auto& b : branches )
      {
        auto const s = win_input_signals.at( counter );

        ntk.set_visited( b, trav_id );
        win.lo.emplace( b, s );
        win.hi.emplace( b, s );
        ++counter;
        win.ntk.set_value( win.ntk.get_node( s ), 1 << counter ); // mask
      }

      /* construct roots recursively */
      signal& out = win.root;
      for ( const auto& r : roots )
      {
        auto const ss = construct_window_rec( win, r, pivot );
        out = win.ntk.create_or( out, win.ntk.create_xor( ss.first, ss.second ) );
      }

      return true;
    }

    bool compute( node const& pivot, std::vector<node> const& leaves )
    {
      ++st.num_wins;

      std::vector<node> roots;
      auto result = dont_care_window( pivot, leaves, roots );
      if ( !result )
      {
        ++st.num_empty_wins;
        return false;
      }

      if ( verbose )
      {
        std::cout << fmt::format( "window: root = {0:>6} l/r/b = {1:>3}/{2:>3}/{3:>3}\n",
                                  pivot, leaves.size(), roots.size(), branches.size() );
      }

      /* simulate to estimate the amount of don't-cares */
      auto num_mints = simulate_window( pivot, leaves, roots, branches );

      auto const num_bits = 1 << ps.vars_max;
      if ( verbose )
      {
        std::cout << fmt::format( "window: root = {0:>6} don't-cares = {1:>3}%\n",
                                  pivot, ( 100.0 * ( num_bits - num_mints ) / num_bits ) );
      }

      /* skip if there is less then the given percentage of don't cares */
      if ( 100.0 * ( num_bits - num_mints ) / num_bits < 1.0 * ps.perc_cutoff )
      {
        ++st.num_sim_cutoffs;
        return false;
      }

      return true;
    }

    void simulate_window_rec( node const& n, node const& pivot,
                                  std::unordered_map<node,kitty::dynamic_truth_table>& tts0,
                                  std::unordered_map<node,kitty::dynamic_truth_table>& tts1 )
    {
      /* skip visited nodes */
      if ( ntk.value( n ) == trav_id )
        return;
      ntk.set_value( n, trav_id );

      /* consider the case when the node is the pivot */
      if ( n == pivot )
      {
        kitty::dynamic_truth_table tt( ps.vars_max );
        tts0.emplace( n,  tt );
        tts1.emplace( n, ~tt );
        return;
      }

      std::vector<kitty::dynamic_truth_table> fanin_tts0, fanin_tts1;
      ntk.foreach_fanin( n, [&]( const auto& f ){
          auto const& p = ntk.get_node( f );
          simulate_window_rec( p, pivot, tts0, tts1 );
          fanin_tts0.emplace_back( ntk.is_complemented( f ) ? ~tts0[p] : tts0[p] );
          fanin_tts1.emplace_back( ntk.is_complemented( f ) ? ~tts1[p] : tts1[p] );
        });

      tts0.emplace( n, ntk.compute( n, fanin_tts0.begin(), fanin_tts0.end() ) );
      tts1.emplace( n, ntk.compute( n, fanin_tts1.begin(), fanin_tts1.end() ) );
    }

    uint32_t simulate_window( node const& pivot,
                              std::vector<node> const& leaves,
                              std::vector<node> const& roots,
                              std::vector<node> const& branches )
    {
      std::unordered_map<node,kitty::dynamic_truth_table> tts0, tts1;

      ++trav_id;
      auto counter = 0;

      for ( const auto& l : leaves )
      {
        kitty::dynamic_truth_table tt( ps.vars_max );
        if ( counter < ps.vars_max )
        {
          kitty::create_nth_var( tt, counter );
        }
        else
        {
          kitty::create_random( tt );
        }
        tts0.emplace( l, tt );
        tts1.emplace( l, tt );
        ntk.set_value( l, trav_id );
        ++counter;
      }

      for ( const auto& b : branches )
      {
        kitty::dynamic_truth_table tt( ps.vars_max );
        if ( counter < ps.vars_max )
        {
          kitty::create_nth_var( tt, counter );
        }
        else
        {
          kitty::create_random( tt );
        }
        tts0.emplace( b, tt );
        tts1.emplace( b, tt );
        ntk.set_value( b, trav_id );
        ++counter;
      }

      kitty::dynamic_truth_table care( ps.vars_max );
      for ( const auto& r : roots )
      {
        simulate_window_rec( r, pivot, tts0, tts1 );
        care |= ( tts0[r] ^ tts1[r] );
      }
      return kitty::count_ones( ~care );
    }

    uint32_t estimate_dont_cares( window const& win )
    {
      return 0;
    }

    Ntk const& ntk;
    bool verbose = true;
    uint64_t trav_id{0};
    uint64_t prev_trav_id{0};

    odc_statistics& st;
    odc_parameters const& ps;

    std::vector<node> branches;
  }; /* odc_manager */
}

/*! \brief Parameters for resubstitution.
 *
 * The data structure `resubstitution_params` holds configurable parameters with
 * default arguments for `resubstitution`.
 */
struct resubstitution_params
{
  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{6};

  /*! \brief Maximum number of nodes per reconvergence-driven window. */
  uint32_t max_nodes{100};

  /*! \brief Maximum number of nodes added by resubstitution. */
  uint32_t max_inserts{1};

  /*! \brief Maximum number of nodes compared during resubstitution. */
  uint32_t max_compare{20};

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
};

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

  /*! \brief Accumulated runtime for window computation. */
  stopwatch<>::duration time_windows{0};

  /*! \brief Accumulated runtime for depth computation. */
  stopwatch<>::duration time_depth{0};

  /*! \brief Accumulated runtime for simulation. */
  stopwatch<>::duration time_simulation{0};

  /*! \brief Accumulated runtime for resubstitution. */
  stopwatch<>::duration time_resubstitution{0};

  /*! \brief Number of accepted zero resubsitutions */
  uint64_t num_zero_accepts{0};

  /*! \brief Number of accepted one resubsitutions */
  uint64_t num_one_accepts{0};

  /*! \brief Number of accepted two resubsitutions */
  uint64_t num_two_accepts{0};

  /*! \brief Number of filtered one resubsitutions */
  uint64_t num_one_filter{0};

  /*! \brief Number of filtered two resubsitutions */
  uint64_t num_two_filter{0};

  void report() const
  {
    std::cout << fmt::format( "[i] total time           = {:>5.2f} secs\n", to_seconds( time_total ) );
    std::cout << fmt::format( "[i]   cut time           = {:>5.2f} secs\n", to_seconds( time_cuts ) );
    std::cout << fmt::format( "[i]   windows time       = {:>5.2f} secs\n", to_seconds( time_windows ) );
    std::cout << fmt::format( "[i]   depth time         = {:>5.2f} secs\n", to_seconds( time_depth ) );
    std::cout << fmt::format( "[i]   simulation time    = {:>5.2f} secs\n", to_seconds( time_simulation ) );
    std::cout << fmt::format( "[i]   resubstituion time = {:>5.2f} secs\n", to_seconds( time_resubstitution ) );
    std::cout << fmt::format( "[i] accepted resubs      = {:8d}\n",         ( num_zero_accepts + num_one_accepts + num_two_accepts ) );
    std::cout << fmt::format( "[i]   0-resubs           = {:8d}\n",         ( num_zero_accepts ) );
    std::cout << fmt::format( "[i]   1-resubs           = {:8d}\n",         ( num_one_accepts ) );
    std::cout << fmt::format( "[i]   2-resubs           = {:8d}\n",         ( num_two_accepts ) );
    std::cout << fmt::format( "[i] filtered cand.       = {:8d}\n",         ( num_one_filter + num_two_filter ) );
    std::cout << fmt::format( "[i]   1-resubs           = {:8d}\n",         ( num_one_filter ) );
    std::cout << fmt::format( "[i]   2-resubs           = {:8d}\n",         ( num_two_filter ) );
  }
};

namespace detail
{

template<class Ntk>
class resubstitution_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using window = depth_view<window_view<fanout_view<Ntk>>>;

  explicit resubstitution_impl( Ntk& ntk, resubstitution_params const& ps, resubstitution_stats& st )
      : ntk( ntk ), fanout_ntk( ntk ), ps( ps ), st( st )
  {
  }

  bool resubstitute_node( window& win, node const& n, signal const& s, bool zero_gain = false )
  {
    const auto& r = ntk.get_node( s );
    int32_t gain = detail::recursive_deref( win, /* original node */ n );
    gain -= detail::recursive_ref( win, /* replace with */ r );
    if ( gain > 0 || zero_gain )
    {
      ++_candidates;
      _estimated_gain += gain;

      win.substitute_node_of_parents( fanout_ntk.fanout( n ), n, s );

      ntk.set_value( n, 0 );
      ntk.set_value( r, ntk.fanout_size( r ) );

      return true;
    }
    else
    {
      detail::recursive_deref( win, /* replaced with */ r );
      detail::recursive_ref( win, /* original node */ n );

      return false;
    }
  }

  void resubstitute( window& win, node const& n, node_map<kitty::dynamic_truth_table, window> const& tts )
  {
    assert( ps.max_inserts >= 0u );
    switch ( ps.max_inserts )
    {
    case 0u:
      zero_resubstitution( win, n, tts );
      break;
    case 1u:
      one_resubstitution( win, n, tts );
      break;
    default: /* >= 2u */
      two_resubstitution( win, n, tts );
      break;
    }
  }

  void zero_resubstitution( window& win, node const& n, node_map<kitty::dynamic_truth_table, window> const& tts )
  {
    auto counter = 0u;
    win.foreach_gate( [&]( auto const& x ) {
      if ( ++counter > ps.max_compare )
        return false;

      if ( x == n || win.level( x ) >= win.level( n ) )
      {
        return true; /* next */
      }

      if ( tts[n] == tts[x] )
      {
        const auto result = resubstitute_node( win, n, ntk.make_signal( x ), ps.zero_gain );
        if ( result )
        {
          ++st.num_zero_accepts;
          return false; /* accept */
        }
      }
      else if ( tts[n] == ~tts[x] )
      {
        const auto result = resubstitute_node( win, n, !ntk.make_signal( x ), ps.zero_gain );
        if ( result )
        {
          ++st.num_zero_accepts;
          return false; /* accept */
        }
      }

      return true; /* next */
    } );
  }

  void one_resubstitution( window& win, node const& n, node_map<kitty::dynamic_truth_table, window> const& tts )
  {
    bool done = false;
    auto counter_x = 0u;
    win.foreach_gate( [&]( auto const& x, auto i ) {
      if ( done )
        return false;
      if ( ++counter_x > ps.max_compare )
        return false;

      if ( x == n || win.level( x ) >= win.level( n ) )
      {
        return true; /* next */
      }

      if ( tts[n] == tts[x] )
      {
        const auto result = resubstitute_node( win, n, ntk.make_signal( x ), ps.zero_gain );
        if ( result )
        {
          ++st.num_zero_accepts;
          return false; /* accept */
        }
      }
      else if ( tts[n] == ~tts[x] )
      {
        const auto result = resubstitute_node( win, n, !ntk.make_signal( x ), ps.zero_gain );
        if ( result )
        {
          ++st.num_zero_accepts;
          return false; /* accept */
        }
      }

      auto counter_y = 0u;
      win.foreach_gate( [&]( auto const& y, auto j ) {
        if ( done )
          return false;
        if ( ++counter_y > ps.max_compare )
          return false;

        if ( i >= j )
          return true;
        assert( j > i );

        if ( y == n || win.level( y ) >= win.level( n ) )
        {
          return true; /* next */
        }

        if ( !ps.disable_maj_one_resub_filter &&
             ( tts[n] != ternary_majority(  tts[x], tts[y], tts[n] ) &&
               tts[n] != ternary_majority( ~tts[x], tts[y], tts[n] ) ) )
          {
          ++st.num_one_filter;
          return true; /* next */
        }

        auto counter_z = 0u;
        win.foreach_gate( [&]( auto const& z, auto k ) {
          if ( done )
            return false;
          if ( ++counter_z > ps.max_compare )
            return false;

          if ( j >= k )
            return true;
          assert( k > j );
          assert( k > i );

          if ( z == n || win.level( z ) >= win.level( n ) )
          {
            return true; /* next */
          }

          std::set<node> fanin_nodes;
          win.foreach_fanin( n, [&]( auto const& s ) { fanin_nodes.insert( win.get_node( s ) ); } );
          if ( fanin_nodes == std::set<node>{x, y, z} )
          {
            return true;
          }

          if ( tts[n] == ternary_majority( tts[x], tts[y], tts[z] ) )
          {
            const auto new_signal = ntk.create_maj( win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) );
            fanout_ntk.resize();
            const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
            if ( result )
            {
              ++st.num_one_accepts;
              done = true; /* accept */
            }
          }
          else if ( tts[n] == ternary_majority( ~tts[x], tts[y], tts[z] ) )
          {
            const auto new_signal = ntk.create_maj( !win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) );
            fanout_ntk.resize();
            const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
            if ( result )
            {
              ++st.num_one_accepts;
              done = true; /* accept */
            }
          }

          return true; /* next */
        } );

        return true; /* next */
      } );

      return true; /* next */
    } );
  }

  void two_resubstitution( window& win, node const& n, node_map<kitty::dynamic_truth_table, window> const& tts )
  {
    bool done = false;
    auto counter_x = 0u;
    win.foreach_gate( [&]( auto const& x, auto i ) {
      if ( done )
        return false;
      if ( ++counter_x > ps.max_compare )
        return false;

      if ( x == n || win.level( x ) >= win.level( n ) )
      {
        return true; /* next */
      }

      if ( tts[n] == tts[x] )
      {
        const auto result = resubstitute_node( win, n, ntk.make_signal( x ), ps.zero_gain );
        if ( result )
        {
          done = true;
          ++st.num_zero_accepts;
          return false;
        } /* accept */
      }
      else if ( tts[n] == ~tts[x] )
      {
        const auto result = resubstitute_node( win, n, !ntk.make_signal( x ), ps.zero_gain );
        if ( result )
        {
          done = true;
          ++st.num_zero_accepts;
          return false;
        } /* accept */
      }

      auto counter_y = 0u;
      win.foreach_gate( [&]( auto const& y, auto j ) {
        if ( done )
          return false;
        if ( ++counter_y > ps.max_compare )
          return false;

        if ( i >= j )
          return true;
        assert( j > i );

        if ( y == n || win.level( y ) >= win.level( n ) )
        {
          return true; /* next */
        }

        bool skip_maj_one_resubstitution = false;
        if ( !ps.disable_maj_one_resub_filter &&
             ( tts[n] != ternary_majority(  tts[x], tts[y], tts[n] ) &&
               tts[n] != ternary_majority( ~tts[x], tts[y], tts[n] ) ) )
        {
          /* skip 1-resub, but keep going and try to find a 2-resub
             with the current pair of nodes */
          ++st.num_one_filter;
          skip_maj_one_resubstitution = true;
        }

        auto counter_z = 0u;
        win.foreach_gate( [&]( auto const& z, auto k ) {
          if ( done )
            return false;
          if ( ++counter_z > ps.max_compare )
            return false;

          if ( j >= k )
            return true;
          assert( k > j );
          assert( k > i );

          if ( z == n || win.level( z ) >= win.level( n ) )
          {
            return true; /* next */
          }

          if ( !skip_maj_one_resubstitution )
          {
            std::set<node> fanin_nodes;
            win.foreach_fanin( n, [&]( auto const& s ) { fanin_nodes.insert( win.get_node( s ) ); } );
            if ( fanin_nodes == std::set<node>{x, y, z} )
            {
              return true;
            }

            if ( tts[n] == ternary_majority( tts[x], tts[y], tts[z] ) )
            {
              const auto new_signal = ntk.create_maj( win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) );
              fanout_ntk.resize();
              const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
              if ( result )
              {
                done = true;
                ++st.num_one_accepts;
                return true;
              } /* accept */
            }
            else if ( tts[n] == ternary_majority( ~tts[x], tts[y], tts[z] ) )
            {
              const auto new_signal = ntk.create_maj( !win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) );
              fanout_ntk.resize();
              const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
              if ( result )
              {
                done = true;
                ++st.num_one_accepts;
                return true;
              } /* accept */
            }
          }

          auto counter_u = 0u;
          win.foreach_gate( [&]( auto const& u, auto l ) {
            if ( done )
              return false;

            if ( ++counter_u > ps.max_compare )
              return false;

            if ( k >= l )
              return true;
            assert( l > k );
            assert( l > j );
            assert( l > i );

            if ( u == n || win.level( u ) >= win.level( n ) )
            {
              return true; /* next */
            }

            if ( !ps.disable_maj_two_resub_filter &&
                 ( tts[n] != ternary_majority( tts[x], tts[n], ternary_majority(  tts[y], tts[n],  tts[u] ) ) ) &&
                 ( tts[n] != ternary_majority( tts[y], tts[n], ternary_majority(  tts[z], tts[n],  tts[u] ) ) ) &&
                 ( tts[n] != ternary_majority( tts[x], tts[n], ternary_majority(  tts[y], tts[n], ~tts[u] ) ) ) &&
                 ( tts[n] != ternary_majority( tts[y], tts[n], ternary_majority(  tts[z], tts[n], ~tts[u] ) ) ) )
              {
              ++st.num_two_filter;
              return true; /* next */
            }

            auto counter_v = 0u;
            win.foreach_gate( [&]( auto const& v, auto m ) {
              if ( done )
                return false;
              if ( ++counter_v > ps.max_compare )
                return false;

              if ( l >= m )
                return true;
              assert( m > l );
              assert( m > k );
              assert( m > j );
              assert( m > i );

              if ( v == n || win.level( v ) >= win.level( n ) )
              {
                return true; /* next */
              }

              if ( tts[n] == ternary_majority( tts[u], tts[v], ternary_majority( tts[x], tts[y], tts[z] ) ) )
              {
                const auto new_signal = ntk.create_maj( win.make_signal( u ), win.make_signal( v ),
                                                        ntk.create_maj( win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) ) );
                fanout_ntk.resize();
                const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
                if ( result )
                {
                  ++st.num_two_accepts;
                  done = true; /* accept */
                }
              }
              else if ( tts[n] == ternary_majority( ~tts[u], tts[v], ternary_majority( tts[x], tts[y], tts[z] ) ) )
              {
                const auto new_signal = ntk.create_maj( !win.make_signal( u ), win.make_signal( v ),
                                                        ntk.create_maj( win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) ) );
                fanout_ntk.resize();
                const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
                if ( result )
                {
                  ++st.num_two_accepts;
                  done = true; /* accept */
                }
              }
              else if ( tts[n] == ternary_majority( tts[u], tts[v], ternary_majority( ~tts[x], tts[y], tts[z] ) ) )
              {
                const auto new_signal = ntk.create_maj( win.make_signal( u ), win.make_signal( v ),
                                                        ntk.create_maj( !win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) ) );
                fanout_ntk.resize();
                const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
                if ( result )
                {
                  ++st.num_two_accepts;
                  done = true; /* accept */
                }
              }
              else if ( tts[n] == ternary_majority( ~tts[u], tts[v], ternary_majority( ~tts[x], tts[y], tts[z] ) ) )
              {
                const auto new_signal = ntk.create_maj( !win.make_signal( u ), win.make_signal( v ),
                                                        ntk.create_maj( !win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) ) );
                fanout_ntk.resize();
                const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
                if ( result )
                {
                  ++st.num_two_accepts;
                  done = true; /* accept */
                }
              }

              return true; /* next */
            } );

            return true; /* next */
          } );

          return true; /* next */
        } );

        return true; /* next */
      } );

      return true; /* next */
    } );
  }

  void run()
  {
    const auto size = ntk.size();
    progress_bar pbar{ntk.size(), "resubstitution |{0}| node = {1:>4}   cand = {2:>4}   est. reduction = {3:>5}", ps.progress};

    stopwatch t( st.time_total );

    ntk.clear_visited();
    ntk.clear_values();
    ntk.foreach_node( [&]( auto const& n ) {
      ntk.set_value( n, ntk.fanout_size( n ) );
    } );

    ntk.foreach_gate( [&]( auto const& n, auto i ) {
      /* skip if all nodes have been tried */
      if ( i >= size )
        return false;

      /* skip nodes with many fanouts */
      if ( ntk.fanout_size( n ) > 1000 )
        return true; /* next */

      pbar( i, i, _candidates, _estimated_gain );

      bool has_mffc{false};
      ntk.foreach_fanin( n, [&]( auto const& f ) {
        if ( ntk.value( ntk.get_node( f ) ) == 1 )
        {
          has_mffc = true;
          return false;
        }
        return true;
      } );
      if ( has_mffc )
      {
        reconv_cut_params params{ps.max_pis};
        auto const leaves = call_with_stopwatch( st.time_cuts,
                                                [&]() { return reconv_cut( params )( ntk, {n} ); } );
        using fanout_view_t = decltype( fanout_ntk );
        const auto extended_cut = make_with_stopwatch<window_view<fanout_view_t>>( st.time_windows, fanout_ntk, leaves, std::vector<typename fanout_view_t::node>{{n}}, /* extend = */ ps.extend );
        if ( extended_cut.size() > ps.max_nodes )
          return true;
        auto win = call_with_stopwatch( st.time_depth, [&]() { return window( extended_cut ); } );

        default_simulator<kitty::dynamic_truth_table> sim( win.num_pis() );
        const auto tts = call_with_stopwatch( st.time_simulation,
                                              [&]() { return simulate_nodes<kitty::dynamic_truth_table>( win, sim ); } );

        call_with_stopwatch( st.time_resubstitution, [&]() { resubstitute( win, n, tts ); } );
      }

      return true;
    } );
  }

private:
  Ntk& ntk;
  fanout_view<Ntk> fanout_ntk;
  resubstitution_params const& ps;
  resubstitution_stats& st;

  uint32_t _candidates{0};
  uint32_t _estimated_gain{0};
};

} /* namespace detail */

/*! \brief Boolean resubstitution.
 *
 * **Required network functions:**
 * - `get_node`
 * - `size`
 * - `make_signal`
 * - `foreach_gate`
 * - `substitute_node_of_parents`
 * - `clear_visited`
 * - `clear_values`
 * - `fanout_size`
 * - `set_value`
 * - `foreach_node`
 *
 * \param ntk Input network (will be changed in-place)
 * \param ps Resubstitution params
 * \param pst Resubstitution statistics
 */
template<class Ntk>
void resubstitution( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
  static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_substitute_node_of_parents_v<Ntk>, "Ntk does not implement the substitute_node_of_parents method" );
  static_assert( has_clear_visited_v<Ntk>, "Ntk does not implement the clear_visited method" );
  static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );

  resubstitution_stats st;
  detail::resubstitution_impl<Ntk> p( ntk, ps, st );
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
