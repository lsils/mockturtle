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
  \file sim_resub.hpp
  \brief Simulation-Guided Resubstitution

  \author Siang-Yun Lee
*/

#pragma once

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../utils/abc_resub.hpp"
#include "../views/depth_view.hpp"
#include "../views/fanout_view.hpp"
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/algorithms/circuit_validator.hpp>
#include <mockturtle/io/write_patterns.hpp>
#include <kitty/partial_truth_table.hpp>
#include "cleanup.hpp"
#include "reconv_cut2.hpp"
#include <bill/sat/interface/z3.hpp>
#include <bill/sat/interface/abc_bsat2.hpp>

namespace mockturtle
{

struct simresub_params
{
  /*! \brief Maximum number of divisors to collect. */
  uint32_t max_divisors{150};

  /*! \brief Maximum number of divisors to consider in k-resub engine. */
  uint32_t max_divisors_k{50};

  /*! \brief Maximum number of trials to call the k-resub engine. */
  uint32_t num_trials_k{100};

  /*! \brief Maximum number of nodes added by resubstitution. */
  uint32_t max_inserts{2};

  /*! \brief Maximum fanout of a node to be considered as root. */
  uint32_t skip_fanout_limit_for_roots{1000};

  /*! \brief Maximum fanout of a node to be considered as divisor. */
  uint32_t skip_fanout_limit_for_divisors{100};

  /*! \brief Whether to save augmented patterns (with CEXs) into file. */
  std::optional<std::string> write_pats{};

  /*! \brief Whether to utilize ODC, and how many levels. 0 = no. -1 = Consider TFO until PO. */
  /* not supported yet. */
  //int odc_levels{0};

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};

  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{8};

  /*! \brief Conflict limit for the SAT solver. */
  uint32_t conflict_limit{1000};

  /*! \brief Random seed for the SAT solver (influences the randomness of counter-examples). */
  uint32_t random_seed{0};
};

struct simresub_stats
{
  /*! \brief Total time. */
  stopwatch<>::duration time_total{0};

  /*! \brief Time for simulation. */
  stopwatch<>::duration time_sim{0};

  /*! \brief Time for SAT solving. */
  stopwatch<>::duration time_sat{0};

  /*! \brief Time for computing reconvergence-driven cut. */
  stopwatch<>::duration time_cut{0};

  /*! \brief Time for MFFC computation. */
  stopwatch<>::duration time_mffc{0};

  /*! \brief Time for divisor collection. */
  stopwatch<>::duration time_divs{0};

  /*! \brief Time for executing the user-specified callback when candidates found. */
  stopwatch<>::duration time_callback{0};

  /*! \brief Time for finding dependency function. */
  stopwatch<>::duration time_compute_function{0};

  /*! \brief Number of counter-examples. */
  uint32_t num_cex{0};

  /*! \brief Number of successful resubstitutions. */
  uint32_t num_resub{0};

  /*! \brief Total number of gain. */
  uint64_t estimated_gain{0};

  /*! \brief Total number of collected divisors. */
  uint64_t num_total_divisors{0};
};

namespace detail
{

template<typename Ntk>
bool substitute_fn( Ntk& ntk, typename Ntk::node const& n, typename Ntk::signal const& g )
{
  ntk.substitute_node( n, g );
  return true;
};

template<typename Ntk>
bool report_fn( Ntk& ntk, typename Ntk::node const& n, typename Ntk::signal const& g )
{
  (void)ntk;
  std::cout<<"substitute node "<<unsigned(n)<<" with node "<<unsigned(ntk.get_node(g))<<std::endl;
  return false;
};

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

template<class Ntk>
class simresub_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = unordered_node_map<kitty::partial_truth_table, Ntk>;
  using resub_callback_t = std::function<bool( Ntk&, node const&, signal const& )>;
  using validator_t = circuit_validator<Ntk, bill::solvers::bsat2, false, true, false>;
  using vgate = typename validator_t::gate;
  using fanin = typename vgate::fanin;
  using gtype = typename validator_t::gate_type;

