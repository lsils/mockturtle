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

#include "dont_cares.hpp"
#include <kitty/operations.hpp>
#include <kitty/print.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/algorithms/xag_resub.hpp>
#include <mockturtle/networks/xag.hpp>

#include "../traits.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/depth_view.hpp"
#include "../views/fanout_view2.hpp"
#include "reconv_cut2.hpp"

namespace mockturtle
{

namespace detail
{

template<typename Ntk, typename Simulator, typename TT>
struct xag_withDC_resub_functor
{
public:
  using node = xag_network::node;
  using signal = xag_network::signal;
  using stats = xag_resub_stats;

  struct unate_divisors
  {
    using signal = typename xag_network::signal;

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
    using signal = typename xag_network::signal;

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

public:
  explicit xag_withDC_resub_functor( Ntk& ntk, Simulator const& sim, std::vector<node> const& divs, uint32_t num_divs, stats& st )
      : ntk( ntk ), sim( sim ), divs( divs ), num_divs( num_divs ), st( st )
  {
  }

  std::optional<signal> operator()( node const& root, TT care, uint32_t required, uint32_t max_inserts, uint32_t num_and_mffc, uint32_t num_xor_mffc, uint32_t& last_gain )
  {
    /* consider constants */
    auto g = call_with_stopwatch( st.time_resubC, [&]() {
      return resub_const( root, care, required );
    } );
    if ( g )
    {
      ++st.num_const_accepts;
      last_gain = num_and_mffc;
      return g; /* accepted resub */
    }

    /* consider equal nodes */
    g = call_with_stopwatch( st.time_resub0, [&]() {
      return resub_div0( root, care,  required );
    } );
    if ( g )
    {
      ++st.num_div0_accepts;
      last_gain = num_and_mffc;
      return g; /* accepted resub */
    }

    if ( num_and_mffc == 0 )
    {
      if ( max_inserts == 0 || num_xor_mffc == 1 )
        return std::nullopt;

      g = call_with_stopwatch( st.time_resub1, [&]() {
        return resub_div1( root, care, required );
      } );
      if ( g )
      {
        ++st.num_div1_accepts;
        last_gain = 0;
        return g; /* accepted resub */
      }

      if ( max_inserts == 1 || num_xor_mffc == 2 )
        return std::nullopt;

      /* consider two nodes */
      g = call_with_stopwatch( st.time_resub2, [&]() { return resub_div2( root, care, required ); } );
      if ( g )
      {
        ++st.num_div2_accepts;
        last_gain = 0;
        return g; /* accepted resub */
      }
    }
    else
    {

      g = call_with_stopwatch( st.time_resub1, [&]() {
        return resub_div1( root, care, required );
      } );
      if ( g )
      {
        ++st.num_div1_accepts;
        last_gain = num_and_mffc;
        return g; /* accepted resub */
      }

      /* consider two nodes */
      g = call_with_stopwatch( st.time_resub2, [&]() { return resub_div2( root, care, required ); } );
      if ( g )
      {
        ++st.num_div2_accepts;
        last_gain = num_and_mffc;
        return g; /*  accepted resub */
      }

      if ( num_and_mffc < 2 ) /* it is worth trying also AND resub here */
        return std::nullopt;

      /* collect level one divisors */
      call_with_stopwatch( st.time_collect_unate_divisors, [&]() {
        collect_unate_divisors( root, required );
      } );

      g = call_with_stopwatch( st.time_resub1_and, [&]() { return resub_div1_and( root, care, required ); } );
      if ( g )
      {
        ++st.num_div1_and_accepts;
        last_gain = num_and_mffc - 1;
        return g; /*  accepted resub */
      }
      if ( num_and_mffc < 3 ) /* it is worth trying also AND-12 resub here */
        return std::nullopt;

      /* consider triples */
      g = call_with_stopwatch( st.time_resub12, [&]() { return resub_div12( root, care, required ); } );
      if ( g )
      {
        ++st.num_div12_accepts;
        last_gain = num_and_mffc - 2;
        return g; /* accepted resub */
      }

      /* collect level two divisors */
      call_with_stopwatch( st.time_collect_binate_divisors, [&]() {
        collect_binate_divisors( root, required );
      } );

      /* consider two nodes */
      g = call_with_stopwatch( st.time_resub2_and, [&]() { return resub_div2_and( root, care, required ); } );
      if ( g )
      {
        ++st.num_div2_and_accepts;
        last_gain = num_and_mffc - 2;
        return g; /* accepted resub */
      }
    }
    return std::nullopt;
  }

