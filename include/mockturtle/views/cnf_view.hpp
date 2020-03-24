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
  \file cnf_view.hpp
  \brief Creates a CNF while creating a network

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "../algorithms/cnf.hpp"
#include "../traits.hpp"

#include <bill/sat/interface/common.hpp>
#include <bill/sat/interface/glucose.hpp>
#include <fmt/format.h>
#include <percy/cnf.hpp>

namespace mockturtle
{

struct cnf_view_params
{
  /*! \brief Write DIMACS file, whenever solve is called. */
  std::optional<std::string> write_dimacs{};
};

/*! \brief A view to connect logic network creation to SAT solving.
 *
 * When using this view to create a new network, it creates a CNF internally
 * while nodes are added to the network.  It also contains a SAT solver.  The
 * network can be solved by calling the `solve` method, which by default assumes
 * that each output should compute `true` (an overload of the `solve` method can
 * override this default behaviour and apply custom assumptions).  Further,
 * the methods `value` and `pi_values` can be used to access model values in
 * case solving was satisfiable.  Finally, methods `var` and `lit` can be used
 * to access variable and literal information for nodes and signals,
 * respectively, in order to add custom clauses with the `add_clause` methods.
 *
 * The CNF is generated additively and cannot be modified after nodes have been
 * added.  Therefore, a network cannot modify or delete nodes when wrapped in a
 * `cnf_view`.
 */
template<typename Ntk, bill::solvers Solver = bill::solvers::glucose_41>
class cnf_view : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  // can only be constructed as empty network
  explicit cnf_view( cnf_view_params const& ps = {} )
    : Ntk(),
      ps_( ps )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
    static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_node_function_v<Ntk>, "Ntk does not implement the node_function method" );

    const auto v = solver_.add_variable(); /* for the constant input */
    assert( v == var( Ntk::get_node( Ntk::get_constant( false ) ) ) );
    (void)v;

    register_events();
  }

  signal create_pi( std::string const& name = std::string() )
  {
    const auto f = Ntk::create_pi( name );

    const auto v = solver_.add_variable();
    assert( v == var( Ntk::get_node( f ) ) );
    (void)v;

    return f;
  }

  /* \brief Returns the variable associated to a node. */
  inline bill::var_type var( node const& n ) const
  {
    return Ntk::node_to_index( n );
  }

  /*! \brief Returns the literal associated to a signal. */
  inline bill::lit_type lit( signal const& f ) const
  {
    return bill::lit_type( var( Ntk::get_node( f ) ), Ntk::is_complemented( f ) ? bill::lit_type::polarities::negative : bill::lit_type::polarities::positive );
  }

  /*! \brief Solves the network with a set of custom assumptions.
   *
   * This function does not assert any primary output, unless specified
   * explicitly through the assumptions.
   *
   * The function returns `nullopt`, if no solution can be found (due to a
   * conflict limit), or `true` in case of SAT, and `false` in case of UNSAT.
   *
   * \param assumptions Vector of literals to be assumped when solving
   * \param limit Conflict limit (unlimited if 0)
   */
  inline std::optional<bool> solve( bill::result::clause_type const& assumptions, uint32_t limit = 0 )
  {
    if ( ps_.write_dimacs )
    {
      for ( const auto& a : assumptions )
      {
        auto l = pabc::Abc_Var2Lit( a.variable(), a.is_complemented() );
        dimacs_.add_clause( &l, &l + 1 );
      }

      dimacs_.set_nr_vars( solver_.num_variables() );

      auto fd = fopen( ps_.write_dimacs->c_str(), "w" );
      dimacs_.to_dimacs( fd );
      fclose( fd );
    }

    const auto res = solver_.solve( assumptions, limit );

    switch ( res )
    {
    case bill::result::states::satisfiable:
      model_ = solver_.get_model().model();
      return true;
    case bill::result::states::unsatisfiable:
      return false;
    default:
      return std::nullopt;
    }

    return std::nullopt;
  }

  /*! \brief Solves the network by asserting all primary outputs to be true
   *
   * The function returns `nullopt`, if no solution can be found (due to a
   * conflict limit), or `true` in case of SAT, and `false` in case of UNSAT.
   *
   * \param limit Conflict limit (unlimited if 0)
   */
  inline std::optional<bool> solve( int limit = 0 )
  {
    bill::result::clause_type assumptions;
    Ntk::foreach_po( [&]( auto const& f ) {
      assumptions.push_back( lit( f ) );
    } );
    return solve( assumptions, limit );
  }

  /*! \brief Return model value for a node. */
  inline bool value( node const& n )
  {
    return model_.at( var( n ) ) == bill::lbool_type::true_;
  }

  /*! \brief Return model value for a node (takes complementation into account). */
  inline bool value( signal const& f )
  {
    return value( Ntk::get_node( f ) ) != Ntk::is_complemented( f );
  }

  /* \brief Returns all model values for all primary inputs. */
  std::vector<bool> pi_values()
  {
    std::vector<bool> values( Ntk::num_pis() );
    Ntk::foreach_pi( [&]( auto const& n, auto i ) {
      values[i] = value( n );
    } );
    return values;
  }

  /*! \brief Blocks last model for primary input values. */
  void block()
  {
    bill::result::clause_type blocking_clause;
    Ntk::foreach_pi( [&]( auto const& n ) {
      blocking_clause.push_back( bill::lit_type( var( n ), value( n ) ? bill::lit_type::polarities::negative : bill::lit_type::polarities::positive ) );
    });
    add_clause( blocking_clause );
  }

  /*! \brief Number of variables. */
  inline uint32_t num_vars()
  {
    return solver_.num_variables();
  }

  /*! \brief Number of clauses. */
  inline uint32_t num_clauses()
  {
    return solver_.num_clauses();
  }

  /*! \brief Adds a clause to the solver. */
  void add_clause( bill::result::clause_type const& clause )
  {
    if ( ps_.write_dimacs )
    {
      std::vector<int> lits;
      for ( auto c : clause )
      {
        lits.push_back( pabc::Abc_Var2Lit( c.variable(), c.is_complemented() ) );
      }
      dimacs_.add_clause( &lits[0], &lits[0] + lits.size() );
    }
    solver_.add_clause( clause );
  }

  /*! \brief Adds a clause from signals to the solver. */
  void add_clause( std::vector<signal> const& clause )
  {
    bill::result::clause_type lits;
    std::transform( clause.begin(), clause.end(), std::back_inserter( lits ), [&]( auto const& s ) { return lit( s ); } );
    add_clause( lits );
  }

  /*! \brief Adds a clause to the solver.
   *
   * Entries are either all literals or network signals.
   */
  template<typename... Lit, typename = std::enable_if_t<
    std::disjunction_v<
      std::conjunction<std::is_same<Lit, bill::lit_type>...>,
      std::conjunction<std::is_same<Lit, signal>...>>>>
  void add_clause( Lit... lits )
  {
    if constexpr ( std::conjunction_v<std::is_same<Lit, bill::lit_type>...> )
    {
      add_clause( bill::result::clause_type{{lits...}} );
    }
    else
    {
      add_clause( bill::result::clause_type{{lit( lits )...}} );
    }
  }

