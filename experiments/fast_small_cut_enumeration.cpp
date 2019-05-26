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

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <mockturtle/algorithms/cut_enumeration.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/topo_view.hpp>

std::string cut_to_str( uint64_t cut ) {
  std::vector<uint64_t> indices;
  for ( auto i = 0U; i < 64; i++ ) {
    if ( cut & ( static_cast<uint64_t>( 1 ) << i ) ) {
      indices.push_back( i );
    }
  }

  std::ostringstream oss;
  oss << "{ ";

  for ( auto i = 0U; i < indices.size(); i++ ) {
    oss << indices.at( i );

    if ( i != indices.size() - 1 ) {
      oss << ", ";
    }
  }

  oss << " }";
  return oss.str();
}

std::string cut_set_to_str( std::vector<uint64_t> cut_set ) {
  std::ostringstream oss;
  oss << "{ ";

  for ( auto i = 0U; i < cut_set.size(); i++ ) {
    oss << cut_to_str( cut_set.at( i ) );

    if ( i != cut_set.size() - 1 ) {
      oss << ", ";
    }
  }

  oss << " }";
  return oss.str();
}

int main() {
  mockturtle::aig_network aig;

  // Example circuit from lecture
  const auto x1 = aig.create_pi();
  const auto x2 = aig.create_pi();
  const auto x3 = aig.create_and( x1, x2 );
  const auto x4 = aig.create_or( x1, x3 );
  const auto x5 = aig.create_or( x2, x3 );
  const auto x6 = aig.create_and( x4, x5 );
  aig.create_po( x6 );

  mockturtle::topo_view aig_topo( aig );

  const auto [cuts_valid, cuts] = mockturtle::fast_small_cut_enumeration( aig_topo );

  if (!cuts_valid) {
    std::cerr << "Error: graph must have <= 64 nodes" << "\n";
    return EXIT_FAILURE;
  }

  for ( auto i = 0U; i < cuts.size(); i++ ) {
    auto const& cut_set( cuts.at( i ) );
    auto const& cut_set_str( cut_set_to_str( cut_set ) );
    std::cout << "Cuts of node " << i << " => " << cut_set_str << "\n";
  }

  return 0;
}
