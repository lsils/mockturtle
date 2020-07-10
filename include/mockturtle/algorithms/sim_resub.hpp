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

#include "../utils/abc_resub.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../utils/abc_resub.hpp"

#include <bill/sat/interface/abc_bsat2.hpp>
#include <bill/sat/interface/z3.hpp>
#include <kitty/partial_truth_table.hpp>

#include <mockturtle/algorithms/resubstitution2.hpp>
#include <mockturtle/algorithms/circuit_validator.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/algorithms/pattern_generation.hpp>
#include <mockturtle/io/write_patterns.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>

namespace mockturtle
{

namespace detail
{

/*! \brief Wrapper class of an imaginary circuit to be verified by `circuit_validator` */
template<typename Ntk, typename validator_t>
struct imaginary_circuit
{
  using vgate = typename validator_t::gate;

  std::vector<vgate> const ckt;
  bool const out_neg;
};

struct abc_resub_functor_stats
{
  /*! \brief Time for finding dependency function. */
  stopwatch<>::duration time_compute_function{0};

  /*! \brief Time for interfacing with ABC. */
  stopwatch<>::duration time_interface{0};

  /*! \brief Number of found solutions. */
  uint32_t num_success{0};

  /*! \brief Number of times that no solution can be found. */
  uint32_t num_fail{0};

  void report() const
  {
    // clang-format off
    std::cout <<              "[i]     <ResubFn: abc_resub_functor>\n";
    std::cout << fmt::format( "[i]         #solution = {:6d}\n", num_success );
    std::cout << fmt::format( "[i]         #invoke   = {:6d}\n", num_success + num_fail );
    std::cout << fmt::format( "[i]         ABC time:   {:>5.2f} secs\n", to_seconds( time_compute_function ) );
    std::cout << fmt::format( "[i]         interface:  {:>5.2f} secs\n", to_seconds( time_compute_function ) );
    // clang-format on
  }
};

template<typename Ntk, typename validator_t>
class abc_resub_functor
{
public:
  using stats = abc_resub_functor_stats;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = unordered_node_map<kitty::partial_truth_table, Ntk>;
  using vgate = typename validator_t::gate;
  using fanin = typename vgate::fanin;
  using gtype = typename validator_t::gate_type;
  using circuit = imaginary_circuit<Ntk, validator_t>;
  using result_t = typename std::variant<signal, circuit>;

  explicit abc_resub_functor( Ntk const& ntk, resubstitution_params const& ps, stats& st, TT const& tts, std::vector<node> const& divs )
      : ntk( ntk ), ps( ps ), st( st ), tts( tts ), divs( divs ), num_blocks( 0 )
  {
  }

  ~abc_resub_functor()
  {
    call_with_stopwatch( st.time_interface, [&]() {
      abcresub::Abc_ResubPrepareManager( 0 );
    } );
  }

  void check_num_blocks()
  {
    if ( tts[ntk.get_constant( false )].num_blocks() != num_blocks )
    {
      num_blocks = tts[ntk.get_constant( false )].num_blocks();
      call_with_stopwatch( st.time_interface, [&]() {
        abcresub::Abc_ResubPrepareManager( num_blocks );
      });
    }
  }

  std::optional<result_t> operator()( node const& root, uint32_t num_inserts, uint32_t& size )
  {
    check_num_blocks();
    abc_resub rs( 2ul + divs.size(), num_blocks, ps.max_divisors_k );
    call_with_stopwatch( st.time_interface, [&]() {
      rs.add_root( root, tts );
      rs.add_divisors( std::begin( divs ), std::end( divs ), tts );
    });

    auto const res = call_with_stopwatch( st.time_compute_function, [&]() {
      if constexpr ( std::is_same<typename Ntk::base_type, xag_network>::value )
      {
        return rs.compute_function( num_inserts, true );
      }
      else
      {
        return rs.compute_function( num_inserts, false );
      }
    } );

    if ( res )
    {
      ++st.num_success;
      auto const& index_list = *res;
      if ( index_list.size() == 1u ) /* div0 or constant */
      {
        if ( index_list[0] < 2 )
        {
          return result_t( ntk.get_constant( bool( index_list[0] ) ) );
        }
        assert( index_list[0] >= 4 );
        return result_t( bool( index_list[0] % 2 ) ? !ntk.make_signal( divs[( index_list[0] >> 1u ) - 2u] ) : ntk.make_signal( divs[( index_list[0] >> 1u ) - 2u] ) );
      }

      uint64_t const num_gates = ( index_list.size() - 1u ) / 2u;
      std::vector<vgate> gates;
      gates.reserve( num_gates );
      size = 0u;

      call_with_stopwatch( st.time_interface, [&]() {
        for ( auto i = 0u; i < num_gates; ++i )
        {
          fanin f0{uint32_t( ( index_list[2 * i] >> 1u ) - 2u ), bool( index_list[2 * i] % 2 )};
          fanin f1{uint32_t( ( index_list[2 * i + 1u] >> 1u ) - 2u ), bool( index_list[2 * i + 1u] % 2 )};
          gates.emplace_back( vgate{{f0, f1}, f0.index < f1.index ? gtype::AND : gtype::XOR} );

          if constexpr ( std::is_same<typename Ntk::base_type, xag_network>::value )
          {
            ++size;
          }
          else
          {
            size += ( gates[i].type == gtype::AND ) ? 1u : 3u;
          }
        }
      });
      bool const out_neg = bool( index_list.back() % 2 );
      assert( size <= num_inserts );
      return result_t( circuit{gates, out_neg} );
    }
    else /* loop until no result can be found by the engine */
    {
      ++st.num_fail;
      return std::nullopt;
    }
  }

private:
  Ntk const& ntk;
  resubstitution_params const& ps;
  stats& st;

