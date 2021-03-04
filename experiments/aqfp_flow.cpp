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

#include "experiments.hpp"

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting_splitters.hpp>
#include <mockturtle/algorithms/mig_resub_splitters.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/akers.hpp>
#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/aqfp_view.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_limit_view.hpp>

#include <lorina/lorina.hpp>
#include <fmt/format.h>

#include <string>
#include <vector>

template<class Ntk>
struct jj_cost
{
  uint32_t operator()( Ntk const& ntk, mockturtle::node<Ntk> const& n ) const
  {
    uint32_t cost = 0;
    if ( ntk.fanout_size( n ) == 1 )
      cost = ntk.fanout_size( n );
    else if ( ntk.fanout_size( n ) <= 4 )
      cost = 3;
    else
      cost = 11;
    return cost;
  }
};

template<class Ntk>
struct fanout_cost_depth_local
{
  uint32_t operator()( Ntk const& ntk, mockturtle::node<Ntk> const& n ) const
  {
    uint32_t cost = 0;
    if ( ntk.is_pi( n ) )
      cost = 0;
    else if ( ntk.fanout_size( n ) == 1 )
      cost = 1;
    else if ( ( ntk.fanout_size( n ) > 1 ) && ( ( ntk.fanout_size( n ) < 5 ) ) )
      cost = 2;
    else if ( ( ( ntk.fanout_size( n ) > 4 ) ) )
      cost = 3;
    return cost;
  }
};

using namespace mockturtle;

using cost_fn_t = fanout_cost_depth_local<mig_network>;
using limit_view_t = fanout_limit_view<mig_network>;
using aqfp_view_t = aqfp_view<limit_view_t, true>;
using depth_view_t = depth_view<limit_view_t>;
using jj_depth_view_t = depth_view<limit_view_t, cost_fn_t>;

void get_statistics( mig_network const& mig, uint32_t& size, uint32_t& depth, uint32_t& jj_count, uint32_t& jj_depth )
{
  limit_view_t mig_limited = cleanup_dangling<mig_network, limit_view_t>( mig );
  aqfp_view_t mig_aqfp( mig_limited );
  depth_view_t mig_depth( mig_limited );
  jj_depth_view_t mig_jj_depth( mig_limited, cost_fn_t() );

  size = mig_limited.num_gates();
  depth = mig_depth.depth();
  jj_count = size * 6 + mig_aqfp.num_buffers() * 2;
  jj_depth = mig_jj_depth.depth();
}

