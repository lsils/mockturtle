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
  \file resubstitution2.hpp
  \brief New resubstitution framework

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../../traits.hpp"
#include "../../utils/progress_bar.hpp"
#include "../../utils/stopwatch.hpp"
#include "../../views/depth_view.hpp"
#include "../../views/fanout_view.hpp"

#include "../detail/resub_utils.hpp"
#include "../dont_cares.hpp"
#include "../reconv_cut.hpp"

#include <vector>

namespace mockturtle::experimental
{

/*! \brief Parameters for window-based resubstitution. */
struct window_based_resub_params
{
  /*! \brief Maximum number of PIs of reconvergence-driven cuts. */
  uint32_t max_pis{8};

  /*! \brief Maximum number of divisors to consider. */
  uint32_t max_divisors{150};

  /*! \brief Maximum number of nodes added by resubstitution. */
  uint32_t max_inserts{2};

  /*! \brief Maximum fanout of a node to be considered as root. */
  uint32_t skip_fanout_limit_for_roots{1000};

  /*! \brief Maximum fanout of a node to be considered as divisor. */
  uint32_t skip_fanout_limit_for_divisors{100};

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};

  /*! \brief Use don't cares for optimization. */
  bool use_dont_cares{false};

  /*! \brief Window size for don't cares calculation. */
  uint32_t window_size{12u};

  /*! \brief Whether to update node levels lazily. */
  bool update_levels_lazily{true};

  /*! \brief Whether to prevent from increasing depth. */
  bool preserve_depth{false};
};

/*! \brief Statistics for window-based resubstitution. */
template<class ResynStats>
struct window_based_resub_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{0};

  /*! \brief Accumulated runtime of structural analysis and simulation. */
  stopwatch<>::duration time_windowing{0};

  /*! \brief Accumulated runtime of resynthesis. */
  stopwatch<>::duration time_resynthesis{0};

  /*! \brief Total number of divisors. */
  uint64_t num_total_divisors{0};

  /*! \brief Total number of gain. */
  uint64_t estimated_gain{0};

  /*! \brief Initial network size (before resubstitution). */
  uint64_t initial_size{0};

  ResynStats resyn_st;

  void report() const
  { }
};

/*! \brief Window-based resubstitution.
 */
template<class Ntk, class ResynEngine, class TT = kitty::dynamic_truth_table>
class window_based_resub
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  using cut_params = reconvergence_driven_cut_parameters;
  using cut_comp_t = std::function<std::vector<node>( Ntk const&, node const&, cut_params const& )>;
  using mffc_comp_t = std::function<void( Ntk const&, node const&, std::vector<node> const&, std::vector<node>& )>;
  using callback_t = std::function<bool( Ntk&, node const&, signal const& )>;

  using stats = window_based_resub_stats<typename ResynEngine::stats>;

  explicit window_based_resub( Ntk& ntk, window_based_resub_params const& ps, stats& st )
      : ntk( ntk ), ps( ps ), st( st ), cps( ps.max_pis ),
        cut_comp( []( Ntk const& ntk, node const& n, cut_params const& cps ){ return reconvergence_driven_cut( ntk, {n}, cps ).first; } ),
        mffc_comp( []( Ntk const& ntk, node const& n, std::vector<node> const& leaves, std::vector<node>& mffc ){ node_mffc_inside mffc_mgr( ntk ); mffc_mgr.run( n, leaves, mffc ); } ),
        callback( detail::substitute_fn<Ntk> )
  {
    st.initial_size = ntk.num_gates();
    if ( ps.update_levels_lazily )
    {
      lazy_update_event = register_lazy_level_update_events( ntk );
    }
    divs.reserve( ps.max_divisors );
    tts.reserve( ps.max_divisors + ps.max_pis );
  }

  ~window_based_resub()
  {
    if ( ps.update_levels_lazily )
    {
      release_lazy_level_update_events( ntk, lazy_update_event );
    }
  }

  void set_cut_comp( cut_comp_t const&& fn )
  {
    cut_comp = fn;
  }

  void set_mffc_comp( mffc_comp_t const&& fn )
  {
    mffc_comp = fn;
  }

  void set_callback( callback_t const&& fn )
  {
    callback = fn;
  }

  void run()
  {
    stopwatch t( st.time_total );
    progress_bar pbar{ntk.size(), "win-resub |{0}| node = {1:>4}   cand = {2:>4}   est. gain = {3:>5}", ps.progress};

    auto const size = ntk.num_gates();
    ntk.foreach_gate( [&]( auto const& n, auto i ) {
      if ( i >= size )
      {
        return false; /* terminate: Do not try to optimize new nodes */
      }
      if ( ntk.fanout_size( n ) > ps.skip_fanout_limit_for_roots )
      {
        return true; /* skip nodes with many fanouts */
      }
      pbar( i, i, candidates, st.estimated_gain );

      /* compute cut, compute MFFC, collect divisors */
      call_with_stopwatch( st.time_windowing, [&]() {
        divs.clear();
        leaves = cut_comp( ntk, n, cps );
        mffc_comp( ntk, n, leaves, mffc );
        collect_divisors( n, leaves, mffc, divs );
      });
      if ( divs.size() > ps.max_divisors )
      {
        return true; /* skip too large window */
      }
      /* simulate the window */
      call_with_stopwatch( st.time_windowing, [&]() {
        tts.clear();
        simulate( ntk, leaves, divs, tts ); // TODO
      });

      st.num_total_divisors += divs.size();

      /* try to find a resubstitution with the divisors */
      auto il = call_with_stopwatch( st.time_resynthesis, [&]() {
        typename ResynEngine::params ps_resyn;
        ps_resyn.reserve = divs.size() + 2;
        ResynEngine engine( st.resyn_st, ps_resyn );
        return engine( tts[n], care, std::begin( divs ), std::end( divs ), tts, std::min( mffc.size() - 1, ps.max_inserts ) );
        // TODO: take care of ids of tts
      });
      if ( !il )
      {
        return true; /* next */
      }

      /* update progress bar */
      candidates++;
      st.estimated_gain += mffc.size() - *il.size(); // TODO

      /* update network */
      insert( *il ); // TODO
      bool updated = callback( ntk, n, *g );
      (void)updated;

      return true; /* next */
    } );
  }

private:
  Ntk& ntk;

  window_based_resub_params const& ps;
  window_based_resub_stats& st;

  std::vector<node> leaves, divs, mffc;
  std::vector<TT> tts;

  /* temporary statistics for progress bar */
  uint32_t candidates{0};

  /* functors for each step */
  cut_params const cps;
  cut_comp_t& cut_comp;
  mffc_comp_t& mffc_comp;
  callback_t& callback;

  /* events */
  std::shared_ptr<typename network_events<Ntk>::modified_event_type> lazy_update_event;
};

} /* namespace mockturtle::experimental */