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
  \file func_reduction.hpp
  \brief Fast functional resubstitution

  \author Hanyu Wang
*/

#pragma once

#include "../../traits.hpp"
#include "../../views/depth_view.hpp"
#include "../../views/fanout_view.hpp"
#include "../../utils/index_list.hpp"
#include "../../networks/xag.hpp"
#include "../../networks/aig.hpp"
#include "../detail/resub_utils.hpp"
#include "../resyn_engines/xag_resyn.hpp"
#include "../resyn_engines/aig_enumerative.hpp"
#include "../resyn_engines/mig_resyn.hpp"
#include "../resyn_engines/mig_enumerative.hpp"
#include "../simulation.hpp"
#include "../dont_cares.hpp"
#include "../circuit_validator.hpp"
#include "../pattern_generation.hpp"
#include "../../io/write_patterns.hpp"
#include <kitty/kitty.hpp>

#include <optional>
#include <functional>
#include <vector>

namespace mockturtle::experimental
{
struct func_reduction_params
{
  /*! \brief Maximum number of clauses of the SAT solver. */
  uint32_t max_clauses{5000};

  /*! \brief Conflict limit for the SAT solver. */
  uint32_t conflict_limit{2};

  /*! \brief Random seed for the SAT solver (influences the randomness of counter-examples). */
  uint32_t random_seed{1};

  /* \brief Be verbose. */
  bool verbose{false};
};

struct func_reduction_stats
{
  /*! \brief Time for simulation. */
  stopwatch<>::duration time_sim{0};

  /*! \brief Time for SAT solving. */
  stopwatch<>::duration time_sat{0};

  uint32_t mistakes{0};
  uint32_t time_out{0};
  uint32_t merged{0};

  void report() const
  {
    // clang-format off
    fmt::print( "[i] functional reduction report\n" );
    fmt::print( "      Merged      : {} \n", merged);
    fmt::print( "      Mistakes    : {} \n", mistakes);
    fmt::print( "      Time-outs   : {} \n", time_out);
    fmt::print( "    ===== Runtime Breakdown =====\n" );
    fmt::print( "      Simulation  : {:>5.2f} secs\n", to_seconds( time_sim ) );
    fmt::print( "      SAT         : {:>5.2f} secs\n", to_seconds( time_sat ) );
    // clang-format on
  }
};

template<class Ntk>
class func_reduction_impl
{

public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = kitty::partial_truth_table;
  using validator_t = circuit_validator<Ntk, bill::solvers::bsat2, /*use_pushpop*/false, /*randomize*/true, /*use_odc*/false>;

  explicit func_reduction_impl( Ntk& ntk, func_reduction_params const& ps, func_reduction_stats& st )
      : ntk( ntk ), ps( ps ), st( st ), 
        validator( ntk, {ps.max_clauses, 0, ps.conflict_limit, ps.random_seed} ), tts( ntk )
  {
    sim = partial_simulator( ntk.num_pis(), ( 1 << 10 ) );
    call_with_stopwatch( st.time_sim, [&](){ simulate_nodes<Ntk>( ntk, tts, sim, true ); } );
  }
  ~func_reduction_impl()
  {
  }

  void run()
  {
    uint32_t initial_size = ntk.num_gates();
    topo_view{ntk}.foreach_node( [&]( auto const n, auto i ) { 
      if ( i >= initial_size )
      {
        return false; /* terminate */
      }
      auto res = mini_solver( n );
      if ( res )
      {
        ntk.substitute_node( n, *res );
      }
      return true;
    } );
  }

