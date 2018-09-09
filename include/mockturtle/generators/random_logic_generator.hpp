/* mockturtle: C++ logic network library
 * Copyright (C) 2018  EPFL
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
  \file random_logic_generator.hpp
  \brief Generate a random logic network

  \author Heinz Riener
*/

#pragma once

namespace mockturtle
{

/*! \brief Generates a random logic network
 *
 * Abstract interface for generating a random logic network.
 *
 */
template<typename Ntk>
class random_logic_generator;
{
public:
  explicit random_logic_generator() = default;
};

/*! \brief Generates a random aig_network
 *
 * Generate a random logic network with a fixed number of primary
 * inputs, a fixed number of gates, and an unrestricted number of
 * primary outputs.  All nodes with no parents are primary outputs.
 *
 */
template<>
class random_logic_generator<aig_network>
{
public:
  using node = aig_network::node;
  using signal = aig_network::signal;

public:
  explicit random_logic_generator() = default;

  aig_network generate( uint32_t num_inputs, uint32_t num_gates, uint64_t seed = 0xcafeaffe )
  {
    std::vector<signal> fs;
    aig_network aig;

    /* generate pis */
    for ( auto i = 0; i < num_inputs; ++i )
    {
      fs.emplace_back( aig.create_pi() );
    }

    /* generate gates */
    std::mt19937 rng( seed );
    auto gate_counter = aig.num_gates();
    while ( gate_counter < num_gates )
    {
      std::uniform_int_distribution<int> dist( 0, fs.size()-1 );
      auto const le = dist( rng );
      auto const ri = dist( rng );
      auto const le_compl = dist( rng ) & 1;
      auto const ri_compl = dist( rng ) & 1;

      auto const g = aig.create_and( le_compl ? !le : le, ri_compl ? !ri : ri );
      
      if ( aig.num_gates() > gate_counter )
      {
        fs.emplace_back( g  );
        ++gate_counter;
      }

      assert( aig.num_gates() == gate_counter );
    }

    /* generate pos */
    aig.foreach_node( [&]( auto const& n ){
        auto const size = aig.fanout_size( n );
        if ( size == 0u )
        {
          aig.create_po( aig.make_signal( n ) );
        }
      });

    assert( aig.num_pis() == num_inputs );
    assert( aig.num_gates() == num_gates );

    return aig;
  } 
};

} // namespace mockturtle
