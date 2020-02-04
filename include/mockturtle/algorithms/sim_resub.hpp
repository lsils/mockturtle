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

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>

namespace mockturtle
{

struct simresub_stats
{
  /*! \brief Accumulated runtime for const-resub */
  stopwatch<>::duration time_resubC{0};

  /*! \brief Accumulated runtime for zero-resub */
  stopwatch<>::duration time_resub0{0};

  /*! \brief Number of accepted constant resubsitutions */
  uint32_t num_const_accepts{0};

  /*! \brief Number of accepted zero resubsitutions */
  uint32_t num_div0_accepts{0};

  void report() const
  {
    std::cout << "[i] kernel: default_resub_functor\n";
    std::cout << fmt::format( "[i]     constant-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_const_accepts, to_seconds( time_resubC ) );
    std::cout << fmt::format( "[i]            0-resub {:6d}                                   ({:>5.2f} secs)\n",
                              num_div0_accepts, to_seconds( time_resub0 ) );
    std::cout << fmt::format( "[i]            total   {:6d}\n",
                              (num_const_accepts + num_div0_accepts) );
  }
};

template<typename Ntk, typename Simulator>
class simresub_functor
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using stats = simresub_stats;

  explicit simresub_functor( Ntk const& ntk, Simulator const& sim, std::vector<node> const& divs, uint32_t num_divs, simresub_stats& st )
    : ntk( ntk )
    , sim( sim )
    , divs( divs )
    , num_divs( num_divs )
    , st( st )
  {
  }

  std::optional<signal> operator()( node const& root, uint32_t required, uint32_t max_inserts, uint32_t num_mffc, uint32_t& last_gain ) const
  {
    /* The default resubstitution functor does not insert any gates
       and consequently does not use the argument `max_inserts`. Other
       functors, however, make use of this argument. */
    (void)max_inserts;

    /* consider constants */
    auto g = call_with_stopwatch( st.time_resubC, [&]() {
        return resub_const( root, required );
      } );
    if ( g )
    {
      ++st.num_const_accepts;
      last_gain = num_mffc;
      return g; /* accepted resub */
    }

    /* consider equal nodes */
    g = call_with_stopwatch( st.time_resub0, [&]() {
        return resub_div0( root, required );
      } );
    if ( g )
    {
      ++st.num_div0_accepts;
      last_gain = num_mffc;;
      return g; /* accepted resub */
    }

    return std::nullopt;
  }

private:
  std::optional<signal> resub_const( node const& root, uint32_t required ) const
  {
    (void)required;
    auto const tt = sim.get_tt( ntk.make_signal( root ) );
    if ( tt == sim.get_tt( ntk.get_constant( false ) ) )
    {
      return sim.get_phase( root ) ? ntk.get_constant( true ) : ntk.get_constant( false );
    }
    return std::nullopt;
  }

  std::optional<signal> resub_div0( node const& root, uint32_t required ) const
  {
    (void)required;
    auto const tt = sim.get_tt( ntk.make_signal( root ) );
    for ( const auto& d : divs )
    {
      if ( root == d )
        break;

      if ( tt != sim.get_tt( ntk.make_signal( d ) ) )
        continue; /* next */

      return ( sim.get_phase( d ) ^ sim.get_phase( root ) ) ? !ntk.make_signal( d ) : ntk.make_signal( d );
    }

    return std::nullopt;
  }

private:
  Ntk const& ntk;
  Simulator const& sim;
  std::vector<node> const& divs;
  uint32_t num_divs;
  stats& st;
}; /* simresub_functor */

template<class Ntk>
void sim_resubstitution( Ntk& ntk, resubstitution_params const& ps = {}, resubstitution_stats* pst = nullptr )
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

  using resub_view_t = fanout_view2<depth_view<Ntk>>;
  depth_view<Ntk> depth_view{ntk};
  resub_view_t resub_view{depth_view};

  resubstitution_stats st;

  using truthtable_t = kitty::static_truth_table<8>;
  using simulator_t = detail::simulator<resub_view_t, truthtable_t>;
  using resubstitution_functor_t = simresub_functor<resub_view_t, simulator_t>;
  typename resubstitution_functor_t::stats resub_st;
  detail::resubstitution_impl<resub_view_t, simulator_t, resubstitution_functor_t> p( resub_view, ps, st, resub_st );
  p.run();
  if ( ps.verbose )
  {
    st.report();
    resub_st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */
