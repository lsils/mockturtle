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

#include <string>
#include <vector>

#include <fmt/format.h>
#include <lorina/aiger.hpp>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/bidecomposition.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_minmc.hpp>
#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/algorithms/xag_resub_withDC.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/xag.hpp>

#include <experiments.hpp>

using namespace experiments;
using namespace mockturtle;

namespace detail
{
template<class Ntk>
struct free_xor_cost
{
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n ) const
  {
    return ntk.is_xor( n ) ? 0 : 1;
  }
};

template<class Ntk>
struct mc_cost
{
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n ) const
  {
    return ntk.is_and( n ) ? 1 : 0;
  }
};
} // namespace detail*/

int main( int argc, char** argv )
{
  if ( argc <= 1 )
  {
    std::cout << "input shoudl be ''db [database]''\n";
    return 0;
  }

  experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, float, uint32_t, float, bool> exp( "xag_minmc", "benchmark", "num_and", "num_xor", "num_and_opt", "num_xor_opt", "improvement %", "iterations", "avg. runtime [s]", "equivalent" );

  for ( auto const& benchmark : crypto_benchmarks() )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    uint32_t num_and = 0u, num_xor = 0u; 
    float num_and_init = 0u;
    xag_network xag;
    lorina::read_verilog( crypto_benchmark_path( benchmark ), verilog_reader( xag ) );

    xag.foreach_gate( [&]( auto f ) {
      if ( xag.is_and( f ) )
      {
        num_and++;
      }
    } );
    xag.foreach_gate( [&]( auto f ) {
      if ( xag.is_xor( f ) )
      {
        num_xor++;
      }
    } );

    std::cout << " num and = " << num_and << std::endl;
    std::cout << " num xor = " << num_xor << std::endl;
    num_and_init = num_and;

    uint32_t num_and_aft = num_and - 1;
    uint32_t num_xor_aft = 0u;

    cut_rewriting_params ps;
    ps.cut_enumeration_ps.cut_size = 6;
    ps.cut_enumeration_ps.cut_limit = 12;
    ps.verbose = true;
    ps.progress = true;
    ps.min_cand_cut_size = 2u;

    refactoring_params ps2;
    ps2.verbose = true;
    ps2.progress = true;
    ps2.allow_zero_gain = false;
    ps2.max_pis = 15;
    ps2.use_dont_cares = true;

    resubstitution_params ps3;
    ps3.max_divisors = 100;
    ps3.max_inserts = 4;
    ps3.max_pis = 8u;
    ps3.progress = true;
    ps3.verbose = true;
    ps3.use_dont_cares = true;

    xag_minmc_resynthesis resyn( argv[1] );
    bidecomposition_resynthesis<xag_network> resyn2;

    auto i = 0u;
    const clock_t begin_time = clock();

    while ( num_and > num_and_aft )
    {
      if ( i > 0 )
      {
        num_and = num_and_aft;
      }
      i++;
      num_and_aft = 0u;
      num_xor_aft = 0u;

      cut_rewriting( xag, resyn, ps, nullptr, ::detail::mc_cost<xag_network>() );
      xag = cleanup_dangling( xag );
      refactoring( xag, resyn2, ps2, nullptr, ::detail::free_xor_cost<xag_network>() );
      xag = cleanup_dangling( xag );
      using view_t = depth_view<fanout_view<xag_network>>;
      fanout_view<xag_network> fanout_view{xag};
      view_t resub_view{fanout_view};
      resubstitution_minmc_withDC( resub_view, ps3 );
      xag = cleanup_dangling( xag );

      xag.foreach_gate( [&]( auto f ) {
        if ( xag.is_and( f ) )
        {
          num_and_aft++;
        }
      } );
      xag.foreach_gate( [&]( auto f ) {
        if ( xag.is_xor( f ) )
        {
          num_xor_aft++;
        }
      } );
    }

    std::cout << " num and after = " << num_and_aft << std::endl;
    std::cout << " num xor after = " << num_xor_aft << std::endl;

    const auto cec = abc_cec( xag, benchmark );

    float impro = (( num_and_init - num_and_aft )/num_and_init) * 100; 
  
    exp( benchmark, num_and_init, num_xor, num_and_aft, num_xor_aft, impro, i, (float( clock () - begin_time ) /  CLOCKS_PER_SEC) / i, cec );
  }

  exp.save();
  exp.table();

  return 0;
}
