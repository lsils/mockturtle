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

#include "../traits.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/depth_view.hpp"
#include "reconv_cut2.hpp"

#include <kitty/static_truth_table.hpp>
#include <kitty/constructors.hpp>
#include <kitty/print.hpp>

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
  uint32_t max_pis{8};

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{150};

  /*! \brief Maximum number of pair-wise divisors to consider. */
  uint32_t max_divisors2{500};

  /*! \brief Maximum number of nodes per reconvergence-driven window. */
  uint32_t max_nodes{100};

  /*! \brief Maximum number of nodes added by resubstitution. */
  uint32_t max_inserts{2};

  /*! \brief Maximum fanout of a node to be considered as root. */
  uint32_t skip_fanout_limit_for_roots{1000};

  /*! \brief Maximum fanout of a node to be considered as divisor. */
  uint32_t skip_fanout_limit_for_divisors{100};

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

  /*! \brief Accumulated runtime for cut evaluation/computing a resubsitution. */
  stopwatch<>::duration time_eval{0};

  /*! \brief Accumulated runtime for mffc computation. */
  stopwatch<>::duration time_mffc{0};

  /*! \brief Accumulated runtime for divisor computation. */
  stopwatch<>::duration time_divs{0};

  /*! \brief Accumulated runtime for updating the network. */
  stopwatch<>::duration time_substitute{0};

  /*! \brief Accumulated runtime for simulation. */
  stopwatch<>::duration time_simulation{0};

  /*! \brief Initial network size (before resubstitution) */
  uint64_t initial_size{0};

  /*! \brief Total number of divisors  */
  uint64_t num_total_divisors{0};

  /*! \brief Total number of leaves  */
  uint64_t num_total_leaves{0};

  /*! \brief Total number of gain  */
  uint64_t estimated_gain{0};

  void report() const
  {
    std::cout << fmt::format( "[i] total time                                                  ({:>5.2f} secs)\n", to_seconds( time_total ) );
    std::cout << fmt::format( "[i]   cut time                                                  ({:>5.2f} secs)\n", to_seconds( time_cuts ) );
    std::cout << fmt::format( "[i]   mffc time                                                 ({:>5.2f} secs)\n", to_seconds( time_mffc ) );
    std::cout << fmt::format( "[i]   divs time                                                 ({:>5.2f} secs)\n", to_seconds( time_divs ) );
    std::cout << fmt::format( "[i]   simulation time                                           ({:>5.2f} secs)\n", to_seconds( time_simulation ) );
    std::cout << fmt::format( "[i]   evaluation time                                           ({:>5.2f} secs)\n", to_seconds( time_eval ) );
    std::cout << fmt::format( "[i]   substitute                                                ({:>5.2f} secs)\n", to_seconds( time_substitute ) );
    std::cout << fmt::format( "[i] total divisors            = {:8d}\n",         ( num_total_divisors ) );
    std::cout << fmt::format( "[i] total leaves              = {:8d}\n",         ( num_total_leaves ) );
    std::cout << fmt::format( "[i] estimated gain            = {:8d} ({:>5.2f}\%)\n",
                              estimated_gain, ( (100.0 * estimated_gain) / initial_size ) );
  }
};

namespace detail
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
      ntk.incr_fanout_size( l );

    /* dereference the node */
    auto count1 = node_deref_rec( n );

    /* collect the nodes inside the MFFC */
    node_mffc_cone( n, inside );

    /* reference it back */
    auto count2 = node_ref_rec( n );
    assert( count1 == count2 );

    for ( const auto& l : leaves )
      ntk.decr_fanout_size( l );

    return count1;
  }

private:
  /* ! \brief Dereference the node's MFFC */
  int32_t node_deref_rec( node const& n )
  {
    if ( ntk.is_pi( n ) )
      return 0;

    int32_t counter = 1;
    ntk.foreach_fanin( n, [&]( const auto& f ){
        auto const& p = ntk.get_node( f );

        ntk.decr_fanout_size( p );
        if ( ntk.fanout_size( p ) == 0 )
          counter += node_deref_rec( p );
      });

    return counter;
  }

  /* ! \brief Reference the node's MFFC */
  int32_t node_ref_rec( node const& n )
  {
    if ( ntk.is_pi( n ) )
      return 0;

    int32_t counter = 1;
    ntk.foreach_fanin( n, [&]( const auto& f ){
        auto const& p = ntk.get_node( f );

        auto v = ntk.fanout_size( p );
        ntk.incr_fanout_size( p );
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

    if ( !top_most && ( ntk.is_pi( n ) || ntk.fanout_size( n ) > 0 ) )
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

template<typename Ntk, int num_pis>
class simulator
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = kitty::static_truth_table<num_pis>;

  explicit simulator( Ntk const& ntk, uint32_t num_divisors )
    : ntk( ntk )
    , num_divisors( num_divisors )
    , tts( num_divisors )
    , node_to_index( ntk.size(), 0u )
    , phase( ntk.size(), false )
  {
    TT tt;
    for ( auto i = 0; i < num_pis; ++i )
    {
      kitty::create_nth_var( tt, i );
      tts[i+1] = tt;
    }
  }

  void assign( node const& n, uint32_t index )
  {
    assert( n < node_to_index.size() );
    assert( index < num_divisors );
    node_to_index[n] = index;
  }

  TT get_tt( signal const& s ) const
  {
    auto const tt = tts.at( node_to_index.at( ntk.get_node( s ) ) );
    return ntk.is_complemented( s ) ? ~tt : tt;
  }

  void set_tt( uint32_t index, TT const& tt )
  {
    tts[index] = tt;
  }

  void normalize( std::vector<node> const& nodes )
  {
    for ( const auto& n : nodes )
    {
      assert( n < phase.size() );
      assert( n < node_to_index.size() );

      if ( n == 0 )
        return;

      auto& tt = tts[node_to_index.at( n )];
      if ( kitty::get_bit( tt, 0 ) )
      {
        tt = ~tt;
        phase[n] = true;
      }
      else
      {
        phase[n] = false;
      }
    }
  }

  bool get_phase( node const& n ) const
  {
    assert( n < phase.size() );
    return phase.at( n );
  }

private:
  Ntk const& ntk;
  uint32_t num_divisors;

  std::vector<TT> tts;
  std::vector<uint32_t> node_to_index;
  std::vector<bool> phase;
}; /* simulator */

struct generic_resub_functor_stats
{
  /*! \brief Accumulated runtime for const-resub */
  stopwatch<>::duration time_resubC{0};

  /*! \brief Accumulated runtime for zero-resub */
  stopwatch<>::duration time_resub0{0};

  /*! \brief Number of accepted constant resubsitutions */
  uint32_t num_const_accepts{0};

  /*! \brief Number of accepted zero resubsitutions */
  uint32_t num_div0_accepts{0};

  void report() const
  {
    std::cout << "[i] kernel: default_resub_functor\n";
    std::cout << fmt::format( "[i]     constant-resub {:6d}                                   ({:>5.2f} secs)\n",
                                num_const_accepts, to_seconds( time_resubC ) );
      std::cout << fmt::format( "[i]            0-resub {:6d}                                   ({:>5.2f} secs)\n",
                                num_div0_accepts, to_seconds( time_resub0 ) );
      std::cout << fmt::format( "[i]            total   {:6d}\n",
                                (num_const_accepts + num_div0_accepts) );
  }
};

template<typename Ntk, typename Simulator>
class generic_resub_functor
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using stats = generic_resub_functor_stats;

  explicit generic_resub_functor( Ntk const& ntk, Simulator const& sim, std::vector<node> const& divs, generic_resub_functor_stats& st )
    : ntk( ntk )
    , sim( sim )
    , divs( divs )
    , st( st )
  {
  }

  std::optional<signal> operator()( node const& root, uint32_t required, uint32_t& cost ) const
  {
    /* consider constants */
    auto g = call_with_stopwatch( st.time_resubC, [&]() {
        return resub_const( root, required );
      } );
    if ( g )
    {
      ++st.num_const_accepts;
      cost = 0;
      return g; /* accepted resub */
    }

    /* consider equal nodes */
    g = call_with_stopwatch( st.time_resub0, [&]() {
        return resub_div0( root, required );
      } );
    if ( g )
    {
      ++st.num_div0_accepts;
      cost = 0;
      return g; /* accepted resub */
    }

    return std::nullopt;
  }

private:
  std::optional<signal> resub_const( node const& root, uint32_t required ) const
  {
    (void)required;
    auto const tt = sim.get_tt( ntk.make_signal( root ) );
    if ( tt == sim.get_tt( ntk.get_constant( false ) ) )
    {
      return sim.get_phase( root ) ? ntk.get_constant( true ) : ntk.get_constant( false );
    }
    return std::nullopt;
  }

  std::optional<signal> resub_div0( node const& root, uint32_t required ) const
  {
    (void)required;
    auto const tt = sim.get_tt( ntk.make_signal( root ) );
    for ( const auto& d : divs )
    {
      if ( root == d )
        break;

      if ( tt != sim.get_tt( ntk.make_signal( d ) ) )
        continue; /* next */

      return ( sim.get_phase( d ) ^ sim.get_phase( root ) ) ? !ntk.make_signal( d ) : ntk.make_signal( d );
    }

    return std::nullopt;
  }

private:
  Ntk const& ntk;
  Simulator const& sim;
  std::vector<node> const& divs;
  stats& st;
}; /* generic_resub_functor */

template<class Ntk, class ResubFn>
class resubstitution_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit resubstitution_impl( Ntk& ntk, resubstitution_params const& ps, resubstitution_stats& st, typename ResubFn::stats& resub_st )
    : ntk( ntk ), sim( ntk, 150 ), ps( ps ), st( st ), resub_st( resub_st )
  {
    st.initial_size = ntk.num_gates();
  }

  void run()
  {
    stopwatch t( st.time_total );

    /* start the managers */
    cut_manager<Ntk> mgr( ps.max_pis );

    const auto size = ntk.size();
    progress_bar pbar{ntk.size(), "resub |{0}| node = {1:>4}   cand = {2:>4}   est. gain = {3:>5}", ps.progress};

    ntk.foreach_gate( [&]( auto const& n, auto i ){
        if ( i >= size )
          return false; /* terminate */

        pbar( i, i, candidates, st.estimated_gain );

        if ( ntk.is_dead( n ) )
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
            return evaluate( n, leaves, ps.max_inserts );
          });
        if ( !g )
        {
          return true; /* next */
        }

        /* update progress bar */
        candidates++;
        st.estimated_gain += last_gain;

        /* update network */
        call_with_stopwatch( st.time_substitute, [&]() {
            ntk.substitute_node( n, *g );
          });

        return true; /* next */
      });
  }

