#include <mockturtle/networks/aig.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/write_aiger.hpp>
#include <mockturtle/views/dont_care_view.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <lorina/aiger.hpp>

#include "experiments.hpp"

#include <iostream>
#include <string>
#include <fmt/format.h>

int main1()
{
  using namespace mockturtle;

  aig_network aig;

  auto const x1 = aig.create_pi();
  auto const x2 = aig.create_pi();
  auto const x3 = aig.create_pi();
  auto const x4 = aig.create_pi();
  auto const x5 = aig.create_pi();
  
  auto const n4 = aig.create_and( !x1, x2 );
  auto const n5 = aig.create_and( x1, !x2 );
  auto const n6 = aig.create_or( n4, n5 );
  auto const n7 = aig.create_and( n6, x3 );
  auto const n8 = aig.create_and( !n7, x4 );
  auto const n9 = aig.create_and( n7, x5 );

  auto const y1 = n8;
  auto const y2 = n9;
  aig.create_po( y1 );
  aig.create_po( y2 );
  //aig.create_po( n6 );

  /* external output (next-stage) logic */
  // auto const n9 = aig.create_and( !y1, !y2 );
  // auto const n10 = aig.create_and( y1, y2 );
  // auto const n11 = aig.create_and( !n9, !n10 );
  // aig.create_po( !n9 ); // y1 OR y2
  // aig.create_po( n11 ); // y1 XOR y2

  write_aiger( aig, "toy.aig" );

#if 1
  /* EXCDC pattern: 11-- */
  aig_network cdc;
  auto const x1cdc = cdc.create_pi();
  auto const x2cdc = cdc.create_pi();
  cdc.create_pi();
  cdc.create_pi();
  cdc.create_po( cdc.create_and( x1cdc, x2cdc ) );

  dont_care_view exdc( aig );
  /* EXOEC pair: 01 = 10 */
  exdc.add_EXOEC_pair( std::vector<bool>( {0, 1} ), std::vector<bool>( {1, 0} ) );
  exdc.add_EXOEC_pair( std::vector<bool>( {0, 0} ), std::vector<bool>( {1, 0} ) );
  //exdc.add_EXOEC_pair( std::vector<bool>( {0, 1} ), std::vector<bool>( {0, 0} ) );
  //std::cout << "num OECs = " << exdc.num_OECs() << "\n";
#else
  dont_care_view exdc( aig );
#endif
  //exdc.build_oec_network();

  fmt::print( "[i] original: I/O = {}/{}, #gates = {}\n", aig.num_pis(), aig.num_pos(), aig.num_gates() );

  resubstitution_params ps;
  ps.max_pis = 100; //std::numeric_limits<uint32_t>::max();
  ps.max_divisors = std::numeric_limits<uint32_t>::max();
  ps.max_inserts = std::numeric_limits<uint32_t>::max();
  ps.odc_levels = -1;
  sim_resubstitution( exdc, ps );
  aig = cleanup_dangling( aig );

  write_aiger( aig, "toyOPT.aig" );
  fmt::print( "[i] optimized: I/O = {}/{}, #gates = {}\n", aig.num_pis(), aig.num_pos(), aig.num_gates() );

  return 0;
}

