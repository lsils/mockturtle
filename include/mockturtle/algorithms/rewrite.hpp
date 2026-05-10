/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2023  EPFL
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
  \file rewrite.hpp
  \brief Inplace rewrite

  \author Alessandro Tempia Calvino
*/

#pragma once

#include "../traits.hpp"
#include "../utils/cost_functions.hpp"
#include "../utils/node_map.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/color_view.hpp"
#include "../views/depth_view.hpp"
#include "../views/fanout_view.hpp"
#include "../views/topo_view.hpp"
#include "../views/window_view.hpp"
#include "cleanup.hpp"
#include "cut_enumeration.hpp"
#include "cut_enumeration/rewrite_cut.hpp"
#include "reconv_cut.hpp"
#include "simulation.hpp"

#include <fmt/format.h>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/npn.hpp>
#include <kitty/operations.hpp>
#include <kitty/static_truth_table.hpp>

namespace mockturtle
{

/*! \brief Parameters for Rewrite.
 *
 * The data structure `rewrite_params` holds configurable parameters with
 * default arguments for `rewrite`.
 */
struct rewrite_params
{
  rewrite_params()
  {
    /* 0 < Cut limit < 16 */
    cut_enumeration_ps.cut_limit = 8;
    cut_enumeration_ps.minimize_truth_table = true;
  }

  /*! \brief Cut enumeration parameters. */
  cut_enumeration_params cut_enumeration_ps{};

  /*! \brief If true, candidates are only accepted if they do not increase logic depth. */
  bool preserve_depth{ false };

  /*! \brief Allow rewrite with multiple structures */
  bool allow_multiple_structures{ true };

  /*! \brief Allow zero-gain substitutions */
  bool allow_zero_gain{ false };

  /*! \brief Use satisfiability don't cares for optimization. */
  bool use_dont_cares{ false };

  /*! \brief Window size for don't cares calculation. */
  uint32_t window_size{ 8u };

  /*! \brief Be verbose. */
  bool verbose{ false };
};

/*! \brief Statistics for rewrite.
 *
 * The data structure `rewrite_stats` provides data collected by running
 * `rewrite`.
 */
struct rewrite_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{ 0 };

  /*! \brief Expected gain. */
  uint32_t estimated_gain{ 0 };

  /*! \brief Candidates */
  uint32_t candidates{ 0 };

  void report() const
  {
    std::cout << fmt::format( "[i] total time       = {:>5.2f} secs\n", to_seconds( time_total ) );
  }
};

namespace detail
{

template<class Ntk, class Library, class NodeCostFn>
class rewrite_impl
{
  static constexpr uint32_t num_vars = 4u;
  static constexpr uint32_t max_window_size = 8u;
  using network_cuts_t = dynamic_network_cuts<Ntk, num_vars, true, cut_enumeration_rewrite_cut>;
  using cut_manager_t = detail::dynamic_cut_enumeration_impl<Ntk, num_vars, true, cut_enumeration_rewrite_cut>;
  using cut_t = typename network_cuts_t::cut_t;
  using node_data = typename Ntk::storage::element_type::node_type;

public:
  rewrite_impl( Ntk& ntk, Library&& library, rewrite_params const& ps, rewrite_stats& st, NodeCostFn const& cost_fn )
      : ntk( ntk ), library( library ), ps( ps ), st( st ), cost_fn( cost_fn ), required( ntk, UINT32_MAX )
  {
  }

