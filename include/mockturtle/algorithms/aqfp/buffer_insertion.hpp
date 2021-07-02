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
  \file buffer_insertion.hpp
  \brief Insert buffers and splitters for the AQFP technology

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../../traits.hpp"
#include "../../utils/node_map.hpp"
#include "../../views/depth_view.hpp"

#include <cmath>
#include <limits>
#include <list>
#include <set>
#include <vector>

namespace mockturtle
{

/*! \brief AQFP technology assumptions.
 * 
 * POs count toward the fanout sizes and always have to be branched.
 * If PIs need to be balanced, then they must also need to be branched.
 */
struct aqfp_assumptions
{
  /*! \brief Whether PIs need to be branched with splitters. */
  bool branch_pis{false};

  /*! \brief Whether PIs need to be path-balanced. */
  bool balance_pis{false};

  /*! \brief Whether POs need to be path-balanced. */
  bool balance_pos{true};

  /*! \brief The maximum number of fanouts each splitter (buffer) can have. */
  uint32_t splitter_capacity{3u};
};

/*! \brief Parameters for (AQFP) buffer insertion.
 */
struct buffer_insertion_params
{
  /*! \brief Technology assumptions. */
  aqfp_assumptions assume;

  /*! \brief The scheduling strategy to get the initial depth assignment.
   * - `provided` = An initial level assignment is given in the constructor, thus
   * no scheduling is performed. It is the user's responsibility to ensure that
   * the provided assignment is legal.
   * - `ASAP` = Classical As-Soon-As-Possible scheduling 
   * - `ALAP` = ASAP (to obtain depth) followed by As-Late-As-Possible scheduling
   * - `better` = ASAP followed by ALAP, then count buffers for both assignments
   * and choose the better one
   */
  enum
  {
    provided,
    ASAP,
    ALAP,
    better
  } scheduling = ASAP;

  /*! \brief The level of chunked-movement-based optimization effort.
   * - `none` = No optimization
   * - `one_pass` = Try to form a chunk starting from each gate, once
   * for all gates
   * - `until_sat` = Iterate over all gates until no more beneficial
   * chunk movement can be found
   */
  enum
  {
    none,
    one_pass,
    until_sat,
  } optimization_effort = none;
};

/*! \brief Insert buffers and splitters for the AQFP technology.
 * 
 * In the AQFP technology, (1) logic gates can only have one fanout. If more than one
 * fanout is needed, a splitter has to be inserted in between, which also 
 * takes one clocking phase (counted towards the network depth). (2) All fanins of
 * a logic gate have to arrive at the same time (be at the same level). If one
 * fanin path is shorter, buffers have to be inserted to balance it. 
 * Buffers and splitters are essentially the same component in this technology.
 *
 * With a given level assignment to all gates in the network, the minimum number of
 * buffers needed is determined. This class implements algorithms to count such
 * "irredundant buffers" and to insert them to obtain a buffered network. Moreover,
 * as buffer optimization is essentially a problem of obtaining a good level assignment,
 * this class also implements algorithms to obtain an initial, legal assignment using
 * scheduling algorithms and to further adjust and optimize it.
 * 
 * This class provides two easy-to-use top-level functions which wrap all the above steps
 * together: `run` and `dry_run`. In addition, the following interfaces are kept for
 * more fine-grained usage:
 * - Query the current level assignment (`level`, `depth`)
 * - Count irredundant buffers based on the current level assignment (`count_buffers`,
 * `num_buffers`)
 * - Optimize buffer count by adjusting the level assignment (`ASAP`, `ALAP`)
 * - Dump the resulting network into a network type which provides representation for 
 * buffers (`dump_buffered_network`)
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      mig_network mig = ...
      
      buffer_insertion_params ps;
      ps.assume.branch_pis = true;
      ps.assume.balance_pis = false;
      ps.assume.balance_pos = true;
      ps.assume.splitter_capacity = 3u;
      ps.scheduling = buffer_insertion_params::ALAP;
      ps.optimization_effort = buffer_insertion_params::one_pass;

      buffer_insertion buffering( mig, ps );
      buffered_mig_network buffered_mig;
      auto const num_buffers = buffering.run( buffered_mig );
      
      std::cout << num_buffers << std::endl;
      write_verilog( buffered_mig, "buffered.v" );
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
class buffer_insertion
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

  explicit buffer_insertion( Ntk const& ntk, buffer_insertion_params const& ps = {} )
      : _ntk( ntk ), _ps( ps ), _levels( _ntk ), _fanouts( _ntk ), _external_ref_count( _ntk ), _buffers( _ntk )
  {
    static_assert( !is_buffered_network_type_v<Ntk>, "Ntk is already buffered" );
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

    assert( !( _ps.assume.balance_pis && !_ps.assume.branch_pis ) && "Does not make sense to balance but not branch PIs" );
    assert( _ps.scheduling != buffer_insertion_params::provided );
  }

  explicit buffer_insertion( Ntk const& ntk, node_map<uint32_t, Ntk> const& levels, buffer_insertion_params const& ps = {} )
      : _ntk( ntk ), _ps( ps ), _levels( levels ), _fanouts( _ntk ), _external_ref_count( _ntk ), _buffers( _ntk )
  {
    static_assert( !is_buffered_network_type_v<Ntk>, "Ntk is already buffered" );
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

    assert( !( _ps.assume.balance_pis && !_ps.assume.branch_pis ) && "Does not make sense to balance but not branch PIs" );
    assert( _ps.scheduling == buffer_insertion_params::provided );
  }

  /*! \brief Insert buffers and obtain a buffered network.
   * \param bufntk An empty network of an appropriate buffered network type to
   * to store the buffer-insertion result
   * \param pLevels A pointer to a node map which will store the resulting 
   * level assignment
   * \return The number of buffers in the resulting network
   */
  template<class BufNtk>
  uint32_t run( BufNtk& bufntk, node_map<uint32_t, Ntk>* pLevels = nullptr )
  {
    dry_run( pLevels );
    dump_buffered_network( bufntk );
    return num_buffers();
  }

  /*! \brief Count the number of buffers without dumping the result into a buffered network.
   * 
   * This function saves some runtime for dumping the resulting network and
   * allows users to experiment on the algorithms with new network types whose
   * corresponding buffered_network are not implemented yet.
   * 
   * \param pLevels A pointer to a node map which will store the resulting 
   * level assignment
   * \return The number of buffers in the resulting network
   */
  uint32_t dry_run( node_map<uint32_t, Ntk>* pLevels = nullptr )
  {
    schedule();
    optimize();
    count_buffers();

    if ( pLevels )
      *pLevels = _levels;

    return num_buffers();
  }

#pragma region Query
  /*! \brief Level of node `n` considering buffer/splitter insertion. */
  uint32_t level( node const& n ) const
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
    if ( _ps.assume.branch_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ) {
        count += num_buffers( n );
      } );
    }

    _ntk.foreach_gate( [&]( auto const& n ) {
      count += num_buffers( n );
    } );
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

    if ( _ps.assume.branch_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ) {
        assert( !_ps.assume.balance_pis || _levels[n] == 0 );
        _buffers[n] = count_buffers( n );
      } );
    }

    _ntk.foreach_gate( [&]( auto const& n ) {
      _buffers[n] = count_buffers( n );
    } );
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
        return _ps.assume.balance_pos ? _depth - _levels[n] : 0u;
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
      assert( !_ps.assume.balance_pos && _ntk.fanout_size( n ) == _external_ref_count[n] );
      return std::ceil( float( _external_ref_count[n] - 1 ) / float( _ps.assume.splitter_capacity - 1 ) );
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
    if ( !_ps.assume.balance_pos && _external_ref_count[n] > 0u )
    {
      /* check if available slots are enough */
      auto slots = count * ( _ps.assume.splitter_capacity - 1 ) + 1;
      int32_t needed = _ntk.fanout_size( n ) - slots;
      if ( needed > 0 )
      {
        count += std::ceil( float( needed ) / float( _ps.assume.splitter_capacity - 1 ) );
      }
    }
    else
    {
      /* if _external_ref_count[n] == 0 : does nothing
         otherwise _ps.assume.balance_pos == true : PO refs were added as num_edges and counted as buffers */
      count -= _external_ref_count[n];
    }

    return count;
  }

  /* (Upper bound on) the additional depth caused by a balanced splitter tree at the output of node `n`. */
  uint32_t num_splitter_levels( node const& n ) const
  {
    assert( n < _ntk.size() );
    return std::ceil( std::log( _ntk.fanout_size( n ) ) / std::log( _ps.assume.splitter_capacity ) );
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
    _ntk.foreach_po( [&]( auto const& f ) {
      _external_ref_count[f]++;
    } );

    _fanouts.reset();
    _ntk.foreach_gate( [&]( auto const& n ) {
      _ntk.foreach_fanin( n, [&]( auto const& fi ) {
        auto const ni = _ntk.get_node( fi );
        if ( !_ntk.is_constant( ni ) )
        {
          insert_fanout( ni, n );
        }
      } );
    } );

    _ntk.foreach_gate( [&]( auto const& n ) {
      count_edges( n );
    } );

    if ( _ps.assume.branch_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ) {
        count_edges( n );
      } );
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
    fo_infos.push_back( {rd, {fanout}, 1u} );
  }

  template<bool verify = false>
  bool count_edges( node const& n )
  {
    auto& fo_infos = _fanouts[n];

    if ( _external_ref_count[n] && _ps.assume.balance_pos )
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
        ++( it->num_edges );
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
    return std::ceil( float( num_fanouts ) / float( _ps.assume.splitter_capacity ) );
  }
