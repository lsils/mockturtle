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
  \file xag_resub.hpp
  \brief Resubstitution with free xor (works for XAGs, XOR gates are considered for free)

  \author Eleonora Testa (inspired by aig_resub.hpp from Heinz Riener)
*/

#pragma once

#include <mockturtle/networks/xag.hpp>
#include <kitty/operations.hpp>
#include <kitty/print.hpp>

namespace mockturtle
{

namespace detail
{

template<typename Ntk>
class node_mffc_inside_xag
{
public:
  using node = typename Ntk::node;

public:
  explicit node_mffc_inside_xag( Ntk const& ntk )
      : ntk( ntk )
  {
  }

  std::pair<int32_t, int32_t> run( node const& n, std::vector<node> const& leaves, std::vector<node>& inside )
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
  std::pair<int32_t, int32_t> node_deref_rec( node const& n )
  {
    if ( ntk.is_pi( n ) )
      return {0, 0};

    int32_t counter_and = 0;
    int32_t counter_xor = 0;

    if ( ntk.is_and( n ) )
      counter_and = 1;
    else if ( ntk.is_xor( n ) )
      counter_xor = 1;

    ntk.foreach_fanin( n, [&]( const auto& f ) {
      auto const& p = ntk.get_node( f );

      ntk.decr_fanout_size( p );
      if ( ntk.fanout_size( p ) == 0 )
      {
        counter_and += node_deref_rec( p ).first;
        counter_xor += node_deref_rec( p ).second;
      }
    } );

    return {counter_and, counter_xor};
  }