  void run()
  {
    stopwatch t( st.time_total );

    if ( ps.preserve_depth )
    {
      compute_required();
    }

    if ( ps.use_dont_cares )
      perform_rewriting_dc();
    else
      perform_rewriting();

    st.estimated_gain = _estimated_gain;
    st.candidates = _candidates;
  }

private:
  void perform_rewriting()
  {
    /* initialize the cut manager */
    cut_enumeration_stats cst;
    network_cuts_t cuts( ntk.size() + ( ntk.size() >> 1 ) );
    cut_manager_t cut_manager( ntk, ps.cut_enumeration_ps, cst, cuts );

    /* initialize cuts for constant nodes and PIs */
    cut_manager.init_cuts();

    auto& db = library.get_database();

    std::array<signal<Ntk>, num_vars> leaves;
    std::array<signal<Ntk>, num_vars> best_leaves;
    std::array<uint8_t, num_vars> permutation;
    signal<Ntk> best_signal;

    const auto size = ntk.size();
    ntk.foreach_gate( [&]( auto const& n, auto i ) {
      if ( ntk.fanout_size( n ) == 0u )
        return;

      int32_t best_gain = -1;
      uint32_t best_level = UINT32_MAX;
      bool best_phase = false;

      /* update level for node */
      if constexpr ( has_level_v<Ntk> )
      {
        if ( ps.preserve_depth )
        {
          best_level = ntk.level( n );
        }
      }

      cut_manager.clear_cuts( n );
      cut_manager.compute_cuts( n );

      uint32_t cut_index = 0;
      for ( auto& cut : cuts.cuts( ntk.node_to_index( n ) ) )
      {
        /* skip trivial cut */
        if ( ( cut->size() == 1 && *cut->begin() == ntk.node_to_index( n ) ) )
        {
          ++cut_index;
          continue;
        }

        /* Boolean matching */
        auto config = kitty::exact_npn_canonization( cuts.truth_table( *cut ) );
        auto tt_npn = std::get<0>( config );
        auto neg = std::get<1>( config );
        auto perm = std::get<2>( config );

        auto const structures = library.get_supergates( tt_npn );

        if ( structures == nullptr )
        {
          ++cut_index;
          continue;
        }

        uint32_t negation = 0;
        for ( auto j = 0u; j < num_vars; ++j )
        {
          permutation[perm[j]] = j;
          negation |= ( ( neg >> perm[j] ) & 1 ) << j;
        }

        /* save output negation to apply */
        bool phase = ( neg >> num_vars == 1 ) ? true : false;

        {
          auto j = 0u;
          for ( auto const leaf : *cut )
          {
            leaves[permutation[j++]] = ntk.make_signal( ntk.index_to_node( leaf ) );
          }

          while ( j < num_vars )
            leaves[permutation[j++]] = ntk.get_constant( false );
        }

        for ( auto j = 0u; j < num_vars; ++j )
        {
          if ( ( negation >> j ) & 1 )
          {
            leaves[j] = !leaves[j];
          }
        }

        {
          /* measure the MFFC contained in the cut */
          int32_t mffc_size = measure_mffc_deref( n, cut );

          for ( auto const& dag : *structures )
          {
            auto [nodes_added, level] = evaluate_entry( n, db.get_node( dag.root ), leaves );
            int32_t gain = mffc_size - nodes_added;

            /* discard if dag.root and n are the same */
            if ( ntk.node_to_index( n ) == db.value( db.get_node( dag.root ) ) >> 1 )
              continue;

            /* discard if no gain */
            if ( gain < 0 || ( !ps.allow_zero_gain && gain == 0 ) )
              continue;

            /* discard if level increases */
            if constexpr ( has_level_v<Ntk> )
            {
              if ( ps.preserve_depth && level > required[n] )
                continue;
            }

            if ( ( gain > best_gain ) || ( gain == best_gain && level < best_level ) )
            {
              ++_candidates;
              best_gain = gain;
              best_signal = dag.root;
              best_leaves = leaves;
              best_phase = phase;
              best_level = level;
            }

            if ( !ps.allow_multiple_structures )
              break;
          }

          /* restore contained MFFC */
          measure_mffc_ref( n, cut );
          ++cut_index;

          if ( cut->size() == 0 || ( cut->size() == 1 && *cut->begin() != ntk.node_to_index( n ) ) )
            break;
        }
      }

      if ( best_gain > 0 || ( ps.allow_zero_gain && best_gain == 0 ) )
      {
        /* replace node wth the new structure */
        topo_view topo{ db, best_signal };
        auto new_f = cleanup_dangling( topo, ntk, best_leaves.begin(), best_leaves.end() ).front();

        assert( n != ntk.get_node( new_f ) );

        _estimated_gain += best_gain;
        ntk.substitute_node_no_restrash( n, new_f ^ best_phase );

        if constexpr ( has_level_v<Ntk> )
        {
          /* propagate new required to leaves */
          if ( ps.preserve_depth )
          {
            propagate_required( ntk.get_node( new_f ), required[n] );
            assert( ntk.level( ntk.get_node( new_f ) ) <= required[n] );
            update_levels( ntk.get_node( new_f ) );
          }
        }

        clear_cuts_fanout_rec( cuts, cut_manager, ntk.get_node( new_f ) );
      }
    } );
  }

