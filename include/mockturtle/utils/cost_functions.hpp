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
  \file cost_functions.hpp
  \brief Various cost functions for (optimization) algorithms

  \author Heinz Riener
  \author Mathias Soeken
*/

#pragma once

#include <cstdint>

#include "../traits.hpp"

namespace mockturtle
{

template<typename CostFn>
using cost = typename CostFn::cost;

template<class Ntk>
struct and_cost
{
public:
  using cost = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    /* dissipate cost */
    if( ntk.is_and( n ) ) tot_cost += 1; /* add dissipate cost */
    /* accumulate cost */
    return 0; /* return accumulate cost */
  }
};

template<class Ntk>
struct gate_cost
{
public:
  using cost = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    /* dissipate cost */
    if( ntk.is_pi( n ) == false ) tot_cost += 1; /* add dissipate cost */
    /* accumulate cost */
    return 0; /* return accumulate cost */
  }
};

template<class Ntk>
struct fanout_cost
{
public:
  using cost = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    /* dissipate cost */
    if ( ntk.is_pi( n ) == false ) tot_cost += ntk.fanout( n ).size(); /* add dissipate cost */
    /* accumulate cost */
    return 0; /* return accumulate cost */
  }
};

template<class Ntk>
struct supp_cost
{
public:
  using cost = std::vector<bool>;
  cost operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<cost> const& fanin_costs = {} ) const
  {
    /* accumulate cost */
    if ( ntk.is_pi( n ) == true )
    {
      cost _c = std::vector<bool>();
      uint32_t pi_cost = 0u;
      ntk.foreach_pi( [&]( node<Ntk> const& _n, uint32_t i ){
                        _c.emplace_back( ntk.make_signal( _n ) == ntk.make_signal( n ) );
                        pi_cost += ntk.make_signal( _n ) == ntk.make_signal( n ) ;
                      } );
      assert( pi_cost == 1 ); // each PI is support only by itself
      assert( _c.size() == ntk.num_pis() );
      return _c;
    }
    /* dissipate cost */
    std::vector<bool> _c( fanin_costs[0].size(), false );
    for ( auto j = 0u; j < fanin_costs.size(); j++ )
    {
      std::vector<bool> const& fanin_cost = fanin_costs[j];
      for ( auto i = 0u; i < fanin_cost.size(); i++ )
      {
        _c[i] = _c[i] || fanin_cost[i];
      }
    }
    for ( auto i = 0u; i < fanin_costs[0].size(); i++ )
    {
      tot_cost += _c[i]; /* add dissipate cost */
    }
    return _c; /* return accumulate cost */
  }
};

template<class Ntk>
struct and_supp_cost
{
public:
  using cost = std::vector<bool>;
  cost operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<cost> const& fanin_costs = {} ) const
  {
    /* accumulate cost */
    if ( ntk.is_pi( n ) == true )
    {
      cost _c = std::vector<bool>();
      uint32_t pi_cost = 0u;
      ntk.foreach_pi( [&]( node<Ntk> const& _n, uint32_t i ){
                        _c.emplace_back( ntk.make_signal( _n ) == ntk.make_signal( n ) );
                        pi_cost += ntk.make_signal( _n ) == ntk.make_signal( n ) ;
                      } );
      assert( pi_cost == 1 ); // each PI is support only by itself
      assert( _c.size() == ntk.num_pis() );
      return _c;
    }
    /* dissipate cost */
    std::vector<bool> _c( fanin_costs[0].size(), false );
    for ( auto j = 0u; j < fanin_costs.size(); j++ )
    {
      std::vector<bool> const& fanin_cost = fanin_costs[j];
      for ( auto i = 0u; i < fanin_cost.size(); i++ )
      {
        _c[i] = _c[i] || fanin_cost[i];
      }
    }
    if ( ntk.is_and( n ) )
    {
      for ( auto i = 0u; i < fanin_costs[0].size(); i++ )
      {
        tot_cost += _c[i]; /* add dissipate cost */
      }      
    }
    return _c; /* return accumulate cost */
  }
};

