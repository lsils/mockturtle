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
  \file aqfp_buffer.hpp
  \brief Count and optimize buffers and splitters in AQFP technology

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../../traits.hpp"
#include "../../utils/node_map.hpp"
#include "../../views/depth_view.hpp"

#include <vector>
#include <list>
#include <set>
#include <cmath> //std::pow, std::ceil
#include <limits> //std::numeric_limits

namespace mockturtle
{

/*! \brief Parameters for AQFP buffer counting.
 */
struct aqfp_buffer_params
{
  /*! \brief Whether PIs need to be branched with splitters */
  bool branch_pis{false};

  /*! \brief Whether PIs need to be path-balanced */
  bool balance_pis{false};

  /*! \brief Whether POs need to be path-balanced */
  bool balance_pos{true};

  /*! \brief The maximum number of fanouts each splitter (buffer) can have */
  uint32_t splitter_capacity{3u};
};

/*! \brief Count and optimize buffers and splitters in AQFP technology.
 * 
 * In AQFP technology, (1) logic gates can only have one fanout. If more than one
 * fanout is needed, a splitter has to be inserted in between, which also 
 * takes one clocking phase (counted towards the network depth). (2) All fanins of
 * a logic gate have to arrive at the same time (be at the same level). If one
 * fanin path is shorter, buffers have to be inserted to balance it. 
 * Buffers and splitters are essentially the same component in this technology.
 *
 * POs count toward the fanout sizes and always have to be branched. The assumptions
 * on whether PIs should be branched and whether PIs and POs have to be balanced 
 * can be set in the parameters (`aqfp_buffer_params`).
 *
 * The network depth and the levels of each gate are initialized with ASAP
 * scheduling considering reserved levels for splitters assuming all fanouts are 
 * at the lowest possible level. Then, various optimization methods can be called,
 * which re-assign or adjust the level assignment. With a given level assignment,
 * the minimum number of buffers needed is determined, which is counted in a way 
 * that signals are only splitted before the level where they are needed (i.e., 
 * sharing of buffers among multiple fanouts is maximized).
 *
 * This class provides interfaces to:
 * - query the current level assignment (`level`, `depth`)
 * - count irredundant buffers based on the current level assignment (`count_buffers`,
 * `num_buffers`)
 * - optimize buffer count by adjusting the level assignment (`ASAP`, `ALAP`)
 * - dump the resulting network into a network type which provides representation for 
 * buffers (`dump_buffered_network`)
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      mig_network mig = ...
      aqfp_buffer bufcnt( mig );
      bufcnt.ALAP();
      bufcnt.count_buffers();
      std::cout << bufcnt.num_buffers() << std::endl;
      auto const buffered = bufcnt.dump_buffered_network<buffered_mig_network>();
      write_verilog( buffered, "buffered.v" );
   \endverbatim
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
 * - `set_visited`
 * - `set_value`
 *
 */
template<typename Ntk>
class aqfp_buffer
{
public:
  using node    = typename Ntk::node;
  using signal  = typename Ntk::signal;

  explicit aqfp_buffer( Ntk const& ntk, aqfp_buffer_params const& ps = {} )
   : _ntk( ntk ), _ps( ps ), _levels( _ntk ), _fanouts( _ntk ), _external_ref_count( _ntk ), _buffers( _ntk )
  {
    static_assert( !has_is_buf_v<Ntk>, "Ntk is already buffered" );
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
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );

    assert( !(_ps.balance_pis && !_ps.branch_pis) && "Does not make sense to balance but not branch PIs" );

    ASAP();
  }

#pragma region Query
  /*! \brief Level of node `n` considering buffer/splitter insertion. */
  uint32_t level ( node const& n ) const
  {
    assert( n < _ntk.size() );
    return _levels[n];
  }

  /*! \brief Network depth considering AQFP buffers/splitters. */
  uint32_t depth() const
  {
    return _depth;
  }

