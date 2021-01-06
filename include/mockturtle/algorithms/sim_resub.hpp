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

#include <variant>
#include <algorithm>

#include "../utils/abc_resub.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../utils/abc_resub.hpp"

#include <bill/sat/interface/abc_bsat2.hpp>
#include <bill/sat/interface/z3.hpp>
#include <kitty/partial_truth_table.hpp>

#include <mockturtle/algorithms/resubstitution.hpp>
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
  using node = typename Ntk::node;
  using vgate = typename validator_t::gate;

  std::vector<node> const& divs;
  std::vector<vgate> const ckt;
  bool const out_neg;
};

struct sim_aig_resub_functor_stats
{
  /*! \brief Accumulated runtime for const-resub */
  stopwatch<>::duration time_resubC{0};

  /*! \brief Accumulated runtime for zero-resub */
  stopwatch<>::duration time_resub0{0};

  /*! \brief Accumulated runtime for collecting unate divisors. */
  stopwatch<>::duration time_collect_unate_divisors{0};

  /*! \brief Accumulated runtime for one-resub */
  stopwatch<>::duration time_resub1{0};

  /*! \brief Number of accepted constant resubsitutions */
  uint32_t num_const_accepts{0};

  /*! \brief Number of accepted zero resubsitutions */
  uint32_t num_div0_accepts{0};

  /*! \brief Total number of unate divisors  */
  uint64_t num_unate_divisors{0};

  /*! \brief Number of accepted one resubsitutions */
  uint64_t num_div1_accepts{0};

  /*! \brief Number of accepted single AND-resubsitutions */
  uint64_t num_div1_and_accepts{0};

  /*! \brief Number of accepted single OR-resubsitutions */
  uint64_t num_div1_or_accepts{0};

  void report() const
  {
    // clang-format off
    std::cout <<              "[i]     <ResubFn: sim_aig_resub_functor_stats>\n";
    std::cout << fmt::format( "[i]         #const = {:6d}   ({:>5.2f} secs)\n", num_const_accepts, to_seconds( time_resubC ) );
    std::cout << fmt::format( "[i]         #div0  = {:6d}   ({:>5.2f} secs)\n", num_div0_accepts, to_seconds( time_resub0 ) );
    std::cout << fmt::format( "[i]        >#udivs = {:6d}   ({:>5.2f} secs)\n", num_unate_divisors, to_seconds( time_collect_unate_divisors ) );
    std::cout << fmt::format( "[i]         #div1  = {:6d}   ({:>5.2f} secs)\n", num_div1_accepts, to_seconds( time_resub1 ) );
    std::cout << fmt::format( "[i]             #div1-AND = {:6d}\n", num_div1_and_accepts );
    std::cout << fmt::format( "[i]             #div1-OR  = {:6d}\n", num_div1_or_accepts );
    // clang-format on
  }
};

template<typename Ntk, typename validator_t>
class sim_aig_resub_functor
{
public:
  using stats = sim_aig_resub_functor_stats;
  using TT = kitty::partial_truth_table;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using vgate = typename validator_t::gate;
  using fanin = typename vgate::fanin;
  using gtype = typename validator_t::gate_type;
  using circuit = imaginary_circuit<Ntk, validator_t>;
  using result_t = typename std::variant<signal, circuit>;

  struct unate_divisors
  {
    using signal = typename Ntk::signal;

    std::vector<std::pair<signal, uint32_t>> positive_divisors;
    std::vector<std::pair<signal, uint32_t>> negative_divisors;

    void clear()
    {
      positive_divisors.clear();
      negative_divisors.clear();
    }

    void sort()
    {
      std::sort(positive_divisors.begin(), positive_divisors.end(), [](std::pair<signal, uint32_t> a, std::pair<signal, uint32_t> b) {
          return a.second > b.second;   
      });
      std::sort(negative_divisors.begin(), negative_divisors.end(), [](std::pair<signal, uint32_t> a, std::pair<signal, uint32_t> b) {
          return a.second > b.second;   
      });
    }
  };

