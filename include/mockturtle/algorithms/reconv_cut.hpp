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
  \file cut_generation.hpp
  \brief Cut generation

  \author Heinz Riener
*/

#pragma once

#include "../traits.hpp"

namespace mockturtle
{

/*! \brief Parameters for cut_generation.
 *
 * The data structure `cut_generation_params` holds configurable parameters
 * with default arguments for `cut_generation`.
 */
struct reconv_cut_params
{
  /*! \brief Maximum number of leaves for a cut. */
  uint32_t cut_size{10u};
};

/*! \cond PRIVATE */
namespace detail
{
template<typename Ntk>
class compute_reconvergence_driven_cut_impl
{
public:
  explicit compute_reconvergence_driven_cut_impl( Ntk const &ntk, node<Ntk> const& pivot, reconv_cut_params const& ps )
    : _ntk( ntk )
    , _pivot( pivot )
    , _ps( ps )
  {}

public:
  std::vector<node<Ntk>> run()
  {
    _ntk.clear_values();

    std::vector<node<Ntk>> cut = { _pivot };
    _ntk.set_value( _pivot, 1 );
    compute_cut_recur( cut );
    return cut;
  }

protected:
  void compute_cut_recur( std::vector<node<Ntk>>& cut )
  {
    assert( cut.size() <= _ps.cut_size && "cut-size overflow" );
    std::sort( cut.begin(), cut.end(), [this]( node<Ntk> const& a, node<Ntk> const& b ){ return cost( a ) < cost( b ); } );

    /* find the first non-pi node to extend the cut (because the vector is sorted, the non-pi is cost-minimal) */
    auto const it = std::find_if( cut.begin(), cut.end(), [&]( auto const& node ){ return !_ntk.is_pi( node ); } );
    if ( cut.end() == it )
    {
      /* if all nodes are pis, then the cut cannot be extended */
      return;
    }

    /* the cost is identical to the number of nodes added to the cut if *it is used to expand the cut */
    auto const c = cost( *it );
    if( cut.size() + c > _ps.cut_size )
    {
      /* if the expansion exceeds the cut_size, then the cut cannot be extended */
      return;
    }

    /* otherwise expand the cut with the children of *it and mark *it visited */
    const auto n = *it;
    cut.erase( it );
    _ntk.foreach_fanin( n, [&]( signal<Ntk> const& s, unsigned ){
        auto const& child = _ntk.get_node( s );
        if ( !_ntk.is_constant( n ) && std::find( cut.begin(), cut.end(), child ) == cut.end() && !_ntk.value( child ) )
        {
          cut.push_back( child );
          _ntk.set_value( child, 1 );
        }
      } );

    compute_cut_recur( cut );
  }

  inline int32_t cost( node<Ntk> const& n ) const
  {
    int32_t current_cost = -1;
    _ntk.foreach_fanin( n, [&]( signal<Ntk> const& s, unsigned ){
        auto const& child = _ntk.get_node( s );
        if ( !_ntk.is_constant( child ) && !_ntk.value( child ) )
          ++current_cost;
      } );
    return current_cost;
  }

private:
  Ntk const& _ntk;
  node<Ntk> const& _pivot;
  reconv_cut_params const& _ps;
};
} /* namespace detail */
/*! \endcond */

/*! \brief reconv_cut
 *
 * This class implements a generation algorithm for
 * reconvergence-driven cuts.  The cut grows towards the primary
 * inputs starting from a pivot node.
 *
 * **Required network functions:**
 * - `is_constant`
 * - `is_pi`
 * - `clear_values`
 * - `set_value`
 * - `value`
 * - `get_node`
 * - `foreach_fanin`
 *
 */
struct reconv_cut
{
public:
  explicit reconv_cut( reconv_cut_params const& ps = {} )
    : _ps( ps )
  {}

  template<typename Ntk>
  std::vector<node<Ntk>> operator()( Ntk const& ntk, node<Ntk> const& pivot )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
    static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_set_value_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_value_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the is_pi method" );

    detail::compute_reconvergence_driven_cut_impl cut_generator( ntk, pivot, _ps );

    return cut_generator.run();
  }

private:
  reconv_cut_params _ps;
};

} /* namespace mockturtle */