  /* ! \brief Reference the node's MFFC */
  std::pair<int32_t, int32_t> node_ref_rec( node const& n )
  {
    if ( ntk.is_pi( n ) )
      return {0, 0};

    int32_t counter_and = 0;
    int32_t counter_xor = 0;

    if ( ntk.is_and( n ) )
      counter_and = 1;
    else if ( ntk.is_xor( n ) )
      counter_xor = 1;

    ntk.foreach_fanin( n, [&]( const auto& f ) {
      auto const& p = ntk.get_node( f );

      auto v = ntk.fanout_size( p );
      ntk.incr_fanout_size( p );
      if ( v == 0 )
      {
        counter_and += node_ref_rec( p ).first;
        counter_xor += node_ref_rec( p ).second;
      }
    } );

    return {counter_and, counter_xor};
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
    ntk.foreach_fanin( n, [&]( const auto& f ) {
      node_mffc_cone_rec( ntk.get_node( f ), cone, false );
    } );

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

/* template<typename Ntk, typename TT>
class simulator
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using truthtable_t = TT;

  explicit simulator( Ntk const& ntk, uint32_t num_divisors, uint32_t max_pis )
      : ntk( ntk ), num_divisors( num_divisors ), tts( num_divisors + 1 ), node_to_index( ntk.size(), 0u ), phase( ntk.size(), false )
  {
    auto tt = kitty::create<truthtable_t>( max_pis );
    tts[0] = tt;

    for ( auto i = 0; i < tt.num_vars(); ++i )
    {
      kitty::create_nth_var( tt, i );
      tts[i + 1] = tt;
    }
  }

  void resize()
  {
    if ( ntk.size() > node_to_index.size() )
      node_to_index.resize( ntk.size(), 0u );
    if ( ntk.size() > phase.size() )
      phase.resize( ntk.size(), false );
  }

  void assign( node const& n, uint32_t index )
  {
    assert( n < node_to_index.size() );
    assert( index < num_divisors + 1 );
    node_to_index[n] = index;
  }

  truthtable_t get_tt( signal const& s ) const
  {
    auto const tt = tts.at( node_to_index.at( ntk.get_node( s ) ) );
    return ntk.is_complemented( s ) ? ~tt : tt;
  }

  void set_tt( uint32_t index, truthtable_t const& tt )
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

  std::vector<truthtable_t> tts;
  std::vector<uint32_t> node_to_index;
  std::vector<bool> phase;
}; /* simulator */

struct xag_resub_stats
{
  /*! \brief Accumulated runtime for const-resub */
  stopwatch<>::duration time_resubC{0};

  /*! \brief Accumulated runtime for zero-resub */
  stopwatch<>::duration time_resub0{0};

  /*! \brief Accumulated runtime for one-resub */
  stopwatch<>::duration time_resub1{0};

  /*! \brief Accumulated runtime for two-resub. */
  stopwatch<>::duration time_resub2{0};

  /*! \brief Accumulated runtime for three-resub. */
  stopwatch<>::duration time_resub3{0};

  /*! \brief Number of accepted constant resubsitutions */
  uint32_t num_const_accepts{0};

  /*! \brief Number of accepted zero resubsitutions */
  uint32_t num_div0_accepts{0};

  /*! \brief Number of accepted one resubsitutions */
  uint64_t num_div1_accepts{0};

  /*! \brief Number of accepted two resubsitutions using triples of divisors */
  uint64_t num_div12_accepts{0};

  /*! \brief Number of accepted two resubsitutions */
  uint64_t num_div2_accepts{0};

  /*! \brief Number of accepted three resubsitutions */
  uint64_t num_div3_accepts{0};

  void report() const
  {
    std::cout << "[i] kernel: xag_resub_functor\n";
    std::cout << fmt::format( "[i]     constant-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_const_accepts, to_seconds( time_resubC ) );
    std::cout << fmt::format( "[i]            0-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_div0_accepts, to_seconds( time_resub0 ) );
    std::cout << fmt::format( "[i]            1-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_div1_accepts, to_seconds( time_resub1 ) );
    std::cout << fmt::format( "[i]            2-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_div2_accepts, to_seconds( time_resub2 ) );
    std::cout << fmt::format( "[i]            3-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_div3_accepts, to_seconds( time_resub3 ) );
    std::cout << fmt::format( "[i]            total   {:6d}\n",
                              ( num_const_accepts + num_div0_accepts + num_div1_accepts + num_div12_accepts + num_div2_accepts + num_div3_accepts ) );
  }
}; /* xag_resub_stats */

template<typename Ntk, typename Simulator>
struct xag_resub_functor
{
public:
  using node = xag_network::node;
  using signal = xag_network::signal;
  using stats = xag_resub_stats;

public:
  explicit xag_resub_functor( Ntk& ntk, Simulator const& sim, std::vector<node> const& divs, uint32_t num_divs, stats& st )
      : ntk( ntk ), sim( sim ), divs( divs ), num_divs( num_divs ), st( st )
  {
  }

  std::optional<signal> operator()( node const& root, uint32_t required, uint32_t max_inserts, uint32_t num_and_mffc, uint32_t num_xor_mffc, uint32_t& last_gain )
  {
    std::cout << " divisors = " << divs.size() << std::endl; 
    /* consider constants */
    auto g = call_with_stopwatch( st.time_resubC, [&]() {
      return resub_const( root, required );
    } );
    if ( g )
    {
      ++st.num_const_accepts;
      last_gain = num_and_mffc;
      return g; /* accepted resub */
    }

    /* consider equal nodes */
    g = call_with_stopwatch( st.time_resub0, [&]() {
      return resub_div0( root, required );
    } );
    if ( g )
    {
      ++st.num_div0_accepts;
      last_gain = num_and_mffc;
      return g; /* accepted resub */
    }

    /* if ( max_inserts == 0 || num_mffc == 1 )  perche per me chissne perche tanto aggiungo solamente XOR
      return std::nullopt; */

    /* consider equal nodes */
    g = call_with_stopwatch( st.time_resub1, [&]() {
      return resub_div1( root, required );
    } );
    std::cout << "finito resub 1\n";
    if ( g )
    {
      ++st.num_div1_accepts;
      last_gain = num_and_mffc;
      "accept resub 1\n";
      return g; /* accepted resub */
    }

    //if ( max_inserts == 1 || num_mffc == 2 )
    //return std::nullopt;

    /* consider two nodes */
    //g = call_with_stopwatch( st.time_resub2, [&]() { return resub_div2( root, required ); } );
    /* if ( g )
    {
      ++st.num_div2_accepts;
      last_gain = num_and_mffc; // - 2;
      return g;                 /* accepted resub 
    }*/

    //if ( max_inserts == 2 || num_mffc == 3 )
    //return std::nullopt;

    /* consider three nodes 
    g = call_with_stopwatch( st.time_resub3, [&]() { return resub_div3( root, required ); } );
    if ( g )
    {
      ++st.num_div3_accepts;
      last_gain = num_and_mffc; // - 3;
      return g;                 /* accepted resub 
  }*/
    std::cout << "non fa nulla\n"; 
    return std::nullopt;
  }

  std::optional<signal> resub_const( node const& root, uint32_t required ) const
  {
    std::cout << "resub const \n";
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
    std::cout << "resub div0 \n";
    (void)required;
    auto const tt = sim.get_tt( ntk.make_signal( root ) );
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const d = divs.at( i );
      if ( tt != sim.get_tt( ntk.make_signal( d ) ) )
        continue;
      return ( sim.get_phase( d ) ^ sim.get_phase( root ) ) ? !ntk.make_signal( d ) : ntk.make_signal( d );
    }
    return std::nullopt;
  }

  std::optional<signal> resub_div1( node const& root, uint32_t required )
  {
    std::cout << "::::::::: \n";
    std::cout << ntk.node_to_index(root) << std::endl;
    std::cout << "resub div1 \n";
    (void)required;
    auto const& tt = sim.get_tt( ntk.make_signal( root ) );

    /* check for divisors  */
    for ( auto i = 0u; i < divs.size(); ++i )
   // ntk.foreach_node( [&]( auto n, auto i )
    {
      std::cout << i << std::endl; 
      auto const& s0 = divs.at( i );

     for ( auto j = i+1; j < divs.size(); ++j )
      {
        std::cout << j << std::endl; 
        auto const& s1 = divs.at( j );

        auto const& tt_s0 = sim.get_tt( ntk.make_signal( s0 ) );
        auto const& tt_s1 = sim.get_tt( ntk.make_signal( s1 ) );
        std::cout << to_binary(tt_s0) << std::endl; 
        std::cout << to_binary(tt_s1) << std::endl;
        std::cout << to_binary(tt_s0^tt_s1 ) << std::endl; 
        std::cout << to_binary(tt) << std::endl;

        if ( ( tt_s0 ^ tt_s1 ) == tt )
        { 
          std::cout << "PERFETTO 1\n";
          auto const l = sim.get_phase( s0 ) ? !ntk.make_signal( s0 ) : ntk.make_signal( s0 );
          std::cout << "PERFETTO 2\n";
          auto const r = sim.get_phase( s1 ) ? !ntk.make_signal( s1 ) : ntk.make_signal( s1 );
          std::cout << "PERFETTO 3\n";
          std::cout << "phase root: " << sim.get_phase( root ) << std::endl; 
          auto h = ntk.create_xor( l, r );
          std::cout << "PERFETTO\n";
          auto g = sim.get_phase( root ) ? !h : ntk.create_xor( l, r );
          
          return g;
        }
        else if ( ( tt_s0 ^ tt_s1 ) == kitty::unary_not( tt ))
        {
          auto const l = sim.get_phase( s0 ) ? !ntk.make_signal( s0 )  : ntk.make_signal( s0 ) ;
          auto const r = sim.get_phase( s1 ) ? !ntk.make_signal( s1 ) : ntk.make_signal( s1 );
          return sim.get_phase( root ) ? ntk.create_xor( l, r ) : !ntk.create_xor( l, r );
        }
      }
    }
    return std::nullopt;
  }

  /* std::optional<signal> resub_div2( node const& root, uint32_t required )
  {
    std::cout << "resub div2 \n";
    (void)required;
    auto const s = ntk.make_signal( root );
    auto const& tt = sim.get_tt( s );
    for ( auto i = 0u; i < divs.size(); ++i )
    {
      auto const s0 = divs.at( i );

      for ( auto j = i + 1; j < divs.size(); ++j )
      {
        auto const s1 = divs.at( j );

        for ( auto k = j + 1; k < divs.size(); ++k )
        {
          std::cout << " k = "<< k <<"\n";
          auto const s2 = divs.at( k );
          auto const& tt_s0 = sim.get_tt( ntk.make_signal( s0 ) );
          auto const& tt_s1 = sim.get_tt( ntk.make_signal( s1 ) );
          auto const& tt_s2 = sim.get_tt( ntk.make_signal( s2 ) );

          if ( ( tt_s0 ^ tt_s1 ^ tt_s2 ) == tt )
          {
            auto const max_level = std::max( {ntk.level( s0  ),
                                              ntk.level( s1  ),
                                              ntk.level(  s2  )} );
            assert( max_level <= required - 1 );

            signal max = ntk.make_signal( s0 );
            signal min0 = ntk.make_signal( s1 );
            signal min1 = ntk.make_signal( s2 );
            if ( ntk.level( s1  ) == max_level )
            {
              max = ntk.make_signal( s1 );
              min0 = ntk.make_signal( s0 );
              min1 = ntk.make_signal( s2 );
            }
            else if ( ntk.level(  s2  ) == max_level )
            {
              max = ntk.make_signal( s2 );
              min0 = ntk.make_signal( s0 );
              min1 = ntk.make_signal( s1 );
            }

            auto const a = sim.get_phase( ntk.get_node( max ) ) ? !max : max;
            auto const b = sim.get_phase( ntk.get_node( min0 ) ) ? !min0 : min0;
            auto const c = sim.get_phase( ntk.get_node( min1 ) ) ? !min1 : min1;

            return sim.get_phase( root ) ? !ntk.create_xor( a, ntk.create_xor( b, c ) ) : ntk.create_xor( a, ntk.create_xor( b, c ) );
          }
          else if ( ( tt_s0 ^ tt_s1 ^ tt_s2 ) == kitty::unary_not(tt) )
          {
            auto const max_level = std::max( {ntk.level( s0  ),
                                              ntk.level( s1  ),
                                              ntk.level(  s2  )} );
            assert( max_level <= required - 1 );

            signal max = ntk.make_signal( s0 );
            signal min0 = ntk.make_signal( s1 );
            signal min1 = ntk.make_signal( s2 );
            if ( ntk.level( s1  ) == max_level )
            {
              max = ntk.make_signal( s1 );
              min0 = ntk.make_signal( s0 );
              min1 = ntk.make_signal( s2 );
            }
            else if ( ntk.level(  s2  ) == max_level )
            {
              max = ntk.make_signal( s2 );
              min0 = ntk.make_signal( s0 );
              min1 = ntk.make_signal( s1 );
            }

            auto const a = sim.get_phase( ntk.get_node( max ) ) ? !max : max;
            auto const b = sim.get_phase( ntk.get_node( min0 ) ) ? !min0 : min0;
            auto const c = sim.get_phase( ntk.get_node( min1 ) ) ? !min1 : min1;

            return sim.get_phase( root ) ? ntk.create_xor( a, ntk.create_xor( b, c ) ) : !ntk.create_xor( a, ntk.create_xor( b, c ) );
          }
        }
      }
    }
    return std::nullopt;
  }*/

  /* per ora questo aspetterei a farlo : vediamo come va 
  std::optional<signal> resub_div3( node const& root, uint32_t required )
  {
    (void)required;

    auto const s = ntk.make_signal( root );
    auto const& tt = sim.get_tt( s );

    for ( auto i = 0u; i < bdivs.positive_divisors0.size(); ++i )
    {
      auto const s0 = bdivs.positive_divisors0.at( i );
      auto const s1 = bdivs.positive_divisors1.at( i );

      for ( auto j = i + 1; j < bdivs.positive_divisors0.size(); ++j )
      {
        auto const s2 = bdivs.positive_divisors0.at( j );
        auto const s3 = bdivs.positive_divisors1.at( j );

        auto const& tt_s0 = sim.get_tt( s0 );
        auto const& tt_s1 = sim.get_tt( s1 );
        auto const& tt_s2 = sim.get_tt( s2 );
        auto const& tt_s3 = sim.get_tt( s3 );

        if ( ( ( tt_s0 | tt_s1 ) & ( tt_s2 | tt_s3 ) ) == tt )
        {
          auto const a = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
          auto const b = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
          auto const c = sim.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;
          auto const d = sim.get_phase( ntk.get_node( s3 ) ) ? !s3 : s3;

          ++st.num_div3_and_2or_accepts;
          return sim.get_phase( root ) ? !ntk.create_and( ntk.create_or( a, b ), ntk.create_or( c, d ) ) : ntk.create_and( ntk.create_or( a, b ), ntk.create_or( c, d ) );
        }
      }
    }

    for ( auto i = 0u; i < bdivs.negative_divisors0.size(); ++i )
    {
      auto const s0 = bdivs.negative_divisors0.at( i );
      auto const s1 = bdivs.negative_divisors1.at( i );

      for ( auto j = i + 1; j < bdivs.negative_divisors0.size(); ++j )
      {
        auto const s2 = bdivs.negative_divisors0.at( j );
        auto const s3 = bdivs.negative_divisors1.at( j );

        auto const& tt_s0 = sim.get_tt( s0 );
        auto const& tt_s1 = sim.get_tt( s1 );
        auto const& tt_s2 = sim.get_tt( s2 );
        auto const& tt_s3 = sim.get_tt( s3 );

        if ( ( ( tt_s0 & tt_s1 ) | ( tt_s2 & tt_s3 ) ) == tt )
        {
          auto const a = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
          auto const b = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
          auto const c = sim.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;
          auto const d = sim.get_phase( ntk.get_node( s3 ) ) ? !s3 : s3;

          ++st.num_div3_or_2and_accepts;
          return sim.get_phase( root ) ? !ntk.create_or( ntk.create_and( a, b ), ntk.create_and( c, d ) ) : ntk.create_or( ntk.create_and( a, b ), ntk.create_and( c, d ) );
        }
      }
    }

    return std::nullopt;
  }*/

private:
  Ntk& ntk;
  Simulator const& sim;
  std::vector<node> const& divs;
  uint32_t const num_divs;
  stats& st;
}; /* xag_resub_functor */

template<class Ntk, class Simulator, class ResubFn>
class resubstitution_impl_xag
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit resubstitution_impl_xag( Ntk& ntk, resubstitution_params const& ps, resubstitution_stats& st, typename ResubFn::stats& resub_st )
      : ntk( ntk ), sim( ntk, ps.max_divisors, ps.max_pis ), ps( ps ), st( st ), resub_st( resub_st )
  {
    st.initial_size = ntk.num_gates();

    auto const update_level_of_new_node = [&]( const auto& n ) {
      ntk.resize_levels();
      update_node_level( n );
    };

    auto const update_level_of_existing_node = [&]( node const& n, const auto& old_children ) {
      (void)old_children;
      update_node_level( n );
    };

    auto const update_level_of_deleted_node = [&]( const auto& n ) {
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
    cut_manager<Ntk> mgr( ps.max_pis );

    const auto size = ntk.size();
    progress_bar pbar{ntk.size(), "resub |{0}| node = {1:>4}   cand = {2:>4}   est. gain = {3:>5}", ps.progress};

    ntk.foreach_gate( [&]( auto const& n, auto i ) {
      std::cout << " for each gate \n" << std::endl; 
      if ( i >= size )
        return false; /* terminate */

      pbar( i, i, candidates, st.estimated_gain );

      if ( ntk.is_dead( n ) )
        return true; /* next */

      /* skip nodes with many fanouts */
      if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
        return true; /* next */

      /* compute a reconvergence-driven cut */
      std::cout << "reconv cut\n";
      auto const leaves = call_with_stopwatch( st.time_cuts, [&]() {
        return reconv_driven_cut( mgr, ntk, n );
      } );

      /* evaluate this cut */
      std::cout << "evaluate\n";
      auto const g = call_with_stopwatch( st.time_eval, [&]() {
        return evaluate( n, leaves, ps.max_inserts );
      } );
      if ( !g )
      {
        return true; /* next */
      }
      
      std::cout << " finito evaluate\n";
      /* update progress bar */
      candidates++;
      st.estimated_gain += last_gain;

      /* update network */
      std::cout << "sub\n";
      call_with_stopwatch( st.time_substitute, [&]() {
        ntk.substitute_node( n, *g );
      } );

      return true; /* next */
    } );
  }

private:
  void update_node_level( node const& n, bool top_most = true )
  {
    uint32_t curr_level = ntk.level( n );

    uint32_t max_level = 0;
    ntk.foreach_fanin( n, [&]( const auto& f ) {
      auto const p = ntk.get_node( f );
      auto const fanin_level = ntk.level( p );
      if ( fanin_level > max_level )
      {
        max_level = fanin_level;
      }
    } );
    ++max_level;

    if ( curr_level != max_level )
    {
      ntk.set_level( n, max_level );

      /* update only one more level */
      if ( top_most )
      {
        ntk.foreach_fanout( n, [&]( const auto& p ) {
          update_node_level( p, false );
        } );
      }
    }
  }

  void simulate( std::vector<node> const& leaves )
  {
    sim.resize();
    for ( auto i = 0u; i < divs.size(); ++i )
    {
      const auto d = divs.at( i );

      /* skip constant 0 */
      if ( d == 0 )
        continue;

      /* assign leaves to variables */
      if ( i < leaves.size() )
      {
        sim.assign( d, i + 1 );
        continue;
      }

      /* compute truth tables of inner nodes */
      sim.assign( d, i - uint32_t( leaves.size() ) + ps.max_pis + 1 );
      std::vector<typename Simulator::truthtable_t> tts;
      ntk.foreach_fanin( d, [&]( const auto& s, auto i ) {
        (void)i;
        tts.emplace_back( sim.get_tt( ntk.make_signal( ntk.get_node( s ) ) ) ); /* ignore sign */
      } );

      auto const tt = ntk.compute( d, tts.begin(), tts.end() );
      sim.set_tt( i - uint32_t( leaves.size() ) + ps.max_pis + 1, tt );
    }

    /* normalize truth tables */
    sim.normalize( divs );
  }

  std::optional<signal> evaluate( node const& root, std::vector<node> const& leaves, uint32_t max_inserts )
  {
    (void)max_inserts;
    uint32_t const required = std::numeric_limits<uint32_t>::max();

    last_gain = 0;

    /* collect the MFFC */
    std::pair<int32_t, int32_t> num_mffc = call_with_stopwatch( st.time_mffc, [&]() {
      node_mffc_inside_xag collector( ntk );
      auto num_mffc = collector.run( root, leaves, temp );
      return num_mffc;
    } );

    /* collect the divisor nodes */
    bool div_comp_success = call_with_stopwatch( st.time_divs, [&]() {
      return collect_divisors( root, leaves, required );
    } );

    if ( !div_comp_success )
    {
      return std::nullopt;
    }

    /* update statistics */
    st.num_total_divisors += num_divs;
    st.num_total_leaves += leaves.size();

    /* simulate the nodes */
    call_with_stopwatch( st.time_simulation, [&]() { simulate( leaves ); } );

    // TODO here I have all the nodes, thus I can create the map.
    ResubFn resub_fn( ntk, sim, divs, num_divs, resub_st );
    std::cout << "resub\n";
    return resub_fn( root, required, ps.max_inserts, num_mffc.first, num_mffc.second, last_gain );
  }

  void collect_divisors_rec( node const& n, std::vector<node>& internal )
  {
    /* skip visited nodes */
    if ( ntk.visited( n ) == ntk.trav_id() )
      return;
    ntk.set_visited( n, ntk.trav_id() );

    ntk.foreach_fanin( n, [&]( const auto& f ) {
      collect_divisors_rec( ntk.get_node( f ), internal );
    } );

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
      ntk.foreach_fanout( d, [&]( node const& p ) {
        if ( ntk.visited( p ) == ntk.trav_id() || ntk.level( p ) > required )
          return true; /* next fanout */

        bool all_fanins_visited = true;
        ntk.foreach_fanin( p, [&]( const auto& g ) {
          if ( ntk.visited( ntk.get_node( g ) ) != ntk.trav_id() )
          {
            all_fanins_visited = false;
            return false; /* terminate fanin-loop */
          }
          return true; /* next fanin */
        } );

        if ( !all_fanins_visited )
          return true; /* next fanout */

        bool has_root_as_child = false;
        ntk.foreach_fanin( p, [&]( const auto& g ) {
          if ( ntk.get_node( g ) == root )
          {
            has_root_as_child = true;
            return false; /* terminate fanin-loop */
          }
          return true; /* next fanin */
        } );

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
      } );

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

    assert( root == divs.at( divs.size() - 1u ) );
    assert( divs.size() - leaves.size() <= ps.max_divisors - ps.max_pis );

    return true;
  }

private:
  Ntk& ntk;
  Simulator sim;

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

template<class Ntk>
void resubstitution_minmc( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  std::cout << " start minmn \n";
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

  resubstitution_stats st;
  if ( ps.max_pis == 8 )
  {
    using truthtable_t = kitty::static_truth_table<8>;
    using simulator_t = detail::simulator<resub_view_t, truthtable_t>;
    using resubstitution_functor_t = detail::xag_resub_functor<resub_view_t, simulator_t>;
    typename resubstitution_functor_t::stats resub_st;
    detail::resubstitution_impl_xag<resub_view_t, simulator_t, resubstitution_functor_t> p( resub_view, ps, st, resub_st );
    std::cout << "run \n";
    p.run();
    if ( ps.verbose )
    {
      st.report();
      resub_st.report();
    }
  }
  else
  {
    using truthtable_t = kitty::dynamic_truth_table;
    using simulator_t = detail::simulator<resub_view_t, truthtable_t>;
    using resubstitution_functor_t = detail::xag_resub_functor<resub_view_t, simulator_t>; // Xag resub is the default here
    typename resubstitution_functor_t::stats resub_st;
    detail::resubstitution_impl_xag<resub_view_t, simulator_t, resubstitution_functor_t> p( resub_view, ps, st, resub_st );
    p.run();
    if ( ps.verbose )
    {
      st.report();
      resub_st.report();
    }
  }

  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */
