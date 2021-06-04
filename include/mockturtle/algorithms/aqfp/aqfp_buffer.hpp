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
#include "mockturtle/networks/mig.hpp"

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
 * - optimize buffer count by adjusting the level assignment (`ASAP`, `ALAP`...)
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      mig_network mig = ...
      aqfp_buffer bufcnt( mig );
      bufcnt.ALAP();
      bufcnt.count_buffers();
      std::cout << bufcnt.num_buffers() << std::endl;
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

    if constexpr ( !std::is_same<typename Ntk::base_type, mig_network>::value )
    {
      std::cerr << "[w] base_type of Ntk is not mig_network.\n";
    }

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

    if ( num_splitter_levels( n ) == 0u )
    {
      /* single fanout */
      if ( _ntk.fanout_size( n ) > 0u )
      {
        assert( _ntk.fanout_size( n ) == 1u );
        if ( _ntk.is_pi( n ) )
        {
          if ( _external_ref_count[n] > 0u ) /* PI -- PO */
          {
            return _ps.balance_pis && _ps.balance_pos ? _depth : 0u;
          }
          else /* PI -- gate */
          {
            assert( fo_infos.size() == 1u );
            return _ps.balance_pis ? fo_infos.front().relative_depth - 1u : 0u;
          } 
        }
        else if ( _external_ref_count[n] > 0u ) /* gate -- PO */
        {
          return _ps.balance_pos ? _depth - _levels[n] : 0u;
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
      assert( !_ps.balance_pos && _ntk.fanout_size( n ) == _external_ref_count[n] );
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

    if ( !_ps.balance_pis && _ntk.is_pi( n ) ) /* only branch PIs, but don't balance them */
    {
      /* remove the lowest balancing buffers, if any */
      it = fo_infos.begin();
      ++it;
      count -= it->relative_depth - fo_infos.front().relative_depth - 1;
    }

    if ( !_ps.balance_pos && _external_ref_count[n] > 0u )
    {
      auto slots = count * ( _ps.splitter_capacity - 1 ) + 1;
      int32_t needed = _ntk.fanout_size( n ) - slots;
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

  /* (Upper bound on) the additional depth caused by a balanced splitter tree at the output of node `n`. */
  uint32_t num_splitter_levels ( node const& n ) const
  {
    assert( n < _ntk.size() );
    return std::ceil( std::log( _ntk.fanout_size( n ) ) / std::log( _ps.splitter_capacity ) );
  }
#pragma endregion

private:
#pragma region Update fanout info
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
      return;
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
      if ( !_ntk.is_constant( n ) && !_ntk.is_pi( n ) && _ntk.visited( n ) != _ntk.trav_id() )
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

private:
#pragma region Chunked movement: data structures
  enum mobility
  {
    FREE = 0,
    UPPER_BOUNDED,
    LOWER_BOUNDED,
    FIXED,
    IGNORED /* PIs and constant */
  };

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
#pragma endregion

public:
#pragma region Chunked movement: top level
  bool optimize()
  {
    if ( outdated )
    {
      update_fanout_info();
    }
    mark_nodes();

    bool updated = false;
    start_id = _ntk.trav_id();

    _ntk.foreach_node( [&]( auto const& n ){
      if ( is_ignored( n ) || is_fixed( n ) || _ntk.visited( n ) > start_id /* belongs to a chunk */ )
      {
        return true;
      }

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

    count_buffers(); // just to be sure; shouldn't be needed in theory
    std::cout << "[i] after one pass: " << num_buffers() << "\n";
    return updated;
  }
#pragma endregion

private:
#pragma region Chunked movement: mark nodes
  void mark_nodes()
  {
    /* reset values */
    _ntk.foreach_node( [&]( auto const& n ){
      _ntk.set_value( n, FREE );
    });

    /* PIs and constant */
    _ntk.set_value( _ntk.get_node( _ntk.get_constant( false ) ), IGNORED );
    if ( _ps.balance_pis ) // branch and balance
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        _ntk.set_value( n, FIXED );
      });
    }
    else if ( !_ps.branch_pis ) // neither branch nor balance
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        _ntk.set_value( n, IGNORED );
      });

      /* first layer nodes */
      _ntk.foreach_gate( [&]( auto const& n ){
        /* assume topological order */
        bool to_mark = true;
        _ntk.foreach_fanin( n, [&]( auto const& fi ) {
          if ( _ntk.value( _ntk.get_node( fi ) ) != IGNORED )
          {
            to_mark = false;
            return false;
          }
          return true;
        });
        if ( to_mark )
        {
          _ntk.set_value( n, FIXED );
          mark_as_lower_bounded( n );
        }
      });
    }
    else // branch but not balance PI
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        mark_as_lower_bounded( n );
      });
    }

    if ( _ps.balance_pos )
    {
      /* PO nodes */
      _ntk.foreach_po( [&]( auto const& f ){
        mark_as_upper_bounded( _ntk.get_node( f ) );
      });
    }
  }

  void mark_as_lower_bounded( node const& n )
  {
    if ( _ntk.value( n ) == FREE )
    {
      _ntk.set_value( n, LOWER_BOUNDED );
    }
    else if ( _ntk.value( n ) == UPPER_BOUNDED )
    {
      _ntk.set_value( n, FIXED );
    }

    auto const& fo_infos = _fanouts[n];
    if ( fo_infos.size() == 1u )
    {
      if ( fo_infos.front().relative_depth == 1u && fo_infos.front().fanouts.size() )
      {
        mark_as_lower_bounded( fo_infos.front().fanouts.front() );
      }
      return;
    }
    auto it = fo_infos.begin();
    ++it;
    if ( it->relative_depth == 2u )
    {
      for ( auto it2 = it->fanouts.begin(); it2 != it->fanouts.end(); ++it2 )
      {
        mark_as_lower_bounded( *it2 );
      }
    }
  }

  void mark_as_upper_bounded( node const& n )
  {
    if ( _ntk.value( n ) == FREE )
    {
      _ntk.set_value( n, UPPER_BOUNDED );
    }
    else if ( _ntk.value( n ) == LOWER_BOUNDED )
    {
      _ntk.set_value( n, FIXED );
    }

    _ntk.foreach_fanin( n, [&]( auto const& fi ) {
      auto const ni = _ntk.get_node( fi );
      if ( ( _ntk.fanout_size( ni ) == 1u && _levels[ni] == _levels[n] - 1 ) ||
           ( _ntk.fanout_size( ni ) > 1u && _levels[ni] == _levels[n] - 2 ) )
      {
        mark_as_upper_bounded( ni );
      }
    });
  }

  bool is_upper_bounded( node const& n ) const
  {
    //return false;
    return _ntk.value( n ) == FIXED || _ntk.value( n ) == UPPER_BOUNDED;
  }

  bool is_lower_bounded( node const& n ) const
  {
    //return false;
    return _ntk.value( n ) == FIXED || _ntk.value( n ) == LOWER_BOUNDED;
  }

  bool is_ignored( node const& n ) const
  {
    return _ntk.value( n ) == IGNORED;
  }

  bool is_fixed( node const& n ) const
  {
    //return false;
    return _ntk.value( n ) == FIXED;
  }
