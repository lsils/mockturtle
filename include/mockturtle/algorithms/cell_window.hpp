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
  \file cell_window.hpp
  \brief Windowing in mapped network

  \author Mathias Soeken
*/

#pragma once

#include <optional>
#include <unordered_set>
#include <vector>

#include "../networks/detail/foreach.hpp"
#include "../traits.hpp"
#include "../utils/algorithm.hpp"
#include "../utils/node_map.hpp"
#include "../utils/stopwatch.hpp"

#include <sparsepp/spp.h>

namespace mockturtle
{

template<class Ntk>
class cell_window : public Ntk
{
public:
  cell_window( Ntk const& ntk, uint32_t max_gates = 64 )
      : Ntk( ntk ),
        _cell_refs( ntk ),
        _cell_parents( ntk ),
        _max_gates( max_gates )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_is_cell_root_v<Ntk>, "Ntk does not implement the is_cell_root method" );
    static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
    static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
    static_assert( has_foreach_cell_fanin_v<Ntk>, "Ntk does not implement the foreach_cell_fanin method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_incr_trav_id_v<Ntk>, "Ntk does not implement the incr_trav_id method" );
    static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
    static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );

    if ( ntk.get_node( ntk.get_constant( true ) ) != ntk.get_node( ntk.get_constant( false ) ) )
    {
      _num_constants++;
    }

    _nodes.reserve( _max_gates >> 1 );
    _gates.reserve( _max_gates );
    init_cell_refs();
  }

  void compute_window_for( node<Ntk> const& pivot )
  {
    //print_time<> pt;
    assert( Ntk::is_cell_root( pivot ) );

    // reset old window
    _nodes.clear();
    _gates.clear();

    std::vector<node<Ntk>> gates;
    gates.reserve( _max_gates );
    collect_mffc( pivot, gates );
    add_node( pivot, gates );

    if ( gates.size() > _max_gates )
    {
      assert( false );
    }

    std::optional<node<Ntk>> next;
    while ( ( next = find_next_pivot() ) )
    {
      gates.clear();
      collect_mffc( *next, gates );

      if ( _gates.size() + gates.size() > _max_gates )
      {
        break;
      }
      add_node( *next, gates );
    }

    find_leaves_and_roots();
  }

  uint32_t num_pis() const
  {
    return _leaves.size();
  }

  uint32_t num_pos() const
  {
    return _roots.size();
  }

  uint32_t num_gates() const
  {
    return _gates.size();
  }

  uint32_t num_cells() const
  {
    return _nodes.size();
  }

  uint32_t size() const
  {
    return _num_constants + _leaves.size() + _gates.size();
  }

  bool is_pi( node<Ntk> const& n ) const
  {
    return _leaves.count( n );
  }

  bool is_cell_root( node<Ntk> const& n ) const
  {
    return _nodes.count( n );
  }

  bool has_mapping() const
  {
    // TODO
    return true;
  }

  bool clear_mapping() const
  {
    // TODO
  }

  template<typename Fn>
  void foreach_pi( Fn&& fn ) const
  {
    detail::foreach_element( _leaves.begin(), _leaves.end(), fn );
  }

  template<typename Fn>
  void foreach_po( Fn&& fn ) const
  {
    detail::foreach_element( _roots.begin(), _roots.end(), fn );
  }

  template<typename Fn>
  void foreach_gate( Fn&& fn ) const
  {
    detail::foreach_element( _gates.begin(), _gates.end(), fn );
  }

