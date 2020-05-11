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

  uint32_t conflict_limit{1000};

  uint32_t random_seed{0};
};

template<class Ntk, bill::solvers Solver = bill::solvers::z3, bool use_bookmark = true>
class circuit_validator
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using add_clause_fn_t = std::function<void( std::vector<bill::lit_type> const& )>;

  enum gate_type 
  {
    AND,
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
    //static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
    //static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );
    //static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
    static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
    //static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
    //static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
    //static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );
    //static_assert( has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
    static_assert( has_size_v<Ntk>, "Ntk does not implement the has_size method" );
    //static_assert( has_value_v<Ntk>, "Ntk does not implement the has_value method" );
    //static_assert( has_visited_v<Ntk>, "Ntk does not implement the has_visited method" );
    if constexpr ( use_bookmark )
    {
      static_assert( Solver == bill::solvers::z3, "Solver does not support bookmark/rollback" );
    }

    restart();
  }

  /* validate with an existing signal in the network */
  bool validate( node const& root, signal const& d )
  {
    return validate( root, lit_not_cond( literals[d], ntk.is_complemented( d ) ) );
  }

  /* validate with a circuit composed of `divs` which are existing nodes in the network */
  bool validate( node const& root, std::vector<node> const& divs, std::vector<gate> const& ckt, bool output_negation = false )
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

  /* add clauses for a new node (created after construction of validator) 
    Note: should be called manually every time or be added to ntk.on_add events
  */
  void add_node( node const& n )
  {
    /* only support AIGs for now */
    assert( ntk.is_and( n ) );

    literals.resize();
    literals[n] = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );

    std::vector<bill::lit_type> lit_fi;
    ntk.foreach_fanin( n, [&]( const auto& f ){
      lit_fi.emplace_back( lit_not_cond( literals[f], ntk.is_complemented( f ) ) );
    });

    detail::on_and<add_clause_fn_t>( literals[n], lit_fi[0], lit_fi[1], [&]( auto const& clause ) {
      solver.add_clause( clause );
    });
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
    solver.set_random_phase( ps.random_seed );

    solver.add_variables( ntk.size() );
    generate_cnf<Ntk, bill::lit_type>( ntk, [&]( auto const& clause ) {
      solver.add_clause( clause );
    }, literals );
  }

  bill::lit_type add_tmp_and( bill::lit_type a, bill::lit_type b, gate_type type = AND )
  {
    /* only support AIGs for now */
    assert( type == AND );

    auto nlit = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
    detail::on_and<add_clause_fn_t>( nlit, a, b, [&]( auto const& clause ) {
      solver.add_clause( clause );
    });

    return nlit;
  }

  bill::lit_type add_tmp_gate( std::vector<bill::lit_type> const& lits, gate const& g )
  {
    assert( g.fanins.size() == 2u );
    assert( g.fanins[0].idx < lits.size() );
    assert( g.fanins[1].idx < lits.size() );

    return add_tmp_and( lit_not_cond( lits[g.fanins[0].idx], g.fanins[0].inv ), lit_not_cond( lits[g.fanins[1].idx], g.fanins[1].inv ), g.type );
  }

  bool validate( node const& root, bill::lit_type const& lit )
  {
    assert( literals[root].variable() != bill::var_type( 0 ) );
    if constexpr ( use_bookmark )
    {
      solver.bookmark();
    }

    auto nlit = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
    solver.add_clause( {literals[root], lit, nlit} );
    solver.add_clause( {~(literals[root]), ~lit, nlit} );
    const auto res = solver.solve( {~nlit}, ps.conflict_limit );
    
    if ( res == bill::result::states::satisfiable )
    {
      auto model = solver.get_model().model();
      for ( auto i = 0u; i < ntk.num_pis(); ++i )
      {
        cex.at( i ) = model.at( i + 1 ) == bill::lbool_type::true_; 
      }
    }

    if constexpr ( use_bookmark )
    {
      solver.rollback();
    }
    
    return res == bill::result::states::unsatisfiable;
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
