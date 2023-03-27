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
  \file explorer.hpp
  \brief Implements the design space explorer engine

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "lut_mapper.hpp"
#include "collapse_mapped.hpp"
#include "klut_to_graph.hpp"
#include "cut_rewriting.hpp"
#include "mapper.hpp"
#include "node_resynthesis/mig_npn.hpp"
#include "resubstitution.hpp"
#include "mig_resub.hpp"
#include "cleanup.hpp"
#include "miter.hpp"
#include "equivalence_checking.hpp"
#include "../networks/klut.hpp"
#include "../networks/mig.hpp"
#include "../views/mapping_view.hpp"
#include "../io/write_verilog.hpp"
#include "../utils/stopwatch.hpp"

#include <random>

#define explorer_debug 0

namespace mockturtle
{

struct explorer_params
{
  /*! \brief Number of iterations to run with different random seed, restarting from the original
   * network (including the first iteration). */
  uint32_t num_restarts{1u};

  /*! \brief Initial random seed used to generate random seeds randomly. */
  uint32_t random_seed{0u};

  /*! \brief Maximum number of steps in each iteration. */
  uint32_t max_steps{100000u};

  /*! \brief Maximum number of steps without improvement in each iteration. */
  uint32_t max_steps_no_impr{1000000u};

  /*! \brief Number of compressing scripts to run per step. */
  uint32_t compressing_scripts_per_step{3u};

  /*! \brief Timeout per iteration in seconds. */
  uint32_t timeout{30u};

  /*! \brief Be verbose. */
  bool verbose{false};

  /*! \brief Be very verbose. */
  bool very_verbose{false};
};

struct explorer_stats {};

template<class Ntk>
using script_t = std::function<void( Ntk&, uint32_t, uint32_t )>;

template<class Ntk>
using cost_fn_t = std::function<uint32_t( Ntk const& )>;

template<class Ntk>
std::function<uint32_t( Ntk const& )> size_cost_fn = []( Ntk const& ntk ){ return ntk.num_gates(); };

template<class Ntk>
class explorer
{
public:
  using RandEngine = std::default_random_engine;

  explorer( explorer_params const& ps, explorer_stats& st, cost_fn_t<Ntk> const& cost_fn = size_cost_fn<Ntk> )
      : _ps( ps ), _st( st ), cost( cost_fn )
  {
  }

  void add_decompressing_script( script_t<Ntk> const& algo, float weight = 1.0 )
  {
    decompressing_scripts.emplace_back( std::make_pair( algo, total_weights_dec ) );
    total_weights_dec += weight;
  }

  void add_compressing_script( script_t<Ntk> const& algo, float weight = 1.0 )
  {
    compressing_scripts.emplace_back( std::make_pair( algo, total_weights_com ) );
    total_weights_com += weight;
  }

  Ntk run( Ntk const& ntk )
  {
    if ( decompressing_scripts.size() == 0 )
    {
      std::cerr << "[e] No decompressing script provided.\n";
      return ntk;
    }
    if ( compressing_scripts.size() == 0 )
    {
      std::cerr << "[e] No compressing script provided.\n";
      return ntk;
    }

    RandEngine rnd( _ps.random_seed );
    Ntk best = ntk.clone();
    for ( auto i = 0u; i < _ps.num_restarts; ++i )
    {
      Ntk current = ntk.clone();
      run_one_iteration( current, rnd() );
      if ( cost( current ) < cost( best ) )
        best = current.clone();
    }
    return best;
  }

private:
  void run_one_iteration( Ntk& ntk, uint32_t seed )
  {
    if ( _ps.verbose )
      fmt::print( "[i] new iteration using seed {}\n", seed );
 
    stopwatch<>::duration elapsed_time{0};
    RandEngine rnd( seed );
    Ntk best = ntk.clone();
    uint32_t last_update{0u};
    for ( auto i = 0u; i < _ps.max_steps; ++i )
    {
    #if explorer_debug
      Ntk backup = ntk.clone();
    #endif

      if ( _ps.very_verbose )
        fmt::print( "[i] step {}: {} -> ", i, cost( ntk ) );
      {
        stopwatch t( elapsed_time );
        decompress( ntk, rnd, i );
        compress( ntk, rnd, i );
      }
      if ( _ps.very_verbose )
        fmt::print( "{}\n", cost( ntk ) );

    #if explorer_debug
      if ( !*equivalence_checking( *miter<Ntk>( ntk, best ) ) )
      {
        write_verilog( backup, "debug.v" );
        write_verilog( ntk, "wrong.v" );
        fmt::print( "NEQ at step {}! k = {}\n", i, 3 + (i % 4) );
        break;
      }
    #endif

      if ( cost( ntk ) < cost( best ) )
      {
        best = ntk.clone();
        last_update = i;
        if ( _ps.verbose )
          fmt::print( "[i] updated new best at step {}: {}\n", i, cost( best ) );
      }
      if ( i - last_update >= _ps.max_steps_no_impr )
      {
        if ( _ps.verbose )
          fmt::print( "[i] break iteration at step {} after {} steps without improvement (elapsed time: {} secs)\n", i, _ps.max_steps_no_impr, to_seconds( elapsed_time ) );
        break;
      }
      if ( to_seconds( elapsed_time ) >= _ps.timeout )
      {
        if ( _ps.verbose )
          fmt::print( "[i] break iteration at step {} after timeout of {} secs\n", i, to_seconds( elapsed_time ) );
        break;
      }
    }
    ntk = best;
  }

