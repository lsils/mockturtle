/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
  \file dont_care_view.hpp
  \brief Implements methods to store external don't-cares

  \author Siang-Yun Lee
*/

#pragma once

#include "../traits.hpp"
#include "../algorithms/simulation.hpp"
#include "../algorithms/cnf.hpp"
#include "../utils/node_map.hpp"
#include "../utils/window_utils.hpp"
#include "../views/color_view.hpp"
#include "../views/window_view.hpp"

#include <bill/sat/interface/common.hpp>
#include <kitty/cube.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/cube.hpp>
#include <kitty/positional_cube.hpp>
#include <kitty/oec_manager.hpp>

#include <vector>
#include <set>
#include <utility>

namespace mockturtle
{

template<class Ntk, class OecMgr = kitty::oec_manager<false>>
class dont_care_view : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  // no EXDC
  dont_care_view( Ntk const& ntk )
    : Ntk( ntk ), _exoec( ntk.num_pos() )
  {
    for ( auto i = 0u; i < ntk.num_pis(); ++i )
    {
      _excdc.create_pi();
    }
    _excdc.create_po( _excdc.get_constant( false ) );
  }

  // only EXCDC
  dont_care_view( Ntk const& ntk, Ntk const& cdc_ntk )
    : Ntk( ntk ), _excdc( cdc_ntk ), _exoec( ntk.num_pos() )
  {
    assert( cdc_ntk.num_pis() == ntk.num_pis() );
    assert( cdc_ntk.num_pos() == 1 );
  }

  // copy constructor
  dont_care_view( dont_care_view<Ntk> const& ntk )
    : Ntk( ntk ), _excdc( ntk._excdc ), _exoec( ntk._exoec )
  {
  }

  // assignment operator
  dont_care_view<Ntk>& operator=( dont_care_view<Ntk> const& other )
  {
    Ntk::operator=( other );
    _excdc = other._excdc;
    _exoec = other._exoec;
    return *this;
  }

  bool pattern_is_EXCDC( std::vector<bool> const& pattern ) const
  {
    assert( pattern.size() == this->num_pis() );

    default_simulator<bool> sim( pattern );
    auto const vals = simulate<bool>( _excdc, sim );
    return vals[0];
  }

  template<typename solver_t>
  void add_EXCDC_clauses( solver_t& solver ) const
  {
    using add_clause_fn_t = std::function<void( std::vector<bill::lit_type> const& )>;
    add_clause_fn_t const add_clause_fn = [&]( auto const& clause ) { solver.add_clause( clause ); };

    // topological order of the gates in _excdc is assumed
    node_map<bill::lit_type, Ntk> cdc_lits( _excdc );
    cdc_lits[_excdc.get_constant( false )] = bill::lit_type( 0, bill::lit_type::polarities::positive );
    if ( _excdc.get_node( _excdc.get_constant( false ) ) != _excdc.get_node( _excdc.get_constant( true ) ) )
    {
      cdc_lits[_excdc.get_constant( true )] = bill::lit_type( 0, bill::lit_type::polarities::negative );
    }
    _excdc.foreach_pi( [&]( auto const& n, auto i ) {
      cdc_lits[n] = bill::lit_type( i + 1, bill::lit_type::polarities::positive );
    } );

    _excdc.foreach_gate( [&]( auto const& n ){
      cdc_lits[n] = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
    });

    auto out_lits = generate_cnf<Ntk, bill::lit_type>( _excdc, add_clause_fn, cdc_lits );
    solver.add_clause( {~out_lits[0]} );
  }

  // ito = in terms of
  void add_EXODC_ito_pos( kitty::cube const& cond, uint32_t po_id )
  {
    cond.foreach_minterm( this->num_pos(), [&]( kitty::cube const& c ){
      assert( c.num_literals() == this->num_pos() );
      if ( c.get_bit( po_id ) ) return true;
      kitty::cube c2 = c;
      c2.set_bit( po_id );
      _exoec.set_equivalent( c._bits, c2._bits );
      return true;
    });
  }