private:
  void simulate( std::vector<node> const &leaves )
  {
    for ( auto i = 0u; i < divs.size(); ++i )
    {
      const auto d = divs.at( i );
      // std::cout << "[i] divisor " << d << " with index " << i << std::endl;

      /* skip constant 0 */
      if ( d == 0 )
      {
        // std::cout << "[i] skip const 0" << std::endl;
        continue;
      }

      /* assign leaves to variables */
      if ( i < leaves.size() )
      {
        sim.assign( d, i + 1 );
        continue;
      }

      sim.assign( d, i - leaves.size() + ps.max_pis + 1 );
      std::vector<kitty::static_truth_table<8>> tts;
      ntk.foreach_fanin( d, [&]( const auto& s, auto i ){
          (void)i;
          tts.emplace_back( sim.get_tt( s ) );
        });

      auto const tt = ntk.compute( d, tts.begin(), tts.end() );
      sim.set_tt( i - leaves.size() + ps.max_pis + 1, tt );
    }

    /* normalize truth tables */
    sim.normalize( divs );
  }

  std::optional<signal> evaluate( node const& root, std::vector<node> const &leaves, uint32_t max_inserts )
  {
    (void)max_inserts;
    uint32_t const required = std::numeric_limits<uint32_t>::max();

    last_gain = 0;

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
    {
      return std::optional<signal>();
    }

    /* update statistics */
    st.num_total_divisors += num_divs;
    st.num_total_leaves += leaves.size();

    /* simulate the nodes */
    call_with_stopwatch( st.time_simulation, [&]() { simulate( leaves ); });

    ResubFn resub_fn( ntk, sim, divs, resub_st );

    uint32_t cost = 0;
    auto const& r = resub_fn( root, required, cost );
    last_gain = num_mffc - cost;
    return r;
  }

  void collect_divisors_rec( node const& n, std::vector<node>& internal )
  {
    /* skip visited nodes */
    if ( ntk.visited( n ) == ntk.trav_id() )
      return;
    ntk.set_visited( n, ntk.trav_id() );

    ntk.foreach_fanin( n, [&]( const auto& f ){
        collect_divisors_rec( ntk.get_node( f ), internal );
      });

    /* collect the internal nodes */
    if ( ntk.value( n ) == 0 && n != 0 ) /* ntk.fanout_size( n ) */
      internal.emplace_back( n );
  }

  bool collect_divisors( node const& root, std::vector<node> const& leaves, uint32_t required )
  {
    /* add the leaves of the cuts to the divisors */
    divs.clear();

    ntk.incr_trav_id();
    for ( const auto& l : leaves )
    {
      divs.emplace_back( l );
      ntk.set_visited( l, ntk.trav_id() );
    }

    /* mark nodes in the MFFC */
    for ( const auto& t : temp )
      ntk.set_value( t, 1 );

    /* collect the cone (without MFFC) */
    collect_divisors_rec( root, divs );

    /* unmark the current MFFC */
    for ( const auto& t : temp )
      ntk.set_value( t, 0 );

    /* check if the number of divisors is not exceeded */
    if ( divs.size() - leaves.size() + temp.size() >= ps.max_divisors - ps.max_pis )
      return false;

    /* get the number of divisors to collect */
    int32_t limit = ps.max_divisors - ps.max_pis - ( divs.size() - leaves.size() + temp.size() );

    /* explore the fanouts, which are not in the MFFC */
    int32_t counter = 0;
    bool quit = false;

    /* NOTE: this is tricky and cannot be converted to a range-based loop */
    auto size = divs.size();
    for ( auto i = 0u; i < size; ++i )
    {
      auto const d = divs.at( i );

      if ( ntk.fanout_size( d ) > ps.skip_fanout_limit_for_divisors )
        continue;

      /* if the fanout has all fanins in the set, add it */
      ntk.foreach_fanout( d, [&]( node const& p ){
          if ( ntk.visited( p ) == ntk.trav_id() || ntk.level( p ) > required )
            return true; /* next fanout */

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
            return true; /* next fanout */

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
      divs.emplace_back( t );
    }

    assert( root == divs.at( divs.size()-1u ) );
    assert( divs.size() - leaves.size() <= ps.max_divisors - ps.max_pis );

    return true;
  }

private:
  Ntk& ntk;
  simulator<Ntk,8> sim;

  resubstitution_params const& ps;
  resubstitution_stats& st;
  typename ResubFn::stats& resub_st;

  /* temporary statistics for progress bar */
  uint32_t candidates{0};
  uint32_t last_gain{0};

  std::vector<node> temp;
  std::vector<node> divs;
  uint32_t num_divs{0};
};

} /* namespace detail */

