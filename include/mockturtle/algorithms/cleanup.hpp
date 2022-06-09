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
  \file cleanup.hpp
  \brief Cleans up networks

  \author Heinz Riener
  \author Mathias Soeken
*/

#pragma once

#include <iostream>
#include <type_traits>
#include <vector>

#include <kitty/operations.hpp>

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

template <class NtkSource, class NtkDest>
typename NtkDest::signal copy_gate(const NtkSource &ntk, NtkDest &dest,
                                 typename NtkSource::node node,
                                 std::vector<signal<NtkDest>> children)
{
  if constexpr ( std::is_same_v<NtkSource, NtkDest> )
  {
    return dest.clone_node( ntk, node, children );
  }
  else
  {
    if constexpr ( has_is_and_v<NtkSource> )
    {
      static_assert( has_create_and_v<NtkDest>, "NtkDest cannot create AND gates" );
      if ( ntk.is_and( node ) )
      {
        return dest.create_and( children[0], children[1] );
      }
    }
    if constexpr ( has_is_or_v<NtkSource> )
    {
      static_assert( has_create_or_v<NtkDest>, "NtkDest cannot create OR gates" );
      if ( ntk.is_or( node ) )
      {
        return dest.create_or( children[0], children[1] );
      }
    }
    if constexpr ( has_is_xor_v<NtkSource> )
    {
      static_assert( has_create_xor_v<NtkDest>, "NtkDest cannot create XOR gates" );
      if ( ntk.is_xor( node ) )
      {
        return dest.create_xor( children[0], children[1] );
      }
    }
    if constexpr ( has_is_maj_v<NtkSource> )
    {
      static_assert( has_create_maj_v<NtkDest>, "NtkDest cannot create MAJ gates" );
      if ( ntk.is_maj( node ) )
      {
        return dest.create_maj( children[0], children[1], children[2] );
      }
    }
    if constexpr ( has_is_ite_v<NtkSource> )
    {
      static_assert( has_create_ite_v<NtkDest>, "NtkDest cannot create ITE gates" );
      if ( ntk.is_ite( node ) )
      {
        return dest.create_ite( children[0], children[1], children[2] );
      }
    }
    if constexpr ( has_is_xor3_v<NtkSource> )
    {
      static_assert( has_create_xor3_v<NtkDest>, "NtkDest cannot create XOR3 gates" );
      if ( ntk.is_xor3( node ) )
      {
        return dest.create_xor3( children[0], children[1], children[2] );
      }
    }
    if constexpr ( has_is_nary_and_v<NtkSource> )
    {
      static_assert( has_create_nary_and_v<NtkDest>, "NtkDest cannot create n-ary AND gates" );
      if ( ntk.is_nary_and( node ) )
      {
        return dest.create_nary_and( children );
      }
    }
    if constexpr ( has_is_nary_or_v<NtkSource> )
    {
      static_assert( has_create_nary_or_v<NtkDest>, "NtkDest cannot create n-ary OR gates" );
      if ( ntk.is_nary_or( node ) )
      {
        return dest.create_nary_or( children );
      }
    }
    if constexpr ( has_is_nary_xor_v<NtkSource> )
    {
      static_assert( has_create_nary_xor_v<NtkDest>, "NtkDest cannot create n-ary XOR gates" );
      if ( ntk.is_nary_xor( node ) )
      {
        return dest.create_nary_xor( children );
      }
    }
    if constexpr ( has_is_function_v<NtkSource> )
    {
      static_assert( has_create_node_v<NtkDest>, "NtkDest cannot create arbitrary function gates" );
      return dest.create_node( children, ntk.node_function( node ) );
    }
    std::cerr << "[e] something went wrong, could not copy node " << ntk.node_to_index( node ) << "\n";
  }
  throw;
}

/*
 * Copy a non-constant and non
 */
template <class NtkSource, class NtkDest>
typename NtkDest::signal copy_gate(const NtkSource &ntk, NtkDest &dest, typename NtkSource::node node,
                        mockturtle::node_map<typename NtkDest::signal, NtkSource> &old_to_new)
{
  /* collect children */
  std::vector<signal<NtkDest>> children;
  ntk.foreach_fanin( node, [&]( auto child, auto ) {
    const auto f = old_to_new[child];
    if ( ntk.is_complemented( child ) )
    {
      children.push_back( dest.create_not( f ) );
    }
    else
    {
      children.push_back( f );
    }
  } );
  return copy_gate(ntk, dest, node, children);
}

template<typename NtkSource, typename NtkDest, typename LeavesIterator>
std::vector<signal<NtkDest>> cleanup_dangling( NtkSource const& ntk, NtkDest& dest, LeavesIterator begin, LeavesIterator end )
{
  (void)end;

  static_assert( is_network_type_v<NtkSource>, "NtkSource is not a network type" );
  static_assert( is_network_type_v<NtkDest>, "NtkDest is not a network type" );

  static_assert( has_get_node_v<NtkSource>, "NtkSource does not implement the get_node method" );
  static_assert( has_get_constant_v<NtkSource>, "NtkSource does not implement the get_constant method" );
  static_assert( has_foreach_pi_v<NtkSource>, "NtkSource does not implement the foreach_pi method" );
  static_assert( has_is_pi_v<NtkSource>, "NtkSource does not implement the is_pi method" );
  static_assert( has_is_constant_v<NtkSource>, "NtkSource does not implement the is_constant method" );
  static_assert( has_is_complemented_v<NtkSource>, "NtkSource does not implement the is_complemented method" );
  static_assert( has_foreach_po_v<NtkSource>, "NtkSource does not implement the foreach_po method" );

  static_assert( has_get_constant_v<NtkDest>, "NtkDest does not implement the get_constant method" );
  static_assert( has_create_not_v<NtkDest>, "NtkDest does not implement the create_not method" );
  static_assert( has_clone_node_v<NtkDest>, "NtkDest does not implement the clone_node method" );

  node_map<signal<NtkDest>, NtkSource> old_to_new( ntk );
  old_to_new[ntk.get_constant( false )] = dest.get_constant( false );

  if ( ntk.get_node( ntk.get_constant( true ) ) != ntk.get_node( ntk.get_constant( false ) ) )
  {
    old_to_new[ntk.get_constant( true )] = dest.get_constant( true );
  }

  /* create inputs in same order */
  auto it = begin;
  ntk.foreach_pi( [&]( auto node ) {
    old_to_new[node] = *it++;
  } );
  assert( it == end );

  /* foreach node in topological order */
  topo_view topo{ntk};
  topo.foreach_node( [&]( auto node ) {
    if ( ntk.is_constant( node ) || ntk.is_ci( node ) )
        return;
    old_to_new[node] = copy_gate(ntk, dest, node, old_to_new);
  });

  /* create outputs in same order */
  std::vector<signal<NtkDest>> fs;
  ntk.foreach_po( [&]( auto po ) {
    const auto f = old_to_new[po];
    if ( ntk.is_complemented( po ) )
    {
      fs.push_back( dest.create_not( f ) );
    }
    else
    {
      fs.push_back( f );
    }
  } );

  return fs;
}

/*! \brief Cleans up dangling nodes.
 *
 * This method reconstructs a network and omits all dangling nodes. If the flag
 * `remove_dangling_PIs` is true, dangling PIs are also omitted. If the flag
 * `remove_redundant_POs` is true, redundant POs, i.e. POs connected to a PI or
 * constant, are also omitted. The network types of the source and destination
 * network are the same.
 *
   \verbatim embed:rst

   .. note::

      This method returns the cleaned up network as a return value.  It does
      *not* modify the input network.
   \endverbatim
 *
 * **Required network functions:**
 * - `get_node`
 * - `node_to_index`
 * - `get_constant`
 * - `create_pi`
 * - `create_po`
 * - `create_not`
 * - `is_complemented`
 * - `foreach_node`
 * - `foreach_pi`
 * - `foreach_po`
 * - `clone_node`
 * - `is_pi`
 * - `is_constant`
 */
template<class NtkSrc, class NtkDest = NtkSrc>
[[nodiscard]] NtkDest cleanup_dangling( NtkSrc const& ntk, bool remove_dangling_PIs = false, bool remove_redundant_POs = false )
{
  static_assert( is_network_type_v<NtkSrc>, "NtkSrc is not a network type" );
  static_assert( is_network_type_v<NtkDest>, "NtkDest is not a network type" );
  static_assert( has_get_node_v<NtkSrc>, "NtkSrc does not implement the get_node method" );
  static_assert( has_node_to_index_v<NtkSrc>, "NtkSrc does not implement the node_to_index method" );
  static_assert( has_get_constant_v<NtkSrc>, "NtkSrc does not implement the get_constant method" );
  static_assert( has_foreach_node_v<NtkSrc>, "NtkSrc does not implement the foreach_node method" );
  static_assert( has_foreach_pi_v<NtkSrc>, "NtkSrc does not implement the foreach_pi method" );
  static_assert( has_foreach_po_v<NtkSrc>, "NtkSrc does not implement the foreach_po method" );
  static_assert( has_is_pi_v<NtkSrc>, "NtkSrc does not implement the is_pi method" );
  static_assert( has_is_constant_v<NtkSrc>, "NtkSrc does not implement the is_constant method" );
  static_assert( has_clone_node_v<NtkDest>, "NtkDest does not implement the clone_node method" );
  static_assert( has_create_pi_v<NtkDest>, "NtkDest does not implement the create_pi method" );
  static_assert( has_create_po_v<NtkDest>, "NtkDest does not implement the create_po method" );
  static_assert( has_create_not_v<NtkDest>, "NtkDest does not implement the create_not method" );
  static_assert( has_is_complemented_v<NtkSrc>, "NtkDest does not implement the is_complemented method" );

  NtkDest dest;
  if constexpr ( has_get_network_name_v<NtkSrc> && has_set_network_name_v<NtkDest> )
  {
    dest.set_network_name( ntk.get_network_name() );
  }

  node_map<signal<NtkDest>, NtkSrc> old_to_new( ntk );

  ntk.foreach_pi( [&]( auto n ) {
    if ( remove_dangling_PIs && ntk.fanout_size( n ) == 0 ) {
      old_to_new[n] = dest.get_constant( false );
      return;
    }
    auto updated = dest.create_pi();
    old_to_new[n] = updated;
    if constexpr ( has_has_name_v<NtkSrc> && has_get_name_v<NtkSrc> && has_set_name_v<NtkDest> )
    {
      auto const s = ntk.make_signal( n );
      if ( ntk.has_name( s ) )
      {
        dest.set_name(updated, ntk.get_name( s ) );
      }
      if ( ntk.has_name( !s ) )
      {
        dest.set_name(dest.create_not(updated), ntk.get_name( !s ) );
      }
    }
  } );

  if constexpr ( has_foreach_register_v<NtkSrc> && has_create_ro_v<NtkDest> )
  {
    ntk.foreach_register( [&]( std::pair<typename NtkSrc::signal, typename NtkSrc::node> reg ) {
      typename NtkSrc::node ro = reg.second;
      auto updated = dest.create_ro();
      old_to_new[ro] = updated;
      if constexpr ( has_has_name_v<NtkSrc> && has_get_name_v<NtkSrc> && has_set_name_v<NtkDest> )
      {
        auto const s = ntk.make_signal( ro );
        if ( ntk.has_name( s ) )
        {
          dest.set_name(updated, ntk.get_name( s ) );
        }
        if ( ntk.has_name( !s ) )
        {
          dest.set_name(dest.create_not(updated), ntk.get_name( !s ) );
        }
      }
      dest._storage->latch_information[dest.get_node(updated)] = ntk._storage->latch_information[ro];
    } );
  }

  old_to_new[ntk.get_constant( false )] = dest.get_constant( false );

  if ( ntk.get_node( ntk.get_constant( true ) ) != ntk.get_node( ntk.get_constant( false ) ) )
  {
    old_to_new[ntk.get_constant( true )] = dest.get_constant( true );
  }

  /* foreach node in topological order */
  topo_view topo{ntk};
  topo.foreach_node( [&]( auto node ) {
    if ( ntk.is_constant( node ) || ntk.is_ci( node ) )
      return;
    auto updated = copy_gate(ntk, dest, node, old_to_new);
    old_to_new[node] = updated;
    if constexpr ( has_has_name_v<NtkSrc> && has_get_name_v<NtkSrc> && has_set_name_v<NtkDest> )
    {
      auto s = ntk.make_signal( node );
      if ( ntk.has_name( s ) )
      {
        dest.set_name(updated, ntk.get_name( s ) );
      }
      if ( ntk.has_name( !s ) )
      {
        dest.set_name(dest.create_not(updated), ntk.get_name( !s ) );
      }
    }
  });

  ntk.foreach_po( [&]( auto po, auto index ) {
    if ( remove_redundant_POs && ( dest.is_pi( dest.get_node( po ) ) || dest.is_constant( dest.get_node( po ) ) ) ) {
      return;
    }
    const auto f = old_to_new[po];
    typename NtkDest::signal g = ntk.is_complemented( po ) ? dest.create_not( old_to_new[po] ) : old_to_new[po];
    auto const s = dest.create_po( g );
  } );

  if constexpr ( has_foreach_ri_v<NtkSrc> && has_create_ri_v<NtkDest> )
  {
    ntk.foreach_ri( [&]( auto ri, auto index ) {
      typename NtkDest::signal g = ntk.is_complemented( ri ) ? dest.create_not( old_to_new[ri] ) : old_to_new[ri];
      auto const s = dest.create_ri( g, 0 );
    } );
  }

  if constexpr ( has_has_output_name_v<NtkSrc> && has_get_output_name_v<NtkSrc> && has_set_output_name_v<NtkDest> )
  {
    ntk.foreach_co( [&]( auto co, auto index ) {
      (void) co;
      if ( ntk.has_output_name( index ) )
      {
        dest.set_output_name( index, ntk.get_output_name( index ) );
      }
    });
  }

  return dest;
}

/*! \brief Cleans up LUT nodes.
 *
 * This method reconstructs a LUT network and optimizes LUTs when they do not
 * depend on all their fanin, or when some of the fanin are constant inputs.
 *
 * Constant gate inputs will be propagated.
 *
   \verbatim embed:rst

   .. note::

      This method returns the cleaned up network as a return value.  It does
      *not* modify the input network.
   \endverbatim
 *
 * **Required network functions:**
 * - `get_node`
 * - `get_constant`
 * - `foreach_pi`
 * - `foreach_po`
 * - `foreach_node`
 * - `foreach_fanin`
 * - `create_pi`
 * - `create_po`
 * - `create_node`
 * - `create_not`
 * - `is_constant`
 * - `is_pi`
 * - `is_complemented`
 * - `node_function`
 */
template<class Ntk>
Ntk cleanup_luts( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
  static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
  static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi method" );
  static_assert( has_create_po_v<Ntk>, "Ntk does not implement the create_po method" );
  static_assert( has_create_node_v<Ntk>, "Ntk does not implement the create_node method" );
  static_assert( has_create_not_v<Ntk>, "Ntk does not implement the create_not method" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
  static_assert( has_constant_value_v<Ntk>, "Ntk does not implement the constant_value method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );
  static_assert( has_node_function_v<Ntk>, "Ntk does not implement the node_function method" );

  Ntk dest;
  if constexpr ( has_get_network_name_v<Ntk> && has_set_network_name_v<Ntk> )
  {
    dest.set_network_name( ntk.get_network_name() );
  }

  node_map<signal<Ntk>, Ntk> old_to_new( ntk );

  // PIs and constants
  ntk.foreach_pi( [&]( auto const& n ) {
    old_to_new[n] = dest.create_pi();
    if constexpr ( has_has_name_v<Ntk> && has_get_name_v<Ntk>)
    {
      auto s = ntk.make_signal( n );
      if ( ntk.has_name( s ) )
      {
        dest.set_name_pi( old_to_new[n], ntk.get_name( s ) );
      }
    }
  } );
  old_to_new[ntk.get_constant( false )] = dest.get_constant( false );
  if ( ntk.get_node( ntk.get_constant( true ) ) != ntk.get_node( ntk.get_constant( false ) ) )
  {
    old_to_new[ntk.get_constant( true )] = dest.get_constant( true );
  }

  // iterate through nodes
  topo_view topo{ntk};
  topo.foreach_node( [&]( auto const& n ) {
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) ) return true; /* continue */

    auto func = ntk.node_function( n );

    /* constant propagation */
    ntk.foreach_fanin( n, [&]( auto const& f, auto i ) {
      if ( dest.is_constant( old_to_new[f] ) )
      {
        if ( dest.constant_value( old_to_new[f] ) != ntk.is_complemented( f ) )
        {
          kitty::cofactor1_inplace( func, i );
        }
        else
        {
          kitty::cofactor0_inplace( func, i );
        }
      }
    } );


    const auto support = kitty::min_base_inplace( func );
    auto new_func = kitty::shrink_to( func, static_cast<unsigned int>( support.size() ) );

    std::vector<signal<Ntk>> children;
    if ( auto var = support.begin(); var != support.end() )
    {
      ntk.foreach_fanin( n, [&]( auto const& f, auto i ) {
        if ( *var == i )
        {
          auto const& new_f = old_to_new[f];
          children.push_back( ntk.is_complemented( f ) ? dest.create_not( new_f ) : new_f );
          if ( ++var == support.end() )
          {
            return false;
          }
        }
        return true;
      } );
    }

    if ( new_func.num_vars() == 0u )
    {
      old_to_new[n] = dest.get_constant( !kitty::is_const0( new_func ) );
    }
    else if ( new_func.num_vars() == 1u )
    {
      old_to_new[n] = *( new_func.begin() ) == 0b10 ? children.front() : dest.create_not( children.front() );
    }
    else
    {
      old_to_new[n] = dest.create_node( children, new_func );
    }

    return true;
  } );

  // POs
  ntk.foreach_po( [&]( auto const& f, auto i ) {
    auto const& new_f = old_to_new[f];
    auto s = ntk.is_complemented( f ) ? dest.create_not( new_f ) : new_f;
    auto po = dest.create_po( s );
    if constexpr ( has_has_output_name_v<Ntk> && has_get_output_name_v<Ntk> )
    {
      if ( ntk.has_output_name( i ) )
      {
        dest.set_output_name( po, ntk.get_output_name( i ) );
      }
    }
  });

  return dest;
}

} // namespace mockturtle