  // full assignment
  void add_EXOEC_pair( std::vector<bool> const& pat1, std::vector<bool> const& pat2 )
  {
    _exoec.set_equivalent( pat1, pat2 );
  }

  // partial assignment
  // cube or positional_cube ?
  void add_EXOEC_pair( kitty::positional_cube const& c1, kitty::positional_cube const& c2 )
  {
    _exoec.set_equivalent( c1, c2 );
  }

/*
  bool is_in_relation( std::vector<bool> const& in_pattern, std::vector<bool> const& out_pattern ) const
  {
    assert( in_pattern.size() == this->num_pis() );
    if ( pattern_is_EXCDC( in_pattern ) )
      return true;

    assert( out_pattern.size() == this->num_pos() );
    default_simulator<bool> sim( in_pattern );
    auto const vals = simulate<bool>( *this, sim );
    for ( auto o = 0u; o < this->num_pos(); ++o )
    {
      if ( vals[o] ^ out_pattern[o] )
      {
        // if sim_odco[o] continue;
        // if sim_odci[o] continue;
        return false;
      }
    }
    return true;
  }

  OecMgr propagate_OECs( std::vector<node> const& cut ) const
  {
    OecMgr cut_oec( cut.size() );

    std::vector<node> const inner = collect_supported( color_view{*this}, cut );
    std::vector<signal> outputs;
    std::vector<uint32_t> const supported_pos, influenced_pos;
    this->incr_trav_id();
    for ( auto const& n : inner )
      this->set_visited( n, this->trav_id() );
    this->foreach_po( [&]( auto f, auto i ) {
      if ( this->visited( this->get_node( f ) ) == this->trav_id() )
      {
        supported_pos.emplace_back( i );
        outputs.emplace_back( f );
      }
      else
      {
        influenced_pos.emplace_back( i );
      }
    } );
    // TODO: collect influenced_pos. dont_care_pos = all pos - supported_pos - influenced_pos

    window_view win( *this, cut, outputs, inner );
    default_simulator<kitty::dynamic_truth_table> sim( cut.size() );
    auto const tts = simulate<kitty::dynamic_truth_table>( win, sim );

    uint32_t num_patterns = 1u << cut.size();
    std::vector<bool> pat1( this->num_pos() ), pat2( this->num_pos() );
    for ( auto i = 0u; i < num_patterns; ++i )
    {
      // collect partial PO minterm
      auto it = supported_pos.begin();
      for ( auto o = 0u; o < supported_pos.size(); ++o )
        pat1[*it++] = tts[o].get_bit( i );

      for ( auto j = i + 1; j < num_patterns; ++j )
      {
        // collect partial PO minterm
        auto it = supported_pos.begin();
        for ( auto o = 0u; o < supported_pos.size(); ++o )
          pat2[*it++] = tts[o].get_bit( j );

        bool to_merge = true;
        // for each possible values at influenced_pos
        for ( ... )
        {
          to_merge &= _exoec.are_equivalent( pat1, pat2 );
        }
        if ( to_merge )
        {
          cut_oec.set_equivalent( i, j );
        }
      }
    }

    return cut_oec;
  }
*/

  // full assignment
  bool are_observability_equivalent( std::vector<bool> const& pat1, std::vector<bool> const& pat2 ) const
  {
    return _exoec.are_equivalent( pat1, pat2 );
  }

  // partial assignment
  bool are_observability_equivalent( kitty::cube const& pat1, kitty::cube const& pat2 ) const
  {
    return _exoec.are_equivalent( pat1, pat2 );
  }

