/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
  \file verilog_reader.hpp
  \brief Lorina reader for VERILOG files

  \author Heinz Riener
  \author Marcel Walter
  \author Mathias Soeken
  \author Siang-Yun (Sonia) Lee
*/

#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <lorina/verilog.hpp>

#include "../generators/arithmetic.hpp"
#include "../generators/modular_arithmetic.hpp"
#include "../traits.hpp"

namespace mockturtle
{

/*! \brief Lorina reader callback for VERILOG files.
 *
 * **Required network functions:**
 * - `create_pi`
 * - `create_po`
 * - `get_constant`
 * - `create_not`
 * - `create_and`
 * - `create_or`
 * - `create_xor`
 * - `create_maj`
 *
 * Unsupported constructs, invalid arithmetic arguments, undefined operands, and undriven outputs throw `std::invalid_argument`.
 * Sized hexadecimal literals contribute their low bits, limited by the declared
 * width and the destination width. Decimal values and declared widths outside
 * the 64-bit unsigned range throw `std::out_of_range`.
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      mig_network mig;
      lorina::read_verilog( "file.v", verilog_reader( mig ) );
   \endverbatim
 */
template<typename Ntk>
class verilog_reader : public lorina::verilog_reader
{
public:
  explicit verilog_reader( Ntk& ntk, std::string const& top_module_name = "top" ) : ntk_( ntk ), top_module_name_( top_module_name )
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi function" );
    static_assert( has_create_po_v<Ntk>, "Ntk does not implement the create_po function" );
    static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant function" );
    static_assert( has_create_not_v<Ntk>, "Ntk does not implement the create_not function" );
    static_assert( has_create_and_v<Ntk>, "Ntk does not implement the create_and function" );
    static_assert( has_create_or_v<Ntk>, "Ntk does not implement the create_or function" );
    static_assert( has_create_xor_v<Ntk>, "Ntk does not implement the create_xor function" );
    static_assert( has_create_ite_v<Ntk>, "Ntk does not implement the create_ite function" );
    static_assert( has_create_maj_v<Ntk>, "Ntk does not implement the create_maj function" );

    signals_["0"] = ntk_.get_constant( false );
    signals_["1"] = ntk_.get_constant( true );
    signals_["1'b0"] = ntk_.get_constant( false );
    signals_["1'b1"] = ntk_.get_constant( true );
  }

  void on_module_header( const std::string& module_name, const std::vector<std::string>& inouts ) const override
  {
    (void)inouts;
    if constexpr ( has_set_network_name_v<Ntk> )
    {
      ntk_.set_network_name( module_name );
    }

    name_ = module_name;
    if ( name_ != top_module_name_ && is_arithmetic_module( name_ ) )
      arithmetic_ports_[name_] = {};
  }

  void on_inputs( const std::vector<std::string>& names, std::string const& size = "" ) const override
  {
    (void)size;
    if ( name_ != top_module_name_ )
    {
      if ( is_arithmetic_module( name_ ) )
      {
        auto& ports = arithmetic_ports_[name_].inputs;
        ports.insert( ports.end(), names.begin(), names.end() );
      }
      return;
    }

    for ( const auto& name : names )
    {
      if ( size.empty() )
      {
        signals_[name] = ntk_.create_pi();
        input_names_.emplace_back( name, 1u );
        if constexpr ( has_set_name_v<Ntk> )
        {
          ntk_.set_name( signals_[name], name );
        }
      }
      else
      {
        std::vector<signal<Ntk>> word;
        const auto length = parse_size( size );
        for ( auto i = 0u; i < length; ++i )
        {
          const auto sname = fmt::format( "{}[{}]", name, i );
          word.push_back( ntk_.create_pi() );
          signals_[sname] = word.back();
          if constexpr ( has_set_name_v<Ntk> )
          {
            ntk_.set_name( signals_[sname], sname );
          }
        }
        registers_[name] = word;
        input_names_.emplace_back( name, length );
      }
    }
  }

  void on_outputs( const std::vector<std::string>& names, std::string const& size = "" ) const override
  {
    (void)size;
    if ( name_ != top_module_name_ )
    {
      if ( is_arithmetic_module( name_ ) )
      {
        auto& ports = arithmetic_ports_[name_].outputs;
        ports.insert( ports.end(), names.begin(), names.end() );
      }
      return;
    }

    for ( const auto& name : names )
    {
      if ( size.empty() )
      {
        outputs_.emplace_back( name );
        output_names_.emplace_back( name, 1u );
      }
      else
      {
        const auto length = parse_size( size );
        for ( auto i = 0u; i < length; ++i )
        {
          outputs_.emplace_back( fmt::format( "{}[{}]", name, i ) );
        }
        output_names_.emplace_back( name, length );
      }
    }
  }