  std::optional<signal> mini_solver( node const n )
  {
    signal res;
    check_tts( n );
    TT const& tt = tts[n];
    if ( kitty::count_ones( tt ) == 0 )
    {
      res = ntk.get_constant( 0 );
      if ( mini_validate( n, res ) ) return res;
    }
    if ( kitty::count_zeros( tt ) == 0 )
    {
      res = ntk.get_constant( 1 );
      if ( mini_validate( n, res ) ) return res;
    }
    if ( div_tts.find( tt ) != div_tts.end() )
    {
      res = ntk.make_signal( div_tts[tt] );
      if ( mini_validate( n, res ) ) return res;
    }
    if ( div_tts.find( ~tt ) != div_tts.end() )
    {
      res = !ntk.make_signal( div_tts[~tt] );
      if ( mini_validate( n, res ) ) return res;
    }
    div_tts[ tt ] = n;
    dirty_nodes.emplace_back( n );
    return std::nullopt;
  }
  bool mini_validate( node const n, signal const d )
  {
    auto valid = call_with_stopwatch( st.time_sat, [&](){ return validator.validate( n, d ); } );
    if ( valid )
    {
      if ( *valid )
      {
        ++st.merged;
        return true;
      }
      else
      {
        sim.add_pattern( validator.cex );
        call_with_stopwatch( st.time_sim, [&]() {
          simulate_nodes<Ntk>( ntk, tts, sim, false );
        } );
        ++st.mistakes;
        div_tts.clear();
        for ( auto div : dirty_nodes ) // update history
        {
          div_tts[ tts[div] ] = div;
        }
      }
    }
    ++st.time_out;
    return false;
  }

private:
  void check_tts( node const& n )
  {
    if ( tts[n].num_bits() != sim.num_bits() )
    {
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_node<Ntk>( ntk, n, tts, sim );
      } );
    }
  }
private:
  Ntk& ntk;

  func_reduction_params const& ps;
  func_reduction_stats& st;

  partial_simulator sim;
  validator_t validator;
  incomplete_node_map<TT, Ntk> tts;
  std::unordered_map<TT, node, kitty::hash<TT>> div_tts;
  std::vector<node> dirty_nodes;
};