private:
  void init_cell_refs()
  {
    /* initial ref counts for cells */
    Ntk::foreach_gate( [&]( auto const& n ) {
      if ( Ntk::is_cell_root( n ) )
      {
        Ntk::foreach_cell_fanin( n, [&]( auto const& n2 ) {
          _cell_refs[n2]++;
          _cell_parents[n2].push_back( n );
        } );
    } } );
    Ntk::foreach_po( [&]( auto const& f ) {
      _cell_refs[f]++;
    } );
  }

  void collect_mffc( node<Ntk> const& pivot, std::vector<node<Ntk>>& gates )
  {
    Ntk::incr_trav_id();
    collect_gates( pivot, gates );
    const auto it = std::remove_if( gates.begin(), gates.end(), [&]( auto const& g ) { return _gates.count( g ); } );
    gates.erase( it, gates.end() );
  }

  void collect_gates( node<Ntk> const& pivot, std::vector<node<Ntk>>& gates )
  {
    assert( !Ntk::is_pi( pivot ) );

    Ntk::set_visited( Ntk::get_node( Ntk::get_constant( false ) ), Ntk::trav_id() );
    Ntk::set_visited( Ntk::get_node( Ntk::get_constant( true ) ), Ntk::trav_id() );

    Ntk::foreach_cell_fanin( pivot, [&]( auto const& n ) {
      Ntk::set_visited( n, Ntk::trav_id() );
    } );

    collect_gates_rec( pivot, gates );
  }

  void collect_gates_rec( node<Ntk> const& n, std::vector<node<Ntk>>& gates )
  {
    if ( Ntk::visited( n ) == Ntk::trav_id() )
      return;
    if ( Ntk::is_constant( n ) || Ntk::is_pi( n ) )
      return;

    Ntk::set_visited( n, Ntk::trav_id() );
    Ntk::foreach_fanin( n, [&]( auto const& f ) {
      collect_gates_rec( Ntk::get_node( f ), gates );
    } );
    gates.push_back( n );
  }

  void add_node( node<Ntk> const& pivot, std::vector<node<Ntk>> const& gates )
  {
    /*std::cout << "add_node(" << pivot << ", { ";
    for ( auto const& g : gates ) {
      std::cout << g << " ";
    }
    std::cout << "})\n";*/
    _nodes.insert( pivot );
    std::copy( gates.begin(), gates.end(), std::insert_iterator( _gates, _gates.begin() ) );
  }

  std::optional<node<Ntk>> find_next_pivot()
  {
    /* deref */
    for ( auto const& n : _nodes )
    {
      Ntk::foreach_cell_fanin( n, [&]( auto const& n2 ) {
        _cell_refs[n2]--;
      } );
    }

    std::vector<node<Ntk>> candidates;
    std::unordered_set<node<Ntk>> inputs;

    do
    {
      for ( auto const& n : _nodes )
      {
        Ntk::foreach_cell_fanin( n, [&]( auto const& n2 ) {
          if ( !_nodes.count( n2 ) && !Ntk::is_pi( n2 ) && !_cell_refs[n2] )
          {
            candidates.push_back( n2 );
            inputs.insert( n2 );
          }
        } );
      }

      if ( !candidates.empty() )
      {
        const auto best = max_element_unary(
            candidates.begin(), candidates.end(),
            [&]( auto const& cand ) {
              auto cnt{0};
              this->foreach_cell_fanin( cand, [&]( auto const& n2 ) {
                cnt += inputs.count( n2 );
              } );
              return cnt;
            },
            -1 );
        candidates[0] = *best;
        break;
      }

      for ( auto const& n : _nodes )
      {
        Ntk::foreach_cell_fanin( n, [&]( auto const& n2 ) {
          if ( !_nodes.count( n2 ) && !Ntk::is_pi( n2 ) )
          {
            candidates.push_back( n2 );
            inputs.insert( n2 );
          }
        } );
      }

      for ( auto const& n : _nodes )
      {
        if ( _cell_refs[n] == 0 )
          continue;
        if ( _cell_refs[n] >= 5 )
          continue;
        if ( _cell_refs[n] == 1 && _cell_parents[n].size() == 1 && !_nodes.count( _cell_parents[n].front() ) )
        {
          candidates.clear();
          candidates.push_back( _cell_parents[n].front() );
          break;
        }
        std::copy_if( _cell_parents[n].begin(), _cell_parents[n].end(),
                      std::back_inserter( candidates ),
                      [&]( auto const& g ) {
                        return !_nodes.count( g );
                      } );
      }

      if ( !candidates.empty() )
      {
        const auto best = max_element_unary(
            candidates.begin(), candidates.end(),
            [&]( auto const& cand ) {
              auto cnt{0};
              this->foreach_cell_fanin( cand, [&]( auto const& n2 ) {
                cnt += inputs.count( n2 );
              } );
              return cnt;
            },
            -1 );
        candidates[0] = *best;
      }
    } while ( false );

    /* ref */
    for ( auto const& n : _nodes )
    {
      Ntk::foreach_cell_fanin( n, [&]( auto const& n2 ) {
        _cell_refs[n2]++;
      } );
    }

    if ( candidates.empty() )
    {
      return std::nullopt;
    }
    else
    {
      return candidates.front();
    }
  }

  void find_leaves_and_roots()
  {
    _leaves.clear();
    for ( auto const& g : _gates )
    {
      Ntk::foreach_fanin( g, [&]( auto const& f ) {
        auto const child = Ntk::get_node( f );
        if ( !_gates.count( child ) )
        {
          _leaves.insert( child );
        }
      } );
    }

    _roots.clear();
    for ( auto const& n : _nodes )
    {
      Ntk::foreach_cell_fanin( n, [&]( auto const& n2 ) {
        _cell_refs[n2]--;
      } );
    }
    for ( auto const& n : _nodes )
    {
      if ( _cell_refs[n] )
      {
        _roots.insert( Ntk::make_signal( n ) );
      }
    }
    for ( auto const& n : _nodes )
    {
      Ntk::foreach_cell_fanin( n, [&]( auto const& n2 ) {
        _cell_refs[n2]++;
      } );
    }
  }

private:
  spp::sparse_hash_set<node<Ntk>> _nodes;   /* cell roots in current window */
  spp::sparse_hash_set<node<Ntk>> _gates;   /* gates in current window */
  spp::sparse_hash_set<node<Ntk>> _leaves;  /* leaves of current window */
  spp::sparse_hash_set<signal<Ntk>> _roots; /* roots of current window */

  node_map<uint32_t, Ntk> _cell_refs;                  /* ref counts for cells */
  node_map<std::vector<node<Ntk>>, Ntk> _cell_parents; /* parent cells */

  uint32_t _num_constants{1u};
  uint32_t _max_gates{};
};

} // namespace mockturtle