  void perform_rewriting_dc()
  {
    /* initialize the cut manager */
    cut_enumeration_stats cst;
    network_cuts_t cuts( ntk.size() + ( ntk.size() >> 1 ) );
    cut_manager_t cut_manager( ntk, ps.cut_enumeration_ps, cst, cuts );

    /* initialize cuts for constant nodes and PIs */
    cut_manager.init_cuts();

    auto& db = library.get_database();

    std::array<signal<Ntk>, num_vars> leaves;
    std::array<signal<Ntk>, num_vars> best_leaves;
    std::array<uint8_t, num_vars> permutation;
    signal<Ntk> best_signal;

    reconvergence_driven_cut_parameters rps;
    rps.max_leaves = ps.window_size;
    reconvergence_driven_cut_statistics rst;
    detail::reconvergence_driven_cut_impl<Ntk, false, has_level_v<Ntk>> reconv_cuts( ntk, rps, rst );
    unordered_node_map<kitty::static_truth_table<max_window_size>, Ntk> tts( ntk );

    color_view<Ntk> color_ntk{ ntk };
    std::array<uint32_t, num_vars> divisors;
    for ( uint32_t i = 0; i < num_vars; ++i )
    {
      divisors[i] = i;
    }

    const auto size = ntk.size();
    ntk.foreach_gate( [&]( auto const& n, auto i ) {
      if ( ntk.fanout_size( n ) == 0u )
        return;

      int32_t best_gain = -1;
      uint32_t best_level = UINT32_MAX;
      bool best_phase = false;

      /* update level for node */
      if constexpr ( has_level_v<Ntk> )
      {
        if ( ps.preserve_depth )
        {
          best_level = ntk.level( n );
        }
      }

      cut_manager.clear_cuts( n );
      cut_manager.compute_cuts( n );

      /* compute window */
      std::vector<node<Ntk>> roots = { n };
      auto const extended_leaves = reconv_cuts.run( roots ).first;
      std::vector<node<Ntk>> gates{ collect_nodes( color_ntk, extended_leaves, roots ) };
      window_view window_ntk{ color_ntk, extended_leaves, roots, gates };

      default_simulator<kitty::static_truth_table<max_window_size>> sim;
      tts.reset();
      simulate_nodes_with_node_map<kitty::static_truth_table<max_window_size>>( window_ntk, tts, sim );

      uint32_t cut_index = 0;
      for ( auto& cut : cuts.cuts( ntk.node_to_index( n ) ) )
      {
        /* skip trivial cut */
        if ( ( cut->size() == 1 && *cut->begin() == ntk.node_to_index( n ) ) )
        {
          ++cut_index;
          continue;
        }

        /* Boolean matching */
        auto config = kitty::exact_npn_canonization( cuts.truth_table( *cut ) );
        auto tt_npn = std::get<0>( config );
        auto neg = std::get<1>( config );
        auto perm = std::get<2>( config );

        kitty::static_truth_table<num_vars> care;

        bool containment = true;
        for ( auto const& l : *cut )
        {
          if ( color_ntk.color( ntk.index_to_node( l ) ) != color_ntk.current_color() )
          {
            containment = false;
            break;
          }
        }

        if ( containment )
        {
          /* compute care set */
          for ( auto i = 0u; i < ( 1u << window_ntk.num_pis() ); ++i )
          {
            uint32_t entry{ 0u };
            auto j = 0u;
            for ( auto const& l : *cut )
            {
              entry |= kitty::get_bit( tts[l], i ) << j;
              ++j;
            }
            kitty::set_bit( care, entry );
          }
        }
        else
        {
          /* completely specified */
          care = ~care;
        }

        auto const dc_npn = apply_npn_transformation( ~care, neg & ~( 1 << num_vars ), perm );
        auto const structures = library.get_supergates( tt_npn, dc_npn, neg, perm );

        if ( structures == nullptr )
        {
          ++cut_index;
          continue;
        }

        uint32_t negation = 0;
        for ( auto j = 0u; j < num_vars; ++j )
        {
          permutation[perm[j]] = j;
          negation |= ( ( neg >> perm[j] ) & 1 ) << j;
        }

        /* save output negation to apply */
        bool phase = ( neg >> num_vars == 1 ) ? true : false;

        {
          auto j = 0u;
          for ( auto const leaf : *cut )
          {
            leaves[permutation[j++]] = ntk.make_signal( ntk.index_to_node( leaf ) );
          }

          while ( j < num_vars )
            leaves[permutation[j++]] = ntk.get_constant( false );
        }

        for ( auto j = 0u; j < num_vars; ++j )
        {
          if ( ( negation >> j ) & 1 )
          {
            leaves[j] = !leaves[j];
          }
        }

        {
          /* measure the MFFC contained in the cut */
          int32_t mffc_size = measure_mffc_deref( n, cut );

          for ( auto const& dag : *structures )
          {
            auto [nodes_added, level] = evaluate_entry( n, db.get_node( dag.root ), leaves );
            int32_t gain = mffc_size - nodes_added;

            /* discard if dag.root and n are the same */
            if ( ntk.node_to_index( n ) == db.value( db.get_node( dag.root ) ) >> 1 )
              continue;

            /* discard if no gain */
            if ( gain < 0 || ( !ps.allow_zero_gain && gain == 0 ) )
              continue;

            /* discard if level increases */
            if constexpr ( has_level_v<Ntk> )
            {
              if ( ps.preserve_depth && level > required[n] )
                continue;
            }

            if ( ( gain > best_gain ) || ( gain == best_gain && level < best_level ) )
            {
              ++_candidates;
              best_gain = gain;
              best_signal = dag.root;
              best_leaves = leaves;
              best_phase = phase;
              best_level = level;
            }

            if ( !ps.allow_multiple_structures )
              break;
          }

          /* restore contained MFFC */
          measure_mffc_ref( n, cut );
          ++cut_index;

          if ( cut->size() == 0 || ( cut->size() == 1 && *cut->begin() != ntk.node_to_index( n ) ) )
            break;
        }
      }

      if ( best_gain > 0 || ( ps.allow_zero_gain && best_gain == 0 ) )
      {
        /* replace node wth the new structure */
        topo_view topo{ db, best_signal };
        auto new_f = cleanup_dangling( topo, ntk, best_leaves.begin(), best_leaves.end() ).front();

        assert( n != ntk.get_node( new_f ) );

        _estimated_gain += best_gain;
        ntk.substitute_node_no_restrash( n, new_f ^ best_phase );

        if constexpr ( has_level_v<Ntk> )
        {
          /* propagate new required to leaves */
          if ( ps.preserve_depth )
          {
            propagate_required( ntk.get_node( new_f ), required[n] );
            assert( ntk.level( ntk.get_node( new_f ) ) <= required[n] );
            update_levels( ntk.get_node( new_f ) );
          }
        }

        clear_cuts_fanout_rec( cuts, cut_manager, ntk.get_node( new_f ) );
      }
    } );
  }

