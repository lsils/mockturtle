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
  \file aqfp_view.hpp
  \brief Constraints for AQFP technology

  \author Heinz Riener
  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../traits.hpp"
#include "../networks/detail/foreach.hpp"
#include "../utils/node_map.hpp"
#include "immutable_view.hpp"
#include "mockturtle/networks/mig.hpp"
#include "mockturtle/views/depth_view.hpp"

#include <vector>
#include <list>
#include <cmath> //std::pow, std::ceil
#include <limits> //std::numeric_limits

namespace mockturtle
{

/*! \brief Parameters for AQFP buffer counting.
 *
 * The data structure `aqfp_view_params` holds configurable parameters with
 * default arguments for `aqfp_view`.
 */
struct aqfp_view_params
{
  /*! \brief Whether PIs need to be branched with splitters */
  bool branch_pis{false};

  /*! \brief Whether PIs need to be path-balanced */
  bool balance_pis{false};

  /*! \brief Whether POs need to be path-balanced */
  bool balance_pos{true};

  /*! \brief The maximum number of fanouts each splitter (buffer) can have */
  uint32_t splitter_capacity{3u};

  /*! \brief The maximum additional depth of a node introduced by splitters (0 = unlimited) */
  uint32_t max_splitter_levels{0u};
};

/*! \brief Computes levels considering AQFP splitters and counts AQFP buffers/splitters.
 *
 * This view calculates the number of buffers (for path balancing) and 
 * splitters (for multi-fanout) after AQFP technology mapping from an MIG
 * network. The calculation is rather naive without much optimization such
 * as retiming, which can serve as an upper bound on the cost or as a
 * baseline for future works on buffer optimization to be compared to.
 * 
 * In AQFP technology, (1) MAJ gates can only have one fanout. If more than one
 * fanout is needed, a splitter has to be inserted in between, which also 
 * takes one clock cycle (counted towards the network depth). (2) All fanins of
 * a MAJ gate have to arrive at the same time (at the same level). If one
 * fanin path is shorter, buffers have to be inserted to balance it. 
 * Buffers and splitters are essentially the same component in this technology.
 *
 * POs count toward the fanout sizes and always have to be branched. The assumptions
 * on whether PIs should be branched and whether PIs and POs have to be balanced 
 * can be set in the parameters.
 *
 * The additional depth of a node introduced by splitters can be limited by 
 * setting the parameter `max_splitter_levels`, making the maximum number of 
 * fanouts of a node in the original MIG network being limited to 
 * `pow(splitter_capacity, max_splitter_levels)`. To ensure this, one should
 * apply `fanout_limit_view` before `aqfp_view` to duplicate the nodes with
 * too many fanouts.
 *
 * The network depth and the levels of each node are determined first by the 
 * number of fanouts and adding sufficient levels for splitters assuming all
 * fanouts are at the lowest possible level. This could be suboptimal when some
 * fanouts are only needed at higher levels. Then, the number of buffers (which
 * also serve as splitters) is counted in a way that signals are only splitted 
 * before the level where they are needed (i.e., sharing of buffers among
 * multiple fanouts is maximized).
 *
 * **Required network functions:**
 * - `foreach_node`
 * - `foreach_gate`
 * - `foreach_pi`
 * - `foreach_po`
 * - `foreach_fanin`
 * - `is_pi`
 * - `is_constant`
 * - `get_node`
 * - `fanout_size`
 * - `size`
 *
 */
template<typename Ntk>
class aqfp_view : public immutable_view<Ntk>
{
public:
  using storage = typename Ntk::storage;
  using node    = typename Ntk::node;
  using signal  = typename Ntk::signal;

  struct node_depth
  {
    node_depth( aqfp_view* p ): aqfp( p ) {}
    uint32_t operator()( depth_view<Ntk, node_depth> const& ntk, node const& n ) const
    {
      (void)ntk;
      return aqfp->num_splitter_levels( n ) + 1u;
    }
    aqfp_view* aqfp;
  };

  explicit aqfp_view( Ntk const& ntk, aqfp_view_params const& ps = {} )
   : immutable_view<Ntk>( ntk ), _ps( ps ), _fanouts( ntk ), _external_ref_count( ntk ),
     _node_depth( this ), _levels( *this )
  {
    static_assert( !has_foreach_fanout_v<Ntk> && "Ntk already has fanout interfaces" );
    static_assert( !has_depth_v<Ntk> && !has_level_v<Ntk> && !has_update_levels_v<Ntk>, "Ntk already has depth interfaces" );
    static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
    static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
    static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );

    if constexpr ( !std::is_same<typename Ntk::base_type, mig_network>::value )
    {
      std::cerr << "[w] base_type of Ntk is not mig_network.\n";
    }

    if ( _ps.max_splitter_levels )
    {
      _max_fanout = std::pow( _ps.splitter_capacity, _ps.max_splitter_levels );
    }

    update();
  }

  /*! \brief Level of node `n` considering buffer/splitter insertion. */
  uint32_t level ( node const& n ) const
  {
    assert( n < this->size() );
    return _levels[n];
  }

  /*! \brief Network depth considering AQFP buffers/splitters. */
  uint32_t depth() const
  {
    return _depth;
  }

  /*! \brief The total number of buffers/splitters in the network. */
  uint32_t num_buffers() const
  {
    uint32_t count = 0u;
    if ( _ps.branch_pis )
    {
      this->foreach_pi( [&]( auto const& n ){
        count += num_buffers( n );
      });
    }
    else
    {
      assert( !_ps.balance_pis && "Does not make sense to balance but not branch PIs" );
    }

    this->foreach_gate( [&]( auto const& n ){
      count += num_buffers( n );
    });
    return count;
  }

  /*! \brief The number of buffers/splitters between `n` and all of its fanouts */
  uint32_t num_buffers( node const& n ) const
  {
    auto const& fo_infos = _fanouts[n];

    if ( num_splitter_levels( n ) == 0u )
    {
      /* single fanout */
      if ( this->fanout_size( n ) > 0u )
      {
        assert( this->fanout_size( n ) == 1u );
        if ( this->is_pi( n ) )
        {
          assert( level( n ) == 0u );
          if ( _external_ref_count[n] > 0u ) /* PI -- PO */
          {
            return _ps.balance_pis && _ps.balance_pos ? depth() : 0u;
          }
          else /* PI -- gate */
          {
            assert( fo_infos.size() == 1u );
            return _ps.balance_pis ? fo_infos.front().relative_depth - 1u : 0u;
          } 
        }
        else if ( _external_ref_count[n] > 0u ) /* gate -- PO */
        {
          return _ps.balance_pos ? depth() - level( n ) : 0u;
        }
        else /* gate -- gate */
        {
          assert( fo_infos.size() == 1u );
          return fo_infos.front().relative_depth - 1u;
        }
      }
      /* dangling */
      return 0u;
    }

    if ( fo_infos.size() == 0u )
    {
      /* special case: don't balance POs; multiple PO refs but no gate fanout */
      assert( !_ps.balance_pos && this->fanout_size( n ) == _external_ref_count[n] );
      return std::ceil( float( _external_ref_count[n] - 1 ) / float( _ps.splitter_capacity - 1 ) );
    }

    auto it = fo_infos.begin();
    auto count = it->num_edges;
    auto rd = it->relative_depth;
    for ( ++it; it != fo_infos.end(); ++it )
    {
      count += it->num_edges - it->fanouts.size() + it->relative_depth - rd - 1;
      rd = it->relative_depth;
    }

    if ( !_ps.balance_pis && this->is_pi( n ) ) /* only branch PIs, but don't balance them */
    {
      /* remove the lowest balancing buffers, if any */
      it = fo_infos.begin();
      ++it;
      count -= it->relative_depth - fo_infos.front().relative_depth - 1;
    }

    if ( !_ps.balance_pos && _external_ref_count[n] > 0u )
    {
      auto slots = count * ( _ps.splitter_capacity - 1 ) + 1;
      int32_t needed = this->fanout_size( n ) - slots;
      if ( needed > 0 )
      {
        count += std::ceil( float( needed ) / float( _ps.splitter_capacity - 1 ) );
      }
    }
    else
    {
      count -= _external_ref_count[n];
    }

    return count;
  }

