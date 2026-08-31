/* lorina: C++ parsing library
 * Copyright (C) 2018-2021  EPFL
 * Copyright (C) 2022  Heinz Riener
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
  \file ast_stringifier.hpp
  \brief Translate a Verilog AST into a string

  \author Heinz Riener
*/

#pragma once

#include "ast_graph.hpp"
#include <fmt/format.h>

namespace lorina
{

class ast_stringifier : public verilog_ast_visitor
{
public:
  explicit ast_stringifier( const verilog_ast_graph* ag )
    : verilog_ast_visitor( ag )
  {}

  void emplace( const ast_node& node, const std::string& s )
  {
    str_.emplace( &node, s );
  }

  std::string string_of( ast_id id ) const
  {
    return str_.at( ag_->node_ptr( id ) );
  }

  void print() const
  {
    for ( const auto& s : str_ )
    {
      fmt::print( "{} \"{}\"\n", (void*)s.first, s.second );
    }
  }

  void visit( const ast_node& node )
  {
    (void)node;
    assert( false );
  }

  void visit( const ast_numeral& node )
  {
    emplace( node, node.value() );
  }

  void visit( const ast_identifier& node )
  {
    emplace( node, node.identifier() );
  }

  void visit( const ast_identifier_list& node )
  {
    std::vector<std::string> strings;
    for ( const ast_id& id : node.identifiers() )
    {
      ag_->node_ptr( id )->accept( *this );
      strings.emplace_back( str_[ag_->node_ptr( id )] );
    }
    emplace( node, fmt::to_string( fmt::join( strings, ", " ) ) );
  }

  void visit( const ast_array_select& node )
  {
    const ast_node* array = ag_->node_ptr( node.array() );
    const ast_node* index = ag_->node_ptr( node.index() );
    array->accept( *this );
    index->accept( *this );
    emplace( node, fmt::format( "{}[{}]", str_[array], str_[index] ) );
  }

  void visit( const ast_range_expression& node )
  {
    const ast_node* hi = ag_->node_ptr( node.hi() );
    const ast_node* lo = ag_->node_ptr( node.lo() );
    hi->accept( *this );
    lo->accept( *this );
    emplace( node, fmt::format( "[{}:{}]", str_[hi], str_[lo] ) );
  }

  void visit( const ast_sign& node )
  {
    const sign_kind kind = node.kind();
    const ast_node* expr = ag_->node_ptr( node.expr() );
    switch ( kind )
    {
    case sign_kind::SIGN_MINUS:
      {
        expr->accept( *this );
        emplace( node, fmt::format( "(-{})", str_[expr] ) );
      }
      break;
    default:
      assert( false );
    }
  }

  void visit( const ast_expression& node )
  {
    const expr_kind kind = node.kind();
    switch ( kind )
    {
    case expr_kind::EXPR_NOT:
      {
        ag_->node_ptr( node.left() )->accept( *this );
        emplace( node, fmt::format( "(~{})", str_[ag_->node_ptr( node.left() )] ) );
      }
      break;
    case expr_kind::EXPR_AND:
      {
        ag_->node_ptr( node.left() )->accept( *this );
        ag_->node_ptr( node.right() )->accept( *this );
        emplace( node, fmt::format( "({} & {})",
                                    str_[ag_->node_ptr( node.left() )],
                                    str_[ag_->node_ptr( node.right() )] ) );
      }
      break;
    case expr_kind::EXPR_OR:
      {
        ag_->node_ptr( node.left() )->accept( *this );
        ag_->node_ptr( node.right() )->accept( *this );
        emplace( node, fmt::format( "({} | {})",
                                    str_[ag_->node_ptr( node.left() )],
                                    str_[ag_->node_ptr( node.right() )] ) );
      }
      break;
    case expr_kind::EXPR_XOR:
      {
        ag_->node_ptr( node.left() )->accept( *this );
        ag_->node_ptr( node.right() )->accept( *this );
        emplace( node, fmt::format( "({} ^ {})",
                                    str_[ag_->node_ptr( node.left() )],
                                    str_[ag_->node_ptr( node.right() )] ) );
      }
      break;
    case expr_kind::EXPR_ADD:
      {
        ag_->node_ptr( node.left() )->accept( *this );
        ag_->node_ptr( node.right() )->accept( *this );
        emplace( node, fmt::format( "({} + {})",
                                    str_[ag_->node_ptr( node.left() )],
                                    str_[ag_->node_ptr( node.right() )] ) );
      }
      break;
    case expr_kind::EXPR_MUL:
      {
        ag_->node_ptr( node.left() )->accept( *this );
        ag_->node_ptr( node.right() )->accept( *this );
        emplace( node, fmt::format( "({} * {})",
                                    str_[ag_->node_ptr( node.left() )],
                                    str_[ag_->node_ptr( node.right() )] ) );
      }
      break;
    default:
      assert( false );
    }
  }

  void visit( const ast_system_function& node )
  {
    std::vector<std::string> args_strings;
    for ( const auto& id : node.args() )
    {
      ag_->node_ptr( id )->accept( *this );
      args_strings.emplace_back( str_[ag_->node_ptr( id )] );
    }
    ag_->node_ptr( node.fun() )->accept( *this );
    emplace( node, fmt::format( "{}{{{}}}",
                                str_[ag_->node_ptr( node.fun() )],
                                fmt::join( args_strings, ", " ) ) );
  }

  void visit( const ast_input_declaration& node )
  {
    std::vector<std::string> strings;
    for ( const ast_id& id : node.identifiers() )
    {
      const ast_node* identifier = ag_->node_ptr( id );
      identifier->accept( *this );
      strings.emplace_back( str_[identifier] );
    }

    if ( node.word_level() )
    {
      const ast_node* hi = ag_->node_ptr( node.hi() );
      const ast_node* lo = ag_->node_ptr( node.lo() );
      hi->accept( *this );
      lo->accept( *this );
      emplace( node,
        fmt::format( "input [{}:{}] {};",
                     str_[hi], str_[lo], fmt::join( strings, ", " ) ) );
    }
    else
    {
      emplace( node,
        fmt::format( "input {};", fmt::join( strings, ", " ) ) );
    }
  }

  void visit( const ast_output_declaration& node )
  {
    std::vector<std::string> strings;
    for ( const ast_id& id : node.identifiers() )
    {
      const ast_node* identifier = ag_->node_ptr( id );
      identifier->accept( *this );
      strings.emplace_back( str_[identifier] );
    }

    if ( node.word_level() )
    {
      const ast_node* hi = ag_->node_ptr( node.hi() );
      const ast_node* lo = ag_->node_ptr( node.lo() );
      hi->accept( *this );
      lo->accept( *this );
      emplace( node,
        fmt::format( "output [{}:{}] {};",
                     str_[hi], str_[lo], fmt::join( strings, ", " ) ) );
    }
    else
    {
      emplace( node,
        fmt::format( "output {};", fmt::join( strings, ", " ) ) );
    }
  }

  void visit( const ast_wire_declaration& node )
  {
    std::vector<std::string> strings;
    for ( const ast_id& id : node.identifiers() )
    {
      const ast_node* identifier = ag_->node_ptr( id );
      identifier->accept( *this );
      strings.emplace_back( str_[identifier] );
    }

    if ( node.word_level() )
    {
      const ast_node* hi = ag_->node_ptr( node.hi() );
      const ast_node* lo = ag_->node_ptr( node.lo() );
      hi->accept( *this );
      lo->accept( *this );
      emplace( node,
        fmt::format( "wire [{}:{}] {};",
                     str_[hi], str_[lo], fmt::join( strings, ", " ) ) );
    }
    else
    {
      emplace( node,
        fmt::format( "wire {};", fmt::join( strings, ", " ) ) );
    }
  }

  void visit( const ast_module_instantiation& node )
  {
    const ast_node* module_name = ag_->node_ptr( node.module_name() );
    const ast_node* instance_name = ag_->node_ptr( node.instance_name() );

    std::vector<std::string> port_assignments;
    for ( const std::pair<ast_id,ast_id>& p : node.port_assignment() )
    {
      const ast_node* lhs = ag_->node_ptr( p.first );
      const ast_node* rhs = ag_->node_ptr( p.second );
      lhs->accept( *this );
      rhs->accept( *this );
      port_assignments.emplace_back(
        fmt::format( ".{}({})", str_[lhs], str_[rhs] ) );
    }

    std::vector<std::string> parameters;
    for ( const ast_id id : node.parameters() )
    {
      const ast_node* param_node = ag_->node_ptr( id );
      param_node->accept( *this );
      parameters.emplace_back( str_[param_node] );
    }

    module_name->accept( *this );
    instance_name->accept( *this );

    emplace( node,
      fmt::format( "{} #({}) {}({});",
                   str_[module_name],
                   fmt::join( parameters, "," ),
                   str_[instance_name],
                   fmt::join( port_assignments, ", " ) ) );
  }

  void visit( const ast_assignment& node )
  {
    const ast_node* signal = ag_->node_ptr( node.signal() );
    const ast_node* expr = ag_->node_ptr( node.expr() );
    signal->accept( *this );
    expr->accept( *this );
    emplace( node, fmt::format( "assign {} = {};",
                                str_[signal], str_[expr] ) );
  }

  void visit( const ast_parameter_declaration& node )
  {
    const ast_node* identifier = ag_->node_ptr( node.identifier() );
    const ast_node* expr = ag_->node_ptr( node.expr() );
    identifier->accept( *this );
    expr->accept( *this );
    emplace( node, fmt::format( "parameter {} = {};",
                                str_[identifier], str_[expr] ) );
  }

  void visit( const ast_module& node )
  {
    const ast_node* module_name = ag_->node_ptr( node.module_name() );
    module_name->accept( *this );

    std::vector<std::string> args;
    for ( ast_id arg : node.args() )
    {
      const ast_node* arg_node = ag_->node_ptr( arg );
      arg_node->accept( *this );
      args.emplace_back( str_[arg_node] );
    }

    std::vector<std::string> decls;
    for ( ast_id decl : node.decls() )
    {
      const ast_node* decl_node = ag_->node_ptr( decl );
      decl_node->accept( *this );
      decls.emplace_back( str_[decl_node] + "\n" );
    }

    emplace( node,
      fmt::format( "module {}({});\n{}endmodule\n",
        str_[module_name],
        fmt::join( args, ", " ),
        fmt::join( decls, "" )
      )
    );
  }

protected:
  std::unordered_map<const ast_node*, std::string> str_;
}; // ast_stringifier

} // namespace lorina