int main()
{
  using namespace experiments;

  bool const verbose = false; // turn on/off verbose printing of intermediate results
  int iteration;

  experiment<std::string, uint32_t, uint32_t, float, uint32_t, uint32_t, float, bool>
    exp1( "table1", "benchmark", "size MIG", "Size Opt MIG", "Impr. Size", "depth MIG", "depth Opt MIG", "Impr. depth", "eq cec" );
  experiment<std::string, uint32_t, uint32_t, float, uint32_t, uint32_t, float, bool>
    exp2( "table3", "benchmark", "jj MIG", "jj Opt MIG", "Impr. jj", "jj levels MIG", "jj levels Opt MIG", "Impr. jj levels", "eq cec" );

  for ( auto const& benchmark : aqfp_benchmarks() )
  {
    if ( verbose )
    {
      fmt::print( "[i] processing {}\n", benchmark );
    }
    mig_network mig;
    lorina::read_verilog( benchmark_aqfp_path( benchmark ), verilog_reader( mig ) );

    uint32_t size_before, depth_before, jj_before, jj_levels_before;
    get_statistics( mig, size_before, depth_before, jj_before, jj_levels_before );

    if ( verbose )
    {
      fmt::print( "--- Starting point: size = {}, depth = {}, JJ count = {}, JJ depth = {}\n", size_before, depth_before, jj_before, jj_levels_before );
      iteration = 0;
    }

    while ( true )
    {
      uint32_t size, depth, jj_count, jj_depth;
      get_statistics( mig, size, depth, jj_count, jj_depth );
      auto const jj_depth_before_rewrite = jj_depth;
      if ( verbose )
      {
        ++iteration;
        fmt::print( "--- > Iteration {}: size = {}, JJ depth = {}", iteration, size, jj_depth );
      }

      /* Section 3.2: Depth optimization with algebraic rewriting -- limiting fanout size increase */
      {
        mig_algebraic_depth_rewriting_params ps_alg_rewrite;
        ps_alg_rewrite.overhead = 1.5;
        ps_alg_rewrite.strategy = mig_algebraic_depth_rewriting_params::dfs;
        ps_alg_rewrite.allow_area_increase = true;

        limit_view_t mig_limited = cleanup_dangling<mig_network, limit_view_t>( mig );
        jj_depth_view_t mig_jj_depth{mig_limited, cost_fn_t()};
        mig_algebraic_depth_rewriting_splitters( mig_jj_depth, ps_alg_rewrite );
        mig = cleanup_dangling<jj_depth_view_t, mig_network>( mig_jj_depth );
      }

      get_statistics( mig, size, depth, jj_count, jj_depth );
      auto const jj_depth_after_rewrite = jj_depth;
      auto const size_before_resub = size;
      if ( verbose )
      {
        fmt::print( " --rewrite--> size = {}, JJ depth = {}", size, jj_depth );
      }

      /* Section 3.3: Size optimization with Boolean resubstitution -- considering fanout size limitation */
      {
        resubstitution_params ps_resub;
        ps_resub.max_divisors = 250;
        ps_resub.max_inserts = 1;
        ps_resub.preserve_depth = true;

        limit_view_t mig_limited = cleanup_dangling<mig_network, limit_view_t>( mig );
        jj_depth_view_t mig_jj_depth{mig_limited, cost_fn_t()};
        mig_resubstitution_splitters( mig_jj_depth, ps_resub );
        mig = cleanup_dangling<jj_depth_view_t, mig_network>( mig_jj_depth );
      }

      get_statistics( mig, size, depth, jj_count, jj_depth );
      if ( verbose )
      {
        fmt::print( " --resub--> size = {}, JJ depth = {}", size, jj_depth );
      }

      /* Section 3.4: Further size optimization with refactoring */
      mig_network mig_copy = mig;
      auto const size_before_refactor = size;
      auto const depth_before_refactor = depth;
      auto const jj_depth_before_refactor = jj_depth;
      {
        limit_view_t mig_limited = cleanup_dangling<mig_network, limit_view_t>( mig );
        akers_resynthesis<mig_network> resyn;
        refactoring( mig_limited, resyn, {}, nullptr, jj_cost<mig_network>() );
        mig = cleanup_dangling<limit_view_t, mig_network>( mig_limited );
      }

      get_statistics( mig, size, depth, jj_count, jj_depth );
      if ( verbose )
      {
        fmt::print( " --refactor--> size = {}, JJ depth = {}", size, jj_depth );
      }
      
      /* Undo refactoring if (1) size increases; or (2) JJ depth increases; or (3) depth increases */
      if ( ( size > size_before_refactor ) || ( jj_depth > jj_depth_before_refactor ) || ( depth > depth_before_refactor ) )
      {
        if ( verbose )
        {
          fmt::print( " [UNDO]" );
        }
        mig = mig_copy;
      }

      get_statistics( mig, size, depth, jj_count, jj_depth );
      if ( verbose )
      {
        fmt::print( " --> size = {}, JJ depth = {}\n", size, jj_depth );
      }
      /* Terminate when (1) [resub + refactor] cannot decrease size anymore; or (2) rewriting cannot decrease JJ depth anymore */
      if ( ( size >= size_before_resub ) || ( jj_depth_after_rewrite >= jj_depth_before_rewrite ) )
      {
        break;
      }
    }

    uint32_t size_after, depth_after, jj_after, jj_levels_after;
    get_statistics( mig, size_after, depth_after, jj_after, jj_levels_after );

    if ( verbose )
    {
      fmt::print( "--- After AQFP flow: size = {}, depth = {}, JJ count = {}, JJ depth = {}\n", size_after, depth_after, jj_after, jj_levels_after );
    }

    bool const cec = abc_cec_aqfp( mig, benchmark );
    float const impr_size = ( (float)size_before - (float)size_after ) / (float)size_before * 100;
    float const impr_depth = ( (float)depth_before - (float)depth_after ) / (float)depth_before * 100;
    float const impr_jj = ( (float)jj_before - (float)jj_after ) / (float)jj_before * 100;
    float const impr_levels = ( (float)jj_levels_before - (float)jj_levels_after ) / (float)jj_levels_before * 100;

    exp1( benchmark, size_before, size_after, impr_size, depth_before, depth_after, impr_depth, cec );
    exp2( benchmark, jj_before, jj_after, impr_jj, jj_levels_before, jj_levels_after, impr_levels, cec );
  }

  fmt::print( "Table 1: Results for size and depth optimization over MIG\n" );
  exp1.save();
  exp1.table();

  fmt::print( "Table 3: Results for area, delay, and number of buffers & splitters for MIGs mapped into AQFP technology\n" );
  exp2.save();
  exp2.table();

  return 0;
}