int main2()
{
  using namespace mockturtle;
  
  aig_network ntk;
  if ( lorina::read_aiger( "../experiments/contest_results/ex42.aig", aiger_reader( ntk ) ) != lorina::return_code::success )
  {
    std::cout << "read failed\n";
    return -1;
  }
  uint32_t size_before = ntk.num_gates();

  {
    default_simulator<kitty::dynamic_truth_table> sim( ntk.num_pis() );
    auto const tts = simulate_nodes<kitty::dynamic_truth_table>( ntk, sim );
    ntk.foreach_po( [&]( auto f ){
      kitty::print_binary( ntk.is_complemented( f ) ? ~tts[f] : tts[f] ); std::cout << "\n";
    });
  }

#if 0
  /* EXCDC */
  using signal = typename aig_network::signal;
  aig_network cdc;
  for ( auto i = 0u; i < ntk.num_pis(); ++i )
    cdc.create_pi();
  std::vector<signal> pats;
  for ( auto i = 0u; i < num_excdc; ++i )
  {
    std::vector<signal> ins;
    cdc.foreach_pi( [&]( auto n ){
      if ( std::rand() % ntk.num_pis() < excdc_involved_rate )
        ins.emplace_back( std::rand() % 2 ? !cdc.make_signal( n ) : cdc.make_signal( n ) );
    });
    if ( ins.size() == 1 )
      pats.emplace_back( ins[0] );
    if ( ins.size() > 1 )
      pats.emplace_back( cdc.create_nary_and( ins ) );
  }
  cdc.create_po( cdc.create_nary_or( pats ) );

  dont_care_view exdc( ntk, cdc );
#else
  dont_care_view exdc( ntk );
#endif
 
#if 1
  /* EXODC */
  kitty::cube cond1( "00-" );
  exdc.add_EXODC_ito_pos( cond1, 2 );
  kitty::cube cond2( "1--" );
  exdc.add_EXODC_ito_pos( cond2, 2 );
  kitty::cube cond3( "--0" );
  exdc.add_EXODC_ito_pos( cond3, 0 );
  //exdc.build_oec_network();
#endif

  resubstitution_params ps;
  ps.max_pis = 100; //std::numeric_limits<uint32_t>::max();
  ps.max_divisors = std::numeric_limits<uint32_t>::max();
  ps.max_inserts = std::numeric_limits<uint32_t>::max();
  ps.odc_levels = -1;

  sim_resubstitution( exdc, ps );
  ntk = cleanup_dangling( ntk );

  {
    default_simulator<kitty::dynamic_truth_table> sim( ntk.num_pis() );
    auto const tts = simulate_nodes<kitty::dynamic_truth_table>( ntk, sim );
    ntk.foreach_po( [&]( auto f ){
      kitty::print_binary( ntk.is_complemented( f ) ? ~tts[f] : tts[f] ); std::cout << "\n";
    });
  }

  fmt::print( "[i] optimized: #gates = {} {}\n", ntk.num_gates(), size_before > ntk.num_gates() ? fmt::format( "(saved {} nodes)", size_before - ntk.num_gates() ) : "" );
  //write_aiger( ntk, "ex16OPT.aig" );
  return 0;
}

int main3( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  const std::string result_path = "../experiments/contest_results/";

  std::string run_only_one = "";
  if ( argc == 2 )
    run_only_one = std::string( argv[1] );

  resubstitution_params ps;
  ps.max_pis = 100; //std::numeric_limits<uint32_t>::max();
  ps.max_divisors = std::numeric_limits<uint32_t>::max();
  ps.max_inserts = std::numeric_limits<uint32_t>::max();
  ps.odc_levels = -1;
  //ps.verbose = true;

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t> exp( "exdc", "benchmark", "I", "O", "#gates ori", "#gates opt", "delta" );

  for ( auto id = 0u; id < 100; ++id )
  {
    std::string benchmark = fmt::format( "ex{:02}", id );
    if ( run_only_one != "" && benchmark != run_only_one )
    {
      continue;
    }

    aig_network ntk;
    if ( lorina::read_aiger( result_path + benchmark + ".aig", aiger_reader( ntk ) ) != lorina::return_code::success )
    {
      std::cout << "read failed\n";
      continue;
    }

    fmt::print( "[i] benchmark {}: I/O = {}/{}, #gates = {}\n", benchmark, ntk.num_pis(), ntk.num_pos(), ntk.num_gates() );
    if ( ntk.num_pos() > 20 || ntk.num_pos() < 3 || ntk.num_gates() > 500 ) continue;
    uint32_t size_before = ntk.num_gates();

#if 1
    /* EXCDC */
    using signal = typename aig_network::signal;
    uint32_t num_excdc = (1 << ntk.num_pis() ) * 0.01 + 2;
    uint32_t excdc_involved_rate = ntk.num_pis() * 0.9;
    fmt::print( "    using {} EXCDC patterns, excdc_involved_rate = {}/{}\n", num_excdc, excdc_involved_rate, ntk.num_pis() );

    aig_network cdc;
    for ( auto i = 0u; i < ntk.num_pis(); ++i )
      cdc.create_pi();
    std::vector<signal> pats;
    for ( auto i = 0u; i < num_excdc; ++i )
    {
      std::vector<signal> ins;
      cdc.foreach_pi( [&]( auto n ){
        if ( std::rand() % ntk.num_pis() < excdc_involved_rate )
          ins.emplace_back( std::rand() % 2 ? !cdc.make_signal( n ) : cdc.make_signal( n ) );
      });
      if ( ins.size() == 1 )
        pats.emplace_back( ins[0] );
      if ( ins.size() > 1 )
        pats.emplace_back( cdc.create_nary_and( ins ) );
    }
    cdc.create_po( cdc.create_nary_or( pats ) );
    write_aiger( cdc, result_path + benchmark + "CDC.aig" );

    dont_care_view exdc( ntk, cdc );
#else
    dont_care_view exdc( ntk );
#endif
 
#if 0
    /* EXODC */
    uint32_t num_exodc = ntk.num_pos()*0.5;
    uint32_t exodc_involved_rate = ntk.num_pos()*0.7;
    fmt::print( "    using {} EXODC conditions, exodc_involved_rate = {}/{}\n", num_exodc, exodc_involved_rate, ntk.num_pos() );
    for ( auto j = 0u; j < num_exodc; ++j )
    {
      uint32_t po_id = std::rand() % ntk.num_pos();
      kitty::cube cond;
      ntk.foreach_po( [&]( auto f, auto i ){
        (void)f;
        if ( i != po_id && std::rand() % ntk.num_pos() < exodc_involved_rate )
        {
          cond.set_mask( i );
          if ( std::rand() % 2 )
            cond.set_bit( i );
        }
      });
      if ( cond.num_literals() > 0 )
      {
        exdc.add_EXODC_ito_pos( cond, po_id );
        fmt::print( "    PO{}: ", po_id ); cond.print( ntk.num_pos() ); std::cout << "\n";
      }
    }
#endif
    //exdc.build_oec_network();

    sim_resubstitution( exdc, ps );
    ntk = cleanup_dangling( ntk );

    write_aiger( ntk, result_path + benchmark + "OPT.aig" );
    fmt::print( "[i] optimized: #gates = {} {}\n", ntk.num_gates(), size_before > ntk.num_gates() ? fmt::format( "(saved {} nodes)", size_before - ntk.num_gates() ) : "" );
    exp( benchmark, ntk.num_pis(), ntk.num_pos(), size_before, ntk.num_gates(), size_before - ntk.num_gates() );
  }

  exp.save();
  exp.table();

  return 0;
}

