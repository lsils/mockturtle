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

#include <lorina/lorina.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting_splitters.hpp>
#include <mockturtle/algorithms/mig_resub_splitters.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/akers.hpp>
#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/aqfp_view.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_limit_view.hpp>
#include <mockturtle/views/fanout_view.hpp>

#include <fmt/format.h>

#include <string>
#include <vector>

namespace detail
{

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
    else if ( ( ( ntk.fanout_size( n ) > 4 ) ) ) //&& ( ntk.fanout_size( n ) < 17 ) )
      cost = 3;
    return cost;
  }
};

} // namespace detail

int main()
{
  using namespace experiments;

  experiments::experiment<std::string, uint32_t, uint32_t, float, uint32_t, uint32_t, float, uint32_t, uint32_t, float, uint32_t, uint32_t, float, bool>
    exp( "mig_aqfp", "benchmark", "size MIG", "Size Opt MIG", "Impr. Size", "depth MIG", "depth Opt MIG", "Impr. depth", "jj MIG", "jj Opt MIG", "Impr. jj", "jj levels MIG", "jj levels Opt MIG", "Impr. jj levels", "eq cec" );

  for ( auto const& benchmark : aqfp_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    /* read starting point benchmark */
    mockturtle::mig_network mig;
    if ( lorina::read_verilog( experiments::benchmark_aqfp_path( benchmark ), mockturtle::verilog_reader( mig ) ) != lorina::return_code::success )
    {
      std::cout << "ERROR reading benchmark file (verilog)" << std::endl;
      return -1;
    }

    mockturtle::depth_view_params ps_d;
    mockturtle::fanout_limit_view_params ps{16u};
    mockturtle::mig_network mig2;
    mockturtle::fanout_limit_view lim_mig( mig2, ps );
    cleanup_dangling( mig, lim_mig );

    mockturtle::aqfp_view aqfp_sonia{lim_mig};
    int buffers = aqfp_sonia.num_buffers();

    mockturtle::depth_view mig_d2{lim_mig};
    mockturtle::depth_view mig_dd_d2{lim_mig, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
    float const size_b = lim_mig.num_gates();
    float const depth_b = mig_d2.depth();
    float const jj_b = lim_mig.num_gates() * 6 + buffers * 2; 
    float const jj_levels_b = mig_dd_d2.depth();

    while ( true )
    {
      mockturtle::mig_network mig46;
      mockturtle::fanout_limit_view lim_mig( mig46, ps );
      cleanup_dangling( mig, lim_mig );

      mockturtle::mig_algebraic_depth_rewriting_params ps_a;
      mockturtle::depth_view mig1_dd_d{lim_mig, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
      auto depth = mig1_dd_d.depth();
      ps_a.overhead = 1.5;
      ps_a.strategy = mockturtle::mig_algebraic_depth_rewriting_params::dfs;
      ps_a.allow_area_increase = true;
      mig_algebraic_depth_rewriting_splitters( mig1_dd_d, ps_a );
      mig = mig1_dd_d;
      mockturtle::mig_network mig3;
      mockturtle::fanout_limit_view lim_mig2( mig3, ps );
      cleanup_dangling( mig, lim_mig2 );

      mockturtle::resubstitution_params ps_r;
      ps_r.max_divisors = 250;
      ps_r.max_inserts = 1;
      ps_r.preserve_depth = true;

      auto size_o = mig.num_gates();
      mockturtle::depth_view mig2_dd_r{lim_mig2, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
      mig_resubstitution_splitters( mig2_dd_r, ps_r );

      mig = mig2_dd_r;
      mockturtle::mig_network mig4;
      mockturtle::fanout_limit_view lim_mig3( mig4, ps );
      cleanup_dangling( mig, lim_mig3 );

      mockturtle::mig_network mig5;
      mockturtle::fanout_limit_view lim_mig3_b( mig5, ps );
      cleanup_dangling( mig, lim_mig3_b );


      mockturtle::akers_resynthesis<mockturtle::mig_network> resyn;
      refactoring( lim_mig3, resyn, {}, nullptr, detail::jj_cost<mockturtle::mig_network>() );

      mig = lim_mig3;
      mockturtle::mig_network mig6;
      mockturtle::fanout_limit_view lim_mig4( mig6, ps );
      cleanup_dangling( mig, lim_mig4 );

      mockturtle::depth_view mig2_dd_a{lim_mig4, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
      mockturtle::depth_view mig2_dd{lim_mig4};

      if ( ( lim_mig4.num_gates() > lim_mig3_b.num_gates() ) || ( mig2_dd_a.depth() > mig2_dd_r.depth() ) || ( mig2_dd.depth() > mig1_dd_d.depth() ) )
        mig = lim_mig3_b;
      else
      {
        mig = lim_mig4;
      }

      if ( ( mig.num_gates() >= size_o ) || ( mig1_dd_d.depth() >= depth ) )
        break;
    }

    mockturtle::mig_network mig3;
    mockturtle::fanout_limit_view lim_mig4( mig3, ps );
    cleanup_dangling( mig, lim_mig4 );

    mockturtle::depth_view mig3_dd{lim_mig4};
    mockturtle::depth_view mig3_dd_s{lim_mig4, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};

    mockturtle::aqfp_view aqfp_sonia2{lim_mig4};
    buffers = aqfp_sonia2.num_buffers();
    
    float const size_c = lim_mig4.num_gates();
    float const depth_c = mig3_dd.depth();
    float const jj_c = size_c * 6 + buffers * 2;
    float const jj_levels_c = mig3_dd_s.depth();

    auto cec = experiments::abc_cec_aqfp( lim_mig4, benchmark );
    auto impr_size = ( size_b - size_c ) / ( size_b );
    auto impr_depth = ( depth_b - depth_c ) / ( depth_b );
    auto impr_jj = ( jj_b - jj_c ) / ( jj_b );
    auto impr_levels = ( jj_levels_b - jj_levels_c ) / ( jj_levels_b );

    exp( benchmark, size_b, size_c, impr_size * 100,
         depth_b, depth_c, impr_depth * 100,
         jj_b, jj_c, impr_jj * 100,
         jj_levels_b, jj_levels_c, impr_levels * 100,
         cec );
  }

  exp.save();
  exp.table();
  return 0;
}
