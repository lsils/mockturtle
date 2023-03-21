/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
#include <lorina/diagnostics.hpp>
#include <lorina/verilog.hpp>
#include <mockturtle/algorithms/aqfp/buffer_insertion.hpp>
#include <mockturtle/algorithms/aqfp/buffer_verification.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/io/write_dot.hpp>
#include <mockturtle/networks/buffered.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/utils/name_utils.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/names_view.hpp>

#include <iostream>

using namespace mockturtle;

template<class Ntk>
std::pair<uint32_t, typename Ntk::node> remove_buffer_chains_rec( Ntk& ntk, typename Ntk::node n, typename Ntk::node parent )
{
  if ( ntk.visited( n ) == ntk.trav_id() )
    return std::make_pair( 0, n );
  ntk.set_visited( n, ntk.trav_id() );
  if ( ntk.is_pi( n ) )
    return std::make_pair( 0, n );

  if ( ntk.is_buf( n ) )
  {
    // splitter
    if ( ntk.fanout_size( n ) > 1 )
    {
      ntk.foreach_fanin( n, [&]( auto f ){
        remove_buffer_chains_rec( ntk, ntk.get_node( f ), n );
      } );
      return std::make_pair( 0, n );
    }

    // single-output buffer: can be part of a chain to be removed
    std::pair<uint32_t, typename Ntk::node> ret;
    ntk.foreach_fanin( n, [&]( auto f ){
      auto [count, origin] = remove_buffer_chains_rec( ntk, ntk.get_node( f ), n );
      if ( count == 3 )
      {
        if ( parent != 0 )
        {
          ntk.replace_in_node( parent, n, ntk.make_signal( origin ) );
          ntk.take_out_node( n );
        }
        else
        {
          ntk.replace_in_outputs( n, ntk.make_signal( origin ) );
          ntk.take_out_node( n );
        }
        ret = std::make_pair( 0, origin ); // TODO: take care of complementation
      }
      else
      {
        ret = std::make_pair( count + 1, origin );
      }
    } );
    return ret;
  }

  // gate
  ntk.foreach_fanin( n, [&]( auto f ){
    remove_buffer_chains_rec( ntk, ntk.get_node( f ), n );
  } );
  return std::make_pair( 0, n );
}

template<class Ntk>
void remove_buffer_chains( Ntk& ntk )
{
  static_assert( is_buffered_network_type_v<Ntk>, "Ntk is not a buffered network" );

  ntk.incr_trav_id();
  ntk.foreach_po( [&]( auto f ){
    remove_buffer_chains_rec( ntk, ntk.get_node( f ), 0 );
  } );
}