  TT const& tts;
  std::vector<node> const& divs;
  uint32_t num_blocks;
};

template<typename ResubFnSt>
struct sim_resub_stats
{
  /*! \brief Time for pattern generation. */
  stopwatch<>::duration time_patgen{0};

  /*! \brief Time for simulation. */
  stopwatch<>::duration time_sim{0};

  /*! \brief Time for SAT solving. */
  stopwatch<>::duration time_sat{0};

  /*! \brief Time for finding dependency function. */
  stopwatch<>::duration time_functor{0};

  /*! \brief Time for interfacing with circuit_validator. */
  stopwatch<>::duration time_interface{0};

  /*! \brief Number of patterns used. */
  uint32_t num_pats{0};

  /*! \brief Number of counter-examples. */
  uint32_t num_cex{0};

  /*! \brief Number of successful resubstitutions. */
  uint32_t num_resub{0};

  /*! \brief Number of SAT solver timeout. */
  uint32_t num_timeout{0};

  ResubFnSt functor_st;

  void report() const
  {
    // clang-format off
    std::cout <<              "[i] <ResubEngine: simulation_based_resub_engine>\n";
    std::cout <<              "[i]     ========  Stats  ========\n";
    std::cout << fmt::format( "[i]     #pat     = {:6d}\n", num_pats );
    std::cout << fmt::format( "[i]     #resub   = {:6d}\n", num_resub );
    std::cout << fmt::format( "[i]     #CEX     = {:6d}\n", num_cex );
    std::cout << fmt::format( "[i]     #timeout = {:6d}\n", num_timeout );
    std::cout <<              "[i]     ======== Runtime ========\n";
    std::cout << fmt::format( "[i]     generate pattern: {:>5.2f} secs\n", to_seconds( time_patgen ) );
    std::cout << fmt::format( "[i]     simulation:       {:>5.2f} secs\n", to_seconds( time_sim ) );
    std::cout << fmt::format( "[i]     SAT:              {:>5.2f} secs\n", to_seconds( time_sat ) );
    std::cout << fmt::format( "[i]     compute function: {:>5.2f} secs\n", to_seconds( time_functor ) );
    std::cout << fmt::format( "[i]     interfacing:      {:>5.2f} secs\n", to_seconds( time_interface ) );
    std::cout <<              "[i]     ======== Details ========\n";
    functor_st.report();
    std::cout <<              "[i]     =========================\n\n";
    // clang-format on
  }
};

template<class Ntk, typename validator_t = circuit_validator<Ntk, bill::solvers::bsat2, false, true, false>, class ResubFn = abc_resub_functor<Ntk, validator_t>>
class simulation_based_resub_engine
{
public:
  static constexpr bool require_leaves_and_MFFC = false;
  using stats = sim_resub_stats<typename ResubFn::stats>;

  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = unordered_node_map<kitty::partial_truth_table, Ntk>;
  using gtype = typename validator_t::gate_type;
  using circuit = imaginary_circuit<Ntk, validator_t>;

  explicit simulation_based_resub_engine( Ntk& ntk, resubstitution_params const& ps, stats& st )
      : ntk( ntk ), ps( ps ), st( st ), tts( ntk ), validator( ntk, vps )
  {
    if constexpr ( !validator_t::use_odc_ )
    {
      assert( ps.odc_levels == 0 && "to consider ODCs, circuit_validator::use_odc has to be turned on" );
    }

    vps.conflict_limit = ps.conflict_limit;
    vps.random_seed = ps.random_seed;

    ntk._events->on_add.emplace_back( [&]( const auto& n ) {
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_node<Ntk>( ntk, n, tts, sim );
      });
    } );

    /* prepare simulation patterns */
    call_with_stopwatch( st.time_patgen, [&]() {
      if ( ps.pattern_filename )
      {
        sim = partial_simulator( *ps.pattern_filename );
      }
      else
      {
        sim = partial_simulator( ntk.num_pis(), 1024 );
        pattern_generation( ntk, sim );
      }
    });
    st.num_pats = sim.num_bits();