  void decompress( Ntk& ntk, RandEngine& rnd, uint32_t i )
  {
    std::uniform_real_distribution<> dis( 0.0, total_weights_dec );
    float r = dis( rnd );
    for ( auto const& p : decompressing_scripts )
    {
      if ( r >= p.second )
      {
        p.first( ntk, i, rnd() );
        break;
      }
    }
  }

  void compress( Ntk& ntk, RandEngine& rnd, uint32_t i )
  {
    std::uniform_real_distribution<> dis( 0.0, total_weights_com );
    for ( auto j = 0u; j < _ps.compressing_scripts_per_step; ++j )
    {
      float r = dis( rnd );
      for ( auto const& p : compressing_scripts )
      {
        if ( r >= p.second )
        {
          p.first( ntk, i, rnd() );
          break;
        }
      }
    }
  }

private:
  const explorer_params _ps;
  explorer_stats& _st;

  std::vector<std::pair<script_t<Ntk>, float>> decompressing_scripts;
  float total_weights_dec{0.0};
  std::vector<std::pair<script_t<Ntk>, float>> compressing_scripts;
  float total_weights_com{0.0};

  cost_fn_t<Ntk> cost;
};

mig_network default_mig_synthesis( mig_network const& ntk, explorer_params const ps = {} )
{
  using Ntk = mig_network;

  explorer_stats st;
  explorer<Ntk> expl( ps, st );

  expl.add_decompressing_script( []( Ntk& _ntk, uint32_t i, uint32_t rand ){
    lut_map_params mps;
    mps.cut_enumeration_ps.cut_size = 3 + (i % 4);
    mapping_view<Ntk> mapped{ _ntk };
    lut_map( mapped, mps );
    const auto klut = *collapse_mapped_network<klut_network>( mapped );
    // missing: k-LUT network optimization (mfs)
    _ntk = convert_klut_to_graph<Ntk>( klut );
  } );

  expl.add_decompressing_script( []( Ntk& _ntk, uint32_t i, uint32_t rand ){
    aig_network broken = cleanup_dangling<mig_network, aig_network>( _ntk );
    _ntk = cleanup_dangling<aig_network, mig_network>( broken );
  } );

  expl.add_compressing_script( []( Ntk& _ntk, uint32_t i, uint32_t rand ){
    //mig_npn_resynthesis resyn;
    //_ntk = cut_rewriting( _ntk, resyn );

    mig_npn_resynthesis resyn{ true };
    exact_library<mig_network, mig_npn_resynthesis> exact_lib( resyn );
    map_params mps;
    mps.skip_delay_round = true;
    mps.required_time = std::numeric_limits<double>::max();
    mps.area_flow_rounds = 1;
    mps.enable_logic_sharing = rand & 0x1; /* high-effort remap */
    _ntk = map( _ntk, exact_lib, mps );

    resubstitution_params rps;
    rps.max_inserts = 5;
    depth_view depth_mig{ _ntk };
    fanout_view fanout_mig{ depth_mig };
    mig_resubstitution2( fanout_mig );
    _ntk = cleanup_dangling( _ntk );
  } );

  return expl.run( ntk );
}

} // namespace mockturtle