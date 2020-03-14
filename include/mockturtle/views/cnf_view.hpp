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
#include <vector>

#include "../algorithms/cnf.hpp"
#include "../traits.hpp"

#include <fmt/format.h>
#include <percy/solvers/bsat2.hpp>

namespace mockturtle
{

template<typename Ntk>
class cnf_view : public Ntk
{
public:
  using storage = typename Ntk::storage;
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  // can only be constructed as empty network
  cnf_view() : Ntk()
  {
    register_events();
  }

  inline uint32_t var( node const& n ) const
  {
    return Ntk::node_to_index( n );
  }

  inline uint32_t lit( signal const& f ) const
  {
    return make_lit( var( Ntk::get_node( f ) ), Ntk::is_complemented( f ) );
  }

  std::optional<bool> solve( int limit = 0 )
  {
    std::vector<int> assumptions;
    Ntk::foreach_po( [&]( auto const& f ) {
      assumptions.push_back( lit( f ) );
    } );
    switch ( solver_.solve( &assumptions[0], &assumptions[0] + 1, limit ) )
    {
    case percy::success:
      return true;
    case percy::failure:
      return false;
    case percy::timeout:
      return std::nullopt;
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
      assert( false && "nodes should not be modified in cnf_view" );
      std::abort();
    } );
  }

  void on_add( node const& n )
  {
    const auto add_clause = [&]( std::vector<uint32_t> const& clause ) {
      solver_.add_clause( clause );
    };

    const auto node_lit = lit( Ntk::make_signal( n ) );
    std::vector<uint32_t> child_lits;
    Ntk::foreach_fanin( n, [&]( auto const& f ) {
      child_lits.push_back( lit( f ) );
    } );

    if constexpr ( has_is_and_v<Ntk> )
    {
      if ( Ntk::is_and( n ) )
      {
        detail::on_and( node_lit, child_lits[0], child_lits[1], add_clause );
        return;
      }
    }

    if constexpr ( has_is_or_v<Ntk> )
    {
      if ( Ntk::is_or( n ) )
      {
        detail::on_or( node_lit, child_lits[0], child_lits[1], add_clause );
        return;
      }
    }

    if constexpr ( has_is_xor_v<Ntk> )
    {
      if ( Ntk::is_xor( n ) )
      {
        detail::on_xor( node_lit, child_lits[0], child_lits[1], add_clause );
        return;
      }
    }

    std::abort();
  }

private:
  percy::bsat_wrapper solver_;
};

} /* namespace mockturtle */
