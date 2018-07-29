/* lorina: C++ parsing library
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
  \file verilog.hpp
  \brief Implements simplistic Verilog parser

  \author Heinz Riener
*/

#pragma once

#include <lorina/common.hpp>
#include <lorina/diagnostics.hpp>
#include <lorina/detail/utils.hpp>
#include <lorina/detail/tokenizer.hpp>
#include <regex>
#include <iostream>

namespace lorina
{

/*! \brief A reader visitor for a simplistic VERILOG format.
 *
 * Callbacks for the VERILOG format.
 */
class verilog_reader
{
public:
  /*! \brief Callback method for parsed module.
   *
   * \param model_name Name of the module
   * \param inouts Container for input and output names
   */
  virtual void on_module_header( const std::string& module_name, const std::vector<std::string>& inouts ) const
  {
    (void)module_name;
    (void)inouts;
  }

  /*! \brief Callback method for parsed inputs.
   *
   * \param inputs Input names
   */
  virtual void on_inputs( const std::vector<std::string>& inputs ) const
  {
    (void)inputs;
  }

  /*! \brief Callback method for parsed outputs.
   *
   * \param outputs Output names
   */
  virtual void on_outputs( const std::vector<std::string>& outputs ) const
  {
    (void)outputs;
  }

  /*! \brief Callback method for parsed wires.
   *
   * \param wires Wire names
   */
  virtual void on_wires( const std::vector<std::string>& wires ) const
  {
    (void)wires;
  }

  /*! \brief Callback method for parsed immediate assignment of form `LHS = RHS ;`.
   *
   * \param lhs Left-hand side of assignment
   * \param rhs Right-hand side of assignment
   */
  virtual void on_assign( const std::string& lhs, const std::pair<std::string, bool>& rhs ) const
  {
    (void)lhs;
    (void)rhs;
  }