#pragma endregion

#pragma region Chunked movement: forming chunks
  void recruit( node const& n, chunk& c )
  {
    if ( _ntk.visited( n ) == c.id )
      return;

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
#pragma endregion

#pragma region Chunked movement: analyze and move chunks
  bool analyze_chunk_down( chunk c )
  {
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
      //std::cout << "This chunk seems to be good:\n"; print_chunk(c);
      bool legal = true;
      auto buffers_before = num_buffers();
      for ( auto m : c.members )
        _levels[m] -= c.slack;
      for ( auto m : c.members )
        update_fanout_info( m );
      for ( auto ii : c.input_interfaces )
        legal &= update_fanout_info<true>( ii.o );
      
      if ( legal ) count_buffers();
      if ( !legal || num_buffers() >= buffers_before )
      {
        //if ( !legal ) std::cout << "...but it's not legal\n";
        //else std::cout << "...but it results in more buffers\n";
        /* UNDO */
        for ( auto m : c.members )
          _levels[m] += c.slack;
        for ( auto m : c.members )
          update_fanout_info( m );
        for ( auto ii : c.input_interfaces )
          update_fanout_info( ii.o );
        count_buffers();
        return false;
      }

      mark_nodes();
      start_id = _ntk.trav_id();

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
    while ( it != fanout_info.end() && it->num_edges == _ps.splitter_capacity )
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
      bool legal = true;
      auto buffers_before = num_buffers();
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
        count_buffers();
      }

      if ( !legal || num_buffers() >= buffers_before )
      {
        /* UNDO */
        for ( auto m : c.members )
          _levels[m] -= c.slack;
        for ( auto m : c.members )
          update_fanout_info( m );
        for ( auto ii : c.input_interfaces )
          update_fanout_info( ii.o );
        count_buffers();
        return false;
      }

      mark_nodes();
      start_id = _ntk.trav_id();

      return true;
    }
    else
    {
      return false;
    }
  }
#pragma endregion

public:
#pragma region Printing methods
  void print_graph() const
  {
    std::vector<std::vector<node>> nodes_by_level( _depth + 1 );
    _ntk.foreach_node( [&]( auto const& n ){
      nodes_by_level[_levels[n]].emplace_back( n );
    });
    for ( int l = _depth; l >= 0; --l )
    {
      std::cout << "level " << std::setw(2) << l << ": ";
      for ( auto n : nodes_by_level[l] )
        std::cout << n << ", ";
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

  std::string mobility_str( node const& n ) const
  {
    switch ( _ntk.value( n ) )
    {
      case FREE: return " FREE";
      case UPPER_BOUNDED: return " UPPER_BOUNDED";
      case LOWER_BOUNDED: return " LOWER_BOUNDED";
      case FIXED: return " FIXED";
      case IGNORED: return " PI/CONST";
      default: return " UNKNOWN";
    }
  }

  std::string node_info( node const& n ) const
  {
    return std::to_string( n ) + " @" + std::to_string( _levels[n] ) + mobility_str( n );
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

  uint32_t start_id;
}; /* aqfp_buffer */

} // namespace mockturtle