/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
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
  \file sim_resub.hpp
  \brief Resubstitution

  \author Siang-Yun Lee
*/

#pragma once

#include <mockturtle/networks/aig.hpp>
//#include <mockturtle/algorithms/resubstitution.hpp>
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/depth_view.hpp"
#include "../views/fanout_view2.hpp"
#include <mockturtle/algorithms/simulation.hpp>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>
#include "../utils/node_map.hpp"
#include "cnf.hpp"
#include "cleanup.hpp"
#include <percy/solvers/bsat2.hpp>
#include "reconv_cut2.hpp"

namespace mockturtle
{

struct simresub_params
{
  /*! \brief Number of initial simulation patterns = 2^num_pattern_base. */
  uint32_t num_pattern_base{7};

  /*! \brief Number of reserved blocks(64 bits) for generated simulation patterns. */
  uint32_t num_reserved_blocks{1};

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{150};

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

  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{8};

  std::default_random_engine::result_type random_seed{0};
};

struct simresub_stats
{
  stopwatch<>::duration time_total{0};

  /* total time for initial simulation and complete pattern generation */
  stopwatch<>::duration time_simgen{0};

  /* time for simulations */
  stopwatch<>::duration time_sim{0};

  /* time for SAT solving */
  stopwatch<>::duration time_sat{0};

  /* time for finding substitutions */
  stopwatch<>::duration time_eval{0};

  /* time for MFFC computation */
  stopwatch<>::duration time_mffc{0};

  /* time for divisor collection */
  stopwatch<>::duration time_divs{0};

  /* time for checking implications (containment) */
  stopwatch<>::duration time_collect_unate_divisors{0};

  /* time for doing substitution */
  stopwatch<>::duration time_substitute{0};

  /* time & number of r == d (equal) node substitutions */
  stopwatch<>::duration time_resub0{0};
  uint32_t num_div0_accepts{0};

  /* time & number of r == d1 &| d2 node substitutions */
  stopwatch<>::duration time_resub1{0};
  uint32_t num_div1_accepts{0};

  /*! \brief Initial network size (before resubstitution) */
  uint64_t initial_size{0};

  uint32_t num_constant{0};
  uint32_t num_generated_patterns{0};
  uint32_t num_cex{0};

  /*! \brief Total number of gain  */
  uint64_t estimated_gain{0};

  uint64_t num_total_divisors{0};