int main( int argc, char* argv[] )
{
  using namespace experiments;
  using namespace mockturtle;

  const std::string result_path = "../experiments/contest_results/";

  std::string run_only_one = "";
  if ( argc == 2 )
    run_only_one = std::string( argv[1] );

  resubstitution_params ps;
  ps.max_pis = 100; //std::numeric_limits<uint32_t>::max();
  ps.max_divisors = std::numeric_limits<uint32_t>::max();
  ps.max_inserts = std::numeric_limits<uint32_t>::max();
  ps.odc_levels = -1;
  //ps.verbose = true;

  experiment<std::string, uint32_t, uint32_t, float, float> exp( "exdc", "benchmark", "#gates ori", "delta", "%", "time" );

  for ( auto id = 70u; id < 80; ++id )
  {
    std::string benchmark = fmt::format( "ex{:02}", id );
    if ( run_only_one != "" && benchmark != run_only_one )
    {
      continue;
    }

    aig_network ntk;
    if ( lorina::read_aiger( result_path + benchmark + ".aig", aiger_reader( ntk ) ) != lorina::return_code::success )
    {
      std::cout << "read failed\n";
      continue;
    }

    fmt::print( "[i] benchmark {}: I/O = {}/{}, #gates = {}\n", benchmark, ntk.num_pis(), ntk.num_pos(), ntk.num_gates() );
    uint32_t size_before = ntk.num_gates();

#if 1
    /* EXCDC */
    aig_network cdc;
    if ( lorina::read_aiger( result_path + "ex70CDC.aig", aiger_reader( cdc ) ) != lorina::return_code::success )
    {
      std::cout << "read failed\n";
      continue;
    }

    default_simulator<kitty::dynamic_truth_table> sim( cdc.num_pis() );
    auto const tts = simulate_nodes<kitty::dynamic_truth_table>( cdc, sim );
    cdc.foreach_po( [&]( auto f ){
      auto const tt = ntk.is_complemented( f ) ? ~tts[f] : tts[f];
      std::cout << kitty::count_ones(tt) << "\n";
    });

    dont_care_view exdc( ntk, cdc );
#else
    dont_care_view exdc( ntk );
#endif
 
#if 0
    /* EXODC */
    kitty::cube cond1( "0--" );
    exdc.add_EXODC_ito_pos( cond1, 1 );
#endif
    //exdc.build_oec_network();

    stopwatch<>::duration time{0};
    {
      stopwatch t(time);
      sim_resubstitution( exdc, ps );
      ntk = cleanup_dangling( ntk );
    }

    //write_aiger( ntk, result_path + benchmark + "OPT.aig" );
    fmt::print( "[i] optimized: #gates = {} {}\n", ntk.num_gates(), size_before > ntk.num_gates() ? fmt::format( "(saved {} nodes)", size_before - ntk.num_gates() ) : "" );
    exp( benchmark, size_before, size_before - ntk.num_gates(), float(size_before - ntk.num_gates()) / float(size_before) * 100.0, to_seconds(time) );
  }

  exp.save();
  exp.table();

  return 0;
}