  int32_t measure_mffc_ref( node<Ntk> const& n, cut_t const* cut )
  {
    /* reference cut leaves */
    for ( auto leaf : *cut )
    {
      ntk.incr_fanout_size( ntk.index_to_node( leaf ) );
    }

    int32_t mffc_size = static_cast<int32_t>( recursive_ref( n ) );

    /* dereference leaves */
    for ( auto leaf : *cut )
    {
      ntk.decr_fanout_size( ntk.index_to_node( leaf ) );
    }

    return mffc_size;
  }

  int32_t measure_mffc_deref( node<Ntk> const& n, cut_t const* cut )
  {
    /* reference cut leaves */
    for ( auto leaf : *cut )
    {
      ntk.incr_fanout_size( ntk.index_to_node( leaf ) );
    }

    int32_t mffc_size = static_cast<int32_t>( recursive_deref( n ) );

    /* dereference leaves */
    for ( auto leaf : *cut )
    {
      ntk.decr_fanout_size( ntk.index_to_node( leaf ) );
    }

    return mffc_size;
  }

  uint32_t recursive_deref( node<Ntk> const& n )
  {
    /* terminate? */
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      return 0;

    /* recursively collect nodes */
    uint32_t value{ cost_fn( ntk, n ) };
    ntk.foreach_fanin( n, [&]( auto const& s ) {
      if ( ntk.decr_fanout_size( ntk.get_node( s ) ) == 0 )
      {
        value += recursive_deref( ntk.get_node( s ) );
      }
    } );
    return value;
  }