template<class Ntk>
void func_reduction( Ntk& ntk, func_reduction_params const& ps = {}, func_reduction_stats* pst = nullptr )
{
  static_assert( std::is_same_v<typename Ntk::base_type, aig_network>, "Ntk::base_type is not aig_network" );
  
  func_reduction_stats st;
  func_reduction_impl<Ntk> p( ntk, ps, st );
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

template<class Ntk>
std::optional<Ntk> reduced_miter( Ntk const& ntk1, Ntk const& ntk )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = kitty::partial_truth_table;
  using validator_t = circuit_validator<Ntk, bill::solvers::bsat2, /*use_pushpop*/false, /*randomize*/true, /*use_odc*/false>;
  
  /* both networks must have same number of inputs and outputs */
  if ( ( ntk1.num_pis() != ntk.num_pis() ) || ( ntk1.num_pos() != ntk.num_pos() ) )
  {
    return std::nullopt;
  }

  /* create primary inputs */
  Ntk dest;
  std::vector<signal> pis;
  for ( auto i = 0u; i < ntk1.num_pis(); ++i )
  {
    pis.push_back( dest.create_pi() );
  }
  /* copy first network */
  const auto pos1 = cleanup_dangling( ntk1, dest, pis.begin(), pis.end() );
  validator_t validator( dest, {1000, 0, 2, 0} );
  partial_simulator sim = partial_simulator( dest.num_pis(), ( 1 << 10 ) );
  // pattern_generation( dest, sim );
  incomplete_node_map<TT, Ntk> tts ( dest );
  simulate_nodes<Ntk>( dest, tts, sim, true );

  std::unordered_map<TT, node, kitty::hash<TT>> funcs;
  dest.foreach_node( [&]( auto n ) {
    funcs[ tts[n] ] = n;
  });
  auto add_event = dest.events().register_add_event( [&]( const auto& n ) {
    tts.resize();
    simulate_node<Ntk>( dest, n, tts, sim );
  } );
  
  /* copy second network */
  node_map<signal, Ntk> old_to_new( ntk );
  old_to_new[ntk.get_constant( false )] = dest.get_constant( false );
  if ( ntk.get_node( ntk.get_constant( true ) ) != ntk.get_node( ntk.get_constant( false ) ) )
  {
    old_to_new[ntk.get_constant( true )] = dest.get_constant( true );
  }
  
  /* mini validation */
  auto mini_validate = [&]( node s, signal d )
  {
    auto valid = validator.validate( s, d );
    if ( valid && *valid ) 
    {
      return true;
    }
    if ( valid && !*valid )
    {
      sim.add_pattern( validator.cex );
      simulate_nodes<Ntk>( ntk, tts, sim, false );
      funcs.clear();
      dest.foreach_node( [&]( auto _n ) {
        funcs[ tts[_n] ] = _n;
      });
    }
    return false;
  };
  /* create inputs in same order */
  auto it = pis.begin();
  ntk.foreach_pi( [&]( auto n ) {
    old_to_new[n] = *it++;
  } );
  assert( it == pis.end() );
  /* foreach node in topological order */
  topo_view{ntk}.foreach_node( [&]( auto n ) {
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      return;
    /* collect children */
    std::vector<signal> children;
    ntk.foreach_fanin( n, [&]( auto child, auto i ) {
      const auto f = old_to_new[child];
      if ( ntk.is_complemented( child ) )
      {
        children.push_back( dest.create_not( f ) );
      }
      else
      {
        children.push_back( f );
      }
    } );
    uint32_t prev_size = dest.num_gates();
    signal s = dest.clone_node( ntk, n, children );
    old_to_new[ n ] = s;
    if ( prev_size != dest.num_gates() - 1 ) return; //TODO: find out better way to check structural hashing
    /* functional hashing */
    TT const& tt = tts[dest.get_node( s )];
    assert( tt.num_bits() == sim.num_bits() );
    if ( kitty::count_ones( tt ) == 0 )
    {
      if ( mini_validate( dest.get_node( s ), dest.get_constant( 0 ) ) )
      {
        old_to_new[n] = dest.get_constant( 0 );
        return;
      }
    }
    if ( kitty::count_zeros( tt ) == 0 )
    {
      if ( mini_validate( dest.get_node( s ), dest.get_constant( 1 ) ) )
      {
        old_to_new[n] = dest.get_constant( 1 );
        return;        
      }
    }
    if ( funcs.find( tt ) != funcs.end() )
    {
      if ( mini_validate( dest.get_node( s ), dest.make_signal( funcs[tt] ) ) )
      {
        old_to_new[n] = dest.make_signal( funcs[tt] );
        return;
      }
    }
    if ( funcs.find( ~tt ) != funcs.end() )
    {
      if ( mini_validate( dest.get_node( s ), dest.create_not( dest.make_signal( funcs[~tt] ) ) ) )
      {
        old_to_new[n] = dest.create_not( dest.make_signal( funcs[~tt] ) );
        return;
      }
    }
    funcs[ tt ] = dest.get_node( s );
  } );
  /* create outputs in same order */
  std::vector<signal> pos2;
  ntk.foreach_po( [&]( auto po ) {
    const auto f = old_to_new[po];
    if ( ntk.is_complemented( po ) )
    {
      pos2.push_back( dest.create_not( f ) );
    }
    else
    {
      pos2.push_back( f );
    }
  } );
  assert( pos1.size() == pos2.size() );
  std::vector<signal> xor_outputs;
  std::transform( pos1.begin(), pos1.end(), pos2.begin(), std::back_inserter( xor_outputs ),
    [&]( signal const& o1, signal const& o2 ) { 
      return dest.create_xor( o1, o2 );
  } );

  /* create big OR of XOR gates */
  dest.create_po( dest.create_nary_or( xor_outputs ) );
  if ( add_event )
  {
    dest.events().release_add_event( add_event );
  }
  Ntk ret = cleanup_dangling( dest );

  return ret;
}

