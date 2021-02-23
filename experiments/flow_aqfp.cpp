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

#include "experiments.hpp"

#include <lorina/lorina.hpp>
#include <mockturtle/algorithms/balancing.hpp>
#include <mockturtle/algorithms/balancing/sop_balancing.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/collapse_mapped.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/detail/database_generator.hpp>
#include <mockturtle/algorithms/lut_mapping.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting.hpp>
// #include <mockturtle/algorithms/mig_algebraic_rewriting_buffers.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting_splitters.hpp>
// #include <mockturtle/algorithms/mig_feasible_resub.hpp>
// #include <mockturtle/algorithms/mig_resub.hpp>
// #include <mockturtle/algorithms/mig_resub_splitters.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/akers.hpp>
#include <mockturtle/algorithms/node_resynthesis/exact.hpp>
// #include <mockturtle/algorithms/node_resynthesis/mig4_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/blif_reader.hpp>
// #include <mockturtle/io/index_list.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/aig.hpp>
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

template<class Ntk>
uint32_t compute_maxfanout( Ntk ntk )
{
  unsigned max_fanout = 0;
  ntk.foreach_gate( [&]( auto const& n ) {
    if ( ntk.fanout_size( n ) > max_fanout )
      max_fanout = ntk.fanout_size( n );
    return true;
  } );
  return max_fanout;
}

template<class Ntk>
int cost( Ntk ntk, mockturtle::node<Ntk> const& n )
{
  auto cost = 0;
  if ( ( ntk.fanout_size( n ) > 1 ) && ( ntk.fanout_size( n ) < 5 ) )
    cost = 1;
  else if ( ( ntk.fanout_size( n ) ) > 4 ) //&& ( ntk.fanout_size( n ) < 17 ) )
    cost = 2;
  return cost;
}

template<class Ntk>
int compute_buffers( Ntk mig )
{
  mockturtle::depth_view_params ps_d;
  mockturtle::depth_view depth_mig{mig, detail::fanout_cost_depth_local<Ntk>(), ps_d};
  std::vector<int> buffers( mig.size(), 0 );
  int number_buff = 0;

  mig.foreach_gate( [&]( auto f ) {
    auto main_depth = depth_mig.level( f );
    mig.foreach_fanin( f, [&]( auto const& s ) {
      if ( mig.is_pi( mig.get_node( s ) ) )
        return true;
      auto index = mig.node_to_index( mig.get_node( s ) );
      if ( index == 0 )
        return true;
      int diff = main_depth - 1 - depth_mig.level( mig.get_node( s ) ) - buffers[index];
      if ( diff >= 0 )
      {
        buffers[index] = buffers[index] + diff;
      }
      return true;
    } );
  } );

  auto total_depth = depth_mig.depth();
  mig.foreach_po( [&]( auto s ) {
    if ( depth_mig.level( mig.get_node( s ) ) == total_depth )
      return true;
    auto index = mig.node_to_index( mig.get_node( s ) );
    if ( index == 0 )
      return true;
    int diff = total_depth - depth_mig.level( mig.get_node( s ) ) - buffers[index];
    if ( diff >= 0 )
    {
      buffers[index] = buffers[index] + diff;
    }
    return true;
  } );
  for ( auto h = 0u; h < buffers.size(); h++ )
  {
    number_buff = number_buff + buffers[h];
  }
  return number_buff;
}

