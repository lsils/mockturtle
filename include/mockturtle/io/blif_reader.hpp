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
  \file blif_reader.hpp
  \brief Lorina reader for BLIF files

  \author Heinz Riener
*/

#pragma once

// #include <iostream>
// #include <map>

#include <mockturtle/networks/aig.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <lorina/blif.hpp>

#include <string>
#include <vector>

#include "../traits.hpp"

namespace mockturtle
{

template<typename Ntk>
class blif_reader : public lorina::blif_reader
{
public:
  explicit blif_reader( Ntk& ntk )
    : ntk_( ntk )
  {}

  ~blif_reader()
  {
    for ( auto const& o : outputs )
    {
      ntk_.create_po( signals[o], o );
    }    
  }
  
  virtual void on_model( const std::string& model_name ) const override
  {
    (void)model_name;
  }

  virtual void on_input( const std::string& name ) const override
  {
    signals[name] = ntk_.create_pi( name );
  }

  virtual void on_output( const std::string& name ) const override
  {
    outputs.emplace_back( name );
  }

  virtual void on_gate( const std::vector<std::string>& inputs, const std::string& output, const output_cover_t& cover ) const override
  {
    auto const len = 1u << inputs.size();

    std::string func( len, '0' );
    for ( const auto& c : cover )
    {
      assert( c.second.size() == 1 );
      
      uint32_t pos = 0u;
      for ( auto i = 0u; i < c.first.size(); ++i )
      {
        if ( c.first.at( i ) == '1' )
          pos += 1u << i;
      }

      func[pos] = c.second.at( 0u );
    }

    kitty::dynamic_truth_table tt( static_cast<int>( inputs.size() ) );
    kitty::create_from_binary_string( tt, func );
      
    std::vector<signal<Ntk>> input_signals;
    for ( const auto& i : inputs )
    {
      input_signals.emplace_back( signals[i] );
    }
    signals[output] = ntk_.create_node( input_signals, tt );
  }

  virtual void on_end() const override {}

  virtual void on_comment( const std::string& comment ) const override
  {
    (void)comment;
  }
  
private:
  Ntk& ntk_;

  mutable std::map<std::string, signal<Ntk>> signals;
  mutable std::vector<std::string> outputs;
}; /* blif_reader */

} /* namespace mockturtle */