template<class Ntk>
Ntk cleanup_func( Ntk const& ntk, uint32_t limit = 2 )
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = kitty::partial_truth_table;
  using validator_t = circuit_validator<Ntk, bill::solvers::bsat2, /*use_pushpop*/false, /*randomize*/true, /*use_odc*/false>;
  /* create primary inputs */
  Ntk dest;
  std::vector<signal> pis;
  for ( auto i = 0u; i < ntk.num_pis(); ++i )
  {
    pis.push_back( dest.create_pi() );
  }
  validator_t validator( dest, {1000, 0, limit, 0} );
  partial_simulator sim = partial_simulator( dest.num_pis(), ( 1 << 11 ) );
  incomplete_node_map<TT, Ntk> tts ( dest );
  simulate_nodes<Ntk>( dest, tts, sim, true );
  std::unordered_map<TT, node, kitty::hash<TT>> funcs;
  dest.foreach_node( [&]( auto n ) {
    funcs[ tts[n] ] = n;
  });
  auto add_event = dest.events().register_add_event( [&]( const auto& n ) {
    tts.resize();
    simulate_node<Ntk>( dest, n, tts, sim );
  } );
  node_map<signal, Ntk> old_to_new( ntk );
  old_to_new[ntk.get_constant( false )] = dest.get_constant( false );
  if ( ntk.get_node( ntk.get_constant( true ) ) != ntk.get_node( ntk.get_constant( false ) ) )
  {
    old_to_new[ntk.get_constant( true )] = dest.get_constant( true );
  }

  /* mini validation */
  auto mini_validate = [&]( node s, signal d )
  {
    auto valid = validator.validate( s, d );
    if ( valid && *valid ) 
    {
      return true;
    }
    if ( valid && !*valid )
    {
      sim.add_pattern( validator.cex );
      simulate_nodes<Ntk>( ntk, tts, sim, false );
      funcs.clear();
      dest.foreach_node( [&]( auto _n ) {
        funcs[ tts[_n] ] = _n;
      });
    }
    return false;
  };
  /* create inputs in same order */
  auto it = pis.begin();
  ntk.foreach_pi( [&]( auto n ) {
    old_to_new[n] = *it++;
  } );
  assert( it == pis.end() );
  /* foreach node in topological order */
  topo_view{ntk}.foreach_node( [&]( auto n ) {
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      return;
    /* collect children */
    std::vector<signal> children;
    ntk.foreach_fanin( n, [&]( auto child, auto i ) {
      const auto f = old_to_new[child];
      if ( ntk.is_complemented( child ) )
      {
        children.push_back( dest.create_not( f ) );
      }
      else
      {
        children.push_back( f );
      }
    } );

    uint32_t prev_size = dest.num_gates();
    signal s = dest.clone_node( ntk, n, children );
    old_to_new[ n ] = s;
    if ( prev_size != dest.num_gates() - 1 ) return; //TODO: find out better way to check structural hashing
    /* functional hashing */
    TT const& tt = tts[dest.get_node( s )];
    assert( tt.num_bits() == sim.num_bits() );
    if ( kitty::count_ones( tt ) == 0 )
    {
      if ( mini_validate( dest.get_node( s ), dest.get_constant( 0 ) ) )
      {
        old_to_new[n] = dest.get_constant( 0 );
        return;
      }
    }
    if ( kitty::count_zeros( tt ) == 0 )
    {
      if ( mini_validate( dest.get_node( s ), dest.get_constant( 1 ) ) )
      {
        old_to_new[n] = dest.get_constant( 1 );
        return;        
      }
    }
    if ( funcs.find( tt ) != funcs.end() )
    {
      if ( mini_validate( dest.get_node( s ), dest.make_signal( funcs[tt] ) ) )
      {
        old_to_new[n] = dest.make_signal( funcs[tt] );
        return;
      }
    }
    if ( funcs.find( ~tt ) != funcs.end() )
    {
      if ( mini_validate( dest.get_node( s ), dest.create_not( dest.make_signal( funcs[~tt] ) ) ) )
      {
        old_to_new[n] = dest.create_not( dest.make_signal( funcs[~tt] ) );
        return;
      }
    }
    funcs[ tt ] = dest.get_node( s );
  } );
  /* create outputs in same order */
  ntk.foreach_po( [&]( auto po ) {
    const auto f = old_to_new[po];
    if ( ntk.is_complemented( po ) )
    {
      dest.create_po( dest.create_not( f ) );
    }
    else
    {
      dest.create_po( f );
    }
  } );
  if ( add_event )
  {
    dest.events().release_add_event( add_event );
  }
  return cleanup_dangling( dest );
}
} /* namespace mockturtle::experimental */
