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
  \file mig_resub.hpp
  \brief Majority-specific resustitution rules

  \author Heinz Riener
*/

#pragma once

#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/algorithms/mig_resyn_engines.hpp>
#include <mockturtle/utils/index_list.hpp>

namespace kitty
{

/*! \brief Relevance */
inline bool relevance( const dynamic_truth_table& tt0, const dynamic_truth_table& tt1, const dynamic_truth_table& tt2, const dynamic_truth_table& tt )
{
  return is_const0( ( ( tt0 ^ tt ) & ( tt1 ^ tt2 ) ) );
}

/*! \brief Relevance */
template<uint32_t NumVars>
inline bool relevance( const static_truth_table<NumVars>& tt0, const static_truth_table<NumVars>& tt1, const static_truth_table<NumVars>& tt2, const static_truth_table<NumVars>& tt )
{
  return is_const0( ( ( tt0 ^ tt ) & ( tt1 ^ tt2 ) ) );
}

} /* namespace kitty */

namespace mockturtle
{

struct mig_exhaustive_resub_stats
{
  /*! \brief Accumulated runtime for const-resub */
  stopwatch<>::duration time_resubC{0};

  /*! \brief Accumulated runtime for zero-resub */
  stopwatch<>::duration time_resub0{0};

  /*! \brief Accumulated runtime for collecting unate divisors. */
  stopwatch<>::duration time_collect_unate_divisors{0};

  /*! \brief Accumulated runtime for one-resub */
  stopwatch<>::duration time_resub1{0};

  /*! \brief Accumulated runtime for relevance resub */
  stopwatch<>::duration time_resubR{0};

  /*! \brief Accumulated runtime for collecting unate divisors. */
  stopwatch<>::duration time_collect_binate_divisors{0};

  /*! \brief Accumulated runtime for two-resub. */
  stopwatch<>::duration time_resub2{0};

  /*! \brief Number of accepted constant resubsitutions */
  uint32_t num_const_accepts{0};

  /*! \brief Number of accepted zero resubsitutions */
  uint32_t num_div0_accepts{0};

  /*! \brief Number of accepted one resubsitutions */
  uint64_t num_div1_accepts{0};

  /*! \brief Number of accepted relevance resubsitutions */
  uint32_t num_divR_accepts{0};

  /*! \brief Number of accepted two resubsitutions */
  uint64_t num_div2_accepts{0};

