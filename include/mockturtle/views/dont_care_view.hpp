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

#include <kitty/dynamic_truth_table.hpp>
#include <kitty/cube.hpp>

#include <vector>
#include <set>
#include <utility>

namespace mockturtle
{

namespace detail
{

/*! \brief A manager to classify bit-strings into equivalence classes
 *
 * This data structure holds and manages equivalence classes of
 * bit-strings of the same length (i.e. complete or partial binary
 * truth tables).
 * The three properties of an equivalence relation are maintained:
 * - Reflexive (x = x)
 * - Symmetric (if x = y then y = x)
 * - Transitive (if x = y and y = z then x = z)
 *
 * In the current implementation, the big-string length may not be
 * larger than 31.
 */
class equivalence_classes_mgr
{
public:
  equivalence_classes_mgr() {}

  equivalence_classes_mgr( uint32_t num_bits ) : _num_bits( num_bits )
  {
    assert( num_bits < 32 );
    uint32_t max_val = 1u << num_bits;
    _classes.resize( max_val );
    for ( uint32_t i = 0u; i < max_val; ++i )
    {
      _classes[i] = i;
    }
  }

  equivalence_classes_mgr& operator=( equivalence_classes_mgr const& other )
  {
    _num_bits = other._num_bits;
    _classes = other._classes;
    return *this;
  }

  void set_equivalent( uint32_t const& a, uint32_t const& b )
  {
    uint32_t repr_class = _classes.at( a );
    uint32_t to_be_replaced = _classes.at( b );
    for ( auto i = 0u; i < _classes.size(); ++i )
    {
      if ( _classes[i] == to_be_replaced )
        _classes[i] = repr_class;
    }
  }

  /*! \brief Set two bit strings to be equivalent. */
  void set_equivalent( std::vector<bool> const& a, std::vector<bool> const& b )
  {
    set_equivalent( vector_bool_to_uint32( a ), vector_bool_to_uint32( b ) );
  }

  /*! \brief Check equivalence of fully-assigned bit-strings */
  bool are_equivalent( uint32_t const& a, uint32_t const& b ) const
  {
    return _classes.at( a ) == _classes.at( b );
  }

  /*! \brief Check equivalence of fully-assigned bit-strings */
  bool are_equivalent( std::vector<bool> const& a, std::vector<bool> const& b ) const
  {
    return are_equivalent( vector_bool_to_uint32( a ), vector_bool_to_uint32( b ) );
  }

  /*! \brief Check equivalence of partially-assigned bit-strings
   *
   * The don't-care bit positions in the two cubes should be the same.
   * Two cubes are equivalent if for all possible assignments to the
   * don't-care bits, they are always equivalent.
   */
  bool are_equivalent( kitty::cube const& a, kitty::cube const& b ) const
  {
    assert( a._mask == b._mask && "The don't-care bit positions in the two cubes should be the same." );
    return are_equivalent_rec( a, b, 0 );
  }

  uint32_t num_classes() const
  {
    std::set<uint32_t> unique_ids;
    for ( auto const& id : _classes )
    {
      unique_ids.insert( id );
    }
    return unique_ids.size();
  }

  template<class Fn>
  void foreach_class( Fn&& fn ) const
  {
    std::unordered_map<uint32_t, std::vector<uint32_t>> class2pats;
    for ( auto pat = 0u; pat < _classes.size(); ++pat )
    {
      auto const& id = _classes[pat];
      class2pats.try_emplace( id );
      class2pats[id].emplace_back( pat );
    }
    
    for ( auto const& p : class2pats )
    {
      if ( !fn( p.second ) )
        break;
    }
  }

private:
  bool are_equivalent_rec( kitty::cube const& a, kitty::cube const& b, uint32_t i ) const
  {
    if ( i == _num_bits )
    {
      return are_equivalent( cube_to_uint32( a ), cube_to_uint32( b ) );
    }

    if ( a.get_mask( i ) )
    {
      return are_equivalent_rec( a, b, i + 1 );
    }
    else
    {
      kitty::cube a0 = a;
      a0.set_mask( i );
      kitty::cube b0 = b;
      b0.set_mask( i );
      if ( !are_equivalent_rec( a0, b0, i + 1 ) )
        return false;
      a0.set_bit( i );
      b0.set_bit( i );
      return are_equivalent_rec( a0, b0, i + 1 );
    }
  }

  uint32_t vector_bool_to_uint32( std::vector<bool> const& vec ) const
  {
    assert( vec.size() == _num_bits );
    uint32_t res{0u};
    for ( auto i = 0u; i < _num_bits; ++i )
    {
      if ( vec[i] )
        res |= 1u << i;
    }
    return res;
  }

  uint32_t cube_to_uint32( cube const& c ) const
  {
    assert( c.num_literals() == _num_bits ); // fully assigned
    return c._bits;
  }

private:
  uint32_t _num_bits;
  std::vector<uint32_t> _classes;
}; // equivalence_classes_mgr

} // namespace detail

template<class Ntk, class OecMgr = detail::equivalence_classes_mgr>
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
