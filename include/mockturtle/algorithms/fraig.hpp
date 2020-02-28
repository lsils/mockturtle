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

/*!
  \file fraig.hpp
  \brief Functionally Equivalent Gate Removal Based on Resubstitution

  \author Siang-Yun Lee
*/

#pragma once

#include <mockturtle/algorithms/sim_resub.hpp>
#include <mockturtle/algorithms/cleanup.hpp>

namespace mockturtle
{
struct fraig_params
{
  /*! \brief Number of initial simulation patterns = 2^num_pattern_base. */
  uint32_t num_pattern_base{15};

  /*! \brief Number of reserved blocks(64 bits) for generated simulation patterns. */
  uint32_t num_reserved_blocks{100};

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{500};

  /*! \brief Maximum fanout of a node to be considered as root. */
  uint32_t skip_fanout_limit_for_roots{1000};

  /*! \brief Maximum fanout of a node to be considered as divisor. */
  uint32_t skip_fanout_limit_for_divisors{100};

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};

  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{10};

  std::default_random_engine::result_type random_seed{0};
};

struct fraig_stats
{
  stopwatch<>::duration time_total{0};

  /* time for simulations */
  stopwatch<>::duration time_sim{0};

  /* time for SAT solving */
  stopwatch<>::duration time_sat{0};

  /* time for divisor collection */
  stopwatch<>::duration time_divs{0};

  /* time for doing substitution */
  stopwatch<>::duration time_substitute{0};

  stopwatch<>::duration time_resub{0};

  /* number of redundant (equivalent) nodes */
  uint32_t num_redundant{0};

  /* number of constant nodes */
  uint32_t num_constant{0};
  
  uint32_t num_generated_patterns{0};
  uint32_t num_cex{0};
  uint64_t num_total_divisors{0};
};


template<class Ntk>
void fraig( Ntk& ntk, fraig_params const& psf = {}, fraig_stats* pst = nullptr )
{
  simresub_params ps;
  simresub_stats st;

  ps.num_pattern_base = psf.num_pattern_base? psf.num_pattern_base: 15u;
  ps.num_reserved_blocks = psf.num_reserved_blocks? psf.num_reserved_blocks: 100u;
  ps.max_divisors = psf.max_divisors? psf.max_divisors: 500u;
  ps.skip_fanout_limit_for_roots = psf.skip_fanout_limit_for_roots? psf.skip_fanout_limit_for_roots: 1000u;
  ps.skip_fanout_limit_for_divisors = psf.skip_fanout_limit_for_divisors? psf.skip_fanout_limit_for_divisors: 100u;
  ps.progress = psf.progress? psf.progress: false;
  ps.verbose = psf.verbose? psf.verbose: false;
  ps.max_pis = psf.max_pis? psf.max_pis: 10u;
  ps.max_inserts = 0u;
  ps.random_seed = psf.random_seed? psf.random_seed: 0;
  

  sim_resubstitution( ntk, ps, &st );
  ntk = cleanup_dangling( ntk );

  if ( pst )
  {
    pst->time_total = st.time_total;
    pst->time_sim = st.time_sim;
    pst->time_sat = st.time_sat;
    pst->time_divs = st.time_divs;
    pst->time_substitute = st.time_substitute;
    pst->time_resub = st.time_resub0;
    pst->num_redundant = st.num_div0_accepts;
    pst->num_constant = st.num_constant;
    pst->num_generated_patterns = st.num_generated_patterns;
    pst->num_cex = st.num_cex;
    pst->num_total_divisors = st.num_total_divisors;
  }
}

} /* namespace mockturtle */