template<class Ntk>
int compute_buffers_not_shared( Ntk mig )
{
  mockturtle::depth_view_params ps_d;
  mockturtle::depth_view depth_mig{mig, detail::fanout_cost_depth_local<Ntk>(), ps_d};
  std::vector<std::vector<int>> buffers( mig.size() );
  int number_buff = 0;

  mig.foreach_gate( [&]( auto f ) {
    auto main_depth = depth_mig.level( f );
    //auto index2 = mig.node_to_index( f );
    mig.foreach_fanin( f, [&]( auto const& s ) {
      auto index = mig.node_to_index( mig.get_node( s ) );
      if ( index == 0 )
        return true;
      if ( mig.is_pi( mig.get_node( s ) ) ) /* this will not balance the pis*/
        return true;
      unsigned diff = main_depth - 1 - depth_mig.level( mig.get_node( s ) ) - detail::cost( mig, f );
      for ( auto g = 0u; g < diff; g++ )
      {
        if ( g < buffers[index].size() )
          buffers[index][g]++;
        else
          buffers[index].push_back( 1 );
      }
      return true;
    } );
  } );

  // balacing pos
  auto total_depth = depth_mig.depth();
  mig.foreach_po( [&]( auto s ) {
    if ( depth_mig.level( mig.get_node( s ) ) == total_depth )
      return true;
    auto index = mig.node_to_index( mig.get_node( s ) );
    if ( index == 0 )
      return true;
    if ( mig.is_pi( mig.get_node( s ) ) )
      return true;
    int diff = total_depth - depth_mig.level( mig.get_node( s ) ); // - buffers[index];
    for ( auto g = 0; g < diff; g++ )
    {
      if ( g < int( buffers[index].size() ) )
        buffers[index][g]++;
      else
        buffers[index].push_back( 1 );
    }
    return true;
  } );

  for ( auto h = 0u; h < buffers.size(); h++ )
  {
    if ( buffers[h].size() == 0 )
      continue;
    number_buff = number_buff + buffers[h].size();
    for ( auto g = 0u; g < buffers[h].size(); g++ )
    {
      int x = buffers[h][g] / 4 + ( buffers[h][g] % 4 != 0 );
      number_buff = number_buff + x - 1;
    }
  }
  return number_buff;
}

template<class Ntk>
uint32_t JJ_number_final( Ntk ntk )
{
  auto JJ = 0;
  ntk.foreach_gate( [&]( auto const& n ) {
    if ( ntk.fanout_size( n ) == 1 )
      JJ = JJ + 6;
    else if ( ntk.fanout_size( n ) <= 4 )
      JJ = JJ + 8;
    else if ( ntk.fanout_size( n ) <= 16 )
      JJ = JJ + 16;
    // this should not happen in the new limit fanout view
    else if ( ntk.fanout_size( n ) <= 20 )
      JJ = JJ + 16 + 8;
    else if ( ntk.fanout_size( n ) <= 32 )
      JJ = JJ + 16 * 2;
    else if ( ntk.fanout_size( n ) <= 36 )
      JJ = JJ + 16 * 2 + 8;
    else if ( ntk.fanout_size( n ) <= 48 )
      JJ = JJ + 16 * 3;
    return true;
  } );
  //std::cout << " JJ = " << JJ << std::endl;
  auto buffers = compute_buffers_not_shared( ntk );
  JJ = JJ + buffers * 2;
  return JJ;
}

template<class Ntk>
uint32_t buffers_number_final( Ntk ntk )
{
  auto JJ = 0;
  ntk.foreach_gate( [&]( auto const& n ) {
    if ( ntk.fanout_size( n ) == 1 )
      return true;
    else if ( ntk.fanout_size( n ) <= 4 )
      JJ = JJ + 1;
    else if ( ntk.fanout_size( n ) <= 16 )
      JJ = JJ + 5;
    return true;
  } );
  //std::cout << " JJ = " << JJ << std::endl;
  auto buffers = compute_buffers_not_shared( ntk );
  JJ = JJ + buffers;
  return JJ;
}

template<class Ntk>
uint32_t compute_fanout4( Ntk ntk )
{
  auto fanout4 = 0;
  ntk.foreach_gate( [&]( auto const& n ) {
    if ( ntk.fanout_size( n ) > 16 )
      fanout4++;
    return true;
  } );
  return fanout4;
}

} // namespace detail

template<typename Ntk>
mockturtle::klut_network lut_map( Ntk const& ntk, uint32_t k = 4 )
{
  mockturtle::write_verilog( ntk, "/tmp/network.v" );
  system( fmt::format( "../../abc/abc -q \"/tmp/network.v; &get; &if -a -K {}; &put; write_blif /tmp/output.blif\"", k ).c_str() );

  mockturtle::klut_network klut;
  if ( lorina::read_blif( "/tmp/output.blif", mockturtle::blif_reader( klut ) ) != lorina::return_code::success )
  {
    std::cout << "ERROR 1" << std::endl;
    std::abort();
    return klut;
  }
  return klut;
}

// void flow_mig_comparison()
// {
//   using namespace experiments;

//   mockturtle::mig_network mig_db;
//   read_verilog( "db.v", mockturtle::verilog_reader( mig_db ) );

//   mockturtle::mig4_npn_resynthesis_params ps;
//   ps.multiple_depth = false;
//   mockturtle::mig4_npn_resynthesis<mockturtle::mig_network> mig_resyn( mockturtle::detail::to_index_list( mig_db ), ps );