  void on_assign( const std::string& lhs, const std::pair<std::string, bool>& rhs ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    auto r = read_signal( rhs.first );
    signals_[lhs] = rhs.second ? ntk_.create_not( r ) : r;
  }

  void on_nand( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    if constexpr ( has_create_nand_v<Ntk> )
      signals_[lhs] = ntk_.create_nand( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b );
    else
      signals_[lhs] = ntk_.create_not( ntk_.create_and( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b ) );
  }

  void on_and( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    if constexpr ( is_crossed_network_type_v<Ntk> )
    {
      if ( !op1.second && !op2.second )
        signals_[lhs] = ntk_.create_and( a, b );
      else if ( !op1.second && op2.second )
        signals_[lhs] = ntk_.create_gt( a, b ); // a & !b
      else if ( op1.second && !op2.second )
        signals_[lhs] = ntk_.create_lt( a, b ); // !a & b
      else
        signals_[lhs] = ntk_.create_nor( a, b ); // !a & !b = !(a | b)
    }
    else
    {
      signals_[lhs] = ntk_.create_and( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b );
    }
  }

  void on_or( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    if constexpr ( is_crossed_network_type_v<Ntk> )
    {
      if ( !op1.second && !op2.second )
        signals_[lhs] = ntk_.create_or( a, b );
      else if ( !op1.second && op2.second )
        signals_[lhs] = ntk_.create_ge( a, b ); // a | !b
      else if ( op1.second && !op2.second )
        signals_[lhs] = ntk_.create_le( a, b ); // !a | b
      else
        signals_[lhs] = ntk_.create_nand( a, b ); // !a | !b = !(a & b)
    }
    else
    {
      signals_[lhs] = ntk_.create_or( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b );
    }
  }

  void on_nor( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    if constexpr ( has_create_nor_v<Ntk> )
      signals_[lhs] = ntk_.create_nor( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b );
    else
      signals_[lhs] = ntk_.create_not( ntk_.create_or( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b ) );
  }

  void on_xor( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    if constexpr ( is_crossed_network_type_v<Ntk> )
    {
      if ( !op1.second == !op2.second )
        signals_[lhs] = ntk_.create_xor( a, b );
      else
        signals_[lhs] = ntk_.create_xnor( a, b );
    }
    else
    {
      signals_[lhs] = ntk_.create_xor( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b );
    }
  }

  void on_xnor( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    if constexpr ( has_create_xnor_v<Ntk> )
      signals_[lhs] = ntk_.create_xnor( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b );
    else
      signals_[lhs] = ntk_.create_not( ntk_.create_xor( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b ) );
  }

  void on_xor3( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2, const std::pair<std::string, bool>& op3 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    if constexpr ( is_crossed_network_type_v<Ntk> )
    {
      assert( false && "3-input gates in crossed_network are not supported (to be implemented)" );
    }

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    auto c = read_signal( op3.first );

    if constexpr ( has_create_xor3_v<Ntk> )
    {
      signals_[lhs] = ntk_.create_xor3( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b, op3.second ? ntk_.create_not( c ) : c );
    }
    else
    {
      signals_[lhs] = ntk_.create_xor( ntk_.create_xor( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b ), op3.second ? ntk_.create_not( c ) : c );
    }
  }

  void on_maj3( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2, const std::pair<std::string, bool>& op3 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    if constexpr ( is_crossed_network_type_v<Ntk> )
    {
      assert( false && "3-input gates in crossed_network are not supported (to be implemented)" );
    }

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    auto c = read_signal( op3.first );
    signals_[lhs] = ntk_.create_maj( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b, op3.second ? ntk_.create_not( c ) : c );
  }

  void on_mux21( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2, const std::pair<std::string, bool>& op3 ) const override
  {
    if ( name_ != top_module_name_ )
      return;

    if constexpr ( is_crossed_network_type_v<Ntk> )
    {
      assert( false && "3-input gates in crossed_network are not supported (to be implemented)" );
    }

    auto a = read_signal( op1.first );
    auto b = read_signal( op2.first );
    auto c = read_signal( op3.first );
    signals_[lhs] = ntk_.create_ite( op1.second ? ntk_.create_not( a ) : a, op2.second ? ntk_.create_not( b ) : b, op3.second ? ntk_.create_not( c ) : c );
  }

