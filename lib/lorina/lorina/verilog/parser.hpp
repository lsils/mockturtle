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
  \file parser.hpp
  \brief Verilog Parser

  \author Heinz Riener
*/

#pragma once

#include "../diagnostics.hpp"
#include "ast_graph.hpp"
#include "lexer.hpp"

namespace lorina
{

class verilog_parser
{
public:
  using Lexer = verilog_lexer<std::istream_iterator<char>>;

public:
  explicit verilog_parser( Lexer& lexer, verilog_ast_graph& ag, diagnostic_engine *diag )
    : lexer_( lexer )
    , ag_( ag )
    , diag_( diag )
  {
    token_id_ = lexer_.next_token();
  }

public:
  /* \brief Test if current token is a numeral
   *
   * \return Returns true iff current token is a numeral
   */
  inline bool is_numeral() const
  {
    const token s = lexer_.get( token_id_ );
    return s.kind == token_kind::TK_NUMERAL;
  }

  /* \brief Consumes a numeral
   *
   * \return Returns AST of the numeral or an error
   */
  inline verilog_ast_graph::ast_or_error consume_numeral()
  {
    assert( is_numeral() );

    const std::string& numeral = lexer_.get( token_id_ ).lexem;
    token_id_ = lexer_.next_token();
    return verilog_ast_graph::ast_or_error( ag_.create_numeral( numeral ) );
  }

  /* \brief Test if current token is the start of a range expression
   *
   * \return Returns true iff current token is the start of a range
   *         expression
   */
  inline bool is_range_expression() const
  {
    return token_id_ == Lexer::TID_OP_LSQUARED;
  }