//   experiments::experiment<std::string, uint32_t, uint32_t, float, uint32_t, uint32_t, float, uint32_t, uint32_t, float, uint32_t, uint32_t, float, bool> exp( "mig_aqfp", "benchmark", "size MIG", "Size Opt MIG", "Impr. Size",
//                                                                                                                                                               "depth MIG", "depth Opt MIG", "Impr. depth", "jj MIG", "jj Opt MIG", "Impr. jj", "jj levels MIG", "jj levels Opt MIG", "Impr. jj levels", "eq cec" );

//   for ( auto const& benchmark : aqfp_benchmarks() )
//   {
//     fmt::print( "[i] processing {}\n", benchmark );

//     if ( benchmark != "c17" )
//       continue;

//     /* read aig */
//     mockturtle::mig_network mig1;
//     if ( lorina::read_aiger( experiments::benchmark_aqfp_path( benchmark ), mockturtle::aiger_reader( mig1 ) ) != lorina::return_code::success )
//     {
//       std::cout << "ERROR 2" << std::endl;
//       std::abort();
//       return;
//     }

//     auto klut = lut_map( mig1, 4u );

//     mockturtle::mig_network mig;
//     mig = mockturtle::node_resynthesis<mockturtle::mig_network>( klut, mig_resyn );
//     mockturtle::write_verilog( mig, fmt::format( "{}.v", benchmark ) );

//     mockturtle::depth_view_params ps_d;
//     mockturtle::depth_view mig_d2{mig};
//     mockturtle::depth_view mig_dd_d2{mig, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
//     float const size_b = mig.num_gates();
//     float const depth_b = mig_d2.depth();
//     float const jj_b = detail::JJ_number_final( mig );
//     float const jj_levels_b = mig_dd_d2.depth();

//     auto i = 0;
//     while ( true )
//     {
//       i++;
//       if ( i > 10 )
//         break;
//       mockturtle::mig_algebraic_depth_rewriting_params ps_a;
//       mockturtle::depth_view mig1_dd_d{mig, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
//       auto depth = mig1_dd_d.depth();
//       ps_a.overhead = 1.4;
//       ps_a.strategy = mockturtle::mig_algebraic_depth_rewriting_params::dfs;
//       ps_a.allow_area_increase = true;
//       mig_algebraic_depth_rewriting_splitters( mig1_dd_d, ps_a );
//       mig = mig1_dd_d;
//       mig = cleanup_dangling( mig );

//       // mockturtle::resubstitution_params ps_r;
//       // ps_r.max_divisors = 200;
//       // ps_r.max_inserts = 1;

//       auto size_o = mig.num_gates(); //lim_mig.num_gates();
//       mockturtle::depth_view mig2_dd_r{mig, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
//       // mig_resubstitution_splitters( mig2_dd_r, ps_r );
//       // mig = mig2_dd_r;
//       // mig = cleanup_dangling( mig );

//       auto const mig2 = cleanup_dangling( mig );

//       mockturtle::akers_resynthesis<mockturtle::mig_network> resyn;

//       refactoring( mig, resyn, {}, nullptr, detail::jj_cost<mockturtle::mig_network>() );
//       mig = cleanup_dangling( mig );

//       mockturtle::depth_view mig2_dd_a{mig, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
//       mockturtle::depth_view mig2_dd{mig};

//       if ( ( mig.num_gates() > mig2.num_gates() ) || ( detail::JJ_number_final( mig ) > detail::JJ_number_final( mig2 ) ) || ( mig2_dd_a.depth() > mig2_dd_r.depth() ) || ( mig2_dd.depth() > mig1_dd_d.depth() ) )
//         mig = mig2;

//       mig = cleanup_dangling( mig );

//       if ( ( mig.num_gates() >= size_o ) || ( mig1_dd_d.depth() >= depth ) )
//         break;
//     }

//     mockturtle::depth_view mig3_dd{mig};
//     mockturtle::depth_view mig3_dd_s{mig, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};

//     float const size_c = mig.num_gates();
//     float const depth_c = mig3_dd.depth();
//     float const jj_c = detail::JJ_number_final( mig );
//     float const jj_levels_c = mig3_dd_s.depth();

//     auto x = experiments::abc_cec_aqfp( mig, benchmark );

//     auto impr_size = ( size_b - size_c ) / ( size_b );
//     auto impr_depth = ( depth_b - depth_c ) / ( depth_b );
//     auto impr_jj = ( jj_b - jj_c ) / ( jj_b );
//     auto impr_levels = ( jj_levels_b - jj_levels_c ) / ( jj_levels_b );

//     exp( benchmark, size_b, size_c, impr_size * 100,
//          depth_b, depth_c, impr_depth * 100,
//          jj_b, jj_c, impr_jj * 100,
//          jj_levels_b, jj_levels_c, impr_levels * 100,
//          x );
//   }

//   exp.save();
//   exp.table();
// }

void flow_mig_lim()
{
  using namespace experiments;

  /* limit the fanout to max 16 */
  //using view_mig_t = mockturtle::fanout_limit_view<mockturtle::mig_network>;

  experiments::experiment<std::string, uint32_t, uint32_t, float, uint32_t, uint32_t, float, uint32_t, uint32_t, float, uint32_t, uint32_t, float, bool> exp( "mig_aqfp", "benchmark", "size MIG", "Size Opt MIG", "Impr. Size",
                                                                                                                                                              "depth MIG", "depth Opt MIG", "Impr. depth", "jj MIG", "jj Opt MIG", "Impr. jj", "jj levels MIG", "jj levels Opt MIG", "Impr. jj levels", "eq cec" );

  unsigned max_fanout = 0;
  std::string max_bench;
  float nodes_larger = 0;
  float count = 0;
  float best_difference = 0;
  std::string best_diff_bench;

  for ( auto const& benchmark : aqfp_benchmarks() )
  {
    count++;
    fmt::print( "[i] processing {}\n", benchmark );

    /* read aig */
    mockturtle::mig_network mig;
    if ( lorina::read_verilog( experiments::benchmark_aqfp_path( benchmark ), mockturtle::verilog_reader( mig ) ) != lorina::return_code::success )
    {
      std::cout << "ERROR 2" << std::endl;
      std::abort();
      return;
    }

  
    mockturtle::depth_view_params ps_d;

    // auto const size_b = mig.num_gates();

    mockturtle::fanout_limit_view_params ps{16u};
    mockturtle::mig_network mig2;
    mockturtle::fanout_limit_view lim_mig( mig2, ps ); //, ps);
    cleanup_dangling( mig, lim_mig );

    mockturtle::aqfp_view aqfp_sonia{lim_mig};
    int buffers = aqfp_sonia.num_buffers();

    std::cout << " buffers before = " << buffers << std::endl;

    lim_mig.foreach_gate( [&]( const auto& n ) { // needs fixing to consider POs?
      if ( lim_mig.fanout_size( n ) > 16 )
      {
        std::cout << " error!\n";
        std::abort();
        return;
      }
    } );

    mockturtle::depth_view mig_d2{lim_mig};
    mockturtle::depth_view mig_dd_d2{lim_mig, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
    float const size_b = lim_mig.num_gates();
    float const depth_b = mig_d2.depth();
    float const jj_b = lim_mig.num_gates() * 6 + buffers * 2; 
    float const jj_levels_b = mig_dd_d2.depth();

    auto const max_fanout_b = detail::compute_maxfanout( mig );
    if ( max_fanout_b > max_fanout )
    {
      max_fanout = max_fanout_b;
      max_bench = benchmark;
    }

    //std::cout << detail::compute_maxfanout( lim_mig ) << std::endl;

    auto const node_larger_16_b = detail::compute_fanout4( mig );
    nodes_larger = nodes_larger + node_larger_16_b;

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

      lim_mig2.foreach_gate( [&]( const auto& n ) { // needs fixing to consider POs
        if ( lim_mig2.fanout_size( n ) > 16 )
        {
          std::cout << " ERROR\n";
          std::abort();
          return;
        }
      } );

      // mockturtle::resubstitution_params ps_r;
      // ps_r.max_divisors = 250;
      // ps_r.max_inserts = 1;

      auto size_o = mig.num_gates();
      mockturtle::depth_view mig2_dd_r{lim_mig2, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
      // mig_resubstitution_splitters( mig2_dd_r, ps_r );

      // mig = mig2_dd_r;
      mockturtle::mig_network mig4;
      mockturtle::fanout_limit_view lim_mig3( mig4, ps );
      cleanup_dangling( mig, lim_mig3 );

      lim_mig3.foreach_gate( [&]( const auto& n ) { // needs fixing to consider POs
        if ( lim_mig3.fanout_size( n ) > 16 )
        {
          std::cout << " error!\n";
          std::abort();
          return;
        }
      } );

      mockturtle::mig_network mig5;
      mockturtle::fanout_limit_view lim_mig3_b( mig5, ps );
      cleanup_dangling( mig, lim_mig3_b );

      // auto g = lim_mig3_b.num_gates();
      std::cout << detail::compute_maxfanout( lim_mig3_b ) << std::endl;
      std::cout << detail::compute_fanout4( lim_mig3_b ) << std::endl;

      mockturtle::akers_resynthesis<mockturtle::mig_network> resyn;

      refactoring( lim_mig3, resyn, {}, nullptr, detail::jj_cost<mockturtle::mig_network>() );

      mig = lim_mig3;
      mockturtle::mig_network mig6;
      mockturtle::fanout_limit_view lim_mig4( mig6, ps );
      cleanup_dangling( mig, lim_mig4 );

      auto h = ( lim_mig3_b.num_gates() - lim_mig4.num_gates() ) / float( lim_mig3_b.num_gates() ) * 100;

      mockturtle::depth_view mig2_dd_a{lim_mig4, detail::fanout_cost_depth_local<mockturtle::mig_network>(), ps_d};
      mockturtle::depth_view mig2_dd{lim_mig4};

      if ( ( lim_mig4.num_gates() > lim_mig3_b.num_gates() ) || ( mig2_dd_a.depth() > mig2_dd_r.depth() ) || ( mig2_dd.depth() > mig1_dd_d.depth() ) )
        mig = lim_mig3_b;
      else
      {
        mig = lim_mig4;
        if ( h > best_difference )
        {
          best_difference = h;
          best_diff_bench = benchmark;
          std::cout << detail::compute_maxfanout( lim_mig4 ) << std::endl;
          std::cout << detail::compute_fanout4( lim_mig4 ) << std::endl;
        }
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

    std::cout << " buffers after = " << buffers << std::endl;
    
    float const size_c = lim_mig4.num_gates();
    float const depth_c = mig3_dd.depth();
    float const jj_c = size_c * 6 + buffers * 2; //detail::JJ_number_final( lim_mig4 );
    float const jj_levels_c = mig3_dd_s.depth();

    //assert(jj = jj_c);

    std::cout << "fanout opt" << detail::compute_maxfanout( lim_mig4 ) << std::endl;

    auto x = experiments::abc_cec_aqfp( lim_mig4, benchmark );

    auto impr_size = ( size_b - size_c ) / ( size_b );
    auto impr_depth = ( depth_b - depth_c ) / ( depth_b );
    auto impr_jj = ( jj_b - jj_c ) / ( jj_b );
    auto impr_levels = ( jj_levels_b - jj_levels_c ) / ( jj_levels_b );

    exp( benchmark, size_b, size_c, impr_size * 100,
         depth_b, depth_c, impr_depth * 100,
         jj_b, jj_c, impr_jj * 100,
         jj_levels_b, jj_levels_c, impr_levels * 100,
         x );
  }

  std::cout << "the max fanout is " << max_fanout << " for benchmark " << max_bench << std::endl;
  std::cout << "the best difference between resub and refactoring " << best_difference << " for benchmark " << best_diff_bench << std::endl;
  std::cout << "the average number of nodes with fanout > 16 is equal to  " << float( float( nodes_larger ) / float( count ) ) << std::endl;

  exp.save();
  exp.table();
}

// void flow_debug()
// {

//   for ( const auto& benchmark : experiments::test_benchmarks() )
//   {
//     fmt::print( "[i] processing {}\n", benchmark );

//     mockturtle::mig_network mig;
//     if ( lorina::read_verilog( experiments::benchmark_test_path( benchmark ), mockturtle::verilog_reader( mig ) ) != lorina::return_code::success )
//     {
//       fmt::print( "[e] parse error\n" );
//     }

//     int const buffers_ele = detail::buffers_number_final( mig );
//     mockturtle::aqfp_view aqfp_sonia{mig};
//     int const buffers_sonia = aqfp_sonia.num_buffers();

//     std::cout << " buffers ele = " << buffers_ele << " vs " << buffers_sonia << std::endl;
//   }
// }

int main()
{
  flow_mig_lim();
  return 0;
}