/*! \brief Boolean resubstitution.
 *
 * **Required network functions:**
 * - `clear_values`
 * - `fanout_size`
 * - `foreach_fanin`
 * - `foreach_gate`
 * - `foreach_node`
 * - `get_constant`
 * - `get_node`
 * - `is_complemented`
 * - `is_pi`
 * - `level`
 * - `make_signal`
 * - `set_value`
 * - `set_visited`
 * - `size`
 * - `substitute_node`
 * - `value`
 * - `visited`
 *
 * \param ntk Input network (will be changed in-place)
 * \param ps Resubstitution params
 * \param pst Resubstitution statistics
 */
template<class Ntk>
void resubstitution( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_level_v<Ntk>, "Ntk does not implement the has_level method" );
  static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
  static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the has_size method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the has substitute_node method" );
  static_assert( has_value_v<Ntk>, "Ntk does not implement the has_value method" );
  static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );

  using simulator_t = detail::simulator<Ntk,8>;
  using resubstitution_functor_t = detail::generic_resub_functor<Ntk,simulator_t>;

  resubstitution_stats st;
  typename resubstitution_functor_t::stats resub_st;
  detail::resubstitution_impl<Ntk,resubstitution_functor_t> p( ntk, ps, st, resub_st );
  p.run();
  if ( ps.verbose )
  {
    st.report();
    resub_st.report();
  }
  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */
