/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2023  EPFL
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
  \file simplify_adders.hpp
  \brief Simplify boxed adders

  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include "../networks/box_aig.hpp"

namespace mockturtle
{

void simplify_half_adder_with_constant( box_aig_network& ntk, typename box_aig_network::box_id b, typename box_aig_network::signal i1, bool const1 )
{
  if ( const1 ) // i0 is const1
  {
    ntk.delete_box( b, {i1, !i1} );
  }
  else // i0 is const0
  {
    ntk.delete_box( b, {ntk.get_constant( false ), i1} );
  }
}

void simplify_half_adder_with_constant_plus_one( box_aig_network& ntk, typename box_aig_network::box_id b, typename box_aig_network::signal i1, bool const1 )
{
  if ( const1 ) // i0 is const1, 1+1+i1: 2+0=10, 2+1=11
  {
    ntk.delete_box( b, {ntk.get_constant( true ), i1} );
  }
  else // i0 is const0, 1+0+i1: 1+0=01, 1+1=10
  {
    ntk.delete_box( b, {i1, !i1} );
  }
}

bool simplify_half_adder_inner( box_aig_network& ntk, typename box_aig_network::box_id b, typename box_aig_network::signal i0, typename box_aig_network::signal i1 )
{
  if ( ntk.is_constant( ntk.get_node( i0 ) ) )
  {
    simplify_half_adder_with_constant( ntk, b, i1, ntk.is_complemented( i0 ) );
  }
  else if ( ntk.is_constant( ntk.get_node( i1 ) ) )
  {
    simplify_half_adder_with_constant( ntk, b, i0, ntk.is_complemented( i1 ) );
  }
  else if ( ntk.get_node( i0 ) == ntk.get_node( i1 ) )
  {
    if ( ntk.is_complemented( i0 ) == ntk.is_complemented( i1 ) ) // 0+0=00, 1+1=10
    {
      ntk.delete_box( b, {i0, ntk.get_constant( false )} );
    }
    else // 0+1=1+0=01
    {
      ntk.delete_box( b, {ntk.get_constant( false ), ntk.get_constant( true )} );
    }
  }
  else
  {
    return false; // nothing to be optimized
  }
  return true; // optimization has been done, outputs are substituted
}

bool simplify_half_adder_plus_one( box_aig_network& ntk, typename box_aig_network::box_id b, typename box_aig_network::signal i0, typename box_aig_network::signal i1 )
{
  if ( ntk.is_constant( ntk.get_node( i0 ) ) )
  {
    simplify_half_adder_with_constant_plus_one( ntk, b, i1, ntk.is_complemented( i0 ) );
  }
  else if ( ntk.is_constant( ntk.get_node( i1 ) ) )
  {
    simplify_half_adder_with_constant_plus_one( ntk, b, i0, ntk.is_complemented( i1 ) );
  }
  else if ( ntk.get_node( i0 ) == ntk.get_node( i1 ) )
  {
    if ( ntk.is_complemented( i0 ) == ntk.is_complemented( i1 ) ) // 1+0+0=01, 1+1+1=11
    {
      ntk.delete_box( b, {i0, ntk.get_constant( true )} );
    }
    else // 1+0+1=1+1+0=10
    {
      ntk.delete_box( b, {ntk.get_constant( true ), ntk.get_constant( false )} );
    }
  }
  else
  {
    return false; // nothing to be optimized
  }
  return true; // optimization has been done, outputs are substituted
}

void simplify_full_adder_with_constant( box_aig_network& ntk, typename box_aig_network::box_id b, typename box_aig_network::signal i1, typename box_aig_network::signal i2, bool const1 )
{
  if ( const1 ) // i0 is const1
  {
    if ( !simplify_half_adder_plus_one( ntk, b, i1, i2 ) )
    {
      // MAJ(1,i1,i2) = !AND(!i1,!i2), XOR(1,i1,i2) = !XOR(!i1,!i2)
      auto new_b = ntk.is_black_box( b ) ? ntk.create_white_box_half_adder( !i1, !i2 ) : ntk.create_black_box( 2, {!i1, !i2}, "ha" );
      ntk.delete_box( b, {!ntk.get_box_output( new_b, 0 ), !ntk.get_box_output( new_b, 1 )} );
    }
  }
  else // i0 is const0: i1, i2 is a half adder
  {
    if ( !simplify_half_adder_inner( ntk, b, i1, i2 ) )
    {
      auto new_b = ntk.is_black_box( b ) ? ntk.create_white_box_half_adder( i1, i2 ) : ntk.create_black_box( 2, {i1, i2}, "ha" );
      ntk.delete_box( b, {ntk.get_box_output( new_b, 0 ), ntk.get_box_output( new_b, 1 )} );
    }
  }
}

void simplify_full_adder_with_same_node( box_aig_network& ntk, typename box_aig_network::box_id b, typename box_aig_network::signal i0, typename box_aig_network::signal i2, bool same_sign )
{
  if ( same_sign ) // MAJ(i0,i0,i2) = i0, XOR(i0,i0,i2) = i2
  {
    ntk.delete_box( b, {i0, i2} );
  }
  else // MAJ(i0,!i0,i2) = i2, XOR(i0,!i0,i2) = !i2
  {
    ntk.delete_box( b, {i2, !i2} );
  }
}

void simplify_half_adder( box_aig_network& ntk, box_aig_network::box_id b )
{
  simplify_half_adder_inner( ntk, b, ntk.get_box_input( b, 0 ), ntk.get_box_input( b, 1 ) );
}

void simplify_full_adder( box_aig_network& ntk, box_aig_network::box_id b )
{
  auto i0 = ntk.get_box_input( b, 0 );
  auto i1 = ntk.get_box_input( b, 1 );
  auto i2 = ntk.get_box_input( b, 2 );
  auto o0 = ntk.get_box_output( b, 0 ); // carry (MAJ)
  auto o1 = ntk.get_box_output( b, 1 ); // sum (XOR)
  if ( ntk.is_constant( ntk.get_node( i0 ) ) )
  {
    simplify_full_adder_with_constant( ntk, b, i1, i2, ntk.is_complemented( i0 ) );
  }
  else if ( ntk.is_constant( ntk.get_node( i1 ) ) )
  {
    simplify_full_adder_with_constant( ntk, b, i0, i2, ntk.is_complemented( i1 ) );
  }
  else if ( ntk.is_constant( ntk.get_node( i2 ) ) )
  {
    simplify_full_adder_with_constant( ntk, b, i0, i1, ntk.is_complemented( i2 ) );
  }
  else if ( ntk.get_node( i0 ) == ntk.get_node( i1 ) )
  {
    simplify_full_adder_with_same_node( ntk, b, i0, i2, ntk.is_complemented( i0 ) == ntk.is_complemented( i1 ) );
  }
  else if ( ntk.get_node( i0 ) == ntk.get_node( i2 ) )
  {
    simplify_full_adder_with_same_node( ntk, b, i0, i1, ntk.is_complemented( i0 ) == ntk.is_complemented( i2 ) );
  }
  else if ( ntk.get_node( i1 ) == ntk.get_node( i2 ) )
  {
    simplify_full_adder_with_same_node( ntk, b, i1, i0, ntk.is_complemented( i1 ) == ntk.is_complemented( i2 ) );
  }
}

void simplify_adders( box_aig_network& ntk )
{
  ntk.foreach_box( [&]( auto b ){
    if ( ntk.num_box_inputs( b ) == 2 )
    {
      auto i0 = ntk.get_node( ntk.get_box_input( b, 0 ) );
      auto i1 = ntk.get_node( ntk.get_box_input( b, 1 ) );
      if ( ntk.is_constant( i0 ) || ntk.is_constant( i1 ) || i0 == i1 )
        simplify_half_adder( ntk, b );
    }
    else if ( ntk.num_box_inputs( b ) == 3 )
    {
      auto i0 = ntk.get_node( ntk.get_box_input( b, 0 ) );
      auto i1 = ntk.get_node( ntk.get_box_input( b, 1 ) );
      auto i2 = ntk.get_node( ntk.get_box_input( b, 2 ) );
      if ( ntk.is_constant( i0 ) || ntk.is_constant( i1 ) || ntk.is_constant( i2 ) || i0 == i1 || i0 == i2 || i1 == i2 )
        simplify_full_adder( ntk, b );
    }
  } );
}

} // namespace mockturtle