#pragma endregion

#pragma region Level assignment
public:
  /*! \breif Obtain the initial level assignment using the specified scheduling policy */
  void schedule()
  {
    if ( _ps.scheduling == buffer_insertion_params::provided )
    {
      _ntk.foreach_po( [&]( auto const& f ) {
        _depth = std::max( _depth, _levels[f] + num_splitter_levels( _ntk.get_node( f ) ) );
      } );
    }
    else
    {
      ASAP();
    }

    if ( _ps.scheduling == buffer_insertion_params::ALAP )
    {
      ALAP();
    }
    else if ( _ps.scheduling == buffer_insertion_params::better )
    {
      count_buffers();
      auto num_buf_ASAP = num_buffers();
      ALAP();
      count_buffers();
      if ( num_buffers() > num_buf_ASAP )
      {
        ASAP();
      }
    }
  }

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
      if ( !_ntk.is_constant( n ) && _ntk.visited( n ) != _ntk.trav_id() && ( !_ps.assume.balance_pis || !_ntk.is_pi( n ) ) )
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
        if ( _ps.assume.branch_pis || !_ntk.is_pi( ni ) )
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
        if ( _ps.assume.balance_pis && _ntk.is_pi( ni ) )
        {
          assert( _levels[n] > 0 );
          _levels[ni] = 0;
        }
        else if ( _ps.assume.branch_pis || !_ntk.is_pi( ni ) )
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
  void dump_buffered_network( BufNtk& bufntk ) const
  {
    static_assert( is_buffered_network_type_v<BufNtk>, "BufNtk is not a buffered network type" );
    static_assert( has_is_buf_v<BufNtk>, "BufNtk does not implement the is_buf method" );
    static_assert( has_create_buf_v<BufNtk>, "BufNtk does not implement the create_buf method" );
    assert( !outdated && "Please call `count_buffers()` first." );

    using buf_signal = typename BufNtk::signal;
    using fanout_tree = std::vector<std::list<buf_signal>>;

    node_map<buf_signal, Ntk> node_to_signal( _ntk );
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
    _ntk.foreach_pi( [&]( auto const& n ) {
      node_to_signal[n] = bufntk.create_pi();
    } );
    if ( _ps.assume.branch_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ) {
        create_buffer_chain( bufntk, buffers, n, node_to_signal[n] );
      } );
    }
    else
    {
      _ntk.foreach_pi( [&]( auto const& n ) {
        buffers[n].emplace_back( 1, node_to_signal[n] );
      } );
    }

    /* gates: assume topological order */
    _ntk.foreach_gate( [&]( auto const& n ) {
      std::vector<buf_signal> children;
      _ntk.foreach_fanin( n, [&]( auto const& fi ) {
        auto ni = _ntk.get_node( fi );
        buf_signal s;
        if ( _ntk.is_constant( ni ) || ( !_ps.assume.branch_pis && _ntk.is_pi( ni ) ) )
          s = node_to_signal[ni];
        else
          s = get_buffer_at_relative_depth( bufntk, buffers[fi], _levels[n] - _levels[fi] - 1 );
        children.push_back( _ntk.is_complemented( fi ) ? !s : s );
      } );
      node_to_signal[n] = bufntk.clone_node( _ntk, n, children );
      create_buffer_chain( bufntk, buffers, n, node_to_signal[n] );
    } );

    /* POs */
    if ( _ps.assume.balance_pos )
    {
      _ntk.foreach_po( [&]( auto const& f ) {
        auto n = _ntk.get_node( f );
        buf_signal s;
        if ( _ntk.is_constant( n ) || ( !_ps.assume.branch_pis && _ntk.is_pi( n ) ) )
          s = node_to_signal[n];
        else
          s = get_buffer_at_relative_depth( bufntk, buffers[f], _depth - _levels[f] );
        bufntk.create_po( _ntk.is_complemented( f ) ? !s : s );
      } );
    }
    else // !_ps.assume.balance_pos
    {
      std::set<node> checked;
      _ntk.foreach_po( [&]( auto const& f ) {
        auto n = _ntk.get_node( f );
        if ( _ntk.is_constant( n ) || ( _ntk.is_pi( n ) && !_ps.assume.branch_pis ) || _ntk.fanout_size( n ) == 1 )
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
              slots += _ps.assume.splitter_capacity - bufntk.fanout_size( bufntk.get_node( bufs.back() ) );
            }
            slots -= _ps.assume.splitter_capacity - 1; // buffers[n][0] is n itself

            /* add enough buffers */
            while ( slots < _external_ref_count[n] )
            {
              get_lowest_spot<true>( bufntk, buffers[n] );
              slots += _ps.assume.splitter_capacity - 1;
            }
          }
          buf_signal s = get_lowest_spot<false>( bufntk, buffers[n] );
          bufntk.create_po( _ntk.is_complemented( f ) ? !s : s );
        }
      } );
    }

    assert( bufntk.size() - bufntk.num_pis() - bufntk.num_gates() - 1 == num_buffers() );
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
  typename BufNtk::signal get_buffer_at_relative_depth( BufNtk& bufntk, FOT& fot, uint32_t rd ) const
  {
    typename BufNtk::signal b = fot[rd].back();
    if ( bufntk.fanout_size( bufntk.get_node( b ) ) == _ps.assume.splitter_capacity )
    {
      assert( rd > 0 );
      typename BufNtk::signal b_lower = get_buffer_at_relative_depth( bufntk, fot, rd - 1 );
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
        if ( bufntk.fanout_size( bufntk.get_node( b ) ) < _ps.assume.splitter_capacity )
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

#pragma region Chunked movement
public:
  /*! \brief Optimize with the specified optimization policy */
  void optimize()
  {
    bool updated = true;
    while ( updated )
    {
      updated = find_chunks();
    }
  }

  enum direction
  {
    ANY,
    DOWN,
    UP
  };

  struct interface
  {
    node c; // chunk node
    node o; // outside node
  };

  struct chunk
  {
    direction purpose;
    uint32_t id;
    std::vector<node> members{};
    std::vector<interface> input_interfaces{};
    std::vector<interface> output_interfaces{};
    int32_t slack{std::numeric_limits<int32_t>::max()};
    int32_t benefits{0};
  };

  bool is_upper_bounded( node const& n ) const
  {
    (void)n;
    return false;
  }

  bool is_lower_bounded( node const& n ) const
  {
    return _levels[n] == 0;
  }

  bool is_ignored( node const& n ) const
  {
    return _ntk.is_constant( n ) || ( !_ps.assume.branch_pis && _ntk.is_pi( n ) );
  }

  bool is_fixed( node const& n ) const
  {
    if ( _ps.assume.balance_pis ) return _ntk.is_pi( n );
    return false;
  }

  bool find_chunks()
  {
    bool updated = false;
    start_id = _ntk.trav_id();

    _ntk.foreach_node( [&]( auto const& n ){
      if ( is_ignored( n ) || is_fixed( n ) || _ntk.visited( n ) > start_id /* belongs to a chunk */ )
      {
        return true;
      }

      //std::cout << "[i] try forming a chunk from node " << node_info( n ) << "\n";
      _ntk.incr_trav_id();
      chunk c{ANY, _ntk.trav_id()};
      recruit( n, c );
      cleanup_interfaces( c );
      //print_chunk( c );

      auto moved = analyze_chunk_down( c );
      if ( !moved ) moved = analyze_chunk_up( c );
      updated |= moved;
      return true;
    });

    if ( !updated )
    {
      std::set<uint32_t> chunk_ids;
      uint32_t num_chunks = 0u;
      _ntk.foreach_gate( [&]( auto const& n ){
        if ( is_ignored( n ) || is_fixed( n ) )
        {
          return true;
        }
        if ( _ntk.visited( n ) <= start_id )
        {
          std::cout << "node " << node_info( n ) << " does not belong to any chunk\n";
        }
        if ( chunk_ids.find( _ntk.visited( n ) ) == chunk_ids.end() )
        {
          chunk_ids.insert( _ntk.visited( n ) );
          ++num_chunks;
        }
        return true;
      });
      std::cout << " > [i] #chunks = " << num_chunks << "\n";
    }

    return updated;
  }

  void recruit( node const& n, chunk& c )
  {
    if ( _ntk.visited( n ) == c.id )
      return;
    //std::cout << "[i] recruit node " << node_info( n ) << "\n";

    assert( _ntk.visited( n ) <= start_id );
    assert( !is_fixed( n ) ); 
    assert( !is_ignored( n ) );
    
    _ntk.set_visited( n, c.id );
    c.members.emplace_back( n );
    recruit_fanins( n, c );
    recruit_fanouts( n, c );
  }

  void recruit_fanins( node const& n, chunk& c )
  {
    _ntk.foreach_fanin( n, [&]( auto const& fi ){
      auto const ni = _ntk.get_node( fi );
      if ( !is_ignored( ni ) && _ntk.visited( ni ) != c.id )
      {
        if ( is_fixed( ni ) )
          c.input_interfaces.push_back( {n, ni} );
        else if ( are_close( ni, n ) )
          recruit( ni, c );
        else
          c.input_interfaces.push_back( {n, ni} );
      }
    });
  }

  void recruit_fanouts( node const& n, chunk& c )
  {
    auto const& fanout_info = _fanouts[n];
    if ( fanout_info.size() == 0 )
      return;

    if ( _ntk.fanout_size( n ) == _external_ref_count[n] ) // only POs
    {
      c.output_interfaces.push_back( {n, n} ); // PO interface
    }
    else if ( fanout_info.size() == 1 ) // single gate fanout
    {
      auto const& no = fanout_info.front().fanouts.front();
      if ( is_fixed( no ) )
        c.output_interfaces.push_back( {n, no} );
      else if ( fanout_info.front().relative_depth == 1 )
        recruit( no, c );
      else
        c.output_interfaces.push_back( {n, no} );
    }
    else
    {
      for ( auto it = fanout_info.begin(); it != fanout_info.end(); ++it )
      {
        for ( auto it2 = it->fanouts.begin(); it2 != it->fanouts.end(); ++it2 )
        {
          if ( is_fixed( *it2 ) )
            c.output_interfaces.push_back( {n, *it2} );

          else if ( it->relative_depth == 2 )
            recruit( *it2, c );
          else if ( _ntk.visited( *it2 ) != c.id )
            c.output_interfaces.push_back( {n, *it2} );
        }
      }
    }
  }

  bool are_close( node const& ni, node const& n )
  {
    auto const& fanout_info = _fanouts[ni];
    if ( fanout_info.size() == 1 && fanout_info.front().relative_depth == 1 )
    {
      assert( fanout_info.front().fanouts.front() == n );
      return true;
    }
    if ( fanout_info.size() > 1 )
    {
      auto it = fanout_info.begin();
      it++;
      if ( it->relative_depth > 2 ) return false;
      for ( auto it2 = it->fanouts.begin(); it2 != it->fanouts.end(); ++it2 )
      {
        if ( *it2 == n )
          return true;
      }
    }
    return false;
  }

  void cleanup_interfaces( chunk& c )
  {
    for ( int i = 0; i < c.input_interfaces.size(); ++i )
    {
      if ( _ntk.visited( c.input_interfaces[i].o ) == c.id )
      {
        c.input_interfaces.erase( c.input_interfaces.begin() + i );
        --i;
      }
    }
    for ( int i = 0; i < c.output_interfaces.size(); ++i )
    {
      if ( _ntk.visited( c.output_interfaces[i].o ) == c.id && c.output_interfaces[i].o != c.output_interfaces[i].c )
      {
        c.output_interfaces.erase( c.output_interfaces.begin() + i );
        --i;
      }
    }
  }

  bool analyze_chunk_down( chunk c )
  {
    //std::cout << "[i] analyze chunk " << c.id << "\n";
    c.purpose = DOWN;
    for ( auto m : c.members )
    {
      if ( is_lower_bounded( m ) )
        return false;
    }

    std::set<node> marked_oi;
    for ( auto oi : c.output_interfaces )
    {
      if ( marked_oi.find( oi.c ) == marked_oi.end() )
      {
        marked_oi.insert( oi.c );
        --c.benefits;
      }
    }

    for ( auto ii : c.input_interfaces )
    {
      auto const rd = _levels[ii.c] - _levels[ii.o];
      auto const lowest = lowest_spot( ii.o );
      if ( rd <= lowest )
      {
        c.slack = 0;
        break;
      }
      c.slack = std::min( c.slack, int32_t(rd - lowest) );
      if ( c.slack == rd - lowest )
        mark_occupied( ii.o, lowest ); // TODO: may be inaccurate
      if ( _fanouts[ii.o].back().relative_depth == rd && _fanouts[ii.o].back().num_edges == 1 ) // is the only highest fanout
      {
        ++c.benefits;
      }
    }

    if ( c.benefits > 0 && c.slack > 0 )
    {
      count_buffers();
      bool legal = true;
      auto buffers_before = num_buffers();
      //print_chunk( c );
      for ( auto m : c.members )
        _levels[m] -= c.slack;
      for ( auto m : c.members )
        update_fanout_info( m );
      for ( auto ii : c.input_interfaces )
        legal &= update_fanout_info<true>( ii.o );
      
      count_buffers();
      if ( !legal || num_buffers() >= buffers_before )
      {
        /* UNDO */
        for ( auto m : c.members )
          _levels[m] += c.slack;
        for ( auto m : c.members )
          update_fanout_info( m );
        for ( auto ii : c.input_interfaces )
          update_fanout_info( ii.o );
        return false;
      }

      //mark_nodes();
      start_id = _ntk.trav_id();

      //std::cout << "predicted gain = (interfaces)" << c.benefits << " * (slack)" << c.slack << "; after moving chunk, buffers = " << num_buffers() << "\n";
      return true;
    }
    else
    {
      /* reset fanout_infos of input_interfaces because num_edges may be modified by mark_occupied */
      for ( auto ii : c.input_interfaces )
        update_fanout_info( ii.o );
      return false;
    }
  }

  /* relative_depth of the lowest available spot in the fanout tree of n */
  uint32_t lowest_spot( node const& n ) const
  {
    auto const& fanout_info = _fanouts[n];
    assert( fanout_info.size() );
    assert( _ntk.fanout_size( n ) != _external_ref_count[n] );
    if ( fanout_info.size() == 1 )
    {
      assert( fanout_info.front().fanouts.size() == 1 );
      return 1;
    }
    auto it = fanout_info.begin(); ++it;
    while ( it != fanout_info.end() && it->num_edges == _ps.assume.splitter_capacity )
      ++it;
    if ( it == fanout_info.end() ) // full fanout tree
      return fanout_info.back().relative_depth + 1;
    --it; // the last full layer
    return it->relative_depth + 1;
  }

  void mark_occupied( node const& n, uint32_t rd )
  {
    auto& fanout_info = _fanouts[n];
    for ( auto it = fanout_info.begin(); it != fanout_info.end(); ++it )
    {
      if ( it->relative_depth == rd )
      {
        ++it->num_edges;
        return;
      }
    }
  }

  bool analyze_chunk_up( chunk c )
  {
    //std::cout << "[i] analyze chunk " << c.id << "\n";
    c.purpose = UP;
    for ( auto m : c.members )
    {
      if ( is_upper_bounded( m ) )
        return false;
    }

    for ( auto ii : c.input_interfaces )
    {
      if ( _fanouts[ii.o].back().relative_depth == _levels[ii.c] - _levels[ii.o] ) // is highest fanout
        --c.benefits;
    }

    std::set<node> marked_oi;
    for ( auto oi : c.output_interfaces )
    {
      if ( marked_oi.find( oi.c ) == marked_oi.end() )
      {
        marked_oi.insert( oi.c );
        ++c.benefits;
        //std::cout << "[i] OI " << oi.c << " -> " <<  oi.o << "\n";
        //print_fanout_infos( oi.c );
      }
      auto const& fanout_info = _fanouts[oi.c];
      if ( _ntk.fanout_size( oi.c ) == _external_ref_count[oi.c] ) // only POs
        c.slack = std::min( c.slack, int32_t(_depth - _levels[oi.c] - num_splitter_levels( oi.c )) );
      else if ( fanout_info.size() == 1 ) // single fanout
        c.slack = std::min( c.slack, int32_t(fanout_info.front().relative_depth - 1) );
      else
        c.slack = std::min( c.slack, int32_t(_levels[oi.o] - _levels[oi.c] - 2) );
    }

    if ( c.benefits > 0 && c.slack > 0 )
    {
      count_buffers();
      bool legal = true;
      auto buffers_before = num_buffers();
      //print_chunk( c );
      for ( auto m : c.members )
        _levels[m] += c.slack;
      for ( auto m : c.members )
      {
        legal &= update_fanout_info<true>( m );
        if (!legal) break;
      }
      if ( legal )
      {
        for ( auto ii : c.input_interfaces )
          update_fanout_info( ii.o );
      }
      
      count_buffers();
      if ( !legal || num_buffers() >= buffers_before )
      {
        /* UNDO */
        for ( auto m : c.members )
          _levels[m] -= c.slack;
        for ( auto m : c.members )
          update_fanout_info( m );
        for ( auto ii : c.input_interfaces )
          update_fanout_info( ii.o );
        return false;
      }

      //mark_nodes();
      start_id = _ntk.trav_id();

      //std::cout << "predicted gain = (interfaces)" << c.benefits << " * (slack)" << c.slack << "; after moving chunk, buffers = " << num_buffers() << "\n";
      return true;
    }
    else
    {
      return false;
    }
  }
#pragma endregion

#pragma region Printing
  void print_graph() const
  {
    std::vector<std::vector<node>> nodes_by_level( depth() + 1 );
    _ntk.foreach_gate( [&]( auto const& n ){
      nodes_by_level[level(n)].emplace_back( n );
    });
    for ( auto l = depth(); l > 0; --l )
    {
      std::cout << "level " << std::setw(2) << l << ": ";
      for ( auto n : nodes_by_level[l] )
        std::cout << std::setw(3) << n << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
  }

  void print_fanout_infos( node const& n ) const
  {
    auto const& fo_infos = _fanouts[n];
    for ( auto it = fo_infos.rbegin(); it != fo_infos.rend(); ++it )
    {
      std::cout << "rd " << it->relative_depth << ", gates = ";
      if ( it->fanouts.size() )
      {
        std::cout << "{ ";
        for ( auto it2 = it->fanouts.begin(); it2 != it->fanouts.end(); ++it2 )
          std::cout << (*it2) << " ";
        std::cout << "}";
      }
      else
      {
        std::cout << "{}";
      }
      std::cout << ", #edges = " << it->num_edges << "\n";
    }
    std::cout << "\n";
  }

  void print_chunk( chunk const& c ) const
  {
    std::cout << "===== chunk ID = " << c.id << " =====\n";
    std::cout << "== purpose: " << (c.purpose == UP ? "UP" : "DOWN") << " ";
    if ( c.benefits > 0 )
      std::cout << "[GOOD! gain = " << c.slack << " * " << c.benefits << "]\n";
    else
      std::cout << "[BAD]\n";
    std::cout << "== members: \n";
    for ( auto const& n : c.members )
    {
      std::cout << "== " << node_info( n ) << ", fanins: ";
      _ntk.foreach_fanin( n, [&]( auto const fi ){
        std::cout << node_info( _ntk.get_node( fi ) ) << ", ";
      });
      std::cout << "\n"; 
      //print_fanout_infos( n );
    }
    std::cout << "== IIs: ";
    for ( auto const& ii : c.input_interfaces )
      std::cout << "{" << ii.o << " -> " << ii.c << "} ";
    std::cout << "\n== OIs: ";
    for ( auto const& oi : c.output_interfaces )
      std::cout << "{" << oi.c << " -> " << oi.o << "} ";
    std::cout << "\n";
    std::cout << "=========================\n";
  }

  std::string node_info( node const& n ) const
  {
    return std::to_string( n ) + " @" + std::to_string( _levels[n] );
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
  buffer_insertion_params const _ps;
  bool outdated{true};

  node_map<uint32_t, Ntk> _levels;
  uint32_t _depth{0u};
  node_map<fanouts_by_level, Ntk> _fanouts;
  node_map<uint32_t, Ntk> _external_ref_count;
  node_map<uint32_t, Ntk> _buffers;

  uint32_t start_id;
}; /* buffer_insertion */

namespace detail
{

template<class Ntk>
void lift_fanin_buffers( Ntk& d, typename Ntk::node const& n )
{
  d.foreach_fanin( n, [&]( auto const& fi ) {
    auto ni = d.get_node( fi );
    uint32_t diff = d.level( n ) - d.level( ni ) - 1;
    if ( diff != 0 && ( d.is_buf( ni ) || d.is_pi( ni ) ) )
    {
      d.set_level( ni, d.level( ni ) + diff );
      lift_fanin_buffers( d, ni );
    }
  } );
}

} // namespace detail

/*! \brief Verify a buffered network according to AQFP assumptions.
 * 
 * \param ntk Buffered network
 * \param ps AQFP constraints
 * \return Whether `ntk` is path-balanced and properly-branched
 */
template<class Ntk>
bool verify_aqfp_buffer( Ntk const& ntk, aqfp_assumptions const& ps )
{
  static_assert( is_buffered_network_type_v<Ntk>, "Ntk is not a buffered network" );
  static_assert( has_is_buf_v<Ntk>, "Ntk does not implement the is_buf method" );
  bool legal = true;

  /* fanout branching */
  ntk.foreach_node( [&]( auto const& n ) {
    if ( ntk.is_constant( n ) )
      return true;
    if ( !ps.branch_pis && ntk.is_pi( n ) )
      return true;

    if ( ntk.is_buf( n ) )
      legal &= ( ntk.fanout_size( n ) <= ps.splitter_capacity );
    else /* logic gate */
      legal &= ( ntk.fanout_size( n ) <= 1 );

    return true;
  } );

  /* compute levels */
  depth_view d{ntk};

  /* adjust PI and their buffers */
  if ( !ps.balance_pis )
  {
    ntk.foreach_gate( [&]( auto const& n ) {
      detail::lift_fanin_buffers( d, n );
    } );
    if ( ps.balance_pos )
    {
      ntk.foreach_po( [&]( auto const& f ) {
        auto n = ntk.get_node( f );
        if ( ntk.is_buf( n ) && d.level( n ) != d.depth() )
        {
          d.set_level( n, d.depth() );
          detail::lift_fanin_buffers( d, n );
        }
      } );
    }
  }

  /* path balancing */
  ntk.foreach_node( [&]( auto const& n ) {
    ntk.foreach_fanin( n, [&]( auto const& fi ) {
      auto ni = ntk.get_node( fi );
      if ( !ntk.is_constant( ni ) && ( ps.balance_pis || !ntk.is_pi( ni ) ) )
        legal &= ( d.level( ni ) == d.level( n ) - 1 );
    } );
  } );

  if ( ps.balance_pos )
  {
    ntk.foreach_po( [&]( auto const& f ) {
      auto n = ntk.get_node( f );
      if ( !ntk.is_constant( n ) && ( ps.balance_pis || !ntk.is_pi( n ) ) )
        legal &= ( d.level( n ) == d.depth() );
    } );
  }

  return legal;
}

} // namespace mockturtle