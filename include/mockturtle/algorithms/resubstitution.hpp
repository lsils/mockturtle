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
  \brief Resubstitution

  \author Heinz Riener
*/

#pragma once

#include "../networks/mig.hpp"
#include "../traits.hpp"
#include "simulation.hpp"
#include "reconv_cut.hpp"
#include "detail/mffc_utils.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/fanout_view.hpp"
#include "../views/window_view.hpp"
#include "../views/depth_view.hpp"

#include <fmt/format.h>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/kitty.hpp>

#include <iostream>

namespace mockturtle
{

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

  /*! \brief Accumulated runtime for simulation. */
  stopwatch<>::duration time_simulation{0};

  /*! \brief Accumulated runtime for resubstitution. */
  stopwatch<>::duration time_resubstitution{0};

  void report() const
  {
    std::cout << fmt::format( "[i] total time         = {:>5.2f} secs\n", to_seconds( time_total ) );
    std::cout << fmt::format( "[i] simulation time    = {:>5.2f} secs\n", to_seconds( time_simulation ) );
    std::cout << fmt::format( "[i] resubstituion time = {:>5.2f} secs\n", to_seconds( time_resubstitution ) );
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
    int32_t gain = detail::recursive_deref( win, /* original node */n );
    gain -= detail::recursive_ref( win, /* replace with */r );
    if ( gain > 0 || zero_gain )
    {
      ++_candidates;
      _estimated_gain += gain;

      ntk.substitute_node_of_parents( fanout_ntk.fanout( n ), n, s );
      ntk.set_value( n, 0 );
      ntk.set_value( r, ntk.fanout_size( r ) );

      return true;
    }
    else
    {
      detail::recursive_deref( win, /* replaced with */r );
      detail::recursive_ref( win, /* original node */n );

      return false;
    }
  }

  void zero_resubstitution( window& win, node const& n, node_map<kitty::dynamic_truth_table,window> const& tts )
  {
    auto counter = 0u;
    win.foreach_gate( [&]( auto const& x ){
        if ( ++counter > ps.max_compare ) return false;

        if ( x == n || win.level( x ) >= win.level( n ) )
        {
          return true; /* next */
        }

        if ( tts[ n ] == tts[ x ] )
        {
          const auto result = resubstitute_node( win, n, ntk.make_signal( x ), ps.zero_gain );
          if ( result ) return false; /* accept */
        }
        else if ( tts[ n ] == ~tts[ x ] )
        {
          const auto result = resubstitute_node( win, n, !ntk.make_signal( x ), ps.zero_gain );
          if ( result ) return false; /* accept */
        }

        return true; /* next */
      });
  }

  void one_resubstitution( window& win, node const& n, node_map<kitty::dynamic_truth_table,window> const& tts )
  {
    bool done = false;
    auto counter_x = 0u;
    win.foreach_gate( [&]( auto const& x, auto i ){
        if ( done ) return false;
        if ( ++counter_x > ps.max_compare ) return false;

        if ( x == n || win.level( x ) >= win.level( n ) )
        {
          return true; /* next */
        }

        if ( tts[ n ] == tts[ x ] )
        {
          const auto result = resubstitute_node( win, n, ntk.make_signal( x ), ps.zero_gain );
          if ( result ) return false; /* accept */
        }
        else if ( tts[ n ] == ~tts[ x ] )
        {
          const auto result = resubstitute_node( win, n, !ntk.make_signal( x ), ps.zero_gain );
          if ( result ) return false; /* accept */
        }

        auto counter_y = 0u;
        win.foreach_gate( [&]( auto const& y, auto j ){
          if ( done ) return false;
          if ( ++counter_y > ps.max_compare ) return false;

          if ( i >= j ) return true;
          assert( j > i );

          if ( y == n || win.level( y ) >= win.level( n ) )
          {
            return true; /* next */
          }

          if ( tts[ n ] != ternary_majority(  tts[ x ],  tts[ y ], tts[ n ] ) ||
               tts[ x ] != ternary_majority(  tts[ x ], ~tts[ y ], tts[ n ] ) ||
               tts[ y ] != ternary_majority( ~tts[ x ],  tts[ y ], tts[ n ] ) )
          {
            return true;
          }

          auto counter_z = 0u;
          win.foreach_gate( [&]( auto const& z, auto k ){
            if ( done ) return false;
            if ( ++counter_z > ps.max_compare ) return false;

            if ( j >= k ) return true;
            assert( k > j );
            assert( k > i );

            if ( z == n || win.level( z ) >= win.level( n ) )
            {
              return true; /* next */
            }

            std::set<node> fanin_nodes;
            win.foreach_fanin( n, [&]( auto const& s ) { fanin_nodes.insert( win.get_node( s ) ); });
            if ( fanin_nodes == std::set<node>{x,y,z} )
            {
              return true;
            }

            if ( tts[ n ] == ternary_majority( tts[ x ], tts[ y ], tts[ z ] ) )
            {
              const auto new_signal = ntk.create_maj( win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) );
              fanout_ntk.resize();
              const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
              if ( result ) done = true; /* accept */
            }
            else if ( tts[ n ] == ternary_majority( ~tts[ x ], tts[ y ], tts[ z ] ) )
            {
              const auto new_signal = ntk.create_maj( !win.make_signal( x ), win.make_signal( y ), win.make_signal( z ) );
              fanout_ntk.resize();
              const auto result = resubstitute_node( win, n, new_signal, ps.zero_gain );
              if ( result ) done = true; /* accept */
            }

            return true; /* next */
          });

          return true; /* next */
        });

        return true; /* next */
      });
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

    ntk.foreach_gate( [&]( auto const& n, auto i ){
      if ( i >= size ) return false;

      pbar( i, i, _candidates, _estimated_gain );

      auto const mffc_size = detail::mffc_size( ntk, n );
      if ( mffc_size > 1 )
      {
        reconv_cut_params params{ ps.max_pis };
        auto const& leaves = reconv_cut( params )( ntk, { n } );
        window_view<fanout_view<Ntk>> extended_cut( fanout_ntk, leaves, { n }, /* extend = */ps.extend );
        if ( extended_cut.size() > ps.max_nodes ) return true;
        window win( extended_cut );

        default_simulator<kitty::dynamic_truth_table> sim( win.num_pis() );
        const auto tts = call_with_stopwatch( st.time_simulation,
                                             [&]() { return simulate_nodes<kitty::dynamic_truth_table>( win, sim ); } );

        call_with_stopwatch( st.time_resubstitution,
                             [&]() { one_resubstitution( win, n, tts ); } );
      }

      return true;
    });
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
void resubstitution( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats *pst = nullptr )
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