  void report() const
  {
    std::cout << "[i] kernel: mig_exhaustive_resub_functor\n";
    std::cout << fmt::format( "[i]     constant-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_const_accepts, to_seconds( time_resubC ) );
    std::cout << fmt::format( "[i]            0-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_div0_accepts, to_seconds( time_resub0 ) );
    std::cout << fmt::format( "[i]            R-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_divR_accepts, to_seconds( time_resubR ) );
    std::cout << fmt::format( "[i]            collect unate divisors                           ({:>5.2f} secs)\n", to_seconds( time_collect_unate_divisors ) );
    std::cout << fmt::format( "[i]            1-resub {:6d} = {:6d} MAJ                      ({:>5.2f} secs)\n",
                              num_div1_accepts, num_div1_accepts, to_seconds( time_resub1 ) );
    std::cout << fmt::format( "[i]            collect binate divisors                          ({:>5.2f} secs)\n", to_seconds( time_collect_binate_divisors ) );
    std::cout << fmt::format( "[i]            2-resub {:6d} = {:6d} 2MAJ                     ({:>5.2f} secs)\n",
                               num_div2_accepts, num_div2_accepts, to_seconds( time_resub2 ) );
    std::cout << fmt::format( "[i]            total   {:6d}\n",
                              (num_const_accepts + num_div0_accepts + num_divR_accepts + num_div1_accepts + num_div2_accepts) );
  }
}; /* mig_exhaustive_resub_stats */

template<typename Ntk, typename Simulator, typename TT, bool use_constant = true>
struct mig_exhaustive_resub_functor
{
public:
  using node = mig_network::node;
  using signal = mig_network::signal;
  using stats = mig_exhaustive_resub_stats;

  struct unate_divisors
  {
    std::vector<signal> u0;
    std::vector<signal> u1;
    std::vector<signal> next_candidates;

    void clear()
    {
      u0.clear();
      u1.clear();
      next_candidates.clear();
    }
  };

  struct binate_divisors
  {
    std::vector<signal> positive_divisors0;
    std::vector<signal> positive_divisors1;
    std::vector<signal> positive_divisors2;
    std::vector<signal> negative_divisors0;
    std::vector<signal> negative_divisors1;
    std::vector<signal> negative_divisors2;

    void clear()
    {
      positive_divisors0.clear();
      positive_divisors1.clear();
      positive_divisors2.clear();
      negative_divisors0.clear();
      negative_divisors1.clear();
      negative_divisors2.clear();
    }
  };

public:
  explicit mig_exhaustive_resub_functor( Ntk& ntk, Simulator const& sim, std::vector<node> const& divs, uint32_t num_divs, stats& st )
    : ntk( ntk )
    , sim( sim )
    , divs( divs )
    , num_divs( num_divs )
    , st( st )
  {
  }

  std::optional<signal> operator()( node const& root, TT care, uint32_t required, uint32_t max_inserts, uint32_t num_mffc, uint32_t& last_gain )
  {
    (void)care;
    assert(is_const0(~care));

    /* consider constants */
    auto g = call_with_stopwatch( st.time_resubC, [&]() {
        return resub_const( root, required );
      } );
    if ( g )
    {
      ++st.num_const_accepts;
      last_gain = num_mffc;
      return g; /* accepted resub */
    }

    /* consider equal nodes */
    g = call_with_stopwatch( st.time_resub0, [&]() {
        return resub_div0( root, required );
      } );
    if ( g )
    {
      ++st.num_div0_accepts;
      last_gain = num_mffc;
      return g; /* accepted resub */
    }

    /* consider relevance optimization */
    g = call_with_stopwatch( st.time_resubR, [&]() {
        return resub_divR( root, required );
      });
    if ( g )
    {
      ++st.num_divR_accepts;
      last_gain = num_mffc;
      return g; /* accepted resub */
    }

    if ( max_inserts == 0 || num_mffc == 1 )
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

    if ( max_inserts == 1 || num_mffc == 2 )
      return std::nullopt;

    /* collect level two divisors */
    call_with_stopwatch( st.time_collect_binate_divisors, [&]() {
        collect_binate_divisors( root, required );
      });

    /* consider two nodes */
    g = call_with_stopwatch( st.time_resub2, [&]() {
        return resub_div2( root, required ); });
    if ( g )
    {
      ++st.num_div2_accepts;
      last_gain = num_mffc - 2;
      return g; /* accepted resub */
    }

    return std::nullopt;
  }

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
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const d = divs.at( i );

      if ( tt != sim.get_tt( ntk.make_signal( d ) ) )
        continue; /* next */

      return ( sim.get_phase( d ) ^ sim.get_phase( root ) ) ? !ntk.make_signal( d ) : ntk.make_signal( d );
    }

    return std::nullopt;
  }

  std::optional<signal> resub_divR( node const& root, uint32_t required )
  {
    (void)required;

    std::vector<signal> fs;
    ntk.foreach_fanin( root, [&]( const auto& f ){
        fs.emplace_back( f );
      });

    for ( auto i = 0u; i < divs.size(); ++i )
    {
      auto const& d0 = divs.at( i );
      auto const& s = ntk.make_signal( d0 );
      auto const& tt = sim.get_tt( s );

      if ( d0 == root )
        break;

      auto const tt0 = sim.get_tt( fs[0] );
      auto const tt1 = sim.get_tt( fs[1] );
      auto const tt2 = sim.get_tt( fs[2] );

      if ( ntk.get_node( fs[0] ) != d0 && ntk.fanout_size( ntk.get_node( fs[0] ) ) == 1 && relevance( tt0, tt1, tt2, tt ) )
      {
        auto const b = sim.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];
        auto const c = sim.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sim.get_phase( root ) ?
          !ntk.create_maj( sim.get_phase( d0 ) ? !s : s, b, c ) :
           ntk.create_maj( sim.get_phase( d0 ) ? !s : s, b, c );
      }
      else if ( ntk.get_node( fs[1] ) != d0 && ntk.fanout_size( ntk.get_node( fs[1] ) ) == 1 && relevance( tt1, tt0, tt2, tt ) )
      {
        auto const a = sim.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const c = sim.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sim.get_phase( root ) ?
          !ntk.create_maj( sim.get_phase( d0 ) ? !s : s, a, c ) :
           ntk.create_maj( sim.get_phase( d0 ) ? !s : s, a, c );
      }
      else if ( ntk.get_node( fs[2] ) != d0 && ntk.fanout_size( ntk.get_node( fs[2] ) ) == 1 && relevance( tt2, tt0, tt1, tt ) )
      {
        auto const a = sim.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const b = sim.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];

        return sim.get_phase( root ) ?
          !ntk.create_maj( sim.get_phase( d0 ) ? !s : s, a, b ) :
          ntk.create_maj( sim.get_phase( d0 ) ? !s : s, a, b );
      }
      else if ( ntk.get_node( fs[0] ) != d0 && ntk.fanout_size( ntk.get_node( fs[0] ) ) == 1 && relevance( ~tt0, tt1, tt2, tt ) )
      {
        auto const b = sim.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];
        auto const c = sim.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sim.get_phase( root ) ?
          !ntk.create_maj( sim.get_phase( d0 ) ? s : !s, b, c ) :
           ntk.create_maj( sim.get_phase( d0 ) ? s : !s, b, c );
      }
      else if ( ntk.get_node( fs[1] ) != d0 && ntk.fanout_size( ntk.get_node( fs[1] ) ) == 1 && relevance( ~tt1, tt0, tt2, tt ) )
      {
        auto const a = sim.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const c = sim.get_phase( ntk.get_node( fs[2] ) ) ? !fs[2] : fs[2];

        return sim.get_phase( root ) ?
          !ntk.create_maj( sim.get_phase( d0 ) ? s : !s, a, c ) :
           ntk.create_maj( sim.get_phase( d0 ) ? s : !s, a, c );
      }
      else if ( ntk.get_node( fs[2] ) != d0 && ntk.fanout_size( ntk.get_node( fs[2] ) ) == 1 && relevance( ~tt2, tt0, tt1, tt ) )
      {
        auto const a = sim.get_phase( ntk.get_node( fs[0] ) ) ? !fs[0] : fs[0];
        auto const b = sim.get_phase( ntk.get_node( fs[1] ) ) ? !fs[1] : fs[1];

        return sim.get_phase( root ) ?
          !ntk.create_maj( sim.get_phase( d0 ) ? s : !s, a, b ) :
           ntk.create_maj( sim.get_phase( d0 ) ? s : !s, a, b );
      }
    }