  std::optional<signal> resub_const( node const& root, TT care, uint32_t required ) const
  {
    (void)required;
    auto tt = sim.get_tt( ntk.make_signal( root ) );
    if ( binary_and(tt, care) == sim.get_tt( ntk.get_constant( false ) ) )
    {
      return sim.get_phase( root ) ? ntk.get_constant( true ) : ntk.get_constant( false );
    }
    return std::nullopt;
  }

  std::optional<signal> resub_div0( node const& root, TT care, uint32_t required ) const
  {
    (void)required;
    auto const tt = sim.get_tt( ntk.make_signal( root ) );
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const d = divs.at( i );
      if ( binary_and(tt, care) != binary_and(sim.get_tt( ntk.make_signal( d ) ), care) )
        continue;
      return ( sim.get_phase( d ) ^ sim.get_phase( root ) ) ? !ntk.make_signal( d ) : ntk.make_signal( d );
    }
    return std::nullopt;
  }

  std::optional<signal> resub_div1( node const& root, TT care, uint32_t required )
  {
    (void)required;
    auto const& tt = sim.get_tt( ntk.make_signal( root ) );

    /* check for divisors  */
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const& s0 = divs.at( i );

      for ( auto j = i + 1; j < num_divs; ++j )
      {
        auto const& s1 = divs.at( j );
        auto const& tt_s0 = sim.get_tt( ntk.make_signal( s0 ) );
        auto const& tt_s1 = sim.get_tt( ntk.make_signal( s1 ) );

        if ( binary_and(( tt_s0 ^ tt_s1 ), care) == binary_and(tt, care) )
        {
          auto const l = sim.get_phase( s0 ) ? !ntk.make_signal( s0 ) : ntk.make_signal( s0 );
          auto const r = sim.get_phase( s1 ) ? !ntk.make_signal( s1 ) : ntk.make_signal( s1 );
          return sim.get_phase( root ) ? !ntk.create_xor( l, r ) : ntk.create_xor( l, r );
        }
        else if ( binary_and(( tt_s0 ^ tt_s1 ),care) == binary_and(kitty::unary_not( tt ), care) )
        {
          auto const l = sim.get_phase( s0 ) ? !ntk.make_signal( s0 ) : ntk.make_signal( s0 );
          auto const r = sim.get_phase( s1 ) ? !ntk.make_signal( s1 ) : ntk.make_signal( s1 );
          return sim.get_phase( root ) ? ntk.create_xor( l, r ) : !ntk.create_xor( l, r );
        }
      }
    }
    return std::nullopt;
  }