  explicit sim_aig_resub_functor( Ntk const& ntk, resubstitution_params const& ps, stats& st, unordered_node_map<TT, Ntk> const& tts, node const& root, std::vector<node> const& divs, uint32_t const num_inserts )
      : ntk( ntk ), ps( ps ), st( st ), tts( tts ), root( root ), divs( divs ), num_inserts( num_inserts ), step( 0 ), i( 0 ), j( 0 )
  {
  }

  std::optional<result_t> operator()( uint32_t& size, TT const& care_ )
  {
    care = care_;
    tt = tts[root] & care;
    ntt = ~tts[root] & care;

    while ( true )
    {
      switch ( step )
      {
        case 0u:
        {
          auto const& res = call_with_stopwatch( st.time_resubC, [&]() {
            return resub_const();
          });
          if ( res )
          {
            ++st.num_const_accepts;
            size = 0;
            return res;
          }
          else
          {
            ++step;
            continue;
          }
        }
        case 1u:
        {
          if ( i == divs.size() )
          {
            if ( num_inserts == 0 )
            {
              return std::nullopt;
            }
            call_with_stopwatch( st.time_collect_unate_divisors, [&]() {
              collect_unate_divisors();
            });
            st.num_unate_divisors += udivs.positive_divisors.size() + udivs.negative_divisors.size();
            w = kitty::count_ones( tt );
            nw = tt.num_bits() - w;
            i = 0; j = 1;
            ++step;
            if ( udivs.positive_divisors.size() < 2 )
            {
              ++step;
              if ( udivs.negative_divisors.size() < 2 )
              {
                ++step;
              }
            }
            continue;
          }

          auto const& res = call_with_stopwatch( st.time_resub0, [&]() {
            return resub_div0();
          });
          ++i;
          if ( res )
          {
            ++st.num_div0_accepts;
            size = 0;
            return res;
          }
          else
          {
            continue;
          }
        }
        case 2u:
        {
          if ( j == udivs.positive_divisors.size() )
          {
            break_div1_pos_inner();
            continue;
          }

          auto const& res = call_with_stopwatch( st.time_resub1, [&]() {
            return resub_div1_pos();
          });
          ++j; 
          if ( res )
          {
            ++st.num_div1_accepts;
            ++st.num_div1_or_accepts;
            size = 1;
            return res;
          }
          else
          {
            continue;
          }
        }
        case 3u:
        {
          if ( j == udivs.negative_divisors.size() )
          {
            break_div1_neg_inner();
            continue;
          }

          auto const& res = call_with_stopwatch( st.time_resub1, [&]() {
            return resub_div1_neg();
          });
          ++j;
          if ( res )
          {
            ++st.num_div1_accepts;
            ++st.num_div1_and_accepts;
            size = 1;
            return res;
          }
          else
          {
            continue;
          }
        }
        default:
        {
          return std::nullopt;
        }
      }
    }
  }

private:
  TT get_tt( node const& n, bool inverse = false )
  {
    return ( inverse? ~tts[n]: tts[n] ) & care;
  }

  void collect_unate_divisors()
  {
    udivs.clear();

    for ( auto const& d : divs )
    {
      auto const tt_d = get_tt( d );
      auto const ntt_d = get_tt( d, true );

      /* check positive containment */
      if ( kitty::implies( tt_d, tt ) )
      {
        udivs.positive_divisors.emplace_back( std::make_pair( ntk.make_signal( d ), kitty::count_ones( tt_d & tt ) ) );
        continue;
      }
      if ( kitty::implies( ntt_d, tt ) )
      {
        udivs.positive_divisors.emplace_back( std::make_pair( !ntk.make_signal( d ), kitty::count_ones( ntt_d & tt ) ) );
        continue;
      }

      /* check negative containment */
      if ( kitty::implies( tt, tt_d ) )
      {
        udivs.negative_divisors.emplace_back( std::make_pair( ntk.make_signal( d ), kitty::count_zeros( tt_d & tt ) ) );
        continue;
      }
      if ( kitty::implies( tt, ntt_d ) )
      {
        udivs.negative_divisors.emplace_back( std::make_pair( !ntk.make_signal( d ), kitty::count_zeros( ntt_d & tt ) ) );
        continue;
      }
    }

    udivs.sort();
  }