  void on_module_instantiation( std::string const& module_name, std::vector<std::string> const& params, std::string const& inst_name,
                                std::vector<std::pair<std::string, std::string>> const& args ) const override
  {
    (void)inst_name;
    if ( name_ != top_module_name_ )
      return;

    const auto check_port_count = [&]( std::size_t expected_count ) {
      if ( args.size() != expected_count )
        throw std::invalid_argument( fmt::format( "{} expects {} ports", module_name, expected_count ) );
    };

    const auto connection = [&]( std::string_view port ) -> const std::string& {
      const auto it = std::find_if( args.begin(), args.end(), [&]( const auto& arg ) { return arg.first == port; } );
      if ( it == args.end() || it->second.empty() )
        throw std::invalid_argument( fmt::format( "missing port {} on {}", port, module_name ) );
      return it->second;
    };

    const auto add_register = [&]( std::string const& name, std::vector<signal<Ntk>> fs ) {
      for ( auto i = 0u; i < fs.size(); ++i )
      {
        signals_[fmt::format( "{}[{}]", name, i )] = fs[i];
      }
      registers_[name] = std::move( fs );
    };

    if ( is_arithmetic_module( module_name ) )
    {
      const auto declared = arithmetic_ports_.find( module_name );
      if ( declared == arithmetic_ports_.end() || declared->second.inputs.size() != 2u || declared->second.outputs.size() != 1u || args.size() != 3u )
        throw std::invalid_argument( fmt::format( "{} expects two input ports and one output port", module_name ) );

      const std::array<std::string_view, 3> ports{ declared->second.inputs[0], declared->second.inputs[1], declared->second.outputs[0] };
      std::array<std::string const*, 3> actual{};
      for ( auto const& arg : args )
      {
        const auto pin = std::string_view( arg.first );
        const auto it = pin.empty() || pin.front() != '.' ? ports.end() : std::find( ports.begin(), ports.end(), pin.substr( 1 ) );
        if ( it == ports.end() || actual[it - ports.begin()] )
          throw std::invalid_argument( fmt::format( "invalid port {} on {}", arg.first, module_name ) );
        actual[it - ports.begin()] = &arg.second;
      }

      const bool is_adder = module_name == "ripple_carry_adder";
      const auto expected_params = is_adder ? 1u : 3u;
      if ( params.size() != expected_params )
        throw std::invalid_argument( fmt::format( "{} expects {} parameters", module_name, expected_params ) );
      const auto bitwidth = parse_small_value( params[0] );
      if ( bitwidth == 0u || bitwidth >= std::numeric_limits<uint32_t>::max() )
        throw std::invalid_argument( fmt::format( "invalid register width on {}", module_name ) );

      const auto operand = [&]( const std::string& name ) -> const std::vector<signal<Ntk>>& {
        const auto it = registers_.find( name );
        if ( it == registers_.end() || it->second.size() != bitwidth )
          throw std::invalid_argument( fmt::format( "register {} must have width {}", name, bitwidth ) );
        return it->second;
      };
      const auto& a = operand( *actual[0] );
      const auto& b = operand( *actual[1] );
      if ( is_adder )
      {
        auto a_copy = a;
        auto carry = ntk_.get_constant( false );
        carry_ripple_adder_inplace( ntk_, a_copy, b, carry );
        a_copy.push_back( carry );
        add_register( *actual[2], std::move( a_copy ) );
      }
      else
      {
        auto N = parse_value( params[1], bitwidth );
        auto NN = parse_value( params[2], bitwidth );
        N.resize( bitwidth );
        NN.resize( bitwidth );
        add_register( *actual[2], montgomery_multiplication( ntk_, a, b, N, NN ) );
      }
    }
    else if ( module_name == "buffer" || module_name == "inverter" )
    {
      if constexpr ( is_buffered_network_type_v<Ntk> )
      {
        static_assert( has_create_buf_v<Ntk>, "Ntk does not implement the create_buf method" );
        check_port_count( 2u );
        const auto& lhs = connection( ".o" );
        const auto buffer = ntk_.create_buf( read_signal( connection( ".i" ) ) );
        if ( module_name == "inverter" )
          ntk_.invert( ntk_.get_node( buffer ) );
        signals_[lhs] = buffer;
      }
      else
      {
        throw std::invalid_argument( fmt::format( "network does not support module {}", module_name ) );
      }
    }
    else if ( module_name == "crossing" )
    {
      if constexpr ( is_crossed_network_type_v<Ntk> )
      {
        check_port_count( 4u );
        const auto fi1 = read_signal( connection( ".i1" ) );
        const auto fi2 = read_signal( connection( ".i2" ) );
        const auto& fo1 = connection( ".o1" );
        const auto& fo2 = connection( ".o2" );

        if ( signals_.find( fo1 ) == signals_.end() ) // due to lorina bug, each crossing will be instantiated twice...
        {
          auto p = ntk_.create_crossing( fi1, fi2 );
          signals_[fo1] = p.first;
          signals_[fo2] = p.second;
        }
      }
      else
      {
        throw std::invalid_argument( fmt::format( "network does not support module {}", module_name ) );
      }
    }
    else
    {
      throw std::invalid_argument( fmt::format( "unsupported module {}", module_name ) );
    }
  }