  uint32_t recursive_ref( node<Ntk> const& n )
  {
    /* terminate? */
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      return 0;

    /* recursively collect nodes */
    uint32_t value{ cost_fn( ntk, n ) };
    ntk.foreach_fanin( n, [&]( auto const& s ) {
      if ( ntk.incr_fanout_size( ntk.get_node( s ) ) == 0 )
      {
        value += recursive_ref( ntk.get_node( s ) );
      }
    } );
    return value;
  }

  inline std::pair<int32_t, uint32_t> evaluate_entry( node<Ntk> const& current_root, node<Ntk> const& n, std::array<signal<Ntk>, num_vars> const& leaves )
  {
    auto& db = library.get_database();
    db.incr_trav_id();

    unordered_node_map<uint32_t, std::decay_t<decltype( db )>> levels( db );
    return evaluate_entry_rec( current_root, n, leaves, levels );
  }

  template<typename Container>
  std::pair<int32_t, uint32_t> evaluate_entry_rec( node<Ntk> const& current_root, node<Ntk> const& n, std::array<signal<Ntk>, num_vars> const& leaves, Container& levels )
  {
    auto& db = library.get_database();
    if ( db.is_pi( n ) || db.is_constant( n ) )
      return { 0, 0 };
    if ( db.visited( n ) == db.trav_id() )
    {
      if constexpr ( has_level_v<Ntk> )
      {
        assert( levels.has( n ) );
        return { 0, levels[n] };
      }
      return { 0, 0 };
    }

    db.set_visited( n, db.trav_id() );

    int32_t area = 0;
    uint32_t level = 0;
    bool hashed = true;

    std::array<signal<Ntk>, Ntk::max_fanin_size> node_data;
    db.foreach_fanin( n, [&]( auto const& f, auto i ) {
      node<Ntk> g = db.get_node( f );
      if ( db.is_constant( g ) )
      {
        node_data[i] = f; /* ntk.get_costant( db.is_complemented( f ) ) */
        return;
      }
      if ( db.is_pi( g ) )
      {
        node_data[i] = leaves[db.node_to_index( g ) - 1] ^ db.is_complemented( f );
        if constexpr ( has_level_v<Ntk> )
        {
          level = std::max( level, ntk.level( ntk.get_node( leaves[db.node_to_index( g ) - 1] ) ) );
        }
        return;
      }

      auto [area_rec, level_rec] = evaluate_entry_rec( current_root, g, leaves, levels );
      area += area_rec;
      level = std::max( level, level_rec );

      /* check value */
      if ( db.value( g ) < UINT32_MAX )
      {
        signal<Ntk> s;
        s.data = static_cast<uint64_t>( db.value( g ) );
        node_data[i] = s ^ db.is_complemented( f );
      }
      else
      {
        hashed = false;
      }
    } );

    if constexpr ( has_level_v<Ntk> )
    {
      levels[n] = level + 1;
    }

    if ( hashed )
    {
      /* try hash */
      /* AIG, XAG, MIG, and XMG are supported now */
      std::optional<signal<Ntk>> val;
      do
      {
        /* XAG */
        if constexpr ( has_has_and_v<Ntk> && has_has_xor_v<Ntk> )
        {
          if ( db.is_and( n ) )
            val = ntk.has_and( node_data[0], node_data[1] );
          else
            val = ntk.has_xor( node_data[0], node_data[1] );
          break;
        }

        /* AIG */
        if constexpr ( has_has_and_v<Ntk> )
        {
          val = ntk.has_and( node_data[0], node_data[1] );
          break;
        }

        /* XMG */
        if constexpr ( has_has_maj_v<Ntk> && has_has_xor3_v<Ntk> )
        {
          if ( db.is_maj( n ) )
            val = ntk.has_maj( node_data[0], node_data[1], node_data[2] );
          else
            val = ntk.has_xor3( node_data[0], node_data[1], node_data[2] );
          break;
        }

        /* MAJ */
        if constexpr ( has_has_maj_v<Ntk> )
        {
          val = ntk.has_maj( node_data[0], node_data[1], node_data[2] );
          break;
        }
        std::cerr << "[e] Only AIGs, XAGs, MAJs, and XMGs are currently supported \n";
      } while ( false );

      if ( val.has_value() )
      {
        /* bad condition (current root is contained in the DAG): return a very high cost */
        if ( db.get_node( *val ) == current_root )
          return { UINT32_MAX / 2, level + 1 };

        /* annotate hashing info */
        db.set_value( n, val->data );
        return { area + ( ntk.fanout_size( ntk.get_node( *val ) ) > 0 ? 0 : cost_fn( ntk, n ) ), level + 1 };
      }
    }

    db.set_value( n, UINT32_MAX );
    return { area + cost_fn( ntk, n ), level + 1 };
  }