int main( int argc, char* argv[] )
{
  std::string run_only_one = "";
  if ( argc == 2 )
    run_only_one = std::string( argv[1] );

  using namespace mockturtle;
  using namespace experiments;
  /* NOTE 1: To run the "optimal" insertion, please clone and build Z3: https://github.com/Z3Prover/z3
   * And have `z3` available as a system call
   */

  /* NOTE 2: Please clone this repository: https://github.com/lsils/SCE-benchmarks
   * And put in the following string the relative path from your build path to SCE-benchmarks/ISCAS/strashed/
   */
  // std::string benchmark_path = "../../SCE-benchmarks/ISCAS/strashed/";
  // std::string benchmark_path = "../../SCE-benchmarks/MCNC/original/";
   std::string benchmark_path = "../../SCE-benchmarks/EPFL/MIGs/";
  static const std::string benchmarks_iscas[] = {
      "adder1", "adder8", "mult8", "counter16", "counter32", "counter64", "counter128",
      "c17", "c432", "c499", "c880", "c1355", "c1908", "c2670", "c3540", "c5315", "c6288", "c7552",
      "sorter32", "sorter48", "alu32" };
  static const std::string benchmarks_mcnc[] = {
      /*"5xp1",*/ "c1908", "c432", "c5315", "c880", "chkn", "count", "dist", "in5", "in6", "k2",
      "m3", "max512", "misex3", "mlp4", "prom2", "sqr6", "x1dn" };
  const auto benchmarks_epfl = experiments::epfl_benchmarks();

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, bool>
      exp( "buffer_insertion", "benchmark", "#gates", "#buffers", "#buff real", "#PI buf", "#PO buf", "#pop buf", "depth_JJ", "runtime", "verified" );

  buffer_insertion_params ps;
  ps.scheduling = buffer_insertion_params::better_depth;
  ps.optimization_effort = buffer_insertion_params::none;
  ps.max_chunk_size = 10000;

  // ASP-DAC etc. SoTA works
  //ps.assume.num_phases = 1;
  //ps.assume.ci_phases = {0u};
  //ps.assume.ci_capacity = 1;
  //ps.assume.splitter_capacity = 4;
  //ps.assume.balance_cios = true;
  
  // best possible relaxation
  ps.assume.ci_capacity = 2;
  ps.assume.ci_phases = { 3u, 4u, 5u };

  uint32_t total_buffers{ 0 }, total_depth{ 0 };
  for ( auto benchmark : benchmarks_epfl )
  {
    if ( run_only_one != "" && benchmark != run_only_one )
      continue;
    if ( benchmark == "hyp" && run_only_one != "hyp" )
      continue;
    std::cout << "\n[i] processing " << benchmark << "\n";

    names_view<mig_network> ntk;
    lorina::text_diagnostics td;
    lorina::diagnostic_engine diag( &td );
    auto res = lorina::read_verilog( benchmark_path + benchmark + ".v", verilog_reader( ntk ), &diag );
    if ( res != lorina::return_code::success )
    {
      std::cout << "read failed\n";
      continue;
    }
    ntk.set_network_name( benchmark );

    stopwatch<>::duration t{ 0 };
    buffer_insertion aqfp( ntk, ps );
    buffered_mig_network bufntk;
    std::vector<uint32_t> pi_levels( ntk.num_pis() );
    uint32_t num_buffers = call_with_stopwatch( t, [&]() {
      return aqfp.run( bufntk, pi_levels );
    } );
    bool verified = verify_aqfp_buffer( bufntk, ps.assume, pi_levels );
    auto const levels = schedule_buffered_network_with_PI_levels( bufntk, pi_levels );

    remove_buffer_chains( bufntk );

    // names_view named_bufntk{bufntk};
    // restore_pio_names_by_order( ntk, named_bufntk );
    // write_verilog( named_bufntk, benchmark_path + "../best_insertion/" + benchmark + "_buffered.v" );

#if 0
    depth_view<buffered_mig_network> depth_buffered( bufntk );
    depth_buffered.foreach_node( [&]( auto n ){ depth_buffered.set_level( n, levels[n] ); } );
    write_dot( depth_buffered, benchmark + ".dot" );
    std::system( fmt::format( "dot -Tpng -o {0}.png {0}.dot; rm {0}.dot; open {0}.png", benchmark ).c_str() );
#endif

    depth_view d{ ntk };
    total_buffers += num_buffers;
    total_depth += aqfp.depth();

    uint32_t max_fanout{ 0 };
    ntk.foreach_node( [&]( auto const& n ) {
      if ( !ntk.is_constant( n ) )
        max_fanout = std::max( max_fanout, ntk.fanout_size( n ) );
    } );

    //node_map<std::pair<uint32_t, uint32_t>, mig_network> extrefs( ntk );
    //ntk.foreach_po( [&]( auto f ){
    //  if ( ntk.is_complemented( f ) )
    //    extrefs[f].second++;
    //  else
    //    extrefs[f].first++;
    //});
    //ntk.foreach_node( [&]( auto n ){
    //  if ( extrefs[n].first > 0 && extrefs[n].second > 0 )
    //  {
    //    fmt::print( "node {} is referenced positively {} times, negatively {} times, and by {} gates.\n"
    //                "its fanout tree has {} buffers. it {} a PI.\n",
    //      n, extrefs[n].first, extrefs[n].second, ntk.fanout_size( n ) - extrefs[n].first - extrefs[n].second,
    //      aqfp.num_buffers( n ), ntk.is_pi(n)? "is": "is not" );
    //  }
    //});

    uint32_t pi_buffers{0}, po_buffers{0}, popular_gates{0}, popular_buffers{0}, unique_pos{0}, num_buffers_real{0};
    ntk.foreach_pi( [&]( auto n ){
      pi_buffers += aqfp.num_buffers( n );
    });
    ntk.incr_trav_id();
    ntk.foreach_po( [&]( auto f ){
      auto n = ntk.get_node( f );
      if ( ntk.visited( n ) == ntk.trav_id() )
        return;
      ntk.set_visited( n, ntk.trav_id() );
      unique_pos++;
      po_buffers += aqfp.num_buffers( n );
    });
    ntk.foreach_node( [&]( auto n ){
      if ( ntk.fanout_size( n ) > 4 )
      {
        ++popular_gates;
        popular_buffers += aqfp.num_buffers( n );
      }
    });
    bufntk.foreach_node( [&]( auto n ){
      if ( bufntk.is_buf( n ) && !bufntk.is_dead( n ) )
        num_buffers_real++;
    });
    exp( benchmark, ntk.num_gates(), num_buffers, num_buffers_real, pi_buffers, po_buffers, popular_buffers, aqfp.depth(), to_seconds( t ), verified );
  }

  exp.save();
  exp.table();

  std::cout << "[i] total buffers = " << total_buffers << ", total depth = " << total_depth << "\n";

  return 0;
}