  explicit simresub_impl( Ntk& ntk, simresub_params const& ps, simresub_stats& st, partial_simulator& sim, validator_params const& vps, resub_callback_t const& callback = substitute_fn<Ntk> )
    : ntk( ntk ), ps( ps ), st( st ), callback( callback ),
      tts( ntk ), sim( sim ), validator( ntk, vps )
  {
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
    ntk._events->on_add.emplace_back( [&]( const auto& n ){
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_node<Ntk>( ntk, n, tts, sim );
      });
    });
    
    ntk._events->on_modified.emplace_back( update_level_of_existing_node );
    
    ntk._events->on_delete.emplace_back( update_level_of_deleted_node );
  }

  void run()
  {
    stopwatch t( st.time_total );

    /* start the managers */
    progress_bar pbar{ntk.size(), "resub |{0}| node = {1:>4}   cand = {2:>4}   est. gain = {3:>5}", ps.progress};

    /* first simulation: the whole circuit; from 0 bits. */
    call_with_stopwatch( st.time_sim, [&]() {
      simulate_nodes<Ntk>( ntk, tts, sim );
    });

    call_with_stopwatch( st.time_compute_function, [&]() {
      abcresub::Abc_ResubPrepareManager( sim.compute_constant( false ).num_blocks() );
    });

    /* iterate through all nodes and try to replace it */
    auto const size = ntk.num_gates();
    ntk.foreach_gate( [&]( auto const& n, auto i ){
        if ( i >= size )
          return false; /* terminate */

        pbar( i, i, candidates, st.estimated_gain );

        /* skip nodes with many fanouts */
        if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
          return true; /* next */

        /* use all the PIs as the cut */
        cut_manager<Ntk> mgr( ps.max_pis );
        auto const leaves = call_with_stopwatch( st.time_cut, [&]() {
            return reconv_driven_cut( mgr, ntk, n );
          });
        
        /* evaluate this cut */
        auto const g = evaluate( n, leaves );
        if ( !g ) return true; /* next */
        
        /* update progress bar */
        candidates++;
        st.estimated_gain += last_gain;

        /* update network */
        call_with_stopwatch( st.time_callback, [&]() {
            //if ( ps.odc_levels != 0 ) validator.update();
            return callback( ntk, n, *g );
          });

        return true; /* next */
      });

    call_with_stopwatch( st.time_compute_function, [&]() {
      abcresub::Abc_ResubPrepareManager( 0 );
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

    /* collect the internal nodes */
    if ( ntk.value( n ) == 0 && n != 0 ) /* ntk.fanout_size( n ) */
      internal.emplace_back( n );

    ntk.foreach_fanin( n, [&]( const auto& f ){
        collect_divisors_rec( ntk.get_node( f ), internal );
      }); 
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
          if ( ntk.is_dead( p ) || ntk.visited( p ) == ntk.trav_id() || ntk.level( p ) > required )
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

  void found_cex()
  {
    ++st.num_cex;
    sim.add_pattern( validator.cex );
    
    /* re-simulate the whole circuit (for the last block) when a block is full */
    if ( sim.num_bits() % 64 == 0 )
    {
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_nodes<Ntk>( ntk, tts, sim, false );
      });

      call_with_stopwatch( st.time_compute_function, [&]() {
        abcresub::Abc_ResubPrepareManager( sim.compute_constant( false ).num_blocks() );
      });
    }
  }

  void check_tts( node const& n )
  {
    if ( tts[n].num_bits() != sim.num_bits() )
    {
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_node<Ntk>( ntk, n, tts, sim );
      });
    }
  }