  /* \brief Consume a range expression
   *
   * A range expression is a pair of a most-significant bit and a
   * least-significant bit.
   *
   * Production rule:
   *   RANGE_EXPR ::= `[` ARITHMETIC_EXPR `:` ARITHMETIC_EXPR `]`
   *
   * Example:
   *   [7:4]
   */
  inline verilog_ast_graph::ast_or_error consume_range_expression()
  {
    verilog_ast_graph::ast_or_error hi, lo;
    assert( token_id_ == Lexer::TID_OP_LSQUARED );
    token_id_ = lexer_.next_token(); // `[`

    hi = consume_arithmetic_expression();
    if ( !hi )
      goto error;

    if ( token_id_ != Lexer::TID_OP_COLON )
      goto error;
    token_id_ = lexer_.next_token(); // `:`

    lo = consume_arithmetic_expression();
    if ( !lo )
      goto error;

    if ( token_id_ != Lexer::TID_OP_RSQUARED )
      goto error;
    token_id_ = lexer_.next_token(); // `]`

    return verilog_ast_graph::ast_or_error( ag_.create_range_expression( hi.ast(), lo.ast() ) );

  error:
    fmt::print("[e] could not parse a range expression.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Test if current token is an identifier
   *
   * \return Returns true iff the current token is an identifier
   */
  inline bool is_identifier() const
  {
    const token s = lexer_.get( token_id_ );
    return s.kind == token_kind::TK_IDENTIFIER;
  }

  /* \brief Consume an identifier
   *
   * \return Returns AST of the identifier or an error
   */
  inline verilog_ast_graph::ast_or_error consume_identifier()
  {
    assert( is_identifier() );

    const std::string& identifier = lexer_.get( token_id_ ).lexem;
    token_id_ = lexer_.next_token();
    return verilog_ast_graph::ast_or_error( ag_.create_identifier( identifier ) );
  }

  /* \brief Consume a list of identifiers
   *
   * A list of identifiers separated by commas
   *
   * Production rule:
   *   IDENTIFIER_LIST ::= IDENTIFIER `,` ... `,` IDENTIFIER
   *
   * Example:
   *   a, b, c
   */
  inline verilog_ast_graph::ast_or_error consume_identifier_list()
  {
    verilog_ast_graph::ast_or_error identifier;
    std::vector<ast_id> identifiers;

    if ( !is_identifier() )
      goto error;

    /* first identifier */
    identifier = consume_identifier();
    if ( !identifier )
      goto error;
    identifiers.emplace_back( identifier.ast() );

    /* other identifiers */
    while ( token_id_ == Lexer::TID_OP_COMMA )
    {
      token_id_ = lexer_.next_token(); // `,`
      if ( !is_identifier() )
        goto error;

      identifier = consume_identifier();
      if ( !identifier )
        goto error;
      identifiers.emplace_back( identifier.ast() );
    }
    return verilog_ast_graph::ast_or_error( identifiers.size() == 1 ?
                                            identifiers[0] : ag_.create_identifier_list( identifiers ) );

  error:
    fmt::print("[e] could not parse an identifier list.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Test if current token is a signal reference
   *
   * \return Returns true iff current token is a signal reference
   */
  inline bool is_signal_reference() const
  {
    return is_identifier();
  }

  /* \brief Consume a signal reference
   *
   * A signal reference is either an identifier, e.g., `x`, or an
   * identifier followed by a single constant bit select, e.g.,
   * `x[1]`.
   *
   * Not supported are:
   *   - multi-indexing, e.g., x[1][2][3]
   *   - vector bit and vector part select, e.g., x[1:2]
   */
  inline verilog_ast_graph::ast_or_error consume_signal_reference()
  {
    verilog_ast_graph::ast_or_error identifier, index;
    identifier = consume_identifier();
    if ( !identifier )
      goto error;

    if ( token_id_ == Lexer::TID_OP_LSQUARED )
    {
      token_id_ = lexer_.next_token(); // `[`

      index = consume_numeral();
      if ( !index )
        goto error;

      if ( token_id_ != Lexer::TID_OP_RSQUARED )
        goto error;
      token_id_ = lexer_.next_token(); // `]`

      return verilog_ast_graph::ast_or_error(
               ag_.create_array_select( identifier.ast(), index.ast() ) );
    }
    else
    {
      return identifier;
    }

  error:
    fmt::print("[e] could not parse signal reference.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume input declaration
   *
   * An input declaration declares a list of identifiers either as
   * bit-level or word-level inputs.
   *
   * Production rule:
   *   INPUT_DECLARATIONS ::= `input` ( `[` MSB `:` LSB `]` )? IDENTIFIER `,` ... `,` IDENTIFIER `;`
   */
  inline verilog_ast_graph::ast_or_error consume_input_declaration()
  {
    verilog_ast_graph::ast_or_error identifiers, range;
    bool word_level = false;

    if ( token_id_ != Lexer::TID_KW_INPUT )
      goto error;
    token_id_ = lexer_.next_token(); // `input`

    if ( is_range_expression() )
    {
      word_level = true;
      range = consume_range_expression();
      if ( !range )
        goto error;
    }

    identifiers = consume_identifier_list();
    if ( !identifiers )
      goto error;

    if ( token_id_ != Lexer::TID_OP_SEMICOLON )
      goto error;
    token_id_ = lexer_.next_token(); // `;`

    if ( word_level )
    {
      return verilog_ast_graph::ast_or_error( ag_.create_input_declaration( identifiers.ast(), range.ast() ) );
    }
    else
    {
      return verilog_ast_graph::ast_or_error( ag_.create_input_declaration( identifiers.ast() ) );
    }

  error:
    fmt::print("[e] could not parse input declaration.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume output declaration
   *
   * An output declaration declares a list of identifiers either as
   * bit-level or word-level outputs.
   *
   * Production rule:
   *   OUTPUT_DECLARATIONS ::= `output` ( `[` MSB `:` LSB `]` )? IDENTIFIER `,` ... `,` IDENTIFIER `;`
   */
  inline verilog_ast_graph::ast_or_error consume_output_declaration()
  {
    verilog_ast_graph::ast_or_error identifiers, range;
    bool word_level = false;

    if ( token_id_ != Lexer::TID_KW_OUTPUT )
      goto error;
    token_id_ = lexer_.next_token(); // `output`

    if ( is_range_expression() )
    {
      word_level = true;
      range = consume_range_expression();
      if ( !range )
        goto error;
    }

    identifiers = consume_identifier_list();
    if ( !identifiers )
      goto error;

    if ( token_id_ != Lexer::TID_OP_SEMICOLON )
      goto error;
    token_id_ = lexer_.next_token(); // `;`

    if ( word_level )
    {
      return verilog_ast_graph::ast_or_error( ag_.create_output_declaration( identifiers.ast(), range.ast() ) );
    }
    else
    {
      return verilog_ast_graph::ast_or_error( ag_.create_output_declaration( identifiers.ast() ) );
    }

  error:
    fmt::print("[e] could not parse output declaration.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume wire declaration
   *
   * An wire declaration declares a list of identifiers either as
   * bit-level or word-level wires.
   *
   * Production rule:
   *   WIRE_DECLARATIONS ::= `wire` ( `[` MSB `:` LSB `]` )? IDENTIFIER `,` ... `,` IDENTIFIER `;`
   */
  inline verilog_ast_graph::ast_or_error consume_wire_declaration()
  {
    verilog_ast_graph::ast_or_error identifiers, range;
    bool word_level = false;

    if ( token_id_ != Lexer::TID_KW_WIRE )
      goto error;
    token_id_ = lexer_.next_token(); // `wire`

    if ( is_range_expression() )
    {
      word_level = true;
      range = consume_range_expression();
      if ( !range )
        goto error;
    }

    identifiers = consume_identifier_list();
    if ( !identifiers )
      goto error;

    if ( token_id_ != Lexer::TID_OP_SEMICOLON )
      goto error;
    token_id_ = lexer_.next_token(); // `;`

    if ( word_level )
    {
      return verilog_ast_graph::ast_or_error( ag_.create_wire_declaration( identifiers.ast(), range.ast() ) );
    }
    else
    {
      return verilog_ast_graph::ast_or_error( ag_.create_wire_declaration( identifiers.ast() ) );
    }

  error:
    fmt::print("[e] could not parse wire declaration.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Test if current token is the start of an arithemtic term
   *
   * \return Returns true iff current token is the start of an
   *         arithmetic term
   *
   */
  inline bool is_arithmetic_term() const
  {
    return token_id_ == Lexer::TID_OP_LPAREN ||
           token_id_ == Lexer::TID_OP_SUB ||
           is_numeral() ||
           is_signal_reference();
  }

  /* \brief Consume arithmetic term
   *
   * An arithmetic term is either a negative arithemtic term, an
   * arithmetic expression, a signal reference, or a numeral.
   *
   * Production rule:
   *   ARITHMETIC_TERM ::= ( - ARITHMETIC_TERM )
   *                     | ( ARITHMETIC_EXPRESSION )
   *                     | SIGNAL_REFERENCE
   *                     | NUMERAL
   */
  inline verilog_ast_graph::ast_or_error consume_arithmetic_term()
  {
    verilog_ast_graph::ast_or_error expr;

    if ( token_id_ == Lexer::TID_OP_LPAREN )
    {
      token_id_ = lexer_.next_token(); // `(`
      if ( token_id_ == Lexer::TID_OP_SUB )
      {
        token_id_ = lexer_.next_token(); // `-`

        expr = consume_arithmetic_term();
        if ( !expr )
          goto error;

        token_id_ = lexer_.next_token(); // `)`
        return verilog_ast_graph::ast_or_error( ag_.create_negative_sign( expr.ast() ) );
      }
      else
      {
        expr = consume_arithmetic_expression();
        if ( !expr )
          goto error;

        if ( token_id_ != Lexer::TID_OP_RPAREN )
          goto error;

        token_id_ = lexer_.next_token(); // `)`
        return expr;
      }
    }

    if ( is_signal_reference() )
    {
      return consume_signal_reference();
    }

    if ( is_numeral() )
    {
      return consume_numeral();
    }

  error:
    fmt::print("[e] could not parse arithmetic term.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume mul expression
   *
   * A mul expression is an arithmetic term optionally followed by a
   * mul expression.
   *
   * Production rule:
   *   MUL_EXPRESSION ::= ARITHMETIC_TERM ( `*` MUL_EXPRESSION )?
   */
  inline verilog_ast_graph::ast_or_error consume_mul_expression()
  {
    verilog_ast_graph::ast_or_error term, mul_expr_rest;

    /* MulExpr -> ArithmeticTerm MulExprRest */
    if ( !is_arithmetic_term() )
      goto error;

    term = consume_arithmetic_term();
    if ( !term )
      goto error;

    /* MulExprRest -> `*` MulExpr | \eps */
    if ( token_id_ == Lexer::TID_OP_MUL )
    {
      token_id_ = lexer_.next_token(); // `*`

      mul_expr_rest = consume_mul_expression();
      if ( !mul_expr_rest )
        goto error;

      return verilog_ast_graph::ast_or_error(
               ag_.create_mul_expression( term.ast(), mul_expr_rest.ast() ) );
    }
    else
    {
      if ( token_id_ != Lexer::TID_EOF &&
           token_id_ != Lexer::TID_OP_RPAREN &&
           token_id_ != Lexer::TID_OP_RSQUARED &&
           token_id_ != Lexer::TID_OP_SEMICOLON &&
           token_id_ != Lexer::TID_OP_COLON &&
           token_id_ != Lexer::TID_OP_COMMA &&
           token_id_ != Lexer::TID_OP_ADD &&
           token_id_ != Lexer::TID_OP_SUB )
        goto error;

      return term;
    }

  error:
    fmt::print("[e] could not parse mul expression.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume sum expression
   *
   * A sum expression is a mul expression optionally followed by a
   * sum expression.
   *
   * Production rule:
   *   SUM_EXPRESSION ::= MUL_EXPRESSION ( `+` SUM_EXPRESSION )?
   */
  inline verilog_ast_graph::ast_or_error consume_sum_expression()
  {
    verilog_ast_graph::ast_or_error mul_expr, sum_expr_rest;

    /* SumExpr -> MulExpr SumExprRest */
    if ( !is_arithmetic_term() )
      goto error;

    mul_expr = consume_mul_expression();
    if ( !mul_expr )
      goto error;

    /* SumExprRest -> + SumExpr | \eps */
    if ( token_id_ == Lexer::TID_OP_ADD )
    {
      token_id_ = lexer_.next_token(); // `+`
      sum_expr_rest = consume_sum_expression();
      if ( !sum_expr_rest )
        goto error;

      return verilog_ast_graph::ast_or_error(
               ag_.create_sum_expression( mul_expr.ast(), sum_expr_rest.ast() ) );
    }
    /* SumExprRest -> - SumExpr | \eps */
    else if ( token_id_ == Lexer::TID_OP_SUB )
    {
      token_id_ = lexer_.next_token(); // `-`
      sum_expr_rest = consume_sum_expression();
      if ( !sum_expr_rest )
        goto error;

      return verilog_ast_graph::ast_or_error(
               ag_.create_sum_expression( mul_expr.ast(), ag_.create_negative_sign( sum_expr_rest.ast() ) ) );
    }
    else
    {
      if ( token_id_ != Lexer::TID_EOF &&
           token_id_ != Lexer::TID_OP_RPAREN &&
           token_id_ != Lexer::TID_OP_RSQUARED &&
           token_id_ != Lexer::TID_OP_SEMICOLON &&
           token_id_ != Lexer::TID_OP_COLON &&
           token_id_ != Lexer::TID_OP_COMMA &&
           token_id_ != Lexer::TID_OP_ADD &&
           token_id_ != Lexer::TID_OP_SUB )
        goto error;

      return mul_expr;
    }

  error:
    fmt::print("[e] could not parse sum expression.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume an arithmetic expression
   *
   * An arithmetic expression is an alias for a sum expression.
   */
  inline verilog_ast_graph::ast_or_error consume_arithmetic_expression()
  {
    return consume_sum_expression();
  }

  /* \brief Test if current token is system function
   *
   * \return Returns true iff current token is system function
   */
  inline bool is_system_function() const
  {
    const token s = lexer_.get( token_id_ );
    return s.kind == token_kind::TK_IDENTIFIER && s.lexem[0] == '$';
  }

  /* \brief Test if current token is the start of a term
   *
   * \return Returns true iff current token is the start of a term
   */
  inline bool is_term() const
  {
    return token_id_ == Lexer::TID_OP_LPAREN ||
           token_id_ == Lexer::TID_OP_BITWISE_NOT ||
           is_numeral() ||
           is_signal_reference();
  }

  /* \brief Consume a term
   *
   * A term is either a negated term, an expression, a signal
   * reference, a numeral, or a system function.
   *
   * Production rules:
   *   Term -> `~` Term | ( Expr ) | SIGNAL_REFERENCE | NUMERAL | SystemFunction { expr, ..., expr }
   */
  inline verilog_ast_graph::ast_or_error consume_term()
  {
    assert( is_term() );

    /* Term -> ~ Expr */
    if ( token_id_ == Lexer::TID_OP_BITWISE_NOT )
    {
      token_id_ = lexer_.next_token(); // `~`

      verilog_ast_graph::ast_or_error term = consume_term();
      if ( !term )
        goto error;

      return verilog_ast_graph::ast_or_error( ag_.create_not_expression( term.ast() ) );
    }

    /* Term -> ( Expr ) */
    if ( token_id_ == Lexer::TID_OP_LPAREN )
    {
      token_id_ = lexer_.next_token(); // (

      verilog_ast_graph::ast_or_error expr = consume_expression();
      if ( !expr )
        goto error;

      if ( token_id_ != Lexer::TID_OP_RPAREN )
        goto error;

      token_id_ = lexer_.next_token(); // )
      return expr;
    }

    /* Term -> SystemFunction { expr , ... , expr } */
    if ( is_system_function() )
    {
      verilog_ast_graph::ast_or_error fun = consume_identifier();

      if ( token_id_ != Lexer::TID_OP_LCURLY )
        goto error;
      token_id_ = lexer_.next_token(); // {

      std::vector<ast_id> args;
      verilog_ast_graph::ast_or_error expr = consume_expression();
      if ( !expr )
        goto error;
      args.emplace_back( expr.ast() );

      while ( token_id_ == Lexer::TID_OP_COMMA )
      {
        token_id_ = lexer_.next_token(); // ,

        expr = consume_expression();
        if ( !expr )
          goto error;

        args.emplace_back( expr.ast() );
      }

      if ( token_id_ != Lexer::TID_OP_RCURLY )
        goto error;
      token_id_ = lexer_.next_token(); // }

      return verilog_ast_graph::ast_or_error( ag_.create_system_function( fun.ast(), args ) );
    }

    /* Term -> SignalReference */
    if ( is_signal_reference() )
    {
      return consume_signal_reference();
    }

    /* Term -> NUMERAL */
    if ( is_numeral() )
    {
      return consume_numeral();
    }

  error:
    fmt::print("[e] could not parse term.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume AND expression
   *
   * An AND expression is a term optionally followed by a an AND expression.
   *
   * Production rules:
   *   AndExpr -> Term ( `&` AndExpr )?
   */
  inline verilog_ast_graph::ast_or_error consume_and_expression()
  {
    verilog_ast_graph::ast_or_error term, and_expr_rest;

    /* AndExpr -> Term AndExprRest */
    if ( !is_term() )
      goto error;

    term = consume_term();
    if ( !term )
      goto error;

    /* AndExprRest -> `&` AndExpr | \eps */
    if ( token_id_ == Lexer::TID_OP_BITWISE_AND )
    {
      token_id_ = lexer_.next_token(); // `&`

      and_expr_rest = consume_and_expression();
      if ( !and_expr_rest )
        goto error;

      return verilog_ast_graph::ast_or_error( ag_.create_and_expression( term.ast(), and_expr_rest.ast() ) );
    }
    else
    {
      if ( token_id_ != Lexer::TID_EOF &&
           token_id_ != Lexer::TID_OP_RPAREN &&
           token_id_ != Lexer::TID_OP_RCURLY &&
           token_id_ != Lexer::TID_OP_COMMA &&
           token_id_ != Lexer::TID_OP_SEMICOLON &&
           token_id_ != Lexer::TID_OP_BITWISE_OR &&
           token_id_ != Lexer::TID_OP_BITWISE_XOR )
        goto error;

      return term;
    }

  error:
    fmt::print("[e] could not parse and expression.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume OR expression
   *
   * An OR expression is an AND expression optionally followed by a an OR expression.
   *
   * Production rules:
   *   OrExpr -> AndExpr ( `|` OrExpr )?
   */
  inline verilog_ast_graph::ast_or_error consume_or_expression()
  {
    verilog_ast_graph::ast_or_error and_expr, or_expr_rest;

    /* OrExpr -> AndExpr OrExprRest */
    if ( !is_term() )
      goto error;

    and_expr = consume_and_expression();
    if ( !and_expr )
      goto error;

    /* OrExprRest -> `|` OrExpr | \eps */
    if ( token_id_ == Lexer::TID_OP_BITWISE_OR )
    {
      token_id_ = lexer_.next_token(); // `|`

      or_expr_rest = consume_or_expression();
      if ( !or_expr_rest )
        goto error;

      return verilog_ast_graph::ast_or_error( ag_.create_or_expression( and_expr.ast(), or_expr_rest.ast() ) );
    }
    else
    {
      if ( token_id_ != Lexer::TID_EOF &&
           token_id_ != Lexer::TID_OP_RPAREN &&
           token_id_ != Lexer::TID_OP_RCURLY &&
           token_id_ != Lexer::TID_OP_COMMA &&
           token_id_ != Lexer::TID_OP_SEMICOLON &&
           token_id_ != Lexer::TID_OP_BITWISE_XOR )
        goto error;

      return and_expr;
    }

  error:
    fmt::print("[e] could not parse or expression.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume XOR expression
   *
   * An XOR expression is an OR expression optionally followed by a an XOR expression.
   *
   * Production rules:
   *   XorExpr -> OrExpr ( `^` XorExpr )?
   */
  inline verilog_ast_graph::ast_or_error consume_xor_expression()
  {
    verilog_ast_graph::ast_or_error or_expr, xor_expr_rest;

    /* XorExpr -> OrExpr XorExprRest */
    if ( !is_term() )
      goto error;

    or_expr = consume_or_expression();
    if ( !or_expr )
      goto error;

    /* XorrExprRest -> `^` XorExpr | \eps */
    if ( token_id_ == Lexer::TID_OP_BITWISE_XOR )
    {
      token_id_ = lexer_.next_token(); // `^`

      xor_expr_rest = consume_xor_expression();
      if ( !xor_expr_rest )
        goto error;

      return verilog_ast_graph::ast_or_error( ag_.create_xor_expression( or_expr.ast(), xor_expr_rest.ast() ) );
    }
    else
    {
      if ( token_id_ != Lexer::TID_EOF &&
           token_id_ != Lexer::TID_OP_RPAREN &&
           token_id_ != Lexer::TID_OP_RCURLY &&
           token_id_ != Lexer::TID_OP_COMMA &&
           token_id_ != Lexer::TID_OP_SEMICOLON &&
           token_id_ != Lexer::TID_OP_BITWISE_XOR )
        goto error;

      return or_expr;
    }

  error:
    fmt::print("[e] could not parse xor expression.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume an expression
   *
   * An expression is an alias for a XOR expression.
   */
  inline verilog_ast_graph::ast_or_error consume_expression()
  {
    return consume_xor_expression();
  }

  /* \brief Consume module instantiation
   *
   * Production rule:
   *   ParameterAssignment ::= `#` `(` ARITH_EXPR `,` ... `,` ARITH_EXPR `)`
   *   PortAssignment ::= `.` IDENTIFIER(SIGNAL_REFERENCE) `,` ... `,` `.` IDENTIFIER(SIGNAL_REFERENCE)
   *   ModuleInstitation ::= IDENTIFIER (ParameterAssignment)? IDENTIFIER `(` PortAssignment `)` `;`
   *
   * Example:
   *   full_adder #(BITWIDTH) fa0(.a(i[0]), .b(i[1]), .c(i[2]), .sum(o[0]), .carry(o[1]));
   */
  inline verilog_ast_graph::ast_or_error consume_module_instantiation()
  {
    verilog_ast_graph::ast_or_error module_name, instance_name;
    std::vector<std::pair<ast_id, ast_id>> port_assignment;
    std::vector<ast_id> parameters;

    /* temporary variables */
    verilog_ast_graph::ast_or_error parameter, port_name, signal_name;

    /* parse module name */
    if ( !is_identifier() )
      goto error;
    module_name = consume_identifier();

    /* parse parameters (optionally) */
    if ( token_id_ == Lexer::TID_OP_ROUTE )
    {
      token_id_ = lexer_.next_token();

      if ( token_id_ != Lexer::TID_OP_LPAREN )
        goto error;
      token_id_ = lexer_.next_token();

      parameter = consume_arithmetic_expression();
      parameters.emplace_back( parameter.ast() );

      while ( token_id_ == Lexer::TID_OP_COMMA )
      {
        token_id_ = lexer_.next_token();

        parameter = consume_arithmetic_expression();
        parameters.emplace_back( parameter.ast() );
      }

      if ( token_id_ != Lexer::TID_OP_RPAREN )
        goto error;
      token_id_ = lexer_.next_token();
    }

    /* parse instance name */
    if ( !is_identifier() )
      goto error;
    instance_name = consume_identifier();

    /* parse port assignment */
    if ( token_id_ != Lexer::TID_OP_LPAREN )
      goto error;
    token_id_ = lexer_.next_token();

    while ( token_id_ == Lexer::TID_OP_DOT )
    {
      token_id_ = lexer_.next_token();

      /* parse port name */
      if ( !is_identifier() )
        goto error;
      port_name = consume_identifier();

      if ( token_id_ != Lexer::TID_OP_LPAREN )
        goto error;
      token_id_ = lexer_.next_token();

      /* parse signal name */
      if ( !is_identifier() )
        goto error;
      signal_name = consume_signal_reference();

      if ( token_id_ != Lexer::TID_OP_RPAREN )
        goto error;
      token_id_ = lexer_.next_token();

      port_assignment.emplace_back( std::make_pair( port_name.ast(), signal_name.ast() ) );

      if ( token_id_ == Lexer::TID_OP_COMMA )
      {
        token_id_ = lexer_.next_token();
        continue;
      }
    }

    if ( token_id_ != Lexer::TID_OP_RPAREN )
      goto error;
    token_id_ = lexer_.next_token();

    if ( token_id_ != Lexer::TID_OP_SEMICOLON )
      goto error;
    token_id_ = lexer_.next_token();

    return verilog_ast_graph::ast_or_error(
             ag_.create_module_instantiation( module_name.ast(), instance_name.ast(), port_assignment, parameters ) );

  error:
    fmt::print("[e] could not parse module instantiation.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume a parameter declaration
   *
   * A parameter declaration assigns an arithmetic expression to an identifier.
   *
   * Production rule:
   *   PARAMETER_DECLARATION ::= `parameter` IDENTIFIER `=` ARITHMETIC_EXPR `;`
   *
   * Examples:
   *   parameter WORD = 32;
   *   parameter SIZE = 10;
   */
  inline verilog_ast_graph::ast_or_error consume_parameter_declaration()
  {
    verilog_ast_graph::ast_or_error identifier, expr;

    if ( token_id_ != Lexer::TID_KW_PARAMETER )
      goto error;
    token_id_ = lexer_.next_token(); // `parameter`

    identifier = consume_identifier(); // identifier
    if ( !identifier )
      goto error;

    if ( token_id_ != Lexer::TID_OP_ASSIGN )
      goto error;
    token_id_ = lexer_.next_token(); // `=`

    expr = consume_arithmetic_expression(); // arithmetic expression
    if ( !expr )
      goto error;

    if ( token_id_ != Lexer::TID_OP_SEMICOLON )
      goto error;
    token_id_ = lexer_.next_token(); // `;`

    return verilog_ast_graph::ast_or_error( ag_.create_parameter_declaration( identifier.ast(), expr.ast() ) );

  error:
    fmt::print("[e] could not parse parameter declaration.\n");
    return verilog_ast_graph::ast_or_error();
  }

  /* \brief Consume an assignment
   *
   * An assignment assigns a signal reference to an expression.
   *
   * Production rule:
   *   ASSIGNMENT ::= `assign` SIGNAL_REFERENCE `=` EXPR `;`
   *
   * Examples:
   *   assign maj = a & b | b & c | a & c;
   *   assign x[2] = x[1] ^ x[0];
   */
  inline verilog_ast_graph::ast_or_error consume_assignment()
  {
    verilog_ast_graph::ast_or_error lhs, rhs;

    if ( token_id_ != Lexer::TID_KW_ASSIGN )
      goto error;
    token_id_ = lexer_.next_token(); // `assign`

    lhs = consume_signal_reference(); // signal reference
    if ( !lhs )
      goto error;

    if ( token_id_ != Lexer::TID_OP_ASSIGN )
      goto error;
    token_id_ = lexer_.next_token(); // `=`

    rhs = consume_expression(); // expression
    if ( !lhs )
      goto error;

    if ( token_id_ != Lexer::TID_OP_SEMICOLON )
      goto error;
    token_id_ = lexer_.next_token(); // `;`

    return verilog_ast_graph::ast_or_error( ag_.create_assignment( lhs.ast(), rhs.ast() ) );

  error:
    fmt::print("[e] could not parse assignment.\n");
    return verilog_ast_graph::ast_or_error();
  }

  inline verilog_ast_graph::ast_or_error consume_module()
  {
    verilog_ast_graph::ast_or_error module_name;
    std::vector<ast_id> args;
    std::vector<ast_id> decls;

    verilog_ast_graph::ast_or_error identifier, arg, decl_or_stmt;

    /* parse module keyword */
    if ( token_id_ != Lexer::TID_KW_MODULE )
      goto error;
    token_id_ = lexer_.next_token(); // `module`

    /* parse module name */
    if ( !is_identifier() )
      goto error;
    module_name = consume_identifier();

    /* parse sensitivity list */
    if ( token_id_ != Lexer::TID_OP_LPAREN )
      goto error;
    token_id_ = lexer_.next_token(); // `(`

    if ( is_identifier() )
    {
      arg = consume_identifier();
      args.emplace_back( arg.ast() );
      while ( token_id_ == Lexer::TID_OP_COMMA )
      {
        token_id_ = lexer_.next_token(); // `,`
        arg = consume_identifier();
        args.emplace_back( arg.ast() );
      }
    }

    if ( token_id_ != Lexer::TID_OP_RPAREN )
      goto error;
    token_id_ = lexer_.next_token(); // `)`

    if ( token_id_ != Lexer::TID_OP_SEMICOLON )
      goto error;
    token_id_ = lexer_.next_token(); // `;`

    while ( token_id_ != Lexer::TID_KW_ENDMODULE )
    {
      if ( token_id_ == Lexer::TID_KW_INPUT )
      {
        decl_or_stmt = consume_input_declaration();
      }
      else if ( token_id_ == Lexer::TID_KW_OUTPUT )
      {
        decl_or_stmt = consume_output_declaration();
      }
      else if ( token_id_ == Lexer::TID_KW_WIRE )
      {
        decl_or_stmt = consume_wire_declaration();
      }
      else if ( token_id_ == Lexer::TID_KW_ASSIGN )
      {
        decl_or_stmt = consume_assignment();
      }
      else if ( token_id_ == Lexer::TID_KW_PARAMETER )
      {
        decl_or_stmt = consume_parameter_declaration();
      }
      else if ( is_identifier() )
      {
        decl_or_stmt = consume_module_instantiation();
      }
      else
      {
        goto error;
      }
      decls.emplace_back( decl_or_stmt.ast() );
    }

    if ( token_id_ != Lexer::TID_KW_ENDMODULE )
      goto error;
    token_id_ = lexer_.next_token(); // `endmodule`

    return verilog_ast_graph::ast_or_error( ag_.create_module( module_name.ast(), args, decls ) );

  error:
    fmt::print("[e] could not parse module.\n");
    return verilog_ast_graph::ast_or_error();
  }

protected:
  Lexer& lexer_;
  verilog_ast_graph& ag_;
  diagnostic_engine *diag_;
  uint32_t token_id_;
};

} // namespace lorina