  /*! \brief Callback method for parsed and gate `LHS = OP1 & OP2 ;`.
   *
   * \param lhs Left-hand side of assignment
   * \param op1 operand1 of assignment
   * \param op2 operand2 of assignment
   */
  virtual void on_and( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const
  {
    (void)lhs;
    (void)op1;
    (void)op2;
  }

  /*! \brief Callback method for parsed or gate `LHS = OP1 | OP2 ;`.
   *
   * \param lhs Left-hand side of assignment
   * \param op1 operand1 of assignment
   * \param op2 operand2 of assignment
   */
  virtual void on_or( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const
  {
    (void)lhs;
    (void)op1;
    (void)op2;
  }

  /*! \brief Callback method for parsed or gate `LHS = OP1 ^ OP2 ;`.
   *
   * \param lhs Left-hand side of assignment
   * \param op1 operand1 of assignment
   * \param op2 operand2 of assignment
   */
  virtual void on_xor( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const
  {
    (void)lhs;
    (void)op1;
    (void)op2;
  }

  /*! \brief Callback method for parsed majority-of-3 gate `LHS = ( OP1 & OP2 ) | ( OP1 & OP3 ) | ( OP2 & OP3 ) ;`.
   *
   * \param lhs Left-hand side of assignment
   * \param op1 operand1 of assignment
   * \param op2 operand2 of assignment
   */
  virtual void on_maj3( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2, const std::pair<std::string, bool>& op3 ) const
  {
    (void)lhs;
    (void)op1;
    (void)op2;
    (void)op3;
  }

  /*! \brief Callback method for parsed comments `// comment string`.
   *
   * \param comment Comment string
   */
  virtual void on_comment( std::string const& comment ) const
  {
    (void)comment;
  }

  /*! \brief Callback method for parsed endmodule.
   *
   */
  virtual void on_endmodule() const {}
}; /* verilog_reader */

/*! \brief A VERILOG reader for prettyprinting the VERILOG format.
 *
 * Callbacks for prettyprinting of BLIF.
 *
 */
class verilog_pretty_printer : public verilog_reader
{
public:
  /*! \brief Constructor of the VERILOG pretty printer.
   *
   * \param os Output stream
   */
  verilog_pretty_printer( std::ostream& os = std::cout )
      : _os( os )
  {
  }

  void on_module_header( const std::string& module_name, const std::vector<std::string>& inouts ) const override
  {
    std::string params;
    if ( inouts.size() > 0 )
    {
      params = inouts[0];
      for ( auto i = 1u; i < inouts.size(); ++i )
      {
        params += " , ";
        params += inouts[i];
      }
    }
    _os << fmt::format( "module {}( {} ) ;\n", module_name, params );
  }

  void on_inputs( const std::vector<std::string>& inputs ) const override
  {
    if ( inputs.size() == 0 ) return;
    _os << "input " << inputs[0];
    for ( auto i = 1u; i < inputs.size(); ++i )
    {
      _os << " , ";
      _os << inputs[i];
    }
    _os << " ;\n";
  }

  void on_outputs( const std::vector<std::string>& outputs ) const override
  {
    if ( outputs.size() == 0 ) return;
    _os << "output " << outputs[0];
    for ( auto i = 1u; i < outputs.size(); ++i )
    {
      _os << " , ";
      _os << outputs[i];
    }
    _os << " ;\n";
  }

  void on_wires( const std::vector<std::string>& wires ) const override
  {
    if ( wires.size() == 0 ) return;
    _os << "wire " << wires[0];
    for ( auto i = 1u; i < wires.size(); ++i )
    {
      _os << " , ";
      _os << wires[i];
    }
    _os << " ;\n";
  }

  void on_assign( const std::string& lhs, const std::pair<std::string, bool>& rhs ) const override
  {
    const std::string param = rhs.second ? fmt::format( "~{}", rhs.first ) : rhs.first;
    _os << fmt::format("assign {} = {} ;\n", lhs, param );
  }

  void on_and( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    const std::string p1 = op1.second ? fmt::format( "~{}", op1.first ) : op1.first;
    const std::string p2 = op2.second ? fmt::format( "~{}", op2.first ) : op2.first;
    _os << fmt::format("assign {} = {} & {} ;\n", lhs, p1, p2 );
  }

  void on_or( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    const std::string p1 = op1.second ? fmt::format( "~{}", op1.first ) : op1.first;
    const std::string p2 = op2.second ? fmt::format( "~{}", op2.first ) : op2.first;
    _os << fmt::format("assign {} = {} | {} ;\n", lhs, p1, p2 );
  }

  void on_xor( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2 ) const override
  {
    const std::string p1 = op1.second ? fmt::format( "~{}", op1.first ) : op1.first;
    const std::string p2 = op2.second ? fmt::format( "~{}", op2.first ) : op2.first;
    _os << fmt::format("assign {} = {} ^ {} ;\n", lhs, p1, p2 );
  }

  void on_maj3( const std::string& lhs, const std::pair<std::string, bool>& op1, const std::pair<std::string, bool>& op2, const std::pair<std::string, bool>& op3 ) const override
  {
    const std::string p1 = op1.second ? fmt::format( "~{}", op1.first ) : op1.first;
    const std::string p2 = op2.second ? fmt::format( "~{}", op2.first ) : op2.first;
    const std::string p3 = op3.second ? fmt::format( "~{}", op3.first ) : op3.first;
    _os << fmt::format("assign {0} = ( {1} & {2} ) | ( {1} & {3} ) | ( {2} & {3} ) ;\n", lhs, p1, p2, p3 );
  }

  void on_endmodule() const override
  {
    _os << "endmodule\n" << std::endl;
  }

  void on_comment( const std::string& comment ) const override
  {
    _os << "// " << comment << std::endl;
  }

  std::ostream& _os; /*!< Output stream */
}; /* verilog_pretty_printer */

namespace verilog_regex
{
static std::regex immediate_assign( R"(^(~)?([[:alnum:]\[\]_']+)$)" );
static std::regex binary_expression( R"(^(~)?([[:alnum:]\[\]_']+)([&|^])(~)?([[:alnum:]\[\]_']+)$)" );
static std::regex maj3_expression( R"(^\((~)?([[:alnum:]\[\]_']+)&(~)?([[:alnum:]\[\]_']+)\)\|\((~)?([[:alnum:]\[\]_']+)&(~)?([[:alnum:]\[\]_']+)\)\|\((~)?([[:alnum:]\[\]_']+)&(~)?([[:alnum:]\[\]_']+)\)$)" );
} // namespace verilog_regex

class verilog_parser
{
public:
  verilog_parser( std::istream& in, const verilog_reader& reader, diagnostic_engine* diag = nullptr )
    : tok( in )
    , reader( reader )
    , diag( diag )
  {}

  bool get_token( std::string& token )
  {
    detail::tokenizer_return_code result;
    do
    {
      result = tok.get_token_internal( token );
      detail::trim( token );

      /* switch to comment mode */
      if ( token == "//" && result == detail::tokenizer_return_code::valid )
      {
        tok.set_comment_mode();
      }
      else if ( result == detail::tokenizer_return_code::comment )
      {
        reader.on_comment( token );
      }
      /* keep parsing if token is empty or if in the middle or at the end of a comment */
    } while ( (token == "" && result == detail::tokenizer_return_code::valid) ||
              tok.get_comment_mode() ||
              result == detail::tokenizer_return_code::comment );

    return ( result == detail::tokenizer_return_code::valid );
  }


  bool parse_module()
  {
    valid = get_token( token );
    if ( !valid ) return false;

    bool success = parse_module_header();
    if ( !success )
    {
      if ( diag )
      {
        diag->report( diagnostic_level::error, "cannot parse module header" );
      }
      return false;
    }

    do
    {
      valid = get_token( token );
      if ( !valid ) return false;

      if ( token == "input" )
      {
        success = parse_inputs();
        if ( !success )
        {
          if ( diag )
          {
            diag->report( diagnostic_level::error, "cannot parse input declaration" );
          }
          return false;
        }
      }
      else if ( token == "output" )
      {
        success = parse_outputs();
        if ( !success )
        {
          if ( diag )
          {
            diag->report( diagnostic_level::error, "cannot parse output declaration" );
          }
          return false;
        }
      }
      else if ( token == "wire" )
      {
        success = parse_wires();
        if ( !success )
        {
          if ( diag )
          {
            diag->report( diagnostic_level::error, "cannot parse wire declaration" );
          }
          return false;
        }
      }
      else if ( token != "assign" &&
                token != "endmodule" )
      {
        return false;
      }
    } while ( token != "assign" && token != "endmodule" );

    while ( token == "assign" )
    {
      success = parse_assign();
      if ( !success )
      {
        if ( diag )
        {
          diag->report( diagnostic_level::error, "cannot parse assign statement" );
        }
        return false;
      }

      valid = get_token( token );
      if ( !valid ) return false;
    }

    if ( token == "endmodule" )
    {
      /* callback */
      reader.on_endmodule();

      return true;
    }
    else
    {
      return false;
    }
  }

  bool parse_module_header()
  {
    if ( token != "module" ) return false;

    valid = get_token( token );
    if ( !valid ) return false;

    std::string module_name = token;

    valid = get_token( token );
    if ( !valid || token != "(" ) return false;

    std::vector<std::string> inouts;
    do
    {
      valid = get_token( token );
      if ( !valid ) return false;
      inouts.push_back( token );

      valid = get_token( token );
      if ( !valid || (token != "," && token != ")") ) return false;
    } while ( valid && token != ")" );

    valid = get_token( token );
    if ( !valid || token != ";" ) return false;

    /* callback */
    reader.on_module_header( module_name, inouts );

    return true;
  }

  bool parse_inputs()
  {
    if ( token != "input" ) return false;

    std::vector<std::string> inputs;
    do
    {
      valid = get_token( token );
      if ( !valid ) return false;
      inputs.push_back( token );

      valid = get_token( token );
      if ( !valid || (token != "," && token != ";") ) return false;
    } while ( valid && token != ";" );

    /* callback */
    reader.on_inputs( inputs );

    return true;
  }

  bool parse_outputs()
  {
    if ( token != "output" ) return false;

    std::vector<std::string> outputs;
    do
    {
      valid = get_token( token );
      if ( !valid ) return false;
      outputs.push_back( token );

      valid = get_token( token );
      if ( !valid || (token != "," && token != ";") ) return false;
    } while ( valid && token != ";" );

    /* callback */
    reader.on_outputs( outputs );

    return true;
  }

  bool parse_wires()
  {
    if ( token != "wire" ) return false;

    std::vector<std::string> wires;
    do
    {
      valid = get_token( token );
      if ( !valid ) return false;
      wires.push_back( token );

      valid = get_token( token );
      if ( !valid || (token != "," && token != ";") ) return false;
    } while ( valid && token != ";" );

    /* callback */
    reader.on_wires( wires );

    return true;
  }

  bool parse_assign()
  {
    if ( token != "assign" ) return false;

    valid = get_token( token );
    if ( !valid ) return false;

    const std::string lhs = token;

    valid = get_token( token );
    if ( !valid || token != "=" ) return false;

    /* expression */
    bool success = parse_rhs_expression( lhs );
    if ( !success )
    {
      if ( diag )
      {
        diag->report( diagnostic_level::error,
                      fmt::format( "cannot parse expression on right-hand side of assign `{0}`", lhs ) );
      }
      return false;
    }

    if ( token != ";" ) return false;

    return true;
  }

  bool parse_rhs_expression( const std::string& lhs )
  {
    std::string s;
    do
    {
      valid = get_token( token );
      if ( !valid ) return false;

      if ( token == ";" || token == "assign" || token == "endmodule" ) break;
      s.append( token );
    } while ( token != ";" && token != "assign" && token != "endmodule" );

    std::smatch sm;
    if ( std::regex_match( s, sm, verilog_regex::immediate_assign ) )
    {
      assert( sm.size() == 3u );
      reader.on_assign( lhs, {sm[2], sm[1] == "~"} );
    }
    else if ( std::regex_match( s, sm, verilog_regex::binary_expression ) )
    {
      assert( sm.size() == 6u );
      std::pair<std::string,bool> arg0 = {sm[2], sm[1] == "~"};
      std::pair<std::string,bool> arg1 = {sm[5], sm[4] == "~"};
      auto op = sm[3];

      if ( op == "&" )
      {
        reader.on_and( lhs, arg0, arg1 );
      }
      else if ( op == "|" )
      {
        reader.on_or( lhs, arg0, arg1 );
      }
      else if ( op == "^" )
      {
        reader.on_xor( lhs, arg0, arg1 );
      }
      else
      {
        return false;
      }
    }
    else if ( std::regex_match( s, sm, verilog_regex::maj3_expression ) )
    {
      assert( sm.size() == 13u );
      std::pair<std::string,bool> a0 = {sm[2],  sm[1]  == "~"};
      std::pair<std::string,bool> b0 = {sm[4],  sm[3]  == "~"};
      std::pair<std::string,bool> a1 = {sm[6],  sm[5]  == "~"};
      std::pair<std::string,bool> c0 = {sm[8],  sm[7]  == "~"};
      std::pair<std::string,bool> b1 = {sm[10], sm[9]  == "~"};
      std::pair<std::string,bool> c1 = {sm[12], sm[11] == "~"};

      if ( a0 != a1 || b0 != b1 || c0 != c1 ) return false;

      std::vector<std::pair<std::string,bool>> args;
      args.push_back( a0 );
      args.push_back( b0 );
      args.push_back( c0 );

      reader.on_maj3( lhs, a0, b0, c0 );
    }
    else
    {
      return false;
    }

    return true;
  }

private:
  detail::tokenizer tok;
  const verilog_reader& reader;
  diagnostic_engine* diag;

  std::string token;
  bool valid = false;
}; /* verilog_parser */

/*! \brief Reader function for VERILOG format.
 *
 * Reads a simplistic VERILOG format from a stream and invokes a callback
 * method for each parsed primitive and each detected parse error.
 *
 * \param in Input stream
 * \param reader A VERILOG reader with callback methods invoked for parsed primitives
 * \param diag An optional diagnostic engine with callback methods for parse errors
 * \return Success if parsing have been successful, or parse error if parsing have failed
 */
inline return_code read_verilog( std::istream& in, const verilog_reader& reader, diagnostic_engine* diag = nullptr )
{
  verilog_parser parser( in, reader, diag );
  auto result = parser.parse_module();
  if ( !result )
  {
    return return_code::parse_error;
  }
  else
  {
    return return_code::success;
  }
}

/*! \brief Reader function for VERILOG format.
 *
 * Reads a simplistic VERILOG format from a file and invokes a callback
 * method for each parsed primitive and each detected parse error.
 *
 * \param filename Name of the file
 * \param reader A VERILOG reader with callback methods invoked for parsed primitives
 * \param diag An optional diagnostic engine with callback methods for parse errors
 * \return Success if parsing have been successful, or parse error if parsing have failed
 */
inline return_code read_verilog( const std::string& filename, const verilog_reader& reader, diagnostic_engine* diag = nullptr )
{
  std::ifstream in( detail::word_exp_filename( filename ), std::ifstream::in );
  return read_verilog( in, reader, diag );
}

} // namespace lorina