private:
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

    /* try k-resub */
    uint32_t size = 0;
    auto g = resub_divk( root, std::min( num_mffc - 1, int( ps.max_inserts ) ), size );
    if ( g )
    {
      ++st.num_resub;
      last_gain = num_mffc - size;
      return g; /* accepted resub */
    }

    return std::nullopt;
  }

  std::optional<signal> resub_divk( node const& root, uint32_t num_inserts, uint32_t& size ) 
  {    
    for ( auto j = 0u; j < ps.num_trials_k; ++j )
    {
      check_tts( root );
      for ( auto i = 0u; i < num_divs; ++i )
      {
        check_tts( divs[i] );
      }

      auto const res = call_with_stopwatch( st.time_compute_function, [&]() {
        abcresub::Abc_ResubPrepareManager( sim.compute_constant( false ).num_blocks() );

        abc_resub rs( 2ul + num_divs, tts[root].num_blocks(), ps.max_divisors_k );
        rs.add_root( root, tts );
        rs.add_divisors( std::begin( divs ), std::begin( divs ) + num_divs, tts );

        if constexpr ( std::is_same<typename Ntk::base_type, xag_network>::value )
          return rs.compute_function( num_inserts, true );
        else
          return rs.compute_function( num_inserts, false );
      });

      if ( res )
      {
        auto const& index_list = *res;
        if ( index_list.size() == 1u ) /* div0 or constant */
        {
          const auto valid = call_with_stopwatch( st.time_sat, [&]() {
            if ( index_list[0] < 2 ) return validator.validate( root, ntk.get_constant( bool( index_list[0] ) ) );
            assert( index_list[0] >= 4 );
            return validator.validate( root, bool( index_list[0] % 2 ) ? !ntk.make_signal( divs[(index_list[0] >> 1u) - 2u] ) : ntk.make_signal( divs[(index_list[0] >> 1u) - 2u] ) );
          });

          if ( !valid ) /* timeout */
          {
            break;
          }
          else
          {
            if ( *valid )
            {
              size = 0u;
              if ( index_list[0] < 2 ) return ntk.get_constant( bool( index_list[0] ) );
              else return bool( index_list[0] % 2 ) ? !ntk.make_signal( divs[(index_list[0] >> 1u) - 2u] ) : ntk.make_signal( divs[(index_list[0] >> 1u) - 2u] );
            }
            else
            {
              found_cex();
              continue;
            }
          }
        }

        uint64_t const num_gates = ( index_list.size() - 1u ) / 2u;
        std::vector<vgate> gates; gates.reserve( num_gates );
        size = 0u;
        for ( auto i = 0u; i < num_gates; ++i )
        {
          fanin f0{uint32_t( ( index_list[2*i] >> 1u ) - 2u ), bool( index_list[2*i] % 2 )};
          fanin f1{uint32_t( ( index_list[2*i + 1u] >> 1u ) - 2u ), bool( index_list[2*i + 1u] % 2 )};
          gates.emplace_back( vgate{{ f0, f1 }, f0.index < f1.index ? gtype::AND : gtype::XOR} );
          
          if constexpr ( std::is_same<typename Ntk::base_type, xag_network>::value )
            ++size;
          else
            size += ( gates[i].type == gtype::AND )? 1u: 3u;
        }
        bool const out_neg = bool( index_list.back() % 2 );
        assert( size <= num_inserts );

        const auto valid = call_with_stopwatch( st.time_sat, [&]() {
          return validator.validate( root, std::begin( divs ), std::begin( divs ) + num_divs, gates, out_neg );
        });

        if ( !valid ) /* timeout */
        {
          break;
        }
        else
        {
          if ( *valid )
          {
            std::vector<signal> ckt;
            for ( auto i = 0u; i < num_divs; ++i )
            {
              ckt.emplace_back( ntk.make_signal( divs[i] ) );
            }

            for ( auto g : gates )
            {
              auto const f0 = g.fanins[0].inverted ? !ckt[g.fanins[0].index] : ckt[g.fanins[0].index];
              auto const f1 = g.fanins[1].inverted ? !ckt[g.fanins[1].index] : ckt[g.fanins[1].index];
              if ( g.type == gtype::AND )
              {
                ckt.emplace_back( ntk.create_and( f0, f1 ) );
              }
              else if ( g.type == gtype::XOR )
              {
                ckt.emplace_back( ntk.create_xor( f0, f1 ) );
              }
            }
            
            return out_neg ? !ckt.back() : ckt.back();
          }
          else
          {
            found_cex();
          }
        }
      }
      else /* loop until no result can be found by the engine */
      {
        return std::nullopt;
      }
    }

    return std::nullopt;
  }

private:
  Ntk& ntk;

  simresub_params const& ps;
  simresub_stats& st;

  resub_callback_t const& callback;

  /* temporary statistics for progress bar */
  uint32_t candidates{0};
  uint32_t last_gain{0};

  TT tts;
  partial_simulator& sim;

  validator_t validator;

  std::vector<node> temp;
  std::vector<node> divs;
  uint32_t num_divs{0};
};

} /* namespace detail */

/*! \brief Simulation-guided Boolean resubstitution algorithm.
 *
 * Please refer to [1] for details of the algorithm.
 *
 * [1] Simulation-Guided Boolean Resubstitution. IWLS 2020 / ICCAD 2020.
 *
 * \param sim Reference of a `partial_simulator` object where the generated 
 * patterns will be stored. It can be empty (`partial_simulator( ntk.num_pis(), 0 )`)
 * or already containing some patterns generated from previous runs 
 * (`partial_simulator( filename )`) or randomly generated
 * (`partial_simulator( ntk.num_pis(), num_random_patterns )`). The generated
 * patterns can then be written out with `write_patterns`
 * or directly be used by passing the simulator to another algorithm.
 */
template<class Ntk>
void sim_resubstitution( Ntk& ntk, partial_simulator& sim, simresub_params const& ps = {}, simresub_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( std::is_same<typename Ntk::base_type, aig_network>::value || std::is_same<typename Ntk::base_type, xag_network>::value, "Currently only supports AIG and XAG" );
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

  using resub_view_t = fanout_view<depth_view<Ntk>>;
  depth_view<Ntk> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  validator_params vps;
  vps.conflict_limit = ps.conflict_limit;
  vps.random_seed = ps.random_seed;

  simresub_stats st;

  detail::simresub_impl<resub_view_t> p( resub_view, ps, st, sim, vps, detail::substitute_fn<resub_view_t> );
  p.run();

  if ( ps.write_pats )
  {
    write_patterns( sim, *(ps.write_pats) );
  }

  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */