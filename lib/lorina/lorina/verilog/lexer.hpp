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
  \file lexer.hpp
  \brief Verilog Lexer

  \author Heinz Riener
*/

#pragma once

#include <fstream>
#include <stack>
#include <unordered_map>

namespace lorina
{

enum class token_kind : int8_t
{
  TK_EOF = 0,
  TK_KEYWORD = 1,
  TK_IDENTIFIER = 2,
  TK_NUMERAL = 3,
  TK_LITERAL = 4,
  TK_OPERATOR = 5,
  TK_UNKNOWN = 6,
}; // enum class token_kind

struct token
{
  token_kind kind;
  std::string lexem;
}; // struct token

/*! \brief A Verilog lexer.
 *
 * Transforms a sequences of characters into a sequence of tokens.
 *
 * Example:
 *   std::string source{ ... };
 *   std::istringstream in( source );
 *   std::noskipws( in );
 *
 *   using Lexer = verilog_lexer<std::istream_iterator<char>>;
 *   Lexer lexer( ( std::istream_iterator<char>(in) ), std::istream_iterator<char>());
 *
 *   uint32_t token_id;
 *   while ( ( token_id = lexer.next_token() ) != Lexer::TID_EOF )
 *   {
 *     fmt::print( "token_id = {}: lexem = {}\n",
 *                 token_id, lexer.get( token_id ).lexem );
 *   }
 *
 */
template<typename Iterator>
class verilog_lexer
{
public:
  static constexpr uint32_t const TID_EOF{0};
  static constexpr uint32_t const TID_KW_MODULE{1};
  static constexpr uint32_t const TID_KW_ENDMODULE{2};
  static constexpr uint32_t const TID_KW_PARAMETER{3};
  static constexpr uint32_t const TID_KW_INPUT{4};
  static constexpr uint32_t const TID_KW_OUTPUT{5};
  static constexpr uint32_t const TID_KW_WIRE{6};
  static constexpr uint32_t const TID_KW_ASSIGN{7};
  static constexpr uint32_t const TID_OP_LPAREN{8};
  static constexpr uint32_t const TID_OP_RPAREN{9};
  static constexpr uint32_t const TID_OP_LSQUARED{10};
  static constexpr uint32_t const TID_OP_RSQUARED{11};
  static constexpr uint32_t const TID_OP_LCURLY{12};
  static constexpr uint32_t const TID_OP_RCURLY{13};
  static constexpr uint32_t const TID_OP_ASSIGN{14};
  static constexpr uint32_t const TID_OP_ROUTE{15};
  static constexpr uint32_t const TID_OP_DOT{16};
  static constexpr uint32_t const TID_OP_COMMA{17};
  static constexpr uint32_t const TID_OP_COLON{18};
  static constexpr uint32_t const TID_OP_SEMICOLON{19};
  static constexpr uint32_t const TID_OP_BITWISE_NOT{20};
  static constexpr uint32_t const TID_OP_BITWISE_AND{21};
  static constexpr uint32_t const TID_OP_BITWISE_OR{22};
  static constexpr uint32_t const TID_OP_BITWISE_XOR{23};
  static constexpr uint32_t const TID_OP_SUB{24};
  static constexpr uint32_t const TID_OP_ADD{25};
  static constexpr uint32_t const TID_OP_MUL{26};
  static constexpr uint32_t const TID_LAST{27};

public:
  explicit verilog_lexer( const Iterator& begin, const Iterator& end )
    : begin_( begin )
    , end_( end )
    , it_( begin )
    , last_char_( ' ' )
  {
    /* register tokens for EOF */
    assert( symbol_table_.size() == TID_EOF );
    symbol_table_.emplace_back( token{token_kind::TK_EOF,""} );

    /* register tokens for keywords */
    assert( symbol_table_.size() == TID_KW_MODULE );
    symbol_table_.emplace_back( token{token_kind::TK_KEYWORD,"module"} );

    assert( symbol_table_.size() == TID_KW_ENDMODULE );
    symbol_table_.emplace_back( token{token_kind::TK_KEYWORD,"endmodule"} );

    assert( symbol_table_.size() == TID_KW_PARAMETER );
    symbol_table_.emplace_back( token{token_kind::TK_KEYWORD,"parameter"} );

    assert( symbol_table_.size() == TID_KW_INPUT );
    symbol_table_.emplace_back( token{token_kind::TK_KEYWORD,"input"} );

    assert( symbol_table_.size() == TID_KW_OUTPUT );
    symbol_table_.emplace_back( token{token_kind::TK_KEYWORD,"output"} );

    assert( symbol_table_.size() == TID_KW_WIRE );
    symbol_table_.emplace_back( token{token_kind::TK_KEYWORD,"wire"} );

    assert( symbol_table_.size() == TID_KW_ASSIGN );
    symbol_table_.emplace_back( token{token_kind::TK_KEYWORD,"assign"} );

    /* register tokens for operators */
    assert( symbol_table_.size() == TID_OP_LPAREN );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"("} );