  void report() const
  {
    //std::cout << "[i] kernel: default_resub_functor\n";
    //std::cout << fmt::format( "[i]     constant-resub {:6d}                                   ({:>5.2f} secs)\n",
    //                          num_const_accepts, to_seconds( time_resubC ) );
    //std::cout << fmt::format( "[i]            0-resub {:6d}                                   ({:>5.2f} secs)\n",
    //                          num_div0_accepts, to_seconds( time_resub0 ) );
    //std::cout << fmt::format( "[i]            total   {:6d}\n",
    //                          (num_const_accepts + num_div0_accepts) );
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
    (void)count2;
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

class partial_simulator
{
public:
  partial_simulator() = delete;
  partial_simulator( unsigned num_pis, unsigned num_pattern_base, unsigned num_reserved_blocks, std::default_random_engine::result_type seed = 0 ) : num_pattern_base( num_pattern_base ), added_bits( 63 )
  {
    patterns.resize(num_pis);
    for ( auto i = 0u; i < num_pis; ++i )
    {
      kitty::dynamic_truth_table tt( num_pattern_base );
      kitty::create_random( tt, seed+i );
      patterns.at(i) = tt._bits;
      /* clear the last `num_reserved_blocks` blocks */
      patterns.at(i).resize( tt.num_blocks() - num_reserved_blocks );
      patterns.at(i).resize( tt.num_blocks(), 0u );
    }
    zero = kitty::dynamic_truth_table( num_pattern_base );
    current_block = zero.num_blocks() - num_reserved_blocks - 1;
  }

  kitty::dynamic_truth_table compute_constant( bool value ) const
  {
    return value ? ~zero : zero;
  }

  kitty::dynamic_truth_table compute_pi( uint32_t index ) const
  {
    kitty::dynamic_truth_table tt( num_pattern_base );
    tt._bits = patterns.at( index );
    return tt;
  }

  kitty::dynamic_truth_table compute_not( kitty::dynamic_truth_table const& value ) const
  {
    return ~value;
  }

  bool add_pattern( std::vector<bool>& pattern )
  {
    if ( ++added_bits >= 64 )
    {
      added_bits = 0;

      if ( ++current_block >= zero.num_blocks() ) // if number of blocks(64) of test patterns is not enough
      {
        return true;
      }
    }

    for ( auto i = 0u; i < pattern.size(); ++i )
    {
      if ( pattern.at(i) )
        patterns.at(i).at(current_block) |= (uint64_t)1u << added_bits;
    }

    return false;
  }

private:
  std::vector<std::vector<uint64_t>> patterns;
  kitty::dynamic_truth_table zero;
  unsigned num_pattern_base;
  unsigned added_bits;
  unsigned current_block;
};

template<class NtkBase, class Ntk>
class simresub_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = unordered_node_map<kitty::dynamic_truth_table, NtkBase>;

  struct unate_divisors
  {
    using signal = typename Ntk::signal;

    std::vector<signal> positive_divisors;
    std::vector<signal> negative_divisors;
    std::vector<signal> next_candidates;

    void clear()
    {
      positive_divisors.clear();
      negative_divisors.clear();
      next_candidates.clear();
    }
  };

  struct binate_divisors
  {
    using signal = typename Ntk::signal;

    std::vector<signal> positive_divisors0;
    std::vector<signal> positive_divisors1;
    std::vector<signal> negative_divisors0;
    std::vector<signal> negative_divisors1;

    void clear()
    {
      positive_divisors0.clear();
      positive_divisors1.clear();
      negative_divisors0.clear();
      negative_divisors1.clear();
    }
  };

  explicit simresub_impl( NtkBase& ntkbase, Ntk& ntk, simresub_params const& ps, simresub_stats& st )
    : ntkbase( ntkbase ), ntk( ntk ), ps( ps ), st( st ), 
      tts( ntkbase ), phase( ntkbase, false ), sim( ntk.num_pis(), ps.num_pattern_base, ps.num_reserved_blocks, ps.random_seed ), literals( node_literals( ntkbase ) )
  {
    st.initial_size = ntk.num_gates(); 

    auto const update_level_of_new_node = [&]( const auto& n ){
      ntk.resize_levels();
      update_node_level( n );
    };
    
    auto const update_level_of_existing_node = [&]( node const& n, const auto& old_children ){
      (void)old_children;
      update_node_level( n );
    };
    
    auto const update_level_of_deleted_node = [&]( const auto& n ){
      /* update fanout */
      ntk.set_level( n, -1 );
    };
    
    ntk._events->on_add.emplace_back( update_level_of_new_node );
    
    ntk._events->on_modified.emplace_back( update_level_of_existing_node );
    
    ntk._events->on_delete.emplace_back( update_level_of_deleted_node );
  }

  void run()
  {
    stopwatch t( st.time_total );

    /* start the managers */
    progress_bar pbar{ntk.size(), "resub |{0}| node = {1:>4}   cand = {2:>4}   est. gain = {3:>5}", ps.progress};
    generate_cnf<NtkBase>( ntkbase, [&]( auto const& clause ) {
      solver.add_clause( clause );
    }, literals );

    /* simulate all nodes and generate complete test patterns, finding out and replace constant nodes at the same time */
    call_with_stopwatch( st.time_simgen, [&]() {
      simulate_generate();
    });

    std::vector<node> PIs( ntk.num_pis() );
    ntk.foreach_pi( [&]( auto const& n, auto i ){ PIs.at(i) = n; });

    /* iterate through all nodes and try to replace it */
    auto const size = ntk.num_gates();
    ntk.foreach_gate( [&]( auto const& n, auto i ){
        if ( i >= size || fStop )
          return false; /* terminate */

        pbar( i, i, candidates, st.estimated_gain );

        if ( ntk.is_dead( n ) )
          return true; /* next */

        /* skip nodes with many fanouts */
        if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
          return true; /* next */

        /* use all the PIs as the cut */
        //auto const leaves = PIs;
        cut_manager<Ntk> mgr( ps.max_pis );
        auto const leaves = reconv_driven_cut( mgr, ntk, n );
        
        /* evaluate this cut */
        auto const g = call_with_stopwatch( st.time_eval, [&]() {
            return evaluate( n, leaves );
          });
        if ( !g ) return true; /* next */
        
        /* update progress bar */
        candidates++;
        st.estimated_gain += last_gain;
        /* update network */
        call_with_stopwatch( st.time_substitute, [&]() {
            ntk.substitute_node( n, *g );
          });
        /* re-simulate */
        call_with_stopwatch( st.time_sim, [&]() {
            simulate_nodes<kitty::dynamic_truth_table, NtkBase, partial_simulator>( ntk, tts, sim );
            normalizeTT();
          });
        return true; /* next */
      });
  }

private:
  void update_node_level( node const& n, bool top_most = true )
  {
    uint32_t curr_level = ntk.level( n );

    uint32_t max_level = 0;
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
    int32_t limit = ps.max_divisors - ps.max_pis - ( uint32_t( divs.size() ) + 1 - uint32_t( leaves.size() ) + uint32_t( temp.size() ) );

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
    num_divs = uint32_t( divs.size() );

    /* add the nodes in the MFFC */
    for ( const auto& t : temp )
    {
      divs.emplace_back( t );
    }

    assert( root == divs.at( divs.size()-1u ) );
    assert( divs.size() - leaves.size() <= ps.max_divisors - ps.max_pis );

    return true;
  }

  void simulate_generate()
  {
    call_with_stopwatch( st.time_sim, [&]() {
      simulate_nodes<kitty::dynamic_truth_table, NtkBase, partial_simulator>( ntk, tts, sim );
    });

    std::vector<pabc::lit> assumptions( 1 );
    kitty::dynamic_truth_table zero = sim.compute_constant(false);
    //unordered_node_map<bool, NtkBase> constant_gates( ntkbase ); /* for checking completeness */
  
    ntk.foreach_gate( [&]( auto const& n ) 
    {
      if ( (tts[n] == zero) || (tts[n] == ~zero) )
      {
        assumptions[0] = lit_not_cond( literals[n], (tts[n] == ~zero) );
      
        const auto res = call_with_stopwatch( st.time_sat, [&]() {
          return solver.solve( &assumptions[0], &assumptions[0] + 1, 0 );
        });
        
        if ( res == percy::synth_result::success )
        {
          //std::cout << "SAT: add pattern. ";
          std::vector<bool> pattern;
          for ( auto i = 1u; i <= ntk.num_pis(); ++i )
            pattern.push_back(solver.var_value( i ));
          if ( sim.add_pattern(pattern) )
          {
            fStop = true;
            return false; /* number of generated patterns exceeds limit, stop generating */
          }
          ++st.num_generated_patterns;

          /* re-simulate */
          call_with_stopwatch( st.time_sim, [&]() {
            simulate_nodes<kitty::dynamic_truth_table, NtkBase, partial_simulator>( ntk, tts, sim );
          });
        }
        else
        {
          //std::cout << "UNSAT: this is a constant node. (" << n << ")" << std::endl;
          ++st.num_constant;
          auto g = ntk.get_constant( tts[n] == ~zero );
          /* update network */
          call_with_stopwatch( st.time_substitute, [&]() {
            ntk.substitute_node( n, g );
          });

        }
      }

      return true; /* next gate */
    } );

    normalizeTT();
  }

  std::optional<signal> evaluate( node const& root, std::vector<node> const &leaves )
  {
    uint32_t const required = std::numeric_limits<uint32_t>::max();

    last_gain = 0;

    /* collect the MFFC */
    int32_t num_mffc = call_with_stopwatch( st.time_mffc, [&]() {
        node_mffc_inside collector( ntk );
        auto num_mffc = collector.run( root, leaves, temp );
        assert( num_mffc > 0 );
        return num_mffc;
      });

    /* collect the divisor nodes in the cut */
    bool div_comp_success = call_with_stopwatch( st.time_divs, [&]() {
        return collect_divisors( root, leaves, required );
      });

    if ( !div_comp_success )
      return std::nullopt;

    /* update statistics */
    st.num_total_divisors += num_divs;

    /* consider equal nodes */
    auto g = call_with_stopwatch( st.time_resub0, [&]() {
        return resub_div0( root, required );
      } );
    if ( g )
    {
      ++st.num_div0_accepts;
      last_gain = num_mffc;
      return g; /* accepted resub */
    }

    if ( ps.max_inserts == 0 || num_mffc == 1 )
      return std::nullopt;

    /* collect level one divisors */
    call_with_stopwatch( st.time_collect_unate_divisors, [&]() {
        collect_unate_divisors( root, required );
      });

    /* consider equal nodes */
    g = call_with_stopwatch( st.time_resub1, [&]() {
        return resub_div1( root, required );
      } );
    if ( g )
    {
      ++st.num_div1_accepts;
      last_gain = num_mffc - 1;
      return g; /* accepted resub */
    }

    if ( ps.max_inserts == 1 || num_mffc == 2 )
      return std::nullopt;


    return std::nullopt;
  }

  void normalizeTT()
  {
    phase.resize();
    ntk.foreach_gate( [&]( auto const& n ){
      if ( kitty::get_bit( tts[n], 0 ) )
      {
        phase[n] = true;
        tts[n] = ~tts[n];
      }
    });
  }

  std::optional<signal> resub_div0( node const& root, uint32_t required ) 
  {
    (void)required;
    auto const tt = tts[root];

    //for ( auto i = 0u; i < num_divs; ++i )
    for ( int i = num_divs-1; i >= 0; --i )
    {
      auto const d = divs.at( i );
      if ( tt == tts[d] )
      {
        solver.add_var();
        auto nlit = make_lit( solver.nr_vars()-1 );
        solver.add_clause( {literals[root], literals[d], nlit} );
        solver.add_clause( {literals[root], lit_not( literals[d] ), lit_not( nlit )} );
        solver.add_clause( {lit_not( literals[root] ), literals[d], lit_not( nlit )} );
        solver.add_clause( {lit_not( literals[root] ), lit_not( literals[d] ), nlit} );
        std::vector<pabc::lit> assumptions( 1, lit_not_cond( nlit, phase[root] == phase[d] ) );
      
        const auto res = call_with_stopwatch( st.time_sat, [&]() {
          return solver.solve( &assumptions[0], &assumptions[0] + 1, 0 );
        });
        
        if ( res == percy::synth_result::success && !fStop ) /* CEX found */
        {
          std::vector<bool> pattern;
          for ( auto j = 1u; j <= ntk.num_pis(); ++j )
            pattern.push_back(solver.var_value( j ));
          if ( sim.add_pattern(pattern) )
          {
            fStop = true;
            return std::nullopt; /* number of generated patterns exceeds limit */
          }
          ++st.num_cex;

          /* re-simulate */
          call_with_stopwatch( st.time_sim, [&]() {
            simulate_nodes<kitty::dynamic_truth_table, NtkBase, partial_simulator>( ntk, tts, sim );
            normalizeTT();
          });
        }
        else /* proved equal */
          return ( phase[root] == phase[d] )? ntk.make_signal( d ): !ntk.make_signal( d );
      }
    }

    return std::nullopt;
  }

  void collect_unate_divisors( node const& root, uint32_t required )
  {
    udivs.clear();

    auto const& tt = tts[root];
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const d = divs.at( i );

      if ( ntk.level( d ) > required - 1 )
        continue;

      auto const& tt_d = tts[d];

      /* check positive containment */
      if ( kitty::implies( tt_d, tt ) )
      {
        udivs.positive_divisors.emplace_back( ntk.make_signal( d ) );
        continue;
      }

      /* check negative containment */
      if ( kitty::implies( tt, tt_d ) )
      {
        udivs.negative_divisors.emplace_back( ntk.make_signal( d ) );
        continue;
      }

      udivs.next_candidates.emplace_back( ntk.make_signal( d ) );
    }
  }