  /*! \brief (Upper bound on) the additional depth caused by a balanced splitter 
             tree at the output of node `n`. */
  uint32_t num_splitter_levels ( node const& n ) const
  {
    assert( n < this->size() );
    return std::ceil( std::log( this->fanout_size( n ) ) / std::log( _ps.splitter_capacity ) );
  }

private:
  /* Return the number of splitters needed in one level lower */
  uint32_t num_splitters( uint32_t const& num_fanouts ) const
  {
    return std::ceil( float( num_fanouts ) / float( _ps.splitter_capacity ) );
  }

  void update()
  {
    compute_levels();
    compute_fanouts();

    this->foreach_gate( [&]( auto const& n ){
      count_edges( n );
    });

    if ( _ps.branch_pis )
    {
      this->foreach_pi( [&]( auto const& n ){
        count_edges( n );
      });
    }
  }

  void compute_levels()
  {
    /* Use depth_view to naively compute an initial level assignment */
    depth_view<Ntk, node_depth> _depth_view( *this, _node_depth, {/*count_complements*/false, /*pi_cost*/_ps.branch_pis} );
    _levels.reset( 0 );
    this->foreach_node( [&]( auto const& n ){
      _levels[n] = _depth_view.level( n ) - num_splitter_levels( n );
    });
    _depth = _depth_view.depth();
  }

  void compute_fanouts()
  {
    _external_ref_count.reset( 0u );
    this->foreach_po( [&]( auto const& f ){
      _external_ref_count[f]++;
    });

    _fanouts.reset();
    this->foreach_gate( [&]( auto const& n ){
      this->foreach_fanin( n, [&]( auto const& fi ){
        auto const ni = this->get_node( fi );
        if ( !this->is_constant( ni ) )
        {
          insert_fanout( ni, n );
        }
      });
    });    
  }

  void insert_fanout( node const& n, node const& fanout )
  {
    auto const rd = level( fanout ) - level( n );
    auto& fo_infos = _fanouts[n];
    for ( auto it = fo_infos.begin(); it != fo_infos.end(); ++it )
    {
      if ( it->relative_depth == rd )
      {
        it->fanouts.push_back( fanout );
        ++it->num_edges;
        return;
      }
      else if ( it->relative_depth > rd )
      {
        fo_infos.insert( it, {rd, {fanout}, 1u} );
        return;
      }
    }
    fo_infos.push_back( { rd, {fanout}, 1u } );
  }

  void count_edges( node const& n )
  {
    assert( this->fanout_size( n ) <= _max_fanout );
    auto& fo_infos = _fanouts[n];

    if ( _external_ref_count[n] && _ps.balance_pos )
    {
      fo_infos.push_back( {depth() + 1 - level( n ), {}, _external_ref_count[n]} );
    }

    if ( fo_infos.size() == 0u || ( fo_infos.size() == 1u && fo_infos.front().num_edges == 1u ) )
    {
      return;
    }
    assert( fo_infos.front().relative_depth > 1u );
    fo_infos.push_front( {1u, {}, 0u} );

    auto it = fo_infos.end();
    --it;
    for ( ; it != fo_infos.begin(); --it )
    {
      auto splitters = num_splitters( it->num_edges );
      auto rd = it->relative_depth;
      --it;
      if ( it->relative_depth == rd - 1 )
      {
        it->num_edges += splitters;
        ++it;
      }
      else if ( splitters == 1 )
      {
        ++(it->num_edges);
        ++it;
      }
      else
      {
        ++it;
        fo_infos.insert( it, {rd - 1, {}, splitters} );
      }
    }
    assert( fo_infos.front().relative_depth == 1u );
    assert( fo_infos.front().num_edges <= 1u );
  }

private:
  struct fanout_info
  {
    uint32_t relative_depth{0u};
    std::list<node> fanouts;
    uint32_t num_edges{0u};
  };
  using fanouts_by_level = std::list<fanout_info>;
  
  aqfp_view_params _ps;
  uint64_t _max_fanout{std::numeric_limits<uint64_t>::max()};

  node_map<fanouts_by_level, Ntk> _fanouts;
  node_map<uint32_t, Ntk> _external_ref_count;

  node_depth _node_depth; /* naive cost function for depth_view */
  node_map<uint32_t, Ntk> _levels;
  uint32_t _depth{0u};
};

template<class T>
aqfp_view( T const&, aqfp_view_params const& ps = {} ) -> aqfp_view<T>;

} // namespace mockturtle