private:
  void register_events()
  {
    Ntk::events().on_add.push_back( [this]( auto const& n ) { on_add( n ); } );
    Ntk::events().on_modified.push_back( []( auto const& n, auto const& previous ) {
      (void)n;
      (void)previous;
      assert( false && "nodes should not be modified in cnf_view" );
      std::abort();
    } );
    Ntk::events().on_delete.push_back( []( auto const& n ) {
      (void)n;
      assert( false && "nodes should not be deleted in cnf_view" );
      std::abort();
    } );
  }

  void on_add( node const& n )
  {
    const auto v = solver_.add_variable();
    assert( v == var( n ) );
    (void)v;

    const auto _add_clause = [&]( bill::result::clause_type const& clause ) {
      add_clause( clause );
    };

    const auto node_lit = lit( Ntk::make_signal( n ) );
    bill::result::clause_type child_lits;
    Ntk::foreach_fanin( n, [&]( auto const& f ) {
      child_lits.push_back( lit( f ) );
    } );

    if constexpr ( has_is_and_v<Ntk> )
    {
      if ( Ntk::is_and( n ) )
      {
        detail::on_and( node_lit, child_lits[0], child_lits[1], _add_clause );
        return;
      }
    }

    if constexpr ( has_is_or_v<Ntk> )
    {
      if ( Ntk::is_or( n ) )
      {
        detail::on_or( node_lit, child_lits[0], child_lits[1], _add_clause );
        return;
      }
    }

    if constexpr ( has_is_xor_v<Ntk> )
    {
      if ( Ntk::is_xor( n ) )
      {
        detail::on_xor( node_lit, child_lits[0], child_lits[1], _add_clause );
        return;
      }
    }

    if constexpr ( has_is_maj_v<Ntk> )
    {
      if ( Ntk::is_maj( n ) )
      {
        detail::on_maj( node_lit, child_lits[0], child_lits[1], child_lits[2], _add_clause );
        return;
      }
    }

    if constexpr ( has_is_ite_v<Ntk> )
    {
      if ( Ntk::is_ite( n ) )
      {
        detail::on_ite( node_lit, child_lits[0], child_lits[1], child_lits[2], _add_clause );
        return;
      }
    }

    if constexpr ( has_is_xor3_v<Ntk> )
    {
      if ( Ntk::is_xor3( n ) )
      {
        detail::on_xor3( node_lit, child_lits[0], child_lits[1], child_lits[2], _add_clause );
        return;
      }
    }

    detail::on_function( node_lit, child_lits, Ntk::node_function( n ), _add_clause );
  }

private:
  bill::solver<Solver> solver_;
  bill::result::model_type model_;
  percy::cnf_formula dimacs_;

  cnf_view_params ps_;
};

} /* namespace mockturtle */