  std::optional<signal> resub_div1( node const& root, uint32_t required )
  {
    (void)required;
    auto const& tt = tts[root];

    /* check for positive unate divisors */
    for ( auto i = 0u; i < udivs.positive_divisors.size(); ++i )
    {
      auto const& s0 = udivs.positive_divisors.at( i );

      for ( auto j = i + 1; j < udivs.positive_divisors.size(); ++j )
      {
        auto const& s1 = udivs.positive_divisors.at( j );

        auto const& tt_s0 = tts[s0];
        auto const& tt_s1 = tts[s1];

        if ( ( tt_s0 | tt_s1 ) == tt )
        {
          auto l_r = lit_not_cond( literals[root], phase[root] );
          auto l_s0 = lit_not_cond( literals[ntk.get_node(s0)], phase[ntk.get_node(s0)]);
          auto l_s1 = lit_not_cond( literals[ntk.get_node(s1)], phase[ntk.get_node(s1)]);

          solver.add_var();
          auto nlit = make_lit( solver.nr_vars()-1 );
          solver.add_clause( {l_r, l_s0, l_s1, nlit} );
          solver.add_clause( {l_r, lit_not( l_s0 ), lit_not( nlit )} );
          solver.add_clause( {l_r, lit_not( l_s1 ), lit_not( nlit )} );
          solver.add_clause( {lit_not( l_r ), l_s0, l_s1, lit_not( nlit )} );
          solver.add_clause( {lit_not( l_r ), lit_not( l_s0 ), nlit} );
          solver.add_clause( {lit_not( l_r ), lit_not( l_s1 ), nlit} );
          std::vector<pabc::lit> assumptions( 1, lit_not( nlit ) );
        
          const auto res = call_with_stopwatch( st.time_sat, [&]() {
            return solver.solve( &assumptions[0], &assumptions[0] + 1, 0 );
          });
          
          if ( res == percy::synth_result::success && !fStop ) /* CEX found */
          {
            std::vector<bool> pattern;
            for ( auto j = 1u; j <= ntk.num_pis(); ++j )
              pattern.push_back(solver.var_value( j ));
            if ( sim.add_pattern(pattern) )
            {
              fStop = true;
              return std::nullopt; /* number of generated patterns exceeds limit */
            }
            ++st.num_cex;
  
            /* re-simulate */
            call_with_stopwatch( st.time_sim, [&]() {
              simulate_nodes<kitty::dynamic_truth_table, NtkBase, partial_simulator>( ntk, tts, sim );
              normalizeTT();
            });
          }
          else /* proved substitution */
          {
            auto g = phase[root]? !ntk.create_or( phase[s0]? !s0: s0, phase[s1]? !s1: s1 ) :ntk.create_or( phase[s0]? !s0: s0, phase[s1]? !s1: s1 );
            /* update CNF */
            literals.resize();
            solver.add_var();
            literals[g] = make_lit( solver.nr_vars()-1 );
            solver.add_clause( {lit_not( l_s0 ), literals[g]} );
            solver.add_clause( {lit_not( l_s1 ), literals[g]} );
            solver.add_clause( {l_s0, l_s1, lit_not( literals[g] )} );
            return g;
          }
        }
      }
    }

    /* check for negative unate divisors */
    for ( auto i = 0u; i < udivs.negative_divisors.size(); ++i )
    {
      auto const& s0 = udivs.negative_divisors.at( i );

      for ( auto j = i + 1; j < udivs.negative_divisors.size(); ++j )
      {
        auto const& s1 = udivs.negative_divisors.at( j );

        auto const& tt_s0 = tts[s0];
        auto const& tt_s1 = tts[s1];

        if ( ( tt_s0 & tt_s1 ) == tt )
        {
          auto l_r = lit_not_cond( literals[root], phase[root] );
          auto l_s0 = lit_not_cond( literals[ntk.get_node(s0)], phase[ntk.get_node(s0)]);
          auto l_s1 = lit_not_cond( literals[ntk.get_node(s1)], phase[ntk.get_node(s1)]);

          solver.add_var();
          auto nlit = make_lit( solver.nr_vars()-1 );
          solver.add_clause( {l_r, l_s0, nlit} );
          solver.add_clause( {l_r, l_s1, nlit} );
          solver.add_clause( {l_r, lit_not( l_s0 ), lit_not( l_s1 ), lit_not( nlit )} );
          solver.add_clause( {lit_not( l_r ), l_s0, lit_not( nlit )} );
          solver.add_clause( {lit_not( l_r ), l_s1, lit_not( nlit )} );
          solver.add_clause( {lit_not( l_r ), lit_not( l_s0 ), lit_not( l_s1 ), nlit} );
          std::vector<pabc::lit> assumptions( 1, lit_not( nlit ) );
        
          const auto res = call_with_stopwatch( st.time_sat, [&]() {
            return solver.solve( &assumptions[0], &assumptions[0] + 1, 0 );
          });
          
          if ( res == percy::synth_result::success && !fStop ) /* CEX found */
          {
            std::vector<bool> pattern;
            for ( auto j = 1u; j <= ntk.num_pis(); ++j )
              pattern.push_back(solver.var_value( j ));
            if ( sim.add_pattern(pattern) )
            {
              fStop = true;
              return std::nullopt; /* number of generated patterns exceeds limit */
            }
            ++st.num_cex;
  
            /* re-simulate */
            call_with_stopwatch( st.time_sim, [&]() {
              simulate_nodes<kitty::dynamic_truth_table, NtkBase, partial_simulator>( ntk, tts, sim );
              normalizeTT();
            });
          }
          else /* proved substitution */
          {
            auto g = phase[root]? !ntk.create_and( phase[s0]? !s0: s0, phase[s1]? !s1: s1 ) :ntk.create_and( phase[s0]? !s0: s0, phase[s1]? !s1: s1 );
            /* update CNF */
            literals.resize();
            solver.add_var();
            literals[g] = make_lit( solver.nr_vars()-1 );
            solver.add_clause( {lit_not( literals[g] ), l_s0} );
            solver.add_clause( {lit_not( literals[g] ), l_s1} );
            solver.add_clause( {lit_not( l_s0 ), lit_not( l_s1 ), literals[g]} );
            return g;
          }
        }
      }
    }

    return std::nullopt;
  }

private:
  NtkBase& ntkbase;
  Ntk& ntk;