    /* first simulation: the whole circuit; from 0 bits. */
    call_with_stopwatch( st.time_sim, [&]() {
      simulate_nodes<Ntk>( ntk, tts, sim );
    });
  }

  ~simulation_based_resub_engine()
  {
    if ( ps.save_patterns )
    {
      write_patterns( sim, *ps.save_patterns );
    }
  }

  std::optional<signal> run( node const& n, std::vector<node> const& divs, uint32_t potential_gain, uint32_t& last_gain )
  {
    ResubFn resub_fn( ntk, ps, st.functor_st, tts, divs );
    for ( auto j = 0u; j < ps.max_trials; ++j )
    {
      check_tts( n );
      for ( auto const& d : divs )
      {
        check_tts( d );
      }

      uint32_t size = 0;
      const auto res = call_with_stopwatch( st.time_functor, [&]() {
        return resub_fn( n, std::min( int( potential_gain ) - 1, int( ps.max_inserts ) ), size );
      });
      if ( res )
      {
        if ( std::get_if<signal>( &(*res) ) )
        {
          signal const& g = std::get<signal>( *res );
          auto valid = call_with_stopwatch( st.time_sat, [&]() {
            return validator.validate( n, g );
          });
          if ( valid )
          {
            if ( *valid )
            {
              ++st.num_resub;
              last_gain = potential_gain;
              return g;
            }
            else
            {
              found_cex();
              continue;
            }
          }
          else /* timeout */
          {
            return std::nullopt;
          }
        }
        else
        {
          circuit const& c = std::get<circuit>( *res );
          auto valid = call_with_stopwatch( st.time_sat, [&]() {
            return validator.validate( n, divs, c.ckt, c.out_neg );
          });
          if ( valid )
          {
            if ( *valid )
            {
              ++st.num_resub;
              last_gain = potential_gain - size;
              return translate( c, divs );
            }
            else
            {
              found_cex();
              continue;
            }
          }
          else /* timeout */
          {
            return std::nullopt;
          }
        }
      }
      else /* functor can not find any potential resubstitution */
      {
        return std::nullopt;
      }
    }
    return std::nullopt;
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
      } );
    }
  }

  void check_tts( node const& n )
  {
    if ( tts[n].num_bits() != sim.num_bits() )
    {
      call_with_stopwatch( st.time_sim, [&]() {
        simulate_node<Ntk>( ntk, n, tts, sim );
      } );
    }
  }

  kitty::partial_truth_table get_tt( node const& n, bool inverse = false )
  {
    check_tts( n );

    if ( ps.odc_levels == 0 )
    {
      return inverse? ~tts[n]: tts[n];
    }

    return ( inverse? ~tts[n]: tts[n] ) | observability_dont_cares( ntk, n, sim, tts, ps.odc_levels );
  }

  signal translate( circuit const& c, std::vector<node> const& divs )
  {
    std::vector<signal> ckt;

    call_with_stopwatch( st.time_interface, [&]() {
      for ( auto i = 0u; i < divs.size(); ++i )
      {
        ckt.emplace_back( ntk.make_signal( divs[i] ) );
      }

      for ( auto g : c.ckt )
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
    });

    return c.out_neg ? !ckt.back() : ckt.back();
  }

private:
  Ntk& ntk;
  resubstitution_params const& ps;
  stats& st;

  TT tts;
  partial_simulator sim;

  validator_params vps;
  validator_t validator;
}; /* simulation_based_resub_engine */

} /* namespace detail */

/*! \brief Simulation-guided Boolean resubstitution algorithm.
 *
 * Please refer to [1] for details of the algorithm.
 *
 * [1] Simulation-Guided Boolean Resubstitution. IWLS 2020 / ICCAD 2020.
 *
 * \param sim Reference of a `partial_simulator` object where simulation patterns are stored.
 * This can be read in from a file or generated with `pattern_generation`.
 */
template<class Ntk>
void sim_resub( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  static_assert( std::is_same<typename Ntk::base_type, aig_network>::value || std::is_same<typename Ntk::base_type, xag_network>::value, "Currently only supports AIG and XAG" );

  using resub_view_t = fanout_view<depth_view<Ntk>>;
  depth_view<Ntk> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  //using resub_functor_t = typename detail::abc_resub_functor<Ntk, kitty::partial_truth_table>;
  using resub_engine_t = typename detail::simulation_based_resub_engine<resub_view_t>;
  using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, resub_engine_t>;
  using engine_st_t = typename resub_impl_t::engine_st_t;
  using collector_st_t = typename resub_impl_t::collector_st_t;
  resubstitution_stats st;
  engine_st_t engine_st;
  collector_st_t collector_st;

  resub_impl_t p( resub_view, ps, st, engine_st, collector_st );
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

} /* namespace mockturtle */