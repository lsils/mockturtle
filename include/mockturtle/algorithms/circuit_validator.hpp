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
  \file circuit_validator.hpp
  \brief Validate potential circuit optimization choices with SAT.

  \author Siang-Yun Lee
*/

#pragma once

#include "../utils/node_map.hpp"
#include "cnf.hpp"
#include <bill/sat/interface/common.hpp>
#include <bill/sat/interface/z3.hpp>
//#include <bill/sat/interface/glucose.hpp>

namespace mockturtle
{

struct validator_params
{
  /*! \brief Whether to consider ODC, and how many levels. 0 = no. -1 = Consider TFO until PO. */
  int odc_levels{0};

  /*! \brief Conflict limit of the SAT solver. */
  uint32_t conflict_limit{1000};

  /*! \brief Whether to randomize counter examples. */
  bool randomize{false};

  /*! \brief Seed for randomized solving. */
  uint32_t random_seed{0};
};

template<class Ntk, bill::solvers Solver = bill::solvers::z3, bool use_bookmark = true, bool use_odc = false>
class circuit_validator
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using add_clause_fn_t = std::function<void( std::vector<bill::lit_type> const& )>;

  enum gate_type 
  {
    AND,
    XOR,
  };

  struct gate
  {
    struct fanin
    {
      uint32_t idx; /* index in the concatenated list of `divs` and `ckt` */
      bool inv{false};
    };

    std::vector<fanin> fanins;
    gate_type type{AND};
  };

  explicit circuit_validator( Ntk const& ntk, validator_params const& ps = {} )
    : ntk( ntk ), ps( ps ), literals( node_literals<Ntk, bill::lit_type>( ntk ) ), cex( ntk.num_pis() )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
    //static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
    static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
    //static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
    //static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
    //static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
    //static_assert( has_value_v<Ntk>, "Ntk does not implement the value method" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
    static_assert( has_is_and_v<Ntk>, "Ntk does not implement the is_and method" );
    static_assert( has_is_xor_v<Ntk>, "Ntk does not implement the is_xor method" );
    
    if constexpr ( use_bookmark )
    {
      static_assert( Solver == bill::solvers::z3 || Solver == bill::solvers::bsat2, "Solver does not support bookmark/rollback" );
    }
    if ( ps.randomize )
    {
      assert( Solver == bill::solvers::z3 || Solver == bill::solvers::bsat2 && "Solver does not support set_random" );
    }
    if constexpr ( use_odc )
    {
      static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
      static_assert( has_visited_v<Ntk>, "Ntk does not implement the visited method" );
      static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
      static_assert( has_foreach_fanout_v<Ntk>, "Ntk does not implement the foreach_fanout method" );
      //static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    }

    restart();
  }

  /* validate with an existing signal in the network */
  std::optional<bool> validate( node const& root, signal const& d )
  {
    return validate( root, lit_not_cond( literals[d], ntk.is_complemented( d ) ) );
  }

  /* validate with a circuit composed of `divs` which are existing nodes in the network */
  std::optional<bool> validate( node const& root, std::vector<node> const& divs, std::vector<gate> const& ckt, bool output_negation = false )
  {
    if constexpr ( use_bookmark )
    {
      solver.bookmark();
    }

    std::vector<bill::lit_type> lits;
    for ( auto n : divs )
    {
      lits.emplace_back( literals[n] );
    }

    for ( auto g : ckt )
    {
      lits.emplace_back( add_tmp_gate( lits, g ) );
    }

    auto const res = validate( root, lit_not_cond( lits.back(), output_negation ) );

    if constexpr ( use_bookmark )
    {
      solver.rollback();
    }

    return res;
  }

