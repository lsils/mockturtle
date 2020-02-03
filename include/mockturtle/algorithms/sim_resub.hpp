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
  \file sim_resub.hpp
  \brief Resubstitution

  \author Siang-Yun Lee
*/

#pragma once

#include "../utils/stopwatch.hpp"
#include <mockturtle/networks/aig.hpp>

namespace mockturtle
{

struct simresub_params
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
};

struct simresub_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{0};

  /*! \brief Accumulated runtime for cut computation. */
  stopwatch<>::duration time_cuts{0};

  /*! \brief Accumulated runtime for cut evaluation/computing a resubsitution. */
  stopwatch<>::duration time_eval{0};

  /*! \brief Accumulated runtime for mffc computation. */
  stopwatch<>::duration time_mffc{0};

  /*! \brief Accumulated runtime for divisor computation. */
  stopwatch<>::duration time_divs{0};

  /*! \brief Accumulated runtime for updating the network. */
  stopwatch<>::duration time_substitute{0};

  /*! \brief Accumulated runtime for simulation. */
  stopwatch<>::duration time_simulation{0};

  /*! \brief Initial network size (before resubstitution) */
  uint64_t initial_size{0};

  /*! \brief Total number of divisors  */
  uint64_t num_total_divisors{0};

  /*! \brief Total number of leaves  */
  uint64_t num_total_leaves{0};

  /*! \brief Total number of gain  */
  uint64_t estimated_gain{0};

  void report() const
  {
    std::cout << fmt::format( "[i] total time                                                  ({:>5.2f} secs)\n", to_seconds( time_total ) );
    std::cout << fmt::format( "[i]   cut time                                                  ({:>5.2f} secs)\n", to_seconds( time_cuts ) );
    std::cout << fmt::format( "[i]   mffc time                                                 ({:>5.2f} secs)\n", to_seconds( time_mffc ) );
    std::cout << fmt::format( "[i]   divs time                                                 ({:>5.2f} secs)\n", to_seconds( time_divs ) );
    std::cout << fmt::format( "[i]   simulation time                                           ({:>5.2f} secs)\n", to_seconds( time_simulation ) );
    std::cout << fmt::format( "[i]   evaluation time                                           ({:>5.2f} secs)\n", to_seconds( time_eval ) );
    std::cout << fmt::format( "[i]   substitute                                                ({:>5.2f} secs)\n", to_seconds( time_substitute ) );
    std::cout << fmt::format( "[i] total divisors            = {:8d}\n",         ( num_total_divisors ) );
    std::cout << fmt::format( "[i] total leaves              = {:8d}\n",         ( num_total_leaves ) );
    std::cout << fmt::format( "[i] estimated gain            = {:8d} ({:>5.2f}%)\n",
                              estimated_gain, ( (100.0 * estimated_gain) / initial_size ) );
  }
};

template<class Ntk>
void sim_resubstitution( Ntk& ntk, simresub_params const& ps = {}, simresub_stats* pst = nullptr )
{
  /* TODO: check if basetype of ntk is aig */
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
  static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the has_size method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the has substitute_node method" );
  static_assert( has_value_v<Ntk>, "Ntk does not implement the has_value method" );
  static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );

  std::cout<<"sim resub"<<std::endl;
  /*using resub_view_t = fanout_view2<depth_view<Ntk>>;
  depth_view<Ntk> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  resubstitution_stats st;
  if ( ps.max_pis == 8 )
  {
    using truthtable_t = kitty::static_truth_table<8>;
    using simulator_t = detail::simulator<resub_view_t, truthtable_t>;
    using resubstitution_functor_t = aig_resub_functor<resub_view_t, simulator_t>;
    typename resubstitution_functor_t::stats resub_st;
    detail::resubstitution_impl<resub_view_t, simulator_t, resubstitution_functor_t> p( resub_view, ps, st, resub_st );
    p.run();
    if ( ps.verbose )
    {
      st.report();
      resub_st.report();
    }
  }
  else
  {
    using truthtable_t = kitty::dynamic_truth_table;
    using simulator_t = detail::simulator<resub_view_t, truthtable_t>;
    using resubstitution_functor_t = aig_resub_functor<resub_view_t, simulator_t>;
    typename resubstitution_functor_t::stats resub_st;
    detail::resubstitution_impl<resub_view_t, simulator_t, resubstitution_functor_t> p( resub_view, ps, st, resub_st );
    p.run();
    if ( ps.verbose )
    {
      st.report();
      resub_st.report();
    }
  }

  if ( pst )
  {
    *pst = st;
  }*/
}

} /* namespace mockturtle */
