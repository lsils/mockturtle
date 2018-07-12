/* lorina: C++ parsing library
 * Copyright (C) 2017-2018  EPFL
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

#pragma once

#include <iostream>
#include <string>

namespace lorina
{

namespace detail
{

class tokenizer
{
public:
  tokenizer( std::istream& is )
    : _is( is )
  {}

  bool get_char( char& c )
  {
    if ( !lookahead.empty() )
    {
      c = *lookahead.rbegin();
      lookahead.pop_back();
      return true;
    }
    else
    {
      return bool(_is.get( c ));
    }
  }

  bool get_token_internal( std::string& token )
  {
    if ( _done )
    {
      return false;
    }
    token = "";

    char c;
    while ( get_char( c ) )
    {
      if ( (c == ' ' || c == '\\' || c == '\n') && !_quote_mode )
      {
        return true;
      }

      if ( ( c == '(' || c == ')' || c == '{' || c == '}' || c == ';' || c == ':' || c == ',' || c == '~' || c == '&' || c == '|' || c == '^' ) && !_quote_mode )
      {
        if ( token.empty() )
        {
          token = std::string() + c;
        }
        else
        {
          lookahead += c;
        }

        return true;
      }

      if ( c == '\"' )
      {
        _quote_mode = !_quote_mode;
      }

      token += c;
    }

    _done = true;
    return true;
  }

  bool get_token( std::string& token )
  {
    bool result;
    do
    {
      result = get_token_internal( token );
      detail::trim( token );
    } while ( token == "" && result );
    return result;
  }

protected:
  bool _done = false;
  bool _quote_mode = false;
  std::istream& _is;
  std::string lookahead;
}; /* tokenizer */

} // namespace detail

} // namespace lorina