    assert( symbol_table_.size() == TID_OP_RPAREN );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,")"} );

    assert( symbol_table_.size() == TID_OP_LSQUARED );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"["} );

    assert( symbol_table_.size() == TID_OP_RSQUARED );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"]"} );

    assert( symbol_table_.size() == TID_OP_LCURLY );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"{"} );

    assert( symbol_table_.size() == TID_OP_RCURLY );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"}"} );

    assert( symbol_table_.size() == TID_OP_ASSIGN );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"="} );

    assert( symbol_table_.size() == TID_OP_ROUTE );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"#"} );

    assert( symbol_table_.size() == TID_OP_DOT );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"."} );

    assert( symbol_table_.size() == TID_OP_COMMA );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,","} );

    assert( symbol_table_.size() == TID_OP_COLON );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,":"} );

    assert( symbol_table_.size() == TID_OP_SEMICOLON );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,";"} );

    assert( symbol_table_.size() == TID_OP_BITWISE_NOT );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"~"} );

    assert( symbol_table_.size() == TID_OP_BITWISE_AND );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"&"} );

    assert( symbol_table_.size() == TID_OP_BITWISE_OR );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"|"} );

    assert( symbol_table_.size() == TID_OP_BITWISE_XOR );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"^"} );

    assert( symbol_table_.size() == TID_OP_SUB );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"-"} );

    assert( symbol_table_.size() == TID_OP_ADD );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"+"} );

    assert( symbol_table_.size() == TID_OP_MUL );
    symbol_table_.emplace_back( token{token_kind::TK_OPERATOR,"*"} );
  }

  inline int consume()
  {
    return last_char_ = it_ != end_ ? *it_++ : std::ifstream::traits_type::eof();
  }

  inline void bookmark()
  {
    the_stack_.emplace( std::make_pair( it_, last_char_ ) );
  }

  inline void rollback()
  {
    std::tie( it_, last_char_ ) = the_stack_.top();
    the_stack_.pop();
  }

  inline uint32_t next_token()
  {
    /* skip whitespaces */
    while ( isspace( last_char_ ) )
    {
      consume();
    }

    /* skip line comment */
    if ( is_line_comment_start( last_char_ ) == 1 )
    {
      bookmark();
      consume();

      if ( is_line_comment_start( last_char_ ) == 2 )
      {
        while ( is_line_comment_rest( consume() ) );

        /* skip whitespaces */
        while ( isspace( last_char_ ) )
        {
          consume();
        }
      }
      else
      {
        rollback();
      }
    }

    /* skip block comment */
    if ( is_block_comment_start( last_char_ ) == 1 )
    {
      bookmark();
      consume();

      if ( is_block_comment_start( last_char_ ) == 2 )
      {
        while ( is_block_comment_rest( consume() ) );

        /* skip whitespaces */
        while ( isspace( last_char_ ) )
        {
          consume();
        }
      }
      else
      {
        rollback();
      }
    }

    /* identifier */
    if ( is_escaped_identifier_first( last_char_ ) )
    {
      std::string identifier;
      identifier += last_char_;

      bookmark();
      while ( is_escaped_identifier_rest( consume() ) )
      {
        identifier += last_char_;
      }

      if ( identifier == "\\" )
      {
        rollback();
      }
      else
      {
        /* register identifier if new */
        auto it = lexem_to_id_.find(identifier);
        if ( it != std::end( lexem_to_id_ ) )
        {
          return it->second;
        }
        else
        {
          const uint32_t symbol_id = symbol_table_.size();
          symbol_table_.emplace_back( token{token_kind::TK_IDENTIFIER,identifier} );
          lexem_to_id_[identifier] = symbol_id;
          return symbol_id;
        }
      }
    }

    if ( is_identifier_first( last_char_ ) )
    {
      std::string identifier;
      identifier += last_char_;
      while ( is_identifier_rest( consume() ) )
      {
        identifier += last_char_;
      }

      if ( identifier == "module" )
      {
        return TID_KW_MODULE;
      }
      else if ( identifier == "endmodule" )
      {
        return TID_KW_ENDMODULE;
      }
      else if ( identifier == "parameter" )
      {
        return TID_KW_PARAMETER;
      }
      else if ( identifier == "input" )
      {
        return TID_KW_INPUT;
      }
      else if ( identifier == "output" )
      {
        return TID_KW_OUTPUT;
      }
      else if ( identifier == "wire" )
      {
        return TID_KW_WIRE;
      }
      else if ( identifier == "assign" )
      {
        return TID_KW_ASSIGN;
      }

      /* register identifier if new */
      auto it = lexem_to_id_.find(identifier);
      if ( it != std::end( lexem_to_id_ ) )
      {
        return it->second;
      }
      else
      {
        const uint32_t symbol_id = symbol_table_.size();
        symbol_table_.emplace_back( token{token_kind::TK_IDENTIFIER,identifier} );
        lexem_to_id_[identifier] = symbol_id;
        return symbol_id;
      }
    }

    if ( is_numeral_first( last_char_ ) )
    {
      std::string numeral;
      numeral += last_char_;
      while ( is_numeral_rest( consume() ) )
      {
        numeral += last_char_;
      }

      if ( last_char_ == '\'' )
      {
        numeral += last_char_;
        consume(); // '
        if ( last_char_ == 'b' || last_char_ == 'd' )
        {
          numeral += last_char_;
          consume(); // b, d
          if ( is_numeral_rest( last_char_ ) )
          {
            numeral += last_char_;
            while ( is_numeral_rest( consume() ) )
            {
              numeral += last_char_;
            }
          }
        }
        else if ( last_char_ == 'o' )
        {
          numeral += last_char_;
          consume(); // o
          if ( is_oct_numeral_rest( last_char_ ) )
          {
            numeral += last_char_;
            while ( is_oct_numeral_rest( consume() ) )
            {
              numeral += last_char_;
            }
          }
        }
        else if ( last_char_ == 'h' )
        {
          numeral += last_char_;
          consume(); // h
          if ( is_hex_numeral_rest( last_char_ ) )
          {
            numeral += last_char_;
            while ( is_hex_numeral_rest( consume() ) )
            {
              numeral += last_char_;
            }
          }
        }
      }

      /* register numeral if new */
      auto it = lexem_to_id_.find(numeral);
      if ( it != std::end( lexem_to_id_ ) )
      {
        return it->second;
      }
      else
      {
        const uint32_t symbol_id = symbol_table_.size();
        symbol_table_.emplace_back( token{token_kind::TK_NUMERAL,numeral} );
        lexem_to_id_[numeral] = symbol_id;
        return symbol_id;
      }
    }

    if ( is_literal_first( last_char_ ) )
    {
      std::string literal;
      literal += last_char_;
      while ( is_literal_rest( consume() ) )
      {
        literal += last_char_;
      }

      /* register literal if new */
      auto it = lexem_to_id_.find(literal);
      if ( it != std::end( lexem_to_id_ ) )
      {
        return it->second;
      }
      else
      {
        const uint32_t symbol_id = symbol_table_.size();
        symbol_table_.emplace_back( token{token_kind::TK_LITERAL,literal} );
        lexem_to_id_[literal] = symbol_id;
        return symbol_id;
      }
    }

    if ( last_char_ == std::ifstream::traits_type::eof() )
    {
      return TID_EOF;
    }

    // otherwise, just return the operator
    const char this_char = last_char_;
    consume();

    switch ( this_char )
    {
    case '(':
      return TID_OP_LPAREN;
    case ')':
      return TID_OP_RPAREN;
    case '[':
      return TID_OP_LSQUARED;
    case ']':
      return TID_OP_RSQUARED;
    case '{':
      return TID_OP_LCURLY;
    case '}':
      return TID_OP_RCURLY;
    case '=':
      return TID_OP_ASSIGN;
    case '#':
      return TID_OP_ROUTE;
    case '.':
      return TID_OP_DOT;
    case ',':
      return TID_OP_COMMA;
    case ':':
      return TID_OP_COLON;
    case ';':
      return TID_OP_SEMICOLON;
    case '~':
      return TID_OP_BITWISE_NOT;
    case '&':
      return TID_OP_BITWISE_AND;
    case '|':
      return TID_OP_BITWISE_OR;
    case '^':
      return TID_OP_BITWISE_XOR;
    case '+':
      return TID_OP_ADD;
    case '-':
      return TID_OP_SUB;
    case '*':
      return TID_OP_MUL;
    default:
      break;
    }

    const uint32_t symbol_id = symbol_table_.size();
    symbol_table_.emplace_back( token{token_kind::TK_UNKNOWN,std::string{this_char}} );
    return symbol_id;
  }

  const token& get( uint32_t id ) const
  {
    assert( symbol_table_.size() > id );
    return symbol_table_[id];
  }