template<class Ntk>
struct level_cost
{
public:
  using cost = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    /* accumulate cost */
    uint32_t _cost = 0u;
    for ( uint32_t fanin_cost : fanin_costs )
    {
      _cost = std::max( _cost, fanin_cost );
    }
    if( !ntk.is_pi( n ) ) _cost += 1; // for both AND and XOR

    /* dissipate cost */
    tot_cost = std::max( tot_cost, _cost ); /* update dissipate cost */
    return _cost; /* return accumulate cost */
  }
};

template<class Ntk>
struct t_depth
{
public:
  using cost = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    /* accumulate cost */
    uint32_t _cost = 0u;
    for ( uint32_t fanin_cost : fanin_costs )
    {
      _cost = std::max( _cost, fanin_cost );
    }
    _cost += ntk.is_and( n ); // and depth

    /* dissipate cost */
    if( ntk.fanout( n ).size() == 0 ) tot_cost = std::max( tot_cost, _cost ); /* update dissipate cost */
    return _cost; /* return accumulate cost */
  }
};

template<class Ntk>
struct and_adp
{
public:
  using cost = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    /* accumulate cost */
    uint32_t _cost = 0u;
    for ( uint32_t fanin_cost : fanin_costs )
    {
      _cost = std::max( _cost, fanin_cost );
    }
    _cost += ntk.is_and( n );

    /* dissipate cost */
    if( ntk.is_and( n ) ) tot_cost += _cost; /* update dissipate cost */
    return _cost; /* return accumulate cost */
  }
};


template<class Ntk>
struct adp_cost
{
public:
  using cost = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    /* accumulate cost */
    uint32_t _cost = 0u;
    for ( uint32_t fanin_cost : fanin_costs )
    {
      _cost = std::max( _cost, fanin_cost );
    }
    _cost += 1; // for both AND and XOR

    /* dissipate cost */
    if( ntk.is_and( n ) || ntk.is_xor( n ) )  tot_cost += _cost; /* sum of level over each node */
    return _cost; /* return accumulate cost */
  }
};

template<class Ntk>
struct unit_cost
{
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& node ) const
  {
    (void)ntk;
    (void)node;
    return 1u;
  }
};

template<class Ntk>
struct mc_cost
{
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& node ) const
  {
    if constexpr ( has_is_xor_v<Ntk> )
    {
      if ( ntk.is_xor( node ) )
      {
        return 0u;
      }
    }

    if constexpr ( has_is_xor3_v<Ntk> )
    {
      if ( ntk.is_xor3( node ) )
      {
        return 0u;
      }
    }

    if constexpr ( has_is_nary_and_v<Ntk> )
    {
      if ( ntk.is_nary_and( node ) )
      {
        if ( ntk.fanin_size( node ) > 1u )
        {
          return ntk.fanin_size( node ) - 1u;
        }
        return 0u;
      }
    }

    if constexpr ( has_is_nary_or_v<Ntk> )
    {
      if ( ntk.is_nary_or( node ) )
      {
        if ( ntk.fanin_size( node ) > 1u )
        {
          return ntk.fanin_size( node ) - 1u;
        }
        return 0u;
      }
    }

    if constexpr ( has_is_nary_xor_v<Ntk> )
    {
      if ( ntk.is_nary_xor( node ) )
      {
        return 0u;
      }
    }

    // TODO (Does not take into account general node functions)
    return 1u;
  }
};

template<class Ntk, class NodeCostFn = unit_cost<Ntk>>
uint32_t costs( Ntk const& ntk )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate method" );

  uint32_t total{0u};
  NodeCostFn cost_fn{};
  ntk.foreach_gate( [&]( auto const& n ) {
    total += cost_fn( ntk, n );
  });
  return total;
}

} /* namespace mockturtle */