  /* validate whether `root` is a constant of `value` */
  std::optional<bool> validate( node const& root, bool value )
  {
    assert( literals[root].variable() != bill::var_type( 0 ) );
    if constexpr ( use_bookmark )
    {
      solver.bookmark();
    }

    std::optional<bool> res;
    if constexpr ( use_odc )
    {
      if ( ps.odc_levels != 0 )
      {
        //std::vector<bill::lit_type> l_fi;
        //ntk.foreach_fanin( root, [&]( auto const& fi ){
        //  l_fi.emplace_back( lit_not_cond( literals[fi], ntk.is_complemented( fi ) ) );
        //});
        //assert( l_fi.size() == 2u );
        //assert( ntk.is_and( root ) || ntk.is_xor( root ) );
        //auto nlit = add_clauses_for_gate( l_fi[0], l_fi[1], std::nullopt, ntk.is_and( root )? AND: XOR );
        res = solve( {build_odc_window( root, ~literals[root] ), lit_not_cond( literals[root], value )} );
      }
      else
      {
        res = solve( {lit_not_cond( literals[root], value )} );
      }
    }
    else 
    {
      res = solve( {lit_not_cond( literals[root], value )} );
    }

    if constexpr ( use_bookmark )
    {
      solver.rollback();
    }
    return res;
  }

  /* add clauses for a new node (created after construction of validator) 
    Note: should be called manually every time or be added to ntk.on_add events
  */
  void add_node( node const& n )
  {
    /* only support AND2 and XOR2 for now */
    assert( ntk.is_and( n ) || ntk.is_xor( n ) );

    std::vector<bill::lit_type> lit_fi;
    ntk.foreach_fanin( n, [&]( const auto& f ){
      lit_fi.emplace_back( lit_not_cond( literals[f], ntk.is_complemented( f ) ) );
    });
    assert( lit_fi.size() == 2u );

    literals.resize();
    literals[n] = add_clauses_for_gate( lit_fi[0], lit_fi[1], std::nullopt, ntk.is_and( n ) ? AND : XOR );
  }

  /* should be called when the function of one or more nodes has been modified (typically when utilizing ODCs) */
  void update()
  {
    restart();
  }

private:
  void restart()
  {
    solver.restart();
    if ( ps.randomize )
    {
      solver.set_random_phase( ps.random_seed );
    }
    
    solver.add_variables( ntk.size() );
    generate_cnf<Ntk, bill::lit_type>( ntk, [&]( auto const& clause ) {
      solver.add_clause( clause );
    }, literals );
  }

  bill::lit_type add_clauses_for_gate( bill::lit_type a, bill::lit_type b, std::optional<bill::lit_type> c = std::nullopt, gate_type type = AND )
  {
    /* only support AND2 and XOR2 for now */
    assert( type == AND || type == XOR );

    auto nlit = c ? *c : bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
    if ( type == AND )
    {
      detail::on_and<add_clause_fn_t>( nlit, a, b, [&]( auto const& clause ) {
        solver.add_clause( clause );
      });
    }
    else if ( type == XOR )
    {
      detail::on_xor<add_clause_fn_t>( nlit, a, b, [&]( auto const& clause ) {
        solver.add_clause( clause );
      });
    }

    return nlit;
  }

  bill::lit_type add_tmp_gate( std::vector<bill::lit_type> const& lits, gate const& g )
  {
    assert( g.fanins.size() == 2u );
    assert( g.fanins[0].idx < lits.size() );
    assert( g.fanins[1].idx < lits.size() );

    return add_clauses_for_gate( lit_not_cond( lits[g.fanins[0].idx], g.fanins[0].inv ), lit_not_cond( lits[g.fanins[1].idx], g.fanins[1].inv ), std::nullopt, g.type );
  }

  std::optional<bool> solve( std::vector<bill::lit_type> assumptions )
  {
    auto const res = solver.solve( assumptions, ps.conflict_limit );
    if ( res == bill::result::states::satisfiable )
    {
      auto model = solver.get_model().model();
      for ( auto i = 0u; i < ntk.num_pis(); ++i )
      {
        cex.at( i ) = model.at( i + 1 ) == bill::lbool_type::true_; 
      }
      return false;
    }
    else if ( res == bill::result::states::unsatisfiable )
    {
      return true;
    }
    return std::nullopt; /* timeout or something wrong */
  }

