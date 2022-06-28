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
  \file func_reduction.hpp
  \brief Fast functional resubstitution

  \author Hanyu Wang
*/

#pragma once

#include "../../traits.hpp"
#include "../../views/depth_view.hpp"
#include "../../views/fanout_view.hpp"
#include "../../utils/index_list.hpp"
#include "../../networks/xag.hpp"
#include "../../networks/aig.hpp"
#include "../detail/resub_utils.hpp"
#include "../resyn_engines/xag_resyn.hpp"
#include "../resyn_engines/aig_enumerative.hpp"
#include "../resyn_engines/mig_resyn.hpp"
#include "../resyn_engines/mig_enumerative.hpp"
#include "../simulation.hpp"
#include "../dont_cares.hpp"
#include "../circuit_validator.hpp"
#include "../pattern_generation.hpp"
#include "../../io/write_patterns.hpp"
#include <kitty/kitty.hpp>

#include <optional>
#include <functional>
#include <vector>

namespace mockturtle::experimental
{
struct func_reduction_params
{
  /*! \brief Maximum number of clauses of the SAT solver. */
  uint32_t max_clauses{1000};

  /*! \brief Conflict limit for the SAT solver. */
  uint32_t conflict_limit{1000};

  /*! \brief Random seed for the SAT solver (influences the randomness of counter-examples). */
  uint32_t random_seed{1};

  /*! \brief Maximum number of trials to call the resub functor. */
  uint32_t max_trials{100};

  /*! \brief Whether to utilize ODC, and how many levels. 0 = no. -1 = Consider TFO until PO. */
  int32_t odc_levels{0};

  /* \brief Be verbose. */
  bool verbose{true};
};

struct func_reduction_stats
{
  void report() const
  {
  }
};

template<class Ntk>
class func_reduction_impl
{

public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using TT = kitty::partial_truth_table;
  using index_list_t = large_xag_index_list;
  using validator_t = circuit_validator<Ntk, bill::solvers::bsat2, /*use_pushpop*/false, /*randomize*/true, /*use_odc*/false>;

  explicit func_reduction_impl( Ntk& ntk, func_reduction_params const& ps, func_reduction_stats& st )
      : ntk( ntk ), ps( ps ), st( st ), 
        validator( ntk, {ps.max_clauses, 0, ps.conflict_limit, ps.random_seed} ), tts( ntk )
  {
    sim = partial_simulator( ntk.num_pis(), 1024 );
    pattern_generation( ntk, sim );
    simulate_nodes<Ntk>( ntk, tts, sim, true );
  }
  ~func_reduction_impl()
  {
  }

  void run()
  {
    uint32_t initial_size = ntk.num_gates();
    topo_view{ntk}.foreach_node( [&]( auto const n, auto i ) { 
      if ( i >= initial_size )
      {
        return false; /* terminate */
      }
      auto res = mini_solver( n );
      if ( res )
      {
        ntk.substitute_node( n, *res );
      }
      return true;
    } );
  }

  std::optional<signal> mini_solver( node const n )
  {
    signal res;
    TT const& tt = tts[n];
    if ( kitty::count_ones( tt ) == 0 )
    {
      res = ntk.get_constant( 0 );
      if ( mini_validate( n, res ) ) return res;
    }
    if ( kitty::count_zeros( tt ) == 0 )
    {
      res = ntk.get_constant( 1 );
      if ( mini_validate( n, res ) ) return res;
    }

    if ( div_tts.find( tt ) != div_tts.end() )
    {
      res = ntk.make_signal( div_tts[tt] );
      if ( mini_validate( n, res ) ) return res;
    }
    if ( div_tts.find( ~tt ) != div_tts.end() )
    {
      res = !ntk.make_signal( div_tts[tt] );
      if ( mini_validate( n, res ) ) return res;
    }
    div_tts[ tt ] = n;
    return std::nullopt;
  }
  bool mini_validate( node const n, signal const d )
  {
    auto valid = validator.validate( n, d );
    if ( valid )
    {
      if ( *valid )
      {
        return true;
      }
      else
      {
        // sim.add_pattern( validator.cex );
      }
    } 
    return false;
  }
private:
  Ntk& ntk;

  func_reduction_params const& ps;
  func_reduction_stats& st;

  partial_simulator sim;
  validator_t validator;
  incomplete_node_map<TT, Ntk> tts;
  std::unordered_map<TT, node, kitty::hash<TT>> div_tts;
};

template<class Ntk>
void func_reduction( Ntk& ntk, func_reduction_params const& ps = {}, func_reduction_stats* pst = nullptr )
{
  static_assert( std::is_same_v<typename Ntk::base_type, aig_network>, "Ntk::base_type is not aig_network" );
  
  func_reduction_stats st;
  func_reduction_impl<Ntk> p( ntk, ps, st );
  p.run();

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
}


} /* namespace mockturtle::experimental */
