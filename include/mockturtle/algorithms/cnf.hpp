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

#include <kitty/cnf.hpp>
#include <kitty/constructors.hpp>

#include "../traits.hpp"
#include "../utils/node_map.hpp"

namespace mockturtle
{

inline constexpr uint32_t make_lit( uint32_t var, bool is_complemented = false )
{
  return ( var << 1 ) | ( is_complemented ? 1 : 0 );
}

inline constexpr uint32_t lit_not( uint32_t lit )
{
  return lit ^ 0x1;
}

inline constexpr uint32_t lit_not_cond( uint32_t lit, bool cond )
{
  return cond ? lit ^ 0x1 : lit;
}

using clause_callback_t = std::function<void( std::vector<uint32_t> const& )>;

template<class Ntk>
node_map<uint32_t, Ntk> node_literals( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_num_pis_v<Ntk>, "Ntk does not implement the num_pis method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );

  node_map<uint32_t, Ntk> node_lits( ntk );

  /* constants are mapped to var 0 */
  node_lits[ntk.get_constant( false )] = make_lit( 0 );
  if ( ntk.get_node( ntk.get_constant( false ) ) != ntk.get_node( ntk.get_constant( true ) ) )
  {
    node_lits[ntk.get_constant( true )] = make_lit( 0, true );
  }

  /* first indexes (starting from 1) are for PIs */
  ntk.foreach_pi( [&]( auto const& n, auto i ) {
    node_lits[n] = make_lit( i + 1 );
  } );

  /* compute literals for nodes */
  uint32_t next_var = ntk.num_pis() + 1;
  ntk.foreach_gate( [&]( auto const& n ) {
    node_lits[n] = make_lit( next_var++ );
  } );

  return node_lits;
}

namespace detail
{

template<class Ntk>
class generate_cnf_impl
{
public:
  generate_cnf_impl( Ntk const& ntk, clause_callback_t const& fn )
      : ntk_( ntk ),
        fn_( fn ),
        node_lits_( node_literals( ntk ) )
  {
  }

  std::vector<uint32_t> run()
  {
    /* unit clause for constant-0 */
    fn_( {1} );

    /* compute clauses for nodes */
    ntk_.foreach_gate( [&]( auto const& n ) {
      std::vector<uint32_t> child_lits;
      ntk_.foreach_fanin( n, [&]( auto const& f ) {
        child_lits.push_back( lit_not_cond( node_lits_[f], ntk_.is_complemented( f ) ) );
      } );
      uint32_t node_lit = node_lits_[n];

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

      /* general case */
      const auto cnf = kitty::cnf_characteristic( ntk_.node_function( n ) );

      child_lits.push_back( node_lit );
      for ( auto const& cube : cnf )
      {
        std::vector<uint32_t> clause;
        for ( auto i = 0u; i <= ntk_.fanin_size( n ); ++i )
        {
          if ( cube.get_mask( i ) )
          {
            clause.push_back( lit_not_cond( child_lits[i], !cube.get_bit( i ) ) );
          }
        }
        fn_( clause );
      }

      return true;
    } );

    std::vector<uint32_t> output_lits;
    ntk_.foreach_po( [&]( auto const& f ) {
      output_lits.push_back( lit_not_cond( node_lits_[f], ntk_.is_complemented( f ) ) );
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

private:
  Ntk const& ntk_;
  clause_callback_t const& fn_;

  node_map<uint32_t, Ntk> node_lits_;
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