    return std::nullopt;
  }

  void collect_unate_divisors( node const& root, uint32_t required )
  {
    udivs.clear();

    auto const& tt = sim.get_tt( ntk.make_signal( root ) );
    auto const& one = sim.get_tt( ntk.get_constant( true ) );
    for ( auto i = 0u; i < num_divs; ++i )
    {
      auto const d0 = divs.at( i );
      if ( ntk.level( d0 ) > required - 1 )
        continue;
      auto const& tt_s0 = sim.get_tt( ntk.make_signal( d0 ) );

      for ( auto j = i + 1; j < num_divs; ++j )
      {
        auto const d1 = divs.at( j );
        if ( ntk.level( d1 ) > required - 1 )
          continue;
        auto const& tt_s1 = sim.get_tt( ntk.make_signal( d1 ) );

        /* Boolean filtering rule for MAJ-3 */
        if ( kitty::ternary_majority( tt_s0, tt_s1, tt ) == tt )
        {
          udivs.u0.emplace_back( ntk.make_signal( d0 ) );
          udivs.u1.emplace_back( ntk.make_signal( d1 ) );
          continue;
        }

        if ( kitty::ternary_majority( ~tt_s0, tt_s1, tt ) == tt )
        {
          udivs.u0.emplace_back( !ntk.make_signal( d0 ) );
          udivs.u1.emplace_back( ntk.make_signal( d1 ) );
          continue;
        }

        if ( kitty::ternary_majority( tt_s0, ~tt_s1, tt ) == tt )
        {
          udivs.u0.emplace_back( ntk.make_signal( d0 ) );
          udivs.u1.emplace_back( !ntk.make_signal( d1 ) );
          continue;
        }

        if ( std::find( udivs.next_candidates.begin(), udivs.next_candidates.end(), ntk.make_signal( d1 ) ) == udivs.next_candidates.end() )
          udivs.next_candidates.emplace_back( ntk.make_signal( d1 ) );
      }

      if constexpr ( use_constant ) /* allowing "not real" MAJ gates (one fanin is constant) */
      {
        if ( kitty::ternary_majority( tt_s0, one, tt ) == tt )
        {
          udivs.u0.emplace_back( ntk.make_signal( d0 ) );
          udivs.u1.emplace_back( ntk.get_constant( true ) );
          continue;
        }

        if ( kitty::ternary_majority( ~tt_s0, one, tt ) == tt )
        {
          udivs.u0.emplace_back( !ntk.make_signal( d0 ) );
          udivs.u1.emplace_back( ntk.get_constant( true ) );
          continue;
        }

        if ( kitty::ternary_majority( tt_s0, ~one, tt ) == tt )
        {
          udivs.u0.emplace_back( ntk.make_signal( d0 ) );
          udivs.u1.emplace_back( ntk.get_constant( false ) );
          continue;
        }
      }

      if ( std::find( udivs.next_candidates.begin(), udivs.next_candidates.end(), ntk.make_signal( d0 ) ) == udivs.next_candidates.end() )
        udivs.next_candidates.emplace_back( ntk.make_signal( d0 ) );
    }

    if constexpr ( use_constant )
    {
      udivs.next_candidates.emplace_back( ntk.get_constant( true ) );
    }
  }

  std::optional<signal> resub_div1( node const& root, uint32_t required )
  {
    (void)required;
    auto const& tt = sim.get_tt( ntk.make_signal( root ) );

    for ( auto i = 0u; i < udivs.u0.size(); ++i )
    {
      auto const s0 = udivs.u0.at( i );
      auto const s1 = udivs.u1.at( i );
      auto const& tt_s0 = sim.get_tt( s0 );
      auto const& tt_s1 = sim.get_tt( s1 );

      for ( auto j = i + 1; j < udivs.u0.size(); ++j )
      {
        auto s2 = udivs.u0.at( j );
        auto tt_s2 = sim.get_tt( s2 );

        if ( kitty::ternary_majority( tt_s0, tt_s1, tt_s2 ) == tt )
        {
          auto const a = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
          auto const b = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
          auto const c = sim.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;
          return sim.get_phase( root ) ? !ntk.create_maj( a, b, c ) : ntk.create_maj( a, b, c );
        }

        s2 = udivs.u1.at( j );
        tt_s2 = sim.get_tt( s2 );

        if ( kitty::ternary_majority( tt_s0, tt_s1, tt_s2 ) == tt )
        {
          auto const a = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
          auto const b = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
          auto const c = sim.get_phase( ntk.get_node( s2 ) ) ? !s2 : s2;
          return sim.get_phase( root ) ? !ntk.create_maj( a, b, c ) : ntk.create_maj( a, b, c );
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

      auto const& tt_s0 = sim.get_tt( s0 );

      for ( auto j = i + 1; j < udivs.next_candidates.size(); ++j )
      {
        auto const& s1 = udivs.next_candidates.at( j );
        if ( ntk.level( ntk.get_node( s1 ) ) > required - 2 )
          continue;

        auto const& tt_s1 = sim.get_tt( s1 );

        for ( auto k = j + 1; k < udivs.next_candidates.size(); ++k )
        {
          auto const& s2 = udivs.next_candidates.at( k );
          if ( ntk.level( ntk.get_node( s2 ) ) > required - 2 )
            continue;

          auto const& tt_s2 = sim.get_tt( s2 );

          if ( kitty::implies( kitty::ternary_majority( tt_s0, tt_s1, tt_s2 ), tt ) )
          {
            bdivs.positive_divisors0.emplace_back(  s0 );
            bdivs.positive_divisors1.emplace_back(  s1 );
            bdivs.positive_divisors2.emplace_back(  s2 );
            continue;
          }

          if ( kitty::implies( kitty::ternary_majority( ~tt_s0, tt_s1, tt_s2 ), tt ) )
          {
            bdivs.positive_divisors0.emplace_back( !s0 );
            bdivs.positive_divisors1.emplace_back(  s1 );
            bdivs.positive_divisors2.emplace_back(  s2 );
            continue;
          }

          if ( kitty::implies( tt, kitty::ternary_majority( tt_s0, tt_s1, tt_s2 ) ) )
          {
            bdivs.negative_divisors0.emplace_back(  s0 );
            bdivs.negative_divisors1.emplace_back(  s1 );
            bdivs.negative_divisors2.emplace_back(  s2 );
            continue;
          }

          if ( kitty::implies( tt, kitty::ternary_majority( ~tt_s0, tt_s1, tt_s2 ) ) )
          {
            bdivs.negative_divisors0.emplace_back( !s0 );
            bdivs.negative_divisors1.emplace_back(  s1 );
            bdivs.negative_divisors2.emplace_back(  s2 );
            continue;
          }
        }
      }
    }
  }

  std::optional<signal> resub_div2( node const& root, uint32_t required )
  {
    (void)required;
    auto const s = ntk.make_signal( root );
    auto const& tt = sim.get_tt( s );

    /* check positive unate divisors */
    for ( auto i = 0u; i < udivs.u0.size(); ++i )
    {
      auto const& s0 = udivs.u0.at( i );
      auto const& s1 = udivs.u1.at( i );

      for ( auto j = 0u; j < bdivs.positive_divisors0.size(); ++j )
      {
        auto const& s2 = bdivs.positive_divisors0.at( j );
        auto const& s3 = bdivs.positive_divisors1.at( j );
        auto const& s4 = bdivs.positive_divisors2.at( j );

        auto const a = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
        auto const b = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
        auto const c = sim.get_phase( ntk.get_node( s2 ) ) ? !s1 : s2;
        auto const d = sim.get_phase( ntk.get_node( s3 ) ) ? !s2 : s3;
        auto const e = sim.get_phase( ntk.get_node( s4 ) ) ? !s3 : s4;

        auto const& tt_s0 = sim.get_tt( s0 );
        auto const& tt_s1 = sim.get_tt( s1 );
        auto const& tt_s2 = sim.get_tt( s2 );
        auto const& tt_s3 = sim.get_tt( s3 );
        auto const& tt_s4 = sim.get_tt( s4 );

        if ( kitty::ternary_majority( kitty::ternary_majority( tt_s0, tt_s1, tt_s2 ), tt_s3, tt_s4 ) == tt )
        {
          return sim.get_phase( root ) ?
            !ntk.create_maj( a, b, ntk.create_maj( c, d, e ) ) :
             ntk.create_maj( a, b, ntk.create_maj( c, d, e ) );
        }
      }
    }

    /* check negative unate divisors */
    for ( auto i = 0u; i < udivs.u0.size(); ++i )
    {
      auto const& s0 = udivs.u0.at( i );
      auto const& s1 = udivs.u1.at( i );

      for ( auto j = 0u; j < bdivs.negative_divisors0.size(); ++j )
      {
        auto const& s2 = bdivs.negative_divisors0.at( j );
        auto const& s3 = bdivs.negative_divisors1.at( j );
        auto const& s4 = bdivs.negative_divisors2.at( j );

        auto const a = sim.get_phase( ntk.get_node( s0 ) ) ? !s0 : s0;
        auto const b = sim.get_phase( ntk.get_node( s1 ) ) ? !s1 : s1;
        auto const c = sim.get_phase( ntk.get_node( s2 ) ) ? !s1 : s2;
        auto const d = sim.get_phase( ntk.get_node( s3 ) ) ? !s2 : s3;
        auto const e = sim.get_phase( ntk.get_node( s4 ) ) ? !s3 : s4;

        auto const& tt_s0 = sim.get_tt( s0 );
        auto const& tt_s1 = sim.get_tt( s1 );
        auto const& tt_s2 = sim.get_tt( s2 );
        auto const& tt_s3 = sim.get_tt( s3 );
        auto const& tt_s4 = sim.get_tt( s4 );

        if ( kitty::ternary_majority( ~kitty::ternary_majority( tt_s0, tt_s1, tt_s2 ), tt_s3, tt_s4 ) == tt )
        {
          return sim.get_phase( root ) ?
            !ntk.create_maj( a, b, ntk.create_maj( c, d, e ) ) :
             ntk.create_maj( a, b, ntk.create_maj( c, d, e ) );
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
}; /* mig_exhaustive_resub_functor */

struct mig_resyn_stats
{
  

  void report() const
  {
    std::cout << "[i] kernel: mig_resyn_functor\n";
    /*std::cout << fmt::format( "[i]     constant-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_const_accepts, to_seconds( time_resubC ) );
    std::cout << fmt::format( "[i]            0-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_div0_accepts, to_seconds( time_resub0 ) );
    std::cout << fmt::format( "[i]            collect unate divisors                           ({:>5.2f} secs)\n", to_seconds( time_collect_unate_divisors ) );
    std::cout << fmt::format( "[i]            R-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_divR_accepts, to_seconds( time_resubR ) );
    std::cout << fmt::format( "[i]            1-resub {:6d} = {:6d} MAJ                      ({:>5.2f} secs)\n",
                              num_div1_accepts, num_div1_accepts, to_seconds( time_resub1 ) );
    std::cout << fmt::format( "[i]            collect binate divisors                          ({:>5.2f} secs)\n", to_seconds( time_collect_binate_divisors ) );
    std::cout << fmt::format( "[i]            2-resub {:6d} = {:6d} 2MAJ                     ({:>5.2f} secs)\n",
                               num_div2_accepts, num_div2_accepts, to_seconds( time_resub2 ) );
    std::cout << fmt::format( "[i]            total   {:6d}\n",
                              (num_const_accepts + num_div0_accepts + num_divR_accepts + num_div1_accepts + num_div2_accepts) );*/
  }
}; /* mig_resyn_stats */

template<typename Ntk, typename Simulator, typename TTcare, typename Engine = mig_resyn_engine<kitty::partial_truth_table>>
struct mig_resyn_functor
{
public:
  using node = mig_network::node;
  using signal = mig_network::signal;
  using stats = mig_resyn_stats;

public:
  explicit mig_resyn_functor( Ntk& ntk, Simulator const& sim, std::vector<node> const& divs, uint32_t num_divs, stats& st )
    : ntk( ntk )
    , sim( sim )
    , tts( ntk )
    , divs( divs )
    , st( st )
    , exh( ntk, sim, divs, num_divs, st2 )
  {
    assert( divs.size() == num_divs ); (void)num_divs;
    div_signals.reserve( divs.size() );
  }

  std::optional<signal> operator()( node const& root, TTcare care, uint32_t required, uint32_t max_inserts, uint32_t potential_gain, uint32_t& real_gain )
  {
    (void)care; (void)required;
    kitty::partial_truth_table root_tt;
    root_tt = sim.get_tt( sim.get_phase( root ) ? !ntk.make_signal( root ) : ntk.make_signal( root ) );
    Engine engine( root_tt );
    for ( auto const& d : divs )
    {
      div_signals.emplace_back( sim.get_phase( d ) ? !ntk.make_signal( d ) : ntk.make_signal( d ) );
      tts[d] = sim.get_tt( div_signals.back() );
    }
    engine.add_divisors( divs.begin(), divs.end(), tts );

    auto const res = engine.compute_function( std::min( potential_gain - 1, max_inserts ) );
    if ( res )
    {
      signal ret;
      real_gain = potential_gain - (*res).num_gates();
      insert( ntk, div_signals.begin(), div_signals.end(), *res, [&]( signal const& s ){ ret = s; } );
      return ret;
    }
    else
    {
      return std::nullopt;
    }
  }

private:
  Ntk& ntk;
  Simulator const& sim;
  unordered_node_map<kitty::partial_truth_table, Ntk> tts;
  std::vector<node> const& divs;
  std::vector<signal> div_signals;
  stats& st;

  mig_exhaustive_resub_stats st2;
  mig_exhaustive_resub_functor<Ntk, Simulator, TTcare> exh;
}; /* mig_resyn_functor */

/*! \brief MIG-specific resubstitution algorithm.
 *
 * This algorithms iterates over each node, creates a
 * reconvergence-driven cut, and attempts to re-express the node's
 * function using existing nodes from the cut.  Node which are no
 * longer used (including nodes in their transitive fanins) can then
 * be removed.  The objective is to reduce the size of the network as
 * much as possible while maintaing the global input-output
 * functionality.
 *
 * **Required network functions:**
 *
 * - `clear_values`
 * - `fanout_size`
 * - `foreach_fanin`
 * - `foreach_fanout`
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
 * \param ntk A network type derived from mig_network
 * \param ps Resubstitution parameters
 * \param pst Resubstitution statistics
 */
template<class Ntk>
void mig_resubstitution( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( std::is_same_v<typename Ntk::base_type, mig_network>, "Network type is not mig_network" );

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
  static_assert( has_level_v<Ntk>, "Ntk does not implement the level method" );
  static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method" );

  if ( ps.max_pis == 8 )
  {
    using truthtable_t = kitty::static_truth_table<8u>;
    using truthtable_dc_t = kitty::dynamic_truth_table;
    //using functor_t = mig_resyn_functor<Ntk, typename detail::window_simulator<Ntk, truthtable_t>, truthtable_dc_t>;
    using functor_t = mig_exhaustive_resub_functor<Ntk, typename detail::window_simulator<Ntk, truthtable_t>, truthtable_dc_t>;
    using resub_impl_t = detail::resubstitution_impl<Ntk, typename detail::window_based_resub_engine<Ntk, truthtable_t, truthtable_dc_t, functor_t>>;

    resubstitution_stats st;
    typename resub_impl_t::engine_st_t engine_st;
    typename resub_impl_t::collector_st_t collector_st;

    resub_impl_t p( ntk, ps, st, engine_st, collector_st );
    p.run();

    if ( ps.verbose )
    {
      st.report();
      collector_st.report();
      engine_st.report();
    }

    if ( pst )
    {
      *pst = st;
    }
  }
  else
  {
    using truthtable_t = kitty::dynamic_truth_table;
    using truthtable_dc_t = kitty::dynamic_truth_table;
    using functor_t = mig_resyn_functor<Ntk, typename detail::window_simulator<Ntk, truthtable_t>, truthtable_dc_t>;
    using resub_impl_t = detail::resubstitution_impl<Ntk, typename detail::window_based_resub_engine<Ntk, truthtable_t, truthtable_dc_t, functor_t>>;

    resubstitution_stats st;
    typename resub_impl_t::engine_st_t engine_st;
    typename resub_impl_t::collector_st_t collector_st;

    resub_impl_t p( ntk, ps, st, engine_st, collector_st );
    p.run();

    if ( ps.verbose )
    {
      st.report();
      collector_st.report();
      engine_st.report();
    }

    if ( pst )
    {
      *pst = st;
    }
  }
}

} /* namespace mockturtle */