  void build_oec_network()
  {
    std::vector<signal> pos1, pos2;
    for ( auto i = 0u; i < this->num_pos(); ++i )
    {
      pos1.emplace_back( _are_oe.create_pi() );
    }
    for ( auto i = 0u; i < this->num_pos(); ++i )
    {
      pos2.emplace_back( _are_oe.create_pi() );
    }

    std::vector<signal> are_both_in_class_i;
    std::vector<signal> is_in_class1, is_in_class2;
    std::vector<signal> ins1, ins2;
    ins1.resize( this->num_pos() );
    ins2.resize( this->num_pos() );
    _exoec.foreach_class( [&]( std::vector<uint32_t> const& pats ){
      is_in_class1.clear();
      is_in_class2.clear();
      for ( uint32_t pat : pats )
      {
        for ( auto i = 0u; i < this->num_pos(); ++i )
        {
          ins1[i] = ( pat & 0x1 ) ? pos1[i] : !pos1[i];
          ins2[i] = ( pat & 0x1 ) ? pos2[i] : !pos2[i];
          pat >>= 1;
        }
        is_in_class1.emplace_back( _are_oe.create_nary_and( ins1 ) );
        is_in_class2.emplace_back( _are_oe.create_nary_and( ins2 ) );
      }
      are_both_in_class_i.emplace_back( _are_oe.create_and( _are_oe.create_nary_or( is_in_class1 ), _are_oe.create_nary_or( is_in_class2 ) ) );
      return true;
    });
    _are_oe.create_po( _are_oe.create_nary_or( are_both_in_class_i ) );
  }

  template<typename solver_t>
  void add_EXOEC_clauses( solver_t& solver, std::vector<bill::lit_type> const& po_lits ) const
  {
    assert( po_lits.size() == this->num_pos() );
    _po_lits = po_lits;

    using add_clause_fn_t = std::function<void( std::vector<bill::lit_type> const& )>;
    add_clause_fn_t add_clause_fn = [&]( auto const& clause ) { solver.add_clause( clause ); };

    /* miter */
    std::vector<bill::lit_type> xors;
    for ( auto i = 0u; i < this->num_pos(); ++i )
    {
      _po_lits_link.emplace_back( solver.add_variable(), bill::lit_type::polarities::positive );
      auto nlit = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
      detail::on_xor<add_clause_fn_t>( nlit, _po_lits[i], _po_lits_link[i], [&]( auto const& clause ) { solver.add_clause( clause ); } );
      xors.emplace_back( nlit );
    }
    solver.add_clause( xors );

    /* OEC */
    assert( _are_oe.num_pis() == this->num_pos() * 2 && _are_oe.num_pos() == 1 );
    node_map<bill::lit_type, Ntk> oe_lits( _are_oe );
    oe_lits[_are_oe.get_constant( false )] = bill::lit_type( 0, bill::lit_type::polarities::positive );
    if ( _are_oe.get_node( _are_oe.get_constant( false ) ) != _are_oe.get_node( _are_oe.get_constant( true ) ) )
    {
      oe_lits[_are_oe.get_constant( true )] = bill::lit_type( 0, bill::lit_type::polarities::negative );
    }
    _are_oe.foreach_pi( [&]( auto const& n, auto i ) {
      oe_lits[n] = i < this->num_pos() ? _po_lits[i] : _po_lits_link[i - this->num_pos()];
    } );

    _are_oe.foreach_gate( [&]( auto const& n ){
      oe_lits[n] = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
    });

    auto out_lits = generate_cnf<Ntk, bill::lit_type>( _are_oe, add_clause_fn, oe_lits );
    solver.add_clause( {~out_lits[0]} );
  }

  template<typename solver_t>
  bill::lit_type add_EXOEC_linking_clauses( solver_t& solver, std::vector<bill::lit_type> const& dup_po_lits ) const
  {
    assert( dup_po_lits.size() == this->num_pos() );
    auto assump = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
    for ( auto i = 0u; i < this->num_pos(); ++i )
    {
      solver.add_clause( {~assump, ~_po_lits_link[i], dup_po_lits[i]} );
      solver.add_clause( {~assump, _po_lits_link[i], ~dup_po_lits[i]} );
    }
    return assump;
  }

private:
  Ntk _excdc;
  OecMgr _exoec;
  Ntk _are_oe;
  mutable std::vector<bill::lit_type> _po_lits, _po_lits_link;
}; /* dont_care_view */

template<class T>
dont_care_view( T const&, T const& ) -> dont_care_view<T>;

} // namespace mockturtle