protected:
  inline bool is_identifier_first(char ch)
  {
    return isalpha(ch) || ch == '$' || ch == '_';
  }

  inline bool is_identifier_rest(char ch)
  {
    return isalnum(ch) || ch == '_';
  }

  inline bool is_escaped_identifier_first(char ch)
  {
    return ch == '\\';
  }

  inline bool is_escaped_identifier_rest(char ch)
  {
    return !( ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' );
  }

  inline bool is_numeral_first(char ch)
  {
    return ch >= '0' && ch <= '9';
  }

  inline bool is_numeral_rest(char ch)
  {
    return ( ch >= '0' && ch <= '9' ) || ch == '_';
  }

  inline bool is_oct_numeral_rest(char ch)
  {
    return ( ch >= '0' && ch <= '7' ) || ch == '_';
  }

  inline bool is_hex_numeral_rest(char ch)
  {
    return ( ch >= '0' && ch <= '9' ) || ( ch >= 'A' && ch <= 'F' ) || ( ch >= 'a' && ch <= 'f' ) || ch == '_';
  }

  inline bool is_literal_first(char ch)
  {
    return ch == '\"';
  }

  inline bool is_literal_rest(char ch)
  {
    static bool next_time_fail = false;
    if ( next_time_fail )
    {
      next_time_fail = false;
      return false;
    }

    if ( ch == '\"' )
    {
      next_time_fail = true;
    }
    return true;
  }

  inline int is_line_comment_start(char ch)
  {
    static int state = 0;
    if ( state < 2 && ch == '/' )
    {
      ++state;
      return state;
    }
    else
    {
      state = 0;
      return state;
    }
  }

  inline bool is_line_comment_rest(char ch)
  {
    static int state = 0;
    switch ( state )
    {
    case 0:
      {
        if ( ch == '\n' )
        {
          ++state;
          return true;
        }
        break;
      }
    case 1:
      {
        state = 0;
        return false;
      }
    default:
      break;
    }
    state = 0;
    return true;
  }

  inline int is_block_comment_start(char ch)
  {
    static int state = 0;
    switch ( state )
    {
    case 0:
      {
        if ( ch == '/' )
        {
          ++state;
          return state;
        }
        break;
      }
    case 1:
      {
        if ( ch == '*' )
        {
          ++state;
          return state;
        }
        break;
      }
    default:
      break;
    }
    state = 0;
    return state;
  }

  inline bool is_block_comment_rest(char ch)
  {
    static int state = 0;
    switch ( state )
    {
    case 0:
      {
        if ( ch == '*' )
        {
          ++state;
          return true;
        }
        break;
      }
    case 1:
      {
        if ( ch == '/' )
        {
          ++state;
          return true;
        }
        break;
      }
    case 2:
      {
        state = 0;
        return false;
      }
    default:
      break;
    }

    state = 0;
    return true;
  }

protected:
  Iterator begin_;
  Iterator end_;

  Iterator it_;
  int last_char_;
  std::stack<std::pair<Iterator,int>> the_stack_;

  std::vector<token> symbol_table_;
  std::unordered_map<std::string, uint32_t> lexem_to_id_;
}; // class verilog_lexer

} // namespace lorina
