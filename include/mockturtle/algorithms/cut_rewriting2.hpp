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
  \file cut_rewriting.hpp
  \brief Cut rewriting

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../traits.hpp"
#include "../utils/cost_functions.hpp"
#include "../utils/node_map.hpp"
#include "../utils/progress_bar.hpp"
#include "../utils/stopwatch.hpp"
#include "cleanup.hpp"
#include "cut_enumeration.hpp"
#include "cut_rewriting.hpp"
#include "detail/mffc_utils.hpp"

#include <fmt/format.h>
#include <kitty/dynamic_truth_table.hpp>

namespace mockturtle::future
{

namespace detail
{

using namespace mockturtle::detail;

template<class Ntk, class RewritingFn, class NodeCostFn>
struct cut_rewriting_impl
{
  cut_rewriting_impl( Ntk const& ntk, RewritingFn& rewriting_fn, cut_rewriting_params const& ps, cut_rewriting_stats& st )
      : ntk_( ntk ),
        rewriting_fn_( rewriting_fn ),
        ps_( ps ),
        st_( st ) {}

  Ntk run()
  {
    stopwatch t( st_.time_total );

    /* initial node map */
    auto [res, old2new] = initialize_copy_network<Ntk>( ntk_ );

    /* enumerate cuts */
    const auto cuts = call_with_stopwatch( st_.time_cuts, [&]() { return cut_enumeration<Ntk, true, cut_enumeration_cut_rewriting_cut>( ntk_, ps_.cut_enumeration_ps ); } );

    /* for cost estimation we use reference counters initialized by the fanout size */
    initialize_values_with_fanout( ntk_ );

    /* original cost */
    const auto orig_cost = costs<Ntk, NodeCostFn>( ntk_ );

    progress_bar pbar{ntk_.num_gates(), "cut_rewriting |{0}| node = {1:>4} / " + std::to_string( ntk_.num_gates() ) + "   original cost = " + std::to_string( orig_cost ), ps_.progress};
    ntk_.foreach_gate( [&]( auto const& n, auto i ) {
      pbar( i, i );

      /* nothing to optimize? */
      int32_t value = mffc_size<Ntk, NodeCostFn>( ntk_, n );
      if ( value == 1 )
      {
        std::vector<signal<Ntk>> children( ntk_.fanin_size( n ) );
        ntk_.foreach_fanin( n, [&]( auto const& f, auto i ) {
          children[i] = old2new[f] ^ ntk_.is_complemented( f );
        } );

        old2new[n] = res.clone_node( ntk_, n, children );
      }
      else
      {
        /* foreach cut */
        int32_t best_gain = -1;
        signal<Ntk> best_signal;
        for ( auto& cut : cuts.cuts( ntk_.node_to_index( n ) ) )
        {
          /* skip small enough cuts */
          if ( cut->size() == 1 || cut->size() < ps_.min_cand_cut_size )
            continue;

          const auto tt = cuts.truth_table( *cut );
          assert( cut->size() == static_cast<unsigned>( tt.num_vars() ) );

          std::vector<signal<Ntk>> children( cut->size() );
          auto ctr = 0u;
          for ( auto l : *cut )
          {
            children[ctr++] = old2new[ntk_.index_to_node( l )];
          }

          const auto on_signal = [&]( auto const& f_new ) {
            auto value2 = recursive_ref<Ntk, NodeCostFn>( res, res.get_node( f_new ) );
            recursive_deref<Ntk, NodeCostFn>( res, res.get_node( f_new ) );
            int32_t gain = value - value2;

            if ( ( gain > 0 || ( ps_.allow_zero_gain && gain == 0 ) ) && gain > best_gain )
            {
              best_gain = gain;
              best_signal = f_new;
            }

            return true;
          };
          stopwatch<> t( st_.time_rewriting );
          rewriting_fn_( res, cuts.truth_table( *cut ), children.begin(), children.end(), on_signal );
        }

        if ( best_gain == -1 )
        {
          std::vector<signal<Ntk>> children( ntk_.fanin_size( n ) );
          ntk_.foreach_fanin( n, [&]( auto const& f, auto i ) {
            children[i] = old2new[f] ^ ntk_.is_complemented( f );
          } );

          old2new[n] = res.clone_node( ntk_, n, children );
        }
        else
        {
          old2new[n] = best_signal;
        }
      }

      recursive_ref<Ntk, NodeCostFn>( res, res.get_node( old2new[n] ) );
    } );

    /* create POs */
    ntk_.foreach_po( [&]( auto const& f ) {
      res.create_po( old2new[f] ^ ntk_.is_complemented( f ) );
    } );

    res = cleanup_dangling( res );

    /* new costs */
    return costs<Ntk, NodeCostFn>( res ) > orig_cost ? ntk_ : res;
  }

private:
  Ntk const& ntk_;
  RewritingFn& rewriting_fn_;
  cut_rewriting_params const& ps_;
  cut_rewriting_stats& st_;
};

} // namespace detail

template<class Ntk, class RewritingFn, class NodeCostFn = unit_cost<Ntk>>
Ntk cut_rewriting( Ntk const& ntk, RewritingFn& rewriting_fn, cut_rewriting_params const& ps = {}, cut_rewriting_stats* pst = nullptr )
{
  cut_rewriting_stats st;
  const auto result = detail::cut_rewriting_impl<Ntk, RewritingFn, NodeCostFn>( ntk, rewriting_fn, ps, st ).run();

  if ( ps.verbose )
  {
    st.report( false );
  }
  if ( pst )
  {
    *pst = st;
  }
  return result;
}

} // namespace mockturtle::future
