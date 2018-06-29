/* mockturtle: C++ logic network library
 * Copyright (C) 2018  EPFL
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
  \file refactoring.hpp
  \brief Refactoring

  \author Mathias Soeken
*/

#pragma once

#include <iostream>

#include "../traits.hpp"
#include "../algorithms/akers_synthesis.hpp"
#include "../algorithms/simulation.hpp"
#include "../networks/mig.hpp"
#include "../utils/progress_bar.hpp"
#include "../views/cut_view.hpp"
#include "../views/mffc_view.hpp"

#include <kitty/dynamic_truth_table.hpp>

namespace mockturtle
{

/*! \brief Parameters for refactoring.
 *
 * The data structure `refactoring_params` holds configurable parameters with
 * default arguments for `refactoring`.
 */
struct refactoring_params
{
  /*! \brief Maximum number of PIs in MFFCs. */
  uint32_t max_pis{6};

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};
};

namespace detail
{

template<class Ntk, class RefactoringFn>
class refactoring_impl
{
public:
  refactoring_impl( Ntk& ntk, RefactoringFn&& refactoring_fn, refactoring_params const& ps )
      : ntk( ntk ), refactoring_fn( refactoring_fn ), ps( ps )
  {
  }

  void run()
  {
    const auto size = ntk.size();
    progress_bar pbar{ntk.size(), "|{0}| node = {1:>4}   cand = {2:>4}   est. reduction = {3:>5}", ps.progress};
    ntk.foreach_gate( [&]( auto const& n, auto i ) {
      if ( i >= size )
      {
        return false;
      }

      mffc_view mffc{ntk, n};

      pbar( i, i, _candidates, _estimated_reduction );

      if ( mffc.num_pis() > ps.max_pis )
      {
        return true;
      }

      std::vector<signal<Ntk>> leaves( mffc.num_pis() );
      mffc.foreach_pi( [&]( auto const& n, auto j ) {
        leaves[j] = ntk.make_signal( n );
      } );

      default_simulator<kitty::dynamic_truth_table> sim( mffc.num_pis() );
      const auto tt = simulate<kitty::dynamic_truth_table>( mffc, sim )[0];
      const auto new_f = refactoring_fn( ntk, tt, leaves.begin(), leaves.end() );
      cut_view cut{ntk, leaves, ntk.get_node( new_f )};
      if ( cut.size() < mffc.size() )
      {
        ++_candidates;
        _estimated_reduction += ( mffc.size() - cut.size() );
        ntk.substitute_node( n, new_f );
      }

      return true;
    } );
  }

private:
  Ntk& ntk;
  RefactoringFn&& refactoring_fn;
  refactoring_params const& ps;

  uint32_t _candidates{0};
  uint32_t _estimated_reduction{0};
};

} /* namespace detail */

/*! \brief Boolean refactoring.
 *
 * This algorithm performs refactoring by collapsing maximal fanout-free cones
 * (MFFCs) into truth tables and recreate a new network structure from it.
 * The algorithm performs changes directly in the input network and keeps the
 * substituted structures dangling in the network.  They can be cleaned up using
 * the `cleanup_dangling` algorithm.
 * 
 * The refactoring function must be of type `NtkDest::signal(NtkDest&,
 * kitty::dynamic_truth_table const&, LeavesIterator, LeavesIterator)` where
 * `LeavesIterator` can be dereferenced to a `NtkDest::signal`.  The last two
 * parameters compose an iterator pair where the distance matches the number of
 * variables of the truth table that is passed as second parameter.  There are
 * some refactoring algorithms in the folder
 * `mockturtle/algorithms/node_resyntesis`, since the resynthesis functions
 * have the same signature.
 *
 * **Required network functions:**
 * - `get_node`
 * - `size`
 * - `make_signal`
 * - `foreach_gate`
 * - `substitute_node`
 *
 * \param ntk Input network (will be changed in-place)
 * \param refactoring_fn Refactoring function
 * \param ps Refactoring params
 */
template<class Ntk, class RefactoringFn>
void refactoring( Ntk& ntk, RefactoringFn&& refactoring_fn, refactoring_params const& ps = {} )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
  static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement the substitute_node method" );

  detail::refactoring_impl<Ntk, RefactoringFn> p( ntk, refactoring_fn, ps );
  p.run();
}

} /* namespace mockturtle */