  std::optional<result_t> resub_const() 
  {
    auto const& zero = tts[ntk.get_constant( false )];

    if ( tt == zero )
    {
      return result_t( ntk.get_constant( false ) );
    }
    else if ( ntt == zero )
    {
      return result_t( ntk.get_constant( true ) );
    }

    return std::nullopt;
  }

  std::optional<result_t> resub_div0() 
  {
    auto const& d = divs.at( i );
    auto const tt_d = get_tt( d );

    if ( tt == tt_d )
    {
      return result_t( ntk.make_signal( d ) );
    }
    else if ( ntt == tt_d )
    {
      return result_t( !ntk.make_signal( d ) );
    }

    return std::nullopt;
  }

  std::optional<result_t> resub_div1_pos()
  {
    auto const& s0 = udivs.positive_divisors.at( i ).first;
    auto const& tt_s0 = get_tt( ntk.get_node( s0 ), ntk.is_complemented( s0 ) );
    auto const& w_s0 = udivs.positive_divisors.at( i ).second;
    if ( w_s0 < uint32_t( w / 2 ) ) /* break div1_pos */
    {
      break_div1_pos();
      return std::nullopt;
    }

    if ( w_s0 + udivs.positive_divisors.at( j ).second < w ) /* break inner loop */
    {
      break_div1_pos_inner();
      return std::nullopt;
    }

    auto const& s1 = udivs.positive_divisors.at( j ).first;
    auto const& tt_s1 = get_tt( ntk.get_node( s1 ), ntk.is_complemented( s1 ) );

    for ( auto b = 0u; b < tt.num_blocks(); ++b )
    {
      if ( ( tt_s0._bits[b] | tt_s1._bits[b] ) != tt._bits[b] )
      {
        return std::nullopt;
      }
    }
    assert( tt == ( tt_s0 | tt_s1 ) );
    fanin fi1{0, !ntk.is_complemented( s0 )};
    fanin fi2{1, !ntk.is_complemented( s1 )};
    vgate gate{{fi1, fi2}, gtype::AND};
    tmp.resize( 2 ); tmp[0] = ntk.get_node( s0 ); tmp[1] = ntk.get_node( s1 );

    return result_t( circuit{tmp, {gate}, true} );
  }

  std::optional<result_t> resub_div1_neg()
  {
    auto const& s0 = udivs.negative_divisors.at( i ).first;
    auto const& tt_s0 = get_tt( ntk.get_node( s0 ), ntk.is_complemented( s0 ) );
    auto const& w_s0 = udivs.negative_divisors.at( i ).second;
    if ( w_s0 < uint32_t( nw / 2 ) ) /* break div1_neg */
    {
      break_div1_neg();
      return std::nullopt;
    }

    if ( w_s0 + udivs.negative_divisors.at( j ).second < nw ) /* break inner loop */
    {
      break_div1_neg_inner();
      return std::nullopt;
    }

    auto const& s1 = udivs.negative_divisors.at( j ).first;
    auto const& tt_s1 = get_tt( ntk.get_node( s1 ), ntk.is_complemented( s1 ) );

    for ( auto b = 0u; b < tt.num_blocks(); ++b )
    {
      if ( ( tt_s0._bits[b] & tt_s1._bits[b] ) != tt._bits[b] )
      {
        return std::nullopt;
      }
    }
    assert( tt == ( tt_s0 & tt_s1 ) );
    fanin fi1{0, ntk.is_complemented( s0 )};
    fanin fi2{1, ntk.is_complemented( s1 )};
    vgate gate{{fi1, fi2}, gtype::AND};
    tmp.resize( 2 ); tmp[0] = ntk.get_node( s0 ); tmp[1] = ntk.get_node( s1 );

    return result_t( circuit{tmp, {gate}, false} );
  }

private:
  void break_div1_pos()
  {
    i = 0; j = 1;
    ++step;
    if ( udivs.negative_divisors.size() < 2 )
    {
      ++step;
    }
  }

  void break_div1_pos_inner()
  {
    if ( ++i == udivs.positive_divisors.size() - 1 )
    {
      break_div1_pos();
    }
    else
    {
      j = i + 1;
    }
  }

  void break_div1_neg()
  {
    i = 0; j = 1;
    ++step;
  }

  void break_div1_neg_inner()
  {
    if ( ++i == udivs.negative_divisors.size() - 1 )
    {
      break_div1_neg();
    }
    else
    {
      j = i + 1;
    }
  }

private:
  Ntk const& ntk;
  resubstitution_params const& ps;
  stats& st;

  unordered_node_map<TT, Ntk> const& tts;
  TT tt;
  TT ntt;
  TT care;
  node const& root;
  std::vector<node> const& divs;
  unate_divisors udivs;
  std::vector<node> tmp;

  uint32_t const num_inserts;
  uint32_t step;
  uint32_t i;
  uint32_t j;

  uint32_t w;
  uint32_t nw;
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
    std::cout << fmt::format( "[i]         interface:  {:>5.2f} secs\n", to_seconds( time_interface ) );
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
  using TT = kitty::partial_truth_table;
  using vgate = typename validator_t::gate;
  using fanin = typename vgate::fanin;
  using gtype = typename validator_t::gate_type;
  using circuit = imaginary_circuit<Ntk, validator_t>;
  using result_t = typename std::variant<signal, circuit>;

  explicit abc_resub_functor( Ntk const& ntk, resubstitution_params const& ps, stats& st, unordered_node_map<TT, Ntk> const& tts, node const& root, std::vector<node> const& divs, uint32_t const num_inserts )
      : ntk( ntk ), ps( ps ), st( st ), tts( tts ), root( root ), divs( divs ), num_inserts( num_inserts ), num_blocks( 0 )
  {
    //std::cout<<"[i] resubing " << root<<"\n";
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

  std::optional<result_t> operator()( uint32_t& size, TT const& care )
  {
    check_num_blocks();
    abc_resub rs( 2ul + divs.size(), num_blocks, ps.max_divisors_k );
    call_with_stopwatch( st.time_interface, [&]() {
      rs.add_root( tts[root], care );
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
      return result_t( circuit{divs, gates, out_neg} );
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

  unordered_node_map<TT, Ntk> const& tts;
  node const& root;
  std::vector<node> const& divs;

  uint32_t const num_inserts;
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
  stopwatch<>::duration time_sat_restart{0};

  /*! \brief Time for computing ODCs. */
  stopwatch<>::duration time_odc{0};

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
    std::cout << fmt::format( "[i]     SAT solve:        {:>5.2f} secs\n", to_seconds( time_sat ) );
    std::cout << fmt::format( "[i]     SAT restart:      {:>5.2f} secs\n", to_seconds( time_sat_restart ) );
    std::cout << fmt::format( "[i]     compute ODCs:     {:>5.2f} secs\n", to_seconds( time_odc ) );
    std::cout << fmt::format( "[i]     compute function: {:>5.2f} secs\n", to_seconds( time_functor ) );
    std::cout << fmt::format( "[i]     interfacing:      {:>5.2f} secs\n", to_seconds( time_interface ) );
    std::cout <<              "[i]     ======== Details ========\n";
    functor_st.report();
    std::cout <<              "[i]     =========================\n\n";
    // clang-format on
  }
};

/*! \brief Simulation-based resubstitution engine.
 * 
 * This engine simulates in the whole network and uses partial truth tables
 * to find potential resubstitutions. It then formally verifies the resubstitution
 * candidates given by the resubstitution functor. If the validation fails,
 * a counter-example will be added to the simulation patterns, and the functor
 * will be invoked again with updated truth tables, looping until it returns
 * `std::nullopt`. This engine only requires the divisor collector to prepare `divs`.
 *
 * Please refer to the following paper for further details.
 *
 * [1] Simulation-Guided Boolean Resubstitution. IWLS 2020 (arXiv:2007.02579).
 *
 * Interfaces of the resubstitution functor:
 * - Constructor: `resub_fn( Ntk const& ntk, resubstitution_params const& ps, stats& st,`
 * `TT const& tts, node const& root, std::vector<node> const& divs, uint32_t const num_inserts )`
 * - A public `operator()`: `std::optional<signal> operator()( uint32_t& size )`
 *
 * Compatible resubstitution functors implemented:
 * - `sim_aig_resub_functor`: compute div0 and div1 resub by truth table comparison
 * - `abc_resub_functor`: dependency function computation engine ported from ABC
 *
 * \param validator_t Specialization of `circuit_validator`.
 * \param ResubFn Resubstitution functor to compute the resubstitution.
 * \param MffcRes Typename of `potential_gain` needed by the resubstitution functor.
 */
template<class Ntk, typename validator_t = circuit_validator<Ntk, bill::solvers::bsat2, false, true, false>, class ResubFn = abc_resub_functor<Ntk, validator_t>, typename MffcRes = uint32_t>
class simulation_based_resub_engine
{
public:
  static constexpr bool require_leaves_and_mffc = false;
  using stats = sim_resub_stats<typename ResubFn::stats>;
  using mffc_result_t = MffcRes;

  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = kitty::partial_truth_table;
  using gtype = typename validator_t::gate_type;
  using circuit = imaginary_circuit<Ntk, validator_t>;

  explicit simulation_based_resub_engine( Ntk& ntk, resubstitution_params const& ps, stats& st )
      : ntk( ntk ), ps( ps ), st( st ), tts( ntk ), validator( ntk, vps )
  {
    if constexpr ( !validator_t::use_odc_ )
    {
      assert( ps.odc_levels == 0 && "to consider ODCs, circuit_validator::use_odc (the last template parameter) has to be turned on" );
    }
    else
    {
      vps.odc_levels = ps.odc_levels;
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
      simulate_nodes<Ntk>( ntk, tts, sim, true );
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
    if ( potential_gain == 0 )
    {
      return std::nullopt;
    }

    ResubFn resub_fn( ntk, ps, st.functor_st, tts, n, divs, std::min( potential_gain - 1, ps.max_inserts ) );
    for ( auto j = 0u; j < ps.max_trials; ++j )
    {
      check_tts( n );
      for ( auto const& d : divs )
      {
        check_tts( d );
      }

      uint32_t size = 0;
      TT const care = call_with_stopwatch( st.time_odc, [&]() {
        return ( ps.odc_levels == 0 ) ? sim.compute_constant( true ) : ~observability_dont_cares( ntk, n, sim, tts, ps.odc_levels );
      });
      const auto res = call_with_stopwatch( st.time_functor, [&]() {
        return resub_fn( size, care );
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
              if constexpr ( validator_t::use_odc_ )
              {
                call_with_stopwatch( st.time_sat_restart, [&]() {
                  validator.update();
                });
              }
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
            return validator.validate( n, c.divs, c.ckt, c.out_neg );
          });
          if ( valid )
          {
            if ( *valid )
            {
              ++st.num_resub;
              last_gain = potential_gain - size;
              if constexpr ( validator_t::use_odc_ )
              {
                call_with_stopwatch( st.time_sat_restart, [&]() {
                  validator.update();
                });
              }
              return translate( c, c.divs );
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
    call_with_stopwatch( st.time_sim, [&]() {
      sim.add_pattern( validator.cex );
    });

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

  unordered_node_map<TT, Ntk> tts;
  partial_simulator sim;

  validator_params vps;
  validator_t validator;
}; /* simulation_based_resub_engine */

} /* namespace detail */

template<class Ntk>
void sim_resubstitution( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
{
  static_assert( std::is_same<typename Ntk::base_type, aig_network>::value || std::is_same<typename Ntk::base_type, xag_network>::value, "Currently only supports AIG and XAG" );

  using resub_view_t = fanout_view<depth_view<Ntk>>;
  depth_view<Ntk> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  if ( ps.odc_levels != 0 )
  {
    using validator_t = circuit_validator<resub_view_t, bill::solvers::bsat2, false, true, true>;
    using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t, validator_t>>;

    resubstitution_stats st;
    typename resub_impl_t::engine_st_t engine_st;
    typename resub_impl_t::collector_st_t collector_st;

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
  else
  {
    using resub_impl_t = typename detail::resubstitution_impl<resub_view_t, typename detail::simulation_based_resub_engine<resub_view_t>>;

    resubstitution_stats st;
    typename resub_impl_t::engine_st_t engine_st;
    typename resub_impl_t::collector_st_t collector_st;

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
}

} /* namespace mockturtle */