  /*! \brief The total number of buffers in the network under the current 
   * level assignment. */
  uint32_t num_buffers() const
  {
    assert( !outdated && "Please call `count_buffers()` first." );

    uint32_t count = 0u;
    if ( _ps.branch_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        count += num_buffers( n );
      });
    }

    _ntk.foreach_gate( [&]( auto const& n ){
      count += num_buffers( n );
    });
    return count;
  }

  /*! \brief The number of buffers between `n` and all of its fanouts under
   * the current level assignment. */
  uint32_t num_buffers( node const& n ) const
  {
    assert( !outdated && "Please call `count_buffers()` first." );
    return _buffers[n];
  }
#pragma endregion

#pragma region Count buffers
  /*! \brief Count the number of buffers needed at the fanout of each gate 
   * according to the current level assignment.
   * 
   * This function must be called after level (re-)assignment and before 
   * querying `num_buffers`.
   */
  void count_buffers()
  {
    if ( outdated )
    {
      update_fanout_info();
    }

    if ( _ps.branch_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        assert( !_ps.balance_pis || _levels[n] == 0 );
        _buffers[n] = count_buffers( n );
      });
    }

    _ntk.foreach_gate( [&]( auto const& n ){
      _buffers[n] = count_buffers( n );
    });
  }

private:
  uint32_t count_buffers( node const& n ) const
  {
    assert( !outdated && "Please call `update_fanout_info()` first." );
    auto const& fo_infos = _fanouts[n];

    if ( _ntk.fanout_size( n ) == 0u ) /* dangling */
    {
      return 0u;
    }
    
    if ( _ntk.fanout_size( n ) == 1u ) /* single fanout */
    {
      if ( _external_ref_count[n] > 0u ) /* -> PO */
      {
        return _ps.balance_pos ? _depth - _levels[n] : 0u;
      }
      else /* -> gate */
      {
        assert( fo_infos.size() == 1u );
        return fo_infos.front().relative_depth - 1u;
      }
    }

    /* special case: don't balance POs; multiple PO refs but no gate fanout */
    if ( fo_infos.size() == 0u )
    {
      assert( !_ps.balance_pos && _ntk.fanout_size( n ) == _external_ref_count[n] );
      return std::ceil( float( _external_ref_count[n] - 1 ) / float( _ps.splitter_capacity - 1 ) );
    }

    /* main counting */
    auto it = fo_infos.begin();
    auto count = it->num_edges;
    auto rd = it->relative_depth;
    for ( ++it; it != fo_infos.end(); ++it )
    {
      count += it->num_edges - it->fanouts.size() + it->relative_depth - rd - 1;
      rd = it->relative_depth;
    }

    /* multiple PO refs: need branching */
    if ( !_ps.balance_pos && _external_ref_count[n] > 0u )
    {
      /* check if available slots are enough */
      auto slots = count * ( _ps.splitter_capacity - 1 ) + 1;
      int32_t needed = _ntk.fanout_size( n ) - slots;
      if ( needed > 0 )
      {
        count += std::ceil( float( needed ) / float( _ps.splitter_capacity - 1 ) );
      }
    }
    else
    {
      /* if _external_ref_count[n] == 0 : does nothing
         otherwise _ps.balance_pos == true : PO refs were added as num_edges and counted as buffers */
      count -= _external_ref_count[n];
    }

    return count;
  }

  /* (Upper bound on) the additional depth caused by a balanced splitter tree at the output of node `n`. */
  uint32_t num_splitter_levels ( node const& n ) const
  {
    assert( n < _ntk.size() );
    return std::ceil( std::log( _ntk.fanout_size( n ) ) / std::log( _ps.splitter_capacity ) );
  }
#pragma endregion

private:
#pragma region Update fanout info
  /* Guarantees on `_fanouts` (when not `outdated`):
   * - If not `branch_pis`: `_fanouts[PI]` is empty.
   * - If `balance_pos`: PO ref count is added to `num_edges` of the last element.
   * - If having only one fanout: `_fanouts[n].size() == 1`.
   * - If having multiple fanouts: `_fanouts[n]` must have at least two elements,
   *   and the first element must have `relative_depth == 1` and `num_edges == 1`.
   */

  /* Update fanout_information of all nodes */
  void update_fanout_info()
  {
    _external_ref_count.reset( 0u );
    _ntk.foreach_po( [&]( auto const& f ){
      _external_ref_count[f]++;
    });

    _fanouts.reset();
    _ntk.foreach_gate( [&]( auto const& n ){
      _ntk.foreach_fanin( n, [&]( auto const& fi ){
        auto const ni = _ntk.get_node( fi );
        if ( !_ntk.is_constant( ni ) )
        {
          insert_fanout( ni, n );
        }
      });
    });

    _ntk.foreach_gate( [&]( auto const& n ){
      count_edges( n );
    });

    if ( _ps.branch_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        count_edges( n );
      });
    }

    outdated = false;
  }

  /* Update the fanout_information of a node */
  template<bool verify = false>
  bool update_fanout_info( node const& n )
  {
    std::vector<node> fos;
    for ( auto it = _fanouts[n].begin(); it != _fanouts[n].end(); ++it )
    {
      if ( it->fanouts.size() )
      {
        for ( auto it2 = it->fanouts.begin(); it2 != it->fanouts.end(); ++it2 )
          fos.push_back( *it2 );
      }
    }

    _fanouts[n].clear();
    for ( auto& fo : fos )
      insert_fanout( n, fo );
    return count_edges<verify>( n );
  }

  void insert_fanout( node const& n, node const& fanout )
  {
    auto const rd = _levels[fanout] - _levels[n];
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

  template<bool verify = false>
  bool count_edges( node const& n )
  {
    auto& fo_infos = _fanouts[n];

    if ( _external_ref_count[n] && _ps.balance_pos )
    {
      fo_infos.push_back( {_depth + 1 - _levels[n], {}, _external_ref_count[n]} );
    }

    if ( fo_infos.size() == 0u || ( fo_infos.size() == 1u && fo_infos.front().num_edges == 1u ) )
    {
      return true;
    }
    assert( fo_infos.front().relative_depth > 1u );
    fo_infos.push_front( {1u, {}, 0u} );

    auto it = fo_infos.end();
    for ( --it; it != fo_infos.begin(); --it )
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
    if constexpr ( verify )
    {
      return fo_infos.front().num_edges == 1u;
    }
    else
    {
      assert( fo_infos.front().num_edges == 1u );
      return true;
    }
  }

  /* Return the number of splitters needed in one level lower */
  uint32_t num_splitters( uint32_t const& num_fanouts ) const
  {
    return std::ceil( float( num_fanouts ) / float( _ps.splitter_capacity ) );
  }
#pragma endregion

#pragma region Level assignment
public:
  /*! \brief ASAP scheduling */
  void ASAP()
  {
    _depth = 0;
    _levels.reset( 0 );
    _ntk.incr_trav_id();

    _ntk.foreach_po( [&]( auto const& f ) {
      auto const no = _ntk.get_node( f );
      auto clevel = compute_levels_ASAP( no ) + num_splitter_levels( no );
      _depth = std::max( _depth, clevel );
    } );

    outdated = true;
  }

  /*! \brief ALAP scheduling.
   *
   * ALAP should follow right after ASAP (i.e., initialization) without other optimization in between.
   */
  void ALAP()
  {
    _levels.reset( 0 );
    _ntk.incr_trav_id();

    _ntk.foreach_po( [&]( auto const& f ) {
      const auto n = _ntk.get_node( f );
      if ( !_ntk.is_constant( n ) && _ntk.visited( n ) != _ntk.trav_id() && ( !_ps.balance_pis || !_ntk.is_pi( n ) ) )
      {
        _levels[n] = _depth - num_splitter_levels( n );
        compute_levels_ALAP( n );
      }
    } );

    outdated = true;
  }

private:
  uint32_t compute_levels_ASAP( node const& n )
  {
    if ( _ntk.visited( n ) == _ntk.trav_id() )
    {
      return _levels[n];
    }
    _ntk.set_visited( n, _ntk.trav_id() );

    if ( _ntk.is_constant( n ) || _ntk.is_pi( n ) )
    {
      return _levels[n] = 0;
    }

    uint32_t level{0};
    _ntk.foreach_fanin( n, [&]( auto const& fi ) {
      auto const ni = _ntk.get_node( fi );
      if ( !_ntk.is_constant( ni ) )
      {
        auto fi_level = compute_levels_ASAP( ni );
        if ( _ps.branch_pis || !_ntk.is_pi( ni ) )
        {
          fi_level += num_splitter_levels( ni );
        }
        level = std::max( level, fi_level );
      }
    } );

    return _levels[n] = level + 1;
  }

  void compute_levels_ALAP( node const& n )
  {
    _ntk.set_visited( n, _ntk.trav_id() );

    _ntk.foreach_fanin( n, [&]( auto const& fi ) {
      auto const ni = _ntk.get_node( fi );
      if ( !_ntk.is_constant( ni ) )
      {
        if ( _ps.balance_pis && _ntk.is_pi( ni ) )
        {
          assert( _levels[n] > 0 );
          _levels[ni] = 0;
        }
        else if ( _ps.branch_pis || !_ntk.is_pi( ni ) )
        {
          assert( _levels[n] > num_splitter_levels( ni ) );
          auto fi_level = _levels[n] - num_splitter_levels( ni ) - 1;
          if ( _ntk.visited( ni ) != _ntk.trav_id() || _levels[ni] > fi_level )
          {
            _levels[ni] = fi_level;
            compute_levels_ALAP( ni );
          }
        }
      }
    } );
  }
#pragma endregion

#pragma region Dump buffered network
public:
  /*! \brief Dump buffered network
   * 
   * After level assignment, (optimization), and buffer counting, this method
   * can be called to dump the resulting buffered network.
   */
  template<class BufNtk>
  BufNtk dump_buffered_network() const
  {
    static_assert( has_is_buf_v<BufNtk>, "BufNtk is not a buffered network type" );
    assert( !outdated && "Please call `count_buffers()` first." );

    using fanout_tree = std::vector<std::list<typename BufNtk::signal>>;

    BufNtk bufntk;
    node_map<typename BufNtk::signal, Ntk> node_to_signal( _ntk );
    node_map<fanout_tree, Ntk> buffers( _ntk );

    /* constants */
    node_to_signal[_ntk.get_constant( false )] = bufntk.get_constant( false );
    buffers[_ntk.get_constant( false )].emplace_back( 1, bufntk.get_constant( false ) );
    if ( _ntk.get_node( _ntk.get_constant( false ) ) != _ntk.get_node( _ntk.get_constant( true ) ) )
    {
      node_to_signal[_ntk.get_constant( true )] = bufntk.get_constant( true );
      buffers[_ntk.get_constant( true )].emplace_back( 1, bufntk.get_constant( true ) );
    }

    /* PIs */
    _ntk.foreach_pi( [&]( auto const& n ){
      node_to_signal[n] = bufntk.create_pi();
    });
    if ( _ps.branch_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        create_buffer_chain( bufntk, buffers, n, node_to_signal[n] );
      });
    }
    else
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        buffers[n].emplace_back( 1, node_to_signal[n] );
      });
    }

    /* gates: assume topological order */
    _ntk.foreach_gate( [&]( auto const& n ){
      std::vector<typename BufNtk::signal> children;
      _ntk.foreach_fanin( n, [&]( auto const& fi ){
        auto ni = _ntk.get_node( fi );
        typename BufNtk::signal s;
        if ( _ntk.is_constant( ni ) || ( !_ps.branch_pis && _ntk.is_pi( ni ) ) )
          s = node_to_signal[ni];
        else
          s = get_buffer_at_rd( bufntk, buffers[fi], _levels[n] - _levels[fi] - 1 );
        children.push_back( _ntk.is_complemented( fi ) ? !s : s );
      });
      node_to_signal[n] = bufntk.clone_node( _ntk, n, children );
      create_buffer_chain( bufntk, buffers, n, node_to_signal[n] );
    });

    /* POs */
    if ( _ps.balance_pos )
    {
      _ntk.foreach_po( [&]( auto const& f ){
        auto n = _ntk.get_node( f );
        typename BufNtk::signal s;
        if ( _ntk.is_constant( n ) || ( !_ps.branch_pis && _ntk.is_pi( n ) ) )
          s = node_to_signal[n];
        else
          s = get_buffer_at_rd( bufntk, buffers[f], _depth - _levels[f] );
        bufntk.create_po( _ntk.is_complemented( f ) ? !s : s );
      });
    }
    else // !_ps.balance_pos
    {
      std::set<node> checked;
      _ntk.foreach_po( [&]( auto const& f ){
        auto n = _ntk.get_node( f );
        if ( _ntk.is_constant( n ) || ( _ntk.is_pi( n ) && !_ps.branch_pis ) || _ntk.fanout_size( n ) == 1 )
        {
          bufntk.create_po( _ntk.is_complemented( f ) ? !node_to_signal[f] : node_to_signal[f] );
        }
        else
        {
          if ( checked.find( n ) == checked.end() )
          {
            checked.insert( n );
            /* count available slots in buffers[n] */
            uint32_t slots{0u};
            for ( auto const& bufs : buffers[n] )
            {
              slots += _ps.splitter_capacity - bufntk.fanout_size( bufntk.get_node( bufs.back() ) );
            }
            slots -= _ps.splitter_capacity - 1; // buffers[n][0] is n itself

            /* add enough buffers */
            while ( slots < _external_ref_count[n] )
            {
              get_lowest_spot<true>( bufntk, buffers[n] );
              slots += _ps.splitter_capacity - 1;
            }
          }
          typename BufNtk::signal s = get_lowest_spot<false>( bufntk, buffers[n] );
          bufntk.create_po( _ntk.is_complemented( f ) ? !s : s );
        }
      });
    }

    assert( bufntk.size() - bufntk.num_pis() - bufntk.num_gates() - 1 == num_buffers() );
    return bufntk;
  }

private:
  template<class BufNtk, typename Buffers>
  void create_buffer_chain( BufNtk& bufntk, Buffers& buffers, node const& n, typename BufNtk::signal const& s ) const
  {
    auto const& fanout_info = _fanouts[n];
    if ( fanout_info.size() == 0u )
    {
      buffers[n].emplace_back( 1, s );
      return;
    }

    buffers[n].resize( fanout_info.back().relative_depth );
    auto& fot = buffers[n];

    typename BufNtk::signal fi = s;
    fot[0].push_back( fi );
    for ( auto i = 1u; i < fot.size(); ++i )
    {
      fi = bufntk.create_buf( fi );
      fot[i].push_back( fi );
    }
  }

  template<class BufNtk, typename FOT>
  typename BufNtk::signal get_buffer_at_rd( BufNtk& bufntk, FOT& fot, uint32_t rd ) const
  {
    typename BufNtk::signal b = fot[rd].back();
    if ( bufntk.fanout_size( bufntk.get_node( b ) ) == _ps.splitter_capacity )
    {
      assert( rd > 0 );
      typename BufNtk::signal b_lower = get_buffer_at_rd( bufntk, fot, rd - 1 );
      b = bufntk.create_buf( b_lower );
      fot[rd].push_back( b );
    }
    return b;
  }

  template<bool create, class BufNtk, typename FOT>
  typename BufNtk::signal get_lowest_spot( BufNtk& bufntk, FOT& fot ) const
  {
    if ( fot.size() == 1 )
    {
      assert( create );
      fot.emplace_back( 1, bufntk.create_buf( fot[0].back() ) );
      return fot[1].back();
    }

    for ( auto rd = 1u; rd < fot.size(); ++rd )
    {
      for ( auto it = fot[rd].begin(); it != fot[rd].end(); ++it )
      {
        typename BufNtk::signal& b = *it;
        if ( bufntk.fanout_size( bufntk.get_node( b ) ) < _ps.splitter_capacity )
        {
          if constexpr ( create )
          {
            if ( rd == fot.size() - 1 )
              fot.emplace_back( 1, bufntk.create_buf( b ) );
            else
              fot[rd + 1].push_back( bufntk.create_buf( b ) );
          }
          return b;
        }
      }
    }
    assert( false );
  }
#pragma endregion

private:
  struct fanout_information
  {
    uint32_t relative_depth{0u};
    std::list<node> fanouts;
    uint32_t num_edges{0u};
  };
  using fanouts_by_level = std::list<fanout_information>;
  
  Ntk const& _ntk;
  aqfp_buffer_params const _ps;
  bool outdated{true};

  node_map<uint32_t, Ntk> _levels;
  uint32_t _depth{0u};
  node_map<fanouts_by_level, Ntk> _fanouts;
  node_map<uint32_t, Ntk> _external_ref_count;
  node_map<uint32_t, Ntk> _buffers;
}; /* aqfp_buffer */

namespace detail
{

template<class Ntk>
void lift_fanin_buffers( Ntk& d, typename Ntk::node const& n )
{
  d.foreach_fanin( n, [&]( auto const& fi ){
    auto ni = d.get_node( fi );
    uint32_t diff = d.level( n ) - d.level( ni ) - 1;
    if ( diff != 0 && ( d.is_buf( ni ) || d.is_pi( ni ) ) )
    {
      d.set_level( ni, d.level( ni ) + diff );
      lift_fanin_buffers( d, ni );
    }
  });
}

} // namespace detail

/*! \brief Verify a buffered network according to AQFP assumptions.
 * 
 * \param ntk Buffered network
 * \param ps AQFP constraints
 * \return Whether `ntk` is path-balanced and properly-branched
 */
template<class Ntk>
bool verify_aqfp_buffer( Ntk const& ntk, aqfp_buffer_params const& ps )
{
  static_assert( has_is_buf_v<Ntk>, "Ntk is not a buffered network" );
  bool legal = true;
  
  /* fanout branching */
  ntk.foreach_node( [&]( auto const& n ){
    if ( ntk.is_constant( n ) ) return true;
    if ( !ps.branch_pis && ntk.is_pi( n ) ) return true;

    if ( ntk.is_buf( n ) )
      legal &= ( ntk.fanout_size( n ) <= ps.splitter_capacity );
    else /* logic gate */
      legal &= ( ntk.fanout_size( n ) <= 1 );

    return true;
  });

  /* compute levels */
  depth_view d{ntk};

  /* adjust PI and their buffers */
  if ( !ps.balance_pis )
  {
    ntk.foreach_gate( [&]( auto const& n ){
      detail::lift_fanin_buffers( d, n );
    });
    if ( ps.balance_pos )
    {
      ntk.foreach_po( [&]( auto const& f ){
        auto n = ntk.get_node( f );
        if ( ntk.is_buf( n ) && d.level( n ) != d.depth() )
        {
          d.set_level( n, d.depth() );
          detail::lift_fanin_buffers( d, n );
        }
      });
    }
  }

  /* path balancing */
  ntk.foreach_node( [&]( auto const& n ){
    ntk.foreach_fanin( n, [&]( auto const& fi ){
      auto ni = ntk.get_node( fi );
      if ( !ntk.is_constant( ni ) && ( ps.balance_pis || !ntk.is_pi( ni ) ) )
        legal &= ( d.level( ni ) == d.level( n ) - 1 );
    });
  });

  if ( ps.balance_pos )
  {
    ntk.foreach_po( [&]( auto const& f ){
      auto n = ntk.get_node( f );
      if ( !ntk.is_constant( n ) && ( ps.balance_pis || !ntk.is_pi( n ) ) )
        legal &= ( d.level( n ) == d.depth() );
    });
  }

  return legal;
}

} // namespace mockturtle