  void compute_required()
  {
    if constexpr ( has_level_v<Ntk> )
    {
      ntk.foreach_po( [&]( auto const& f ) {
        required[f] = ntk.depth();
      } );

      topo_view<Ntk, false> topo{ ntk };

      topo.foreach_node_reverse( [&]( auto const& n ) {
        if ( ntk.is_constant( n ) || ntk.is_pi( n ) || required[n] == UINT32_MAX )
          return;

        ntk.foreach_fanin( n, [&]( auto const& f ) {
          required[f] = std::min( required[f], ntk.compute_fanin_required( n, f, required[n] ) );
        } );
      } );
    }
  }

  void propagate_required_rec( node<Ntk> const& n, uint32_t req )
  {
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      return;

    if ( required[n] <= req )
      return;

    required[n] = req;

    ntk.foreach_fanin( n, [&]( auto const& f ) {
      auto const g = ntk.get_node( f );
      propagate_required_rec( g, ntk.compute_fanin_required( n, f, req ) );
    } );
  }

  void propagate_required( node<Ntk> const& n, uint32_t req )
  {
    required.resize( UINT32_MAX );
    propagate_required_rec( n, req );
  }

  void clear_cuts_fanout_rec( network_cuts_t& cuts, cut_manager_t& cut_manager, node<Ntk> const& n )
  {
    ntk.foreach_fanout( n, [&]( auto const& g ) {
      auto const index = ntk.node_to_index( g );
      if ( cuts.cuts( index ).size() > 0 )
      {
        cut_manager.clear_cuts( g );
        clear_cuts_fanout_rec( cuts, cut_manager, g );
      }
    } );
  }

private:
  void update_levels( node<Ntk> const& n )
  {
    /*
    ntk.foreach_fanout( n, [&]( const auto& p ) {
      update_levels_naive( p );
    } );
    return;
    */

    ntk.incr_trav_id();
    auto const mark = ntk.trav_id();
    std::vector<node<Ntk>> deferred;

    ntk.foreach_fanout( n, [&]( const auto& p ) {
      update_levels_fast( p, mark, deferred );
    } );

    if ( deferred.empty() )
      return;

    ntk.incr_trav_id();
    auto const mark2 = ntk.trav_id();
    std::vector<node<Ntk>> sinks;

    for ( auto const& p : deferred )
      mark_tfo( p, mark2, sinks );

    ntk.incr_trav_id();
    auto const mark3 = ntk.trav_id();
    std::vector<node<Ntk>> tfi_topo;

    for ( auto const& p : sinks )
      collect_marked_tfi_topo( p, mark2, mark3, tfi_topo );

    sinks.clear();

    ntk.incr_trav_id();
    auto const mark4 = ntk.trav_id();
    for ( auto const& p : deferred )
      ntk.set_visited( p, mark4 );

    for ( auto const& p : tfi_topo )
    {
      if ( ntk.visited( p ) != mark4 )
        continue;

      if ( update_level( p ) )
      {
        ntk.foreach_fanout( p, [&]( const auto& q ) {
          ntk.set_visited( q, mark4 );
        } );
      }
    }
  }

  /*
  void update_levels_naive( node<Ntk> const& n )
  {
    if ( update_level( n ) )
    {
      ntk.foreach_fanout( n, [&]( auto const& fo ) {
        update_levels_naive( fo );
      } );
    }
  }
  */

  void update_levels_fast( node<Ntk> const& n, uint32_t mark, std::vector<node<Ntk>>& deferred )
  {
    if ( ntk.visited( n ) == mark )
    {
      if constexpr ( has_level_v<Ntk> )
      {
        if ( ntk.level( n ) != ntk.compute_level( n ) )
          deferred.push_back( n );
      }
      return;
    }

    ntk.set_visited( n, mark );

    if ( update_level( n ) )
    {
      ntk.foreach_fanout( n, [&]( auto const& fo ) {
        update_levels_fast( fo, mark, deferred );
      } );
    }
  }

  bool update_level( node<Ntk> const& n )
  {
    if constexpr ( has_level_v<Ntk> )
    {
      uint32_t level = ntk.compute_level( n );
      if ( ntk.level( n ) != level )
      {
        ntk.set_level( n, level );
        return true;
      }
    }
    return false;
  }

  void mark_tfo( node<Ntk> const& n, uint32_t mark, std::vector<node<Ntk>>& sinks )
  {
    if ( ntk.visited( n ) == mark )
      return;

    ntk.set_visited( n, mark );

    bool f = false;
    ntk.foreach_fanout( n, [&]( auto const& fo ) {
      mark_tfo( fo, mark, sinks );
      f = true;
    } );

    if ( !f )
      sinks.push_back( n );
  }

  void collect_marked_tfi_topo( node<Ntk> const& n, uint32_t mark, uint32_t visited, std::vector<node<Ntk>>& tfi_topo )
  {
    if ( ntk.visited( n ) == visited )
      return;

    if ( ntk.visited( n ) != mark )
      return;

    ntk.set_visited( n, visited );

    ntk.foreach_fanin( n, [&]( auto const& f ) {
      auto const p = ntk.get_node( f );
      collect_marked_tfi_topo( p, mark, visited, tfi_topo );
    } );

    tfi_topo.push_back( n );
  }

private:
  Ntk& ntk;
  Library&& library;
  rewrite_params const& ps;
  rewrite_stats& st;
  NodeCostFn cost_fn;

  node_map<uint32_t, Ntk> required;

  uint32_t _candidates{ 0 };
  uint32_t _estimated_gain{ 0 };
};

} /* namespace detail */

/*! \brief Boolean rewrite.
 *
 * This algorithm rewrites enumerated cuts using new network structures from a database.
 * The algorithm performs changes in-place and keeps the substituted structures dangling
 * in the network.
 *
 * **Required network functions:**
 * - `get_node`
 * - `size`
 * - `make_signal`
 * - `foreach_gate`
 * - `substitute_node`
 * - `clear_visited`
 * - `clear_values`
 * - `fanout_size`
 * - `set_value`
 * - `foreach_node`
 *
 * \param ntk Input network (will be changed in-place)
 * \param library Exact library containing pre-computed structures
 * \param ps Rewrite params
 * \param pst Rewrite statistics
 * \param cost_fn Node cost function (a functor with signature `uint32_t(Ntk const&, node<Ntk> const&)`)
 */
template<class Ntk, class Library, class NodeCostFn = unit_cost<Ntk>>
void rewrite( Ntk& ntk, Library&& library, rewrite_params const& ps = {}, rewrite_stats* pst = nullptr, NodeCostFn const& cost_fn = {} )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
  static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the substitute_node method" );
  static_assert( has_clear_visited_v<Ntk>, "Ntk does not implement the clear_visited method" );
  static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );

  rewrite_stats st;

  if ( ps.preserve_depth || ps.use_dont_cares )
  {
    using depth_view_t = depth_view<Ntk, NodeCostFn>;
    depth_view_t depth_ntk{ ntk };
    using fanout_view_t = fanout_view<depth_view_t>;
    fanout_view_t fanout_view{ depth_ntk };

    detail::rewrite_impl<fanout_view_t, Library, NodeCostFn> p( fanout_view, library, ps, st, cost_fn );
    p.run();
  }
  else
  {
    using fanout_view_t = fanout_view<Ntk>;
    fanout_view_t fanout_view{ ntk };

    detail::rewrite_impl<fanout_view_t, Library, NodeCostFn> p( fanout_view, library, ps, st, cost_fn );
    p.run();
  }

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }

  ntk = cleanup_dangling( ntk );
}

} /* namespace mockturtle */