  simresub_params const& ps;
  simresub_stats& st;

  /* temporary statistics for progress bar */
  uint32_t candidates{0};
  uint32_t last_gain{0};

  bool fStop{false}; /* signal indicating cex limit exceeded */

  TT tts;
  node_map<bool, NtkBase> phase;
  partial_simulator sim;
  node_map<uint32_t, NtkBase> literals;
  percy::bsat_wrapper solver;

  unate_divisors udivs;
  binate_divisors bdivs;

  std::vector<node> temp;
  std::vector<node> divs;
  uint32_t num_divs{0};
};

} /* namespace detail */

template<class Ntk>
void sim_resubstitution( Ntk& ntk, simresub_params const& ps = {}, simresub_stats* pst = nullptr )
{
  /* TODO: check if basetype of ntk is aig */
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
  static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
  static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the has_size method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the has substitute_node method" );
  static_assert( has_value_v<Ntk>, "Ntk does not implement the has_value method" );
  static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );

  using resub_view_t = fanout_view2<depth_view<Ntk>>;
  depth_view<Ntk> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  simresub_stats st;

  detail::simresub_impl<Ntk, resub_view_t> p( ntk, resub_view, ps, st );
  p.run();
  if ( ps.verbose )
    st.report();

  if ( pst )
    *pst = st;
}

} /* namespace mockturtle */