  void on_endmodule() const override
  {
    if ( name_ != top_module_name_ )
      return;

    std::vector<signal<Ntk>> output_signals;
    output_signals.reserve( outputs_.size() );
    for ( auto const& o : outputs_ )
    {
      const auto it = signals_.find( o );
      if ( it == signals_.end() )
        throw std::invalid_argument( fmt::format( "undriven Verilog output {}", o ) );
      output_signals.push_back( it->second );
    }
    for ( auto const& s : output_signals )
      ntk_.create_po( s );

    if constexpr ( has_set_output_name_v<Ntk> )
    {
      uint32_t ctr{ 0u };
      for ( auto const& output_name : output_names_ )
      {
        if ( output_name.second == 1u )
        {
          ntk_.set_output_name( ctr++, output_name.first );
        }
        else
        {
          for ( auto i = 0u; i < output_name.second; ++i )
          {
            ntk_.set_output_name( ctr++, fmt::format( "{}[{}]", output_name.first, i ) );
          }
        }
      }
      assert( ctr == ntk_.num_pos() );
    }
  }

  const std::string& name() const
  {
    return name_;
  }

  const std::vector<std::pair<std::string, uint32_t>> input_names()
  {
    return input_names_;
  }

  const std::vector<std::pair<std::string, uint32_t>> output_names()
  {
    return output_names_;
  }

private:
  /*! \brief Return the signal for a defined operand. */
  signal<Ntk> read_signal( const std::string& name ) const
  {
    const auto it = signals_.find( name );
    if ( it == signals_.end() )
      throw std::invalid_argument( fmt::format( "undefined Verilog signal {}", name ) );
    return it->second;
  }

  /*! \brief Recognize the modules implemented by the arithmetic generators. */
  static bool is_arithmetic_module( std::string_view name )
  {
    return name == "ripple_carry_adder" || name == "montgomery_multiplier";
  }

  std::vector<bool> parse_value( const std::string& value, uint64_t max_width ) const
  {
    std::smatch match;

    if ( !value.empty() && std::all_of( value.begin(), value.end(), []( char c ) { return c >= '0' && c <= '9'; } ) )
    {
      std::vector<bool> res( std::min<uint64_t>( 64u, max_width ) );
      bool_vector_from_dec( res, std::stoull( value ) );
      return res;
    }
    else if ( std::regex_match( value, match, hex_string ) )
    {
      const auto width = std::stoull( match.str( 1 ) );
      if ( width == 0u )
        throw std::invalid_argument( "a Verilog literal must have positive width" );
      std::vector<bool> res( std::min<uint64_t>( width, max_width ) );
      bool_vector_from_hex( res, match.str( 2 ) );
      return res;
    }
    throw std::invalid_argument( fmt::format( "cannot parse Verilog number '{}'", value ) );
  }

  uint64_t parse_small_value( const std::string& value ) const
  {
    const auto bits = parse_value( value, 64u );
    uint64_t result = 0u;
    for ( auto it = bits.rbegin(); it != bits.rend(); ++it )
      result = ( result << 1 ) | *it;
    return result;
  }

  uint32_t parse_size( const std::string& size ) const
  {
    if ( size.empty() )
    {
      return 1u;
    }

    if ( auto const l = size.size(); l > 2 && size[l - 2] == ':' && size[l - 1] == '0' )
    {
      const auto high = parse_small_value( size.substr( 0u, l - 2 ) );
      if ( high < std::numeric_limits<uint32_t>::max() )
        return static_cast<uint32_t>( high + 1u );
    }

    throw std::invalid_argument( fmt::format( "unsupported Verilog port range [{}]", size ) );
  }

private:
  Ntk& ntk_;

  std::string const top_module_name_;

  mutable std::map<std::string, signal<Ntk>> signals_;
  mutable std::map<std::string, std::vector<signal<Ntk>>> registers_;
  mutable std::vector<std::string> outputs_;
  mutable std::string name_;
  mutable std::vector<std::pair<std::string, uint32_t>> input_names_;
  mutable std::vector<std::pair<std::string, uint32_t>> output_names_;

  /*! \brief Formal ports of an arithmetic module. */
  struct arithmetic_ports
  {
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
  };

  /*! \brief Arithmetic module ports indexed by module name. */
  mutable std::map<std::string, arithmetic_ports> arithmetic_ports_;

  std::regex hex_string{ "(\\d+)'h([0-9a-fA-F]+)" };
};

} /* namespace mockturtle */