  std::optional<signal> resub_div2( node const& root, TT care, uint32_t required )
  {
    (void)required;
    auto const s = ntk.make_signal( root );
    auto const& tt = sim.get_tt( s );
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const s0 = divs.at( i );

      for ( auto j = i + 1; j < num_divs; ++j )
      {
        auto const s1 = divs.at( j );

        for ( auto k = j + 1; k < num_divs; ++k )
        {
          auto const s2 = divs.at( k );
          auto const& tt_s0 = sim.get_tt( ntk.make_signal( s0 ) );
          auto const& tt_s1 = sim.get_tt( ntk.make_signal( s1 ) );
          auto const& tt_s2 = sim.get_tt( ntk.make_signal( s2 ) );

          if ( binary_and(( tt_s0 ^ tt_s1 ^ tt_s2 ), care) == binary_and(tt, care) )
          {
            auto const max_level = std::max( {ntk.level( s0 ),
                                              ntk.level( s1 ),
                                              ntk.level( s2 )} );
            assert( max_level <= required - 1 );

            signal max = ntk.make_signal( s0 );
            signal min0 = ntk.make_signal( s1 );
            signal min1 = ntk.make_signal( s2 );
            if ( ntk.level( s1 ) == max_level )
            {
              max = ntk.make_signal( s1 );
              min0 = ntk.make_signal( s0 );
              min1 = ntk.make_signal( s2 );
            }
            else if ( ntk.level( s2 ) == max_level )
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
          else if ( binary_and(( tt_s0 ^ tt_s1 ^ tt_s2 ), care) == binary_and(kitty::unary_not( tt ), care) )
          {
            auto const max_level = std::max( {ntk.level( s0 ),
                                              ntk.level( s1 ),
                                              ntk.level( s2 )} );
            assert( max_level <= required - 1 );

            signal max = ntk.make_signal( s0 );
            signal min0 = ntk.make_signal( s1 );
            signal min1 = ntk.make_signal( s2 );
            if ( ntk.level( s1 ) == max_level )
            {
              max = ntk.make_signal( s1 );
              min0 = ntk.make_signal( s0 );
              min1 = ntk.make_signal( s2 );
            }
            else if ( ntk.level( s2 ) == max_level )
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
  }

  void collect_unate_divisors( node const& root, uint32_t required )
  {
    udivs.clear();

    auto const& tt = sim.get_tt( ntk.make_signal( root ) );
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const d = divs.at( i );

      if ( ntk.level( d ) > required - 1 )
        continue;

      auto const& tt_d = sim.get_tt( ntk.make_signal( d ) );

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

  std::optional<signal> resub_div1_and( node const& root, TT care, uint32_t required )
  {
    (void)required;
    auto const& tt = sim.get_tt( ntk.make_signal( root ) );

    /* check for positive unate divisors */
    for ( auto i = 0u; i < udivs.positive_divisors.size(); ++i )
    {
      auto const& s0 = udivs.positive_divisors.at( i );

      for ( auto j = i + 1; j < udivs.positive_divisors.size(); ++j )
      {
        auto const& s1 = udivs.positive_divisors.at( j );

        auto const& tt_s0 = sim.get_tt( s0 );
        auto const& tt_s1 = sim.get_tt( s1 );

        if ( binary_and(( tt_s0 | tt_s1 ), care) == binary_and(tt, care) )
        {
          auto const l = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
          auto const r = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
          return sim.get_phase( root ) ? !ntk.create_or( l, r ) : ntk.create_or( l, r );
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

        auto const& tt_s0 = sim.get_tt( s0 );
        auto const& tt_s1 = sim.get_tt( s1 );

        if ( binary_and(( tt_s0 & tt_s1 ), care) == binary_and(tt, care) )
        {
          auto const l = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
          auto const r = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
          return sim.get_phase( root ) ? !ntk.create_and( l, r ) : ntk.create_and( l, r );
        }
      }
    }

    return std::nullopt;
  }

  std::optional<signal> resub_div12( node const& root, TT care, uint32_t required )
  {
    (void)required;
    auto const s = ntk.make_signal( root );
    auto const& tt = sim.get_tt( s );

    /* check positive unate divisors */
    for ( auto i = 0u; i < udivs.positive_divisors.size(); ++i )
    {
      auto const s0 = udivs.positive_divisors.at( i );

      for ( auto j = i + 1; j < udivs.positive_divisors.size(); ++j )
      {
        auto const s1 = udivs.positive_divisors.at( j );

        for ( auto k = j + 1; k < udivs.positive_divisors.size(); ++k )
        {
          auto const s2 = udivs.positive_divisors.at( k );

          auto const& tt_s0 = sim.get_tt( s0 );
          auto const& tt_s1 = sim.get_tt( s1 );
          auto const& tt_s2 = sim.get_tt( s2 );

          if ( binary_and(( tt_s0 | tt_s1 | tt_s2 ), care) == binary_and(tt, care) )
          {
            auto const max_level = std::max( {ntk.level( ntk.get_node( s0 ) ),
                                              ntk.level( ntk.get_node( s1 ) ),
                                              ntk.level( ntk.get_node( s2 ) )} );
            assert( max_level <= required - 1 );

            signal max = s0;
            signal min0 = s1;
            signal min1 = s2;
            if ( ntk.level( ntk.get_node( s1 ) ) == max_level )
            {
              max = s1;
              min0 = s0;
              min1 = s2;
            }
            else if ( ntk.level( ntk.get_node( s2 ) ) == max_level )
            {
              max = s2;
              min0 = s0;
              min1 = s1;
            }

            auto const a = sim.get_phase( ntk.get_node( max ) ) ? !max : max;
            auto const b = sim.get_phase( ntk.get_node( min0 ) ) ? !min0 : min0;
            auto const c = sim.get_phase( ntk.get_node( min1 ) ) ? !min1 : min1;

            return sim.get_phase( root ) ? !ntk.create_or( a, ntk.create_or( b, c ) ) : ntk.create_or( a, ntk.create_or( b, c ) );
          }
        }
      }
    }

    /* check negative unate divisors */
    for ( auto i = 0u; i < udivs.positive_divisors.size(); ++i )
    {
      auto const s0 = udivs.positive_divisors.at( i );

      for ( auto j = i + 1; j < udivs.positive_divisors.size(); ++j )
      {
        auto const s1 = udivs.positive_divisors.at( j );

        for ( auto k = j + 1; k < udivs.positive_divisors.size(); ++k )
        {
          auto const s2 = udivs.positive_divisors.at( k );

          auto const& tt_s0 = sim.get_tt( s0 );
          auto const& tt_s1 = sim.get_tt( s1 );
          auto const& tt_s2 = sim.get_tt( s2 );

          if ( binary_and(( tt_s0 & tt_s1 & tt_s2 ), care) == binary_and(tt, care) )
          {
            auto const max_level = std::max( {ntk.level( ntk.get_node( s0 ) ),
                                              ntk.level( ntk.get_node( s1 ) ),
                                              ntk.level( ntk.get_node( s2 ) )} );
            assert( max_level <= required - 1 );

            signal max = s0;
            signal min0 = s1;
            signal min1 = s2;
            if ( ntk.level( ntk.get_node( s1 ) ) == max_level )
            {
              max = s1;
              min0 = s0;
              min1 = s2;
            }
            else if ( ntk.level( ntk.get_node( s2 ) ) == max_level )
            {
              max = s2;
              min0 = s0;
              min1 = s1;
            }

            auto const a = sim.get_phase( ntk.get_node( max ) ) ? !max : max;
            auto const b = sim.get_phase( ntk.get_node( min0 ) ) ? !min0 : min0;
            auto const c = sim.get_phase( ntk.get_node( min1 ) ) ? !min1 : min1;

            return sim.get_phase( root ) ? !ntk.create_and( a, ntk.create_and( b, c ) ) : ntk.create_and( a, ntk.create_and( b, c ) );
          }
        }
      }
    }

    return std::nullopt;
  }

  void collect_binate_divisors( node const& root, uint32_t required )
  {
    bdivs.clear();

    auto const& tt = sim.get_tt( ntk.make_signal( root ) );
    for ( auto i = 0u; i < udivs.next_candidates.size(); ++i )
    {
      auto const& s0 = udivs.next_candidates.at( i );
      if ( ntk.level( ntk.get_node( s0 ) ) > required - 2 )
        continue;

      for ( auto j = i + 1; j < udivs.next_candidates.size(); ++j )
      {
        auto const& s1 = udivs.next_candidates.at( j );
        if ( ntk.level( ntk.get_node( s1 ) ) > required - 2 )
          continue;

        if ( bdivs.positive_divisors0.size() < 500 ) // ps.max_divisors2
        {
          auto const& tt_s0 = sim.get_tt( s0 );
          auto const& tt_s1 = sim.get_tt( s1 );
          if ( kitty::implies( tt_s0 & tt_s1, tt ) )
          {
            bdivs.positive_divisors0.emplace_back( s0 );
            bdivs.positive_divisors1.emplace_back( s1 );
          }

          if ( kitty::implies( ~tt_s0 & tt_s1, tt ) )
          {
            bdivs.positive_divisors0.emplace_back( !s0 );
            bdivs.positive_divisors1.emplace_back( s1 );
          }

          if ( kitty::implies( tt_s0 & ~tt_s1, tt ) )
          {
            bdivs.positive_divisors0.emplace_back( s0 );
            bdivs.positive_divisors1.emplace_back( !s1 );
          }

          if ( kitty::implies( ~tt_s0 & ~tt_s1, tt ) )
          {
            bdivs.positive_divisors0.emplace_back( !s0 );
            bdivs.positive_divisors1.emplace_back( !s1 );
          }
        }

        if ( bdivs.negative_divisors0.size() < 500 ) // ps.max_divisors2
        {
          auto const& tt_s0 = sim.get_tt( s0 );
          auto const& tt_s1 = sim.get_tt( s1 );
          if ( kitty::implies( tt, tt_s0 & tt_s1 ) )
          {
            bdivs.negative_divisors0.emplace_back( s0 );
            bdivs.negative_divisors1.emplace_back( s1 );
          }

          if ( kitty::implies( tt, ~tt_s0 & tt_s1 ) )
          {
            bdivs.negative_divisors0.emplace_back( !s0 );
            bdivs.negative_divisors1.emplace_back( s1 );
          }

          if ( kitty::implies( tt, tt_s0 & ~tt_s1 ) )
          {
            bdivs.negative_divisors0.emplace_back( s0 );
            bdivs.negative_divisors1.emplace_back( !s1 );
          }

          if ( kitty::implies( tt, ~tt_s0 & ~tt_s1 ) )
          {
            bdivs.negative_divisors0.emplace_back( !s0 );
            bdivs.negative_divisors1.emplace_back( !s1 );
          }
        }
      }
    }
  }

  std::optional<signal> resub_div2_and( node const& root, TT care, uint32_t required )
  {
    (void)required;
    auto const s = ntk.make_signal( root );
    auto const& tt = sim.get_tt( s );

    /* check positive unate divisors */
    for ( const auto& s0 : udivs.positive_divisors )
    {
      auto const& tt_s0 = sim.get_tt( s0 );

      for ( auto j = 0u; j < bdivs.positive_divisors0.size(); ++j )
      {
        auto const s1 = bdivs.positive_divisors0.at( j );
        auto const s2 = bdivs.positive_divisors1.at( j );

        auto const& tt_s1 = sim.get_tt( s1 );
        auto const& tt_s2 = sim.get_tt( s2 );

        auto const a = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
        auto const b = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
        auto const c = sim.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;

        if ( binary_and(( tt_s0 | ( tt_s1 & tt_s2 ) ), care) == binary_and(tt, care) )
        {
          return sim.get_phase( root ) ? !ntk.create_or( a, ntk.create_and( b, c ) ) : ntk.create_or( a, ntk.create_and( b, c ) );
        }
      }
    }

    /* check negative unate divisors */
    for ( const auto& s0 : udivs.negative_divisors )
    {
      auto const& tt_s0 = sim.get_tt( s0 );

      for ( auto j = 0u; j < bdivs.negative_divisors0.size(); ++j )
      {
        auto const s1 = bdivs.negative_divisors0.at( j );
        auto const s2 = bdivs.negative_divisors1.at( j );

        auto const& tt_s1 = sim.get_tt( s1 );
        auto const& tt_s2 = sim.get_tt( s2 );

        auto const a = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
        auto const b = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
        auto const c = sim.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;

        if ( binary_and(( tt_s0 | ( tt_s1 & tt_s2 ) ), care) == binary_and(tt, care) )
        {
          return sim.get_phase( root ) ? !ntk.create_and( a, ntk.create_or( b, c ) ) : ntk.create_and( a, ntk.create_or( b, c ) );
        }
      }
    }

    return std::nullopt;
  }

private:
  Ntk& ntk;
  Simulator const& sim;
  std::vector<node> const& divs;
  uint32_t const num_divs;
  stats& st;

  unate_divisors udivs;
  binate_divisors bdivs;
}; // namespace detail

template<class Ntk, class Simulator, class ResubFn, class TT>
class resubstitution_impl_xag_withDC
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit resubstitution_impl_xag_withDC( Ntk& ntk, resubstitution_params const& ps, resubstitution_stats& st, typename ResubFn::stats& resub_st )
      : ntk( ntk ), sim( ntk, ps.max_divisors, ps.max_pis ), ps( ps ), st( st ), resub_st( resub_st )
  {
    st.initial_size = ntk.num_gates();

    auto const update_level_of_new_node = [&]( const auto& n ) {
      ntk.resize_levels();
      ntk.resize_fanout();
      update_node_level( n );
      update_node_fanout( n );
    };

    auto const update_level_of_existing_node = [&]( node const& n, const auto& old_children ) {
      (void)old_children;
      ntk.resize_levels();
      ntk.resize_fanout();
      update_node_level( n );
      update_node_fanout( n );
    };

    auto const update_level_of_deleted_node = [&]( const auto& n ) {
      ntk.set_level( n, -1 );
      ntk.update_fanout();
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
      } );

      /* evaluate this cut */
      auto const g = call_with_stopwatch( st.time_eval, [&]() {
        return evaluate( n, leaves, ps.max_inserts );
      } );
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

  void update_node_fanout( node const& n )
  {
    auto curr_fanout = ntk.fanout( n );
    std::vector<node> new_fanout;
    ntk.foreach_fanout( n, [&]( const auto& p ) { new_fanout.push_back( p ); } );

    if ( curr_fanout != new_fanout )
    {
      ntk.set_fanout( n, new_fanout );
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

    TT care = ~satisfiability_dont_cares( ntk, leaves, 12u );

    ResubFn resub_fn( ntk, sim, divs, num_divs, resub_st );
    return resub_fn( root, care, required, ps.max_inserts, num_mffc.first, num_mffc.second, last_gain );
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

} // namespace detail

template<class Ntk>
void resubstitution_minmc_withDC( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
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

  using truthtable_t = kitty::dynamic_truth_table;
  using simulator_t = detail::simulator<resub_view_t, truthtable_t>;
  using resubstitution_functor_t = detail::xag_withDC_resub_functor<resub_view_t, simulator_t, truthtable_t>; // Xag resub is the default here
  typename resubstitution_functor_t::stats resub_st;
  detail::resubstitution_impl_xag_withDC<resub_view_t, simulator_t, resubstitution_functor_t, truthtable_t> p( resub_view, ps, st, resub_st );
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

} // namespace mockturtle
