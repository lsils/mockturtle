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
  \file balancing.hpp
  \brief Cut-based depth-optimization

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <kitty/dynamic_truth_table.hpp>

#include "../utils/node_map.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/topo_view.hpp"
#include "cleanup.hpp"
#include "cut_enumeration.hpp"

namespace mockturtle
{

struct balancing_params
{
  /*! \brief Cut enumeration params. */
  cut_enumeration_params cut_enumeration_ps;

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};
};

struct balancing_stats
{
  /*! \brief Total run-time. */
  stopwatch<>::duration time_total{};

  /*! \brief Cut enumeration run-time. */
  cut_enumeration_stats cut_enumeration_st;

  /*! \brief Prints report. */
  void report() const
  {
    fmt::print( "[i] total time             = {:>5.2f} secs\n", to_seconds( time_total ) );
    fmt::print( "[i] Cut enumeration stats\n" );
    cut_enumeration_st.report();
  }
};

template<class Ntk>
struct arrival_time_pair
{
  signal<Ntk> f;
  uint32_t level;
};

/*! \brief Callback function for `rebalancing_function_t`.
 *
 * This callback is used in the rebalancing function to announce a new candidate that
 * could be used for replacement in the main balancing algorithm.  Using a callback
 * makes it possible to account for situations in which none, a single, or multiple
 * candidates are generated.
 * 
 * The callback returns a pair composed of the output signal of the replacement
 * candidate and the level of the new candidate.  Ideally, the rebalancing function
 * should not call the callback with candidates that a worse level.
 */
template<class Ntk>
using rebalancing_function_callback_t = std::function<void(arrival_time_pair<Ntk> const&, uint32_t)>;

template<class Ntk>
using rebalancing_function_t = std::function<void(Ntk&, kitty::dynamic_truth_table const&, std::vector<arrival_time_pair<Ntk>> const&, uint32_t, uint32_t, rebalancing_function_callback_t<Ntk> const&)>;

namespace detail
{

template<class Ntk>
struct balancing_impl
{
  balancing_impl( Ntk const& ntk, rebalancing_function_t<Ntk> const& rebalancing_fn, balancing_params const& ps, balancing_stats& st )
      : ntk_( ntk ),
        rebalancing_fn_( rebalancing_fn ),
        ps_( ps ),
        st_( st ),
        old_to_new_( ntk ),
        levels_( ntk )
  {
  }

  Ntk run()
  {
    /* input arrival times and mapping */
    old_to_new_[ntk_.get_constant( false )] = dest_.get_constant( false );
    levels_[ntk_.get_constant( false )] = 0u;
    if ( ntk_.get_node( ntk_.get_constant( false ) ) != ntk_.get_node( ntk_.get_constant( true ) ) )
    {
      old_to_new_[ntk_.get_constant( true )] = dest_.get_constant( true );
      levels_[ntk_.get_constant( true )] = 0u;
    }
    ntk_.foreach_pi( [&]( auto const& n ) {
      old_to_new_[n] = dest_.create_pi();
      levels_[n] = 0u;
    } );

    stopwatch<> t( st_.time_total );
    const auto cuts = cut_enumeration<Ntk, true>( ntk_, ps_.cut_enumeration_ps, &st_.cut_enumeration_st );

    uint32_t current_level{};
    const auto size = ntk_.size();
    progress_bar pbar{ntk_.size(), "balancing |{0}| node = {1:>4} / " + std::to_string( size ) + "   current level = {2}", ps_.progress};
    topo_view<Ntk>{ntk_}.foreach_node( [&]( auto const& n, auto index ) {
      if ( index == size )
        return false;

      pbar( index, index, current_level );

      if ( ntk_.is_constant( n ) || ntk_.is_pi( n ) )
      {
        return true;
      }

      signal<Ntk> best_signal;
      uint32_t best_level = std::numeric_limits<uint32_t>::max();
      uint32_t best_size{};
      for ( auto& cut : cuts.cuts( ntk_.node_to_index( n ) ) )
      {
        if ( cut->size() == 1u )
          continue;

        std::vector<arrival_time_pair<Ntk>> arrival_times( cut->size() );
        std::transform( cut->begin(), cut->end(), arrival_times.begin(), [&]( auto leaf ) -> arrival_time_pair<Ntk> {
          auto leaf_node = ntk_.index_to_node( leaf );
          auto leaf_level = levels_[leaf_node];
          return {old_to_new_[leaf_node], leaf_level};
        });

        rebalancing_fn_( dest_, cuts.truth_table( *cut ), arrival_times, best_level, best_size, [&]( arrival_time_pair<Ntk> const& cand, uint32_t cand_size ) {
          if ( cand.level < best_level || ( cand.level == best_level && cand_size < best_size ) )
          {
            best_signal = cand.f;
            best_level = cand.level;
            best_size = cand_size;
          }
        });
      }
      levels_[n] = best_level;
      old_to_new_[n] = best_signal;
      current_level = std::max( current_level, best_level );

      return true;
    } );

    ntk_.foreach_po( [&]( auto const& f ) {
      const auto s = old_to_new_[f];
      dest_.create_po( ntk_.is_complemented( f ) ? dest_.create_not( s ) : s );
    } );

    return cleanup_dangling( dest_ );
  }

private:
  Ntk const& ntk_;
  rebalancing_function_t<Ntk> const& rebalancing_fn_;
  Ntk dest_;
  balancing_params const& ps_;
  balancing_stats& st_;

  node_map<signal<Ntk>, Ntk> old_to_new_;
  node_map<uint32_t, Ntk> levels_;
};

} // namespace detail

template<class Ntk>
Ntk balancing( Ntk const& ntk, rebalancing_function_t<Ntk> const& rebalancing_fn = {}, balancing_params const& ps = {}, balancing_stats* pst = nullptr )
{
  balancing_stats st;
  const auto dest = detail::balancing_impl<Ntk>{ntk, rebalancing_fn, ps, st}.run();

  if ( pst )
  {
    *pst = st;
  }
  if ( ps.verbose )
  {
    st.report();
  }

  return dest;
}

} // namespace mockturtle
