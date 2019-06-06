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
  \file bi_decomposition_f.hpp
  \brief BI decomposition

  \author Eleonora Testa 
*/

#pragma once

#include <cstdint>
#include <vector>

#include "../traits.hpp"

#include <kitty/constructors.hpp>
#include <kitty/decomposition.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>

namespace mockturtle
{

namespace detail
{

template<class Ntk>
class bi_decomposition_impl
{
public:
  bi_decomposition_impl( Ntk& ntk, kitty::dynamic_truth_table const& func, kitty::dynamic_truth_table const& dc, std::vector<signal<Ntk>> const& children )
      : _ntk( ntk ),
        remainder( func ),
        dc_remainder( dc ),
        support( children.size() ),
        pis( children )
  {
    std::iota( support.begin(), support.end(), 0u );
  }

  signal<Ntk> run()
  {
    /* bi_decomposition */
    auto bi_dec = kitty::is_bi_decomposable( remainder, dc_remainder );
    if ( auto res = std::get<1>( bi_dec );
         res != kitty::bi_decomposition::none )
    {
      remainder = std::get<2>( bi_dec )[0];
      dc_remainder = std::get<2>( bi_dec )[1];
      const auto right = run();

      remainder = std::get<2>( bi_dec )[2];
      dc_remainder = std::get<2>( bi_dec )[3];
      const auto left = run();

      switch ( res )
      {
      default:
        //assert( false );
      case kitty::bi_decomposition::and_:
        return _ntk.create_and( left, right );
      case kitty::bi_decomposition::or_:
        return _ntk.create_or( left, right );
      case kitty::bi_decomposition::weak_and_:
        return _ntk.create_and( left, right );
      case kitty::bi_decomposition::weak_or_:
        return _ntk.create_or( left, right );
      case kitty::bi_decomposition::xor_:
        return _ntk.create_xor( left, right );
      }
    }
    else
    {
      if ( kitty::is_const0( binary_and( remainder, dc_remainder ) ) )
      {
        return _ntk.get_constant( false );
      }
      else if ( kitty::is_const0( binary_and( ~remainder, dc_remainder ) ) )
      {
        return _ntk.get_constant( true );
      }
      else
      {
        for ( auto h = 0; h < remainder.num_vars(); h++ )
        {
          auto var = remainder.construct();
          kitty::create_nth_var( var, h );
          if ( binary_and( remainder, dc_remainder ) == var )
          {
            return pis[h];
          }
          else if (binary_and( remainder, dc_remainder ) == ~var )
          {
            return _ntk.create_not( pis[h] );
          }
        }
      }
    }

  
  }

private:
  Ntk& _ntk;
  kitty::dynamic_truth_table remainder;
  kitty::dynamic_truth_table dc_remainder;
  std::vector<uint8_t> support;
  std::vector<signal<Ntk>> pis;
};

} // namespace detail

/*! \brief Bi decomposition
 *
 * This function applies BI decomposition on an input truth table and
 * constructs a network based on all possible decompositions.  
 *
 * TO BE CONTINUED
 *
 * **Required network functions:**
 * - `create_not`
 * - `create_and`
 * - `create_or`
 * - `create_xor`
 */

template<class Ntk>
signal<Ntk> bi_decomposition_f( Ntk& ntk, kitty::dynamic_truth_table const& func, kitty::dynamic_truth_table const& dc, std::vector<signal<Ntk>> const& children )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_create_not_v<Ntk>, "Ntk does not implement the create_not method" );
  static_assert( has_create_and_v<Ntk>, "Ntk does not implement the create_and method" );
  static_assert( has_create_or_v<Ntk>, "Ntk does not implement the create_or method" );
  static_assert( has_create_xor_v<Ntk>, "Ntk does not implement the create_xor method" );

  detail::bi_decomposition_impl<Ntk> impl( ntk, func, dc, children );
  return impl.run();
}

} // namespace mockturtle
