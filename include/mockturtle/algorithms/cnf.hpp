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
  \file cnf.hpp
  \brief CNF generation methods

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <kitty/constructors.hpp>
#include <kitty/cnf.hpp>

#include "../traits.hpp"
#include "../utils/node_map.hpp"

namespace mockturtle
{

using clause_callback_t = std::function<void( std::vector<uint32_t> const& )>;

namespace detail
{

template<class Ntk>
class generate_cnf_impl
{
public:
  generate_cnf_impl( Ntk const& ntk, clause_callback_t const& fn )
      : ntk_( ntk ),
        fn_( fn ),
        node_vars_( ntk )
  {
  }

  std::vector<uint32_t> run()
  {
    // TODO constants

    /* first indexes are for PIs */
    ntk_.foreach_pi( [&]( auto const& n, auto i ) {
      node_vars_[n] = i;
    } );

    /* compute vars for nodes */
    uint32_t next_var = ntk_.num_pis();
    ntk_.foreach_gate( [&]( auto const& n ) {
      node_vars_[n] = next_var++;
    } );

    /* compute clauses for nodes */
    ntk_.foreach_gate( [&]( auto const& n ) {
      std::vector<uint32_t> child_lits;
      ntk_.foreach_fanin( n, [&]( auto const& f ) {
        child_lits.push_back( make_lit( node_vars_[f], ntk_.is_complemented( f ) ) );
      } );
      uint32_t node_lit = make_lit( node_vars_[n], false );

      if constexpr ( has_is_and_v<Ntk> )
      {
        if ( ntk_.is_and( n ) )
        {
          on_and( node_lit, child_lits[0], child_lits[1] );
          return true;
        }
      }

      if constexpr ( has_is_or_v<Ntk> )
      {
        if ( ntk_.is_or( n ) )
        {
          on_or( node_lit, child_lits[0], child_lits[1] );
          return true;
        }
      }

      if constexpr ( has_is_xor_v<Ntk> )
      {
        if ( ntk_.is_xor( n ) )
        {
          on_xor( node_lit, child_lits[0], child_lits[1] );
          return true;
        }
      }

      if constexpr ( has_is_maj_v<Ntk> )
      {
        if ( ntk_.is_maj( n ) )
        {
          on_maj( node_lit, child_lits[0], child_lits[1], child_lits[2] );
          return true;
        }
      }

      if constexpr ( has_is_ite_v<Ntk> )
      {
        if ( ntk_.is_ite( n ) )
        {
          on_ite( node_lit, child_lits[0], child_lits[1], child_lits[2] );
          return true;
        }
      }

      if constexpr ( has_is_xor3_v<Ntk> )
      {
        if ( ntk_.is_xor3( n ) )
        {
          on_xor3( node_lit, child_lits[0], child_lits[1], child_lits[2] );
          return true;
        }
      }

      // TODO general case
      const auto cnf = kitty::cnf_characteristic( ntk_.node_function( n ) );

      assert( false );
      return true;
    } );

    std::vector<uint32_t> output_lits;
    ntk_.foreach_po( [&]( auto const& f ) {
      output_lits.push_back( make_lit( node_vars_[f], ntk_.is_complemented( f ) ) );
    } );

    return output_lits;
  }

private:
  /* c = a & b */
  inline void on_and( uint32_t c, uint32_t a, uint32_t b )
  {
    fn_( {a, lit_not( c )} );
    fn_( {b, lit_not( c )} );
    fn_( {lit_not( a ), lit_not( b ), c} );
  }

  /* c = a | b */
  inline void on_or( uint32_t c, uint32_t a, uint32_t b )
  {
    fn_( {lit_not( a ), c} );
    fn_( {lit_not( b ), c} );
    fn_( {a, b, lit_not( c )} );
  }

  /* c = a ^ b */
  inline void on_xor( uint32_t c, uint32_t a, uint32_t b )
  {
    fn_( {lit_not( a ), lit_not( b ), lit_not( c )} );
    fn_( {lit_not( a ), b, c} );
    fn_( {a, lit_not( b ), c} );
    fn_( {a, b, lit_not( c )} );
  }

  /* d = <abc> */
  inline void on_maj( uint32_t d, uint32_t a, uint32_t b, uint32_t c )
  {
    fn_( {lit_not( a ), lit_not( b ), d} );
    fn_( {lit_not( a ), lit_not( c ), d} );
    fn_( {lit_not( b ), lit_not( c ), d} );
    fn_( {a, b, lit_not( d )} );
    fn_( {a, c, lit_not( d )} );
    fn_( {b, c, lit_not( d )} );
  }

  /* d = a ^ b ^ c */
  inline void on_xor3( uint32_t d, uint32_t a, uint32_t b, uint32_t c )
  {
    fn_( {lit_not( a ), b, c, d} );
    fn_( {a, lit_not( b ), c, d} );
    fn_( {a, b, lit_not( c ), d} );
    fn_( {a, b, c, lit_not( d )} );
    fn_( {a, lit_not( b ), lit_not( c ), lit_not( d )} );
    fn_( {lit_not( a ), b, lit_not( c ), lit_not( d )} );
    fn_( {lit_not( a ), lit_not( b ), c, lit_not( d )} );
    fn_( {lit_not( a ), lit_not( b ), lit_not( c ), d} );
  }

  /* d = a ? b : c */
  inline void on_ite( uint32_t d, uint32_t a, uint32_t b, uint32_t c )
  {
    fn_( {lit_not( a ), lit_not( b ), d} );
    fn_( {lit_not( a ), b, lit_not( d )} );
    fn_( {a, lit_not( c ), d} );
    fn_( {a, c, lit_not( d )} );
  }

  constexpr uint32_t make_lit( uint32_t var, bool is_complemented ) const
  {
    return ( var << 1 ) | ( is_complemented ? 1 : 0 );
  }

  constexpr uint32_t lit_not( uint32_t lit ) const
  {
    return lit ^ 0x1;
  }

private:
  Ntk const& ntk_;
  clause_callback_t const& fn_;

  node_map<uint32_t, Ntk> node_vars_;
};

} // namespace detail

template<class Ntk>
std::vector<uint32_t> generate_cnf( Ntk const& ntk, clause_callback_t const& fn )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  detail::generate_cnf_impl<Ntk> impl( ntk, fn );
  return impl.run();
}

} // namespace mockturtle