  std::optional<bool> validate( node const& root, bill::lit_type const& lit )
  {
    assert( literals[root].variable() != bill::var_type( 0 ) );
    if constexpr ( use_bookmark )
    {
      solver.bookmark();
    }

    std::optional<bool> res;
    if constexpr ( use_odc )
    {
      if ( ps.odc_levels != 0 )
      {
        res = solve( {build_odc_window( root, lit )} );
      }
      else
      {
        auto nlit = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
        solver.add_clause( {literals[root], lit, nlit} );
        solver.add_clause( {~(literals[root]), ~lit, nlit} );
        res = solve( {~nlit} );
      }
    }
    else 
    {
      auto nlit = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
      solver.add_clause( {literals[root], lit, nlit} );
      solver.add_clause( {~(literals[root]), ~lit, nlit} );
      res = solve( {~nlit} );
    }

    if constexpr ( use_bookmark )
    {
      solver.rollback();
    }
    return res;
  }

private:
  template<bool enabled = use_odc, typename = std::enable_if_t<enabled>>
  bill::lit_type build_odc_window( node const& root, bill::lit_type const& lit )
  {
    /* literals for the duplicated fanout cone */
    unordered_node_map<bill::lit_type, Ntk> lits( ntk );
    /* literals of XORs in the miter */
    std::vector<bill::lit_type> miter;

    lits[root] = lit;
    ntk.incr_trav_id();
    make_lit_fanout_cone_rec( root, lits, miter, 1 );
    ntk.incr_trav_id();
    duplicate_fanout_cone_rec( root, lits, 1 );

    /* miter for POs */
    ntk.foreach_po( [&]( auto const& f ){
      if ( !lits.has( ntk.get_node( f ) ) ) return true; /* PO not in TFO, skip */
      add_miter_clauses( ntk.get_node( f ), lits, miter );
      return true; /* next */
    });

    assert( miter.size() > 0 && "max fanout depth < odc_levels (-1 is infinity) and there is no PO in TFO cone" );
    auto nlit2 = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
    miter.emplace_back( nlit2 );
    solver.add_clause( miter );
    return ~nlit2 ;
  }

  template<bool enabled = use_odc, typename = std::enable_if_t<enabled>>
  void duplicate_fanout_cone_rec( node const& n, unordered_node_map<bill::lit_type, Ntk> const& lits, int level )
  {
    ntk.foreach_fanout( n, [&]( auto const& fo ){
      if ( ntk.visited( fo ) == ntk.trav_id() ) return true; /* skip */
      ntk.set_visited( fo, ntk.trav_id() );

      std::vector<bill::lit_type> l_fi;
      ntk.foreach_fanin( fo, [&]( auto const& fi ){
        l_fi.emplace_back( lit_not_cond( lits.has( ntk.get_node(fi) )? lits[fi]: literals[fi], ntk.is_complemented( fi ) ) );
      });
      assert( l_fi.size() == 2u );
      assert( ntk.is_and( fo ) || ntk.is_xor( fo ) );
      add_clauses_for_gate( l_fi[0], l_fi[1], lits[fo], ntk.is_and( fo )? AND: XOR );

      if ( level == ps.odc_levels ) return true;

      duplicate_fanout_cone_rec( fo, lits, level + 1 );
      return true; /* next */
    });
  }

  template<bool enabled = use_odc, typename = std::enable_if_t<enabled>>
  void make_lit_fanout_cone_rec( node const& n, unordered_node_map<bill::lit_type, Ntk>& lits, std::vector<bill::lit_type>& miter, int level )
  {
    ntk.foreach_fanout( n, [&]( auto const& fo ){
      if ( ntk.visited( fo ) == ntk.trav_id() ) return true; /* skip */
      ntk.set_visited( fo, ntk.trav_id() );

      lits[fo] = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );

      if ( level == ps.odc_levels )
      {
        add_miter_clauses( fo, lits, miter );
        return true;
      }
      
      make_lit_fanout_cone_rec( fo, lits, miter, level + 1 );
      return true; /* next */
    });
  }

  template<bool enabled = use_odc, typename = std::enable_if_t<enabled>>
  void add_miter_clauses( node const& n, unordered_node_map<bill::lit_type, Ntk> const& lits, std::vector<bill::lit_type>& miter )
  {
    miter.emplace_back( add_clauses_for_gate( literals[n], lits[n], std::nullopt, XOR ) );
  }

private:
  Ntk const& ntk;

  validator_params const& ps;

  node_map<bill::lit_type, Ntk> literals;
  bill::solver<Solver> solver;

public:
  std::vector<bool> cex;
};

} /* namespace mockturtle */
