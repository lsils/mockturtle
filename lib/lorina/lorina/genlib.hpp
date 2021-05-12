/* lorina: C++ parsing library
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
  \file genlib.hpp
  \brief Implements GENLIB parser

  \author Heinz Riener
  \author Shubham Rai
*/

#pragma once

#include "common.hpp"
#include "detail/utils.hpp"
#include "diagnostics.hpp"
#include <fstream>
#include <istream>
#include <optional>
#include <sstream>
#include <string>

namespace lorina
{

enum class phase_type : uint8_t
{
  INV = 0,
  NONINV = 1,
  UNKNOWN = 2,
};

// PIN <pin-name> <phase> <input-load> <max-load> <rise-block-delay> <rise-fanout-delay> <fall-block-delay> <fall-fanout-delay>
struct pin_spec
{
  std::string name;
  phase_type phase;
  double input_load;
  double max_load;
  double rise_block_delay;
  double rise_fanout_delay;
  double fall_block_delay;
  double fall_fanout_delay;
}; /* pin_spec */

/*! \brief A reader visitor for a GENLIB format.
 *
 * Callbacks for the GENLIB format.
 */
class genlib_reader
{
public:
  virtual void on_gate( std::string const& name, std::string const& expression, double area, std::vector<pin_spec> const& pins ) const
  {
    (void)name;
    (void)expression;
    (void)area;
    (void)pins;
  }
}; /* genlib_reader */

/*! \brief Parse for the GENLIB format.
 *
 */
class genlib_parser
{
public:
  explicit genlib_parser( std::istream& in, genlib_reader const& reader, diagnostic_engine* diag )
    : in( in )
    , reader( reader )
    , diag( diag )
  {}

public:
  bool run()
  {
    std::string line;
    while ( std::getline( in, line ) )
    {
      /* remove whitespaces */
      detail::trim( line );

      /* skip comments and empty lines */
      if ( line[0] == '#' || line.empty() )
      {
        continue;
      }

      if ( !parse_gate_definition( line ) )
      {
        return false;
      }
    }

    return true;
  }

private:
  bool parse_gate_definition( std::string const& line )
  {
    std::stringstream ss( line );
    std::string const deliminators{" \t\r\n"};

    std::string token;

    std::vector<std::string> tokens;
    while ( std::getline( ss, token ) )
    {
      std::size_t prev = 0, pos;
      while ( ( pos = line.find_first_of( deliminators, prev ) ) != std::string::npos )
      {
        if ( pos > prev )
        {
          tokens.emplace_back( token.substr( prev, pos - prev ) );
        }
        prev = pos + 1;
      }

      if ( prev < line.length() )
      {
        tokens.emplace_back( token.substr( prev, std::string::npos ) );
      }
    }

    if ( tokens.size() < 4u )
    {
      if ( diag )
      {
        diag->report( diag_id::ERR_GENLIB_UNEXPECTED_STRUCTURE ).add_argument( line );
      }
      return false;
    }

    if ( tokens[0] != "GATE" )
    {
      if ( diag )
      {
        diag->report( diag_id::ERR_GENLIB_GATE ).add_argument( line );
      }
      return false;
    }
    auto const beg = tokens[3].find_first_of( "=" );
    auto const end = tokens[3].find_first_of( ";" );
    if ( beg == std::string::npos || end == std::string::npos )
    {
      if ( diag )
      {
        diag->report( diag_id::ERR_GENLIB_EXPRESSION ).add_argument( tokens[3] );
      }
      return false;
    }

    std::string const& name = tokens[1];
    std::string const& expression = tokens[3].substr( beg + 1, end - beg - 1 );
    double const area = std::stod( tokens[2] );

    std::vector<pin_spec> pins;

    uint64_t i{4};
    for ( ; i+8 < tokens.size(); i += 9 )
    {
      /* check PIN specification */
      if ( tokens[i] != "PIN" )
      {
        if ( diag )
        {
          diag->report( diag_id::ERR_GENLIB_PIN ).add_argument( tokens[i] );
        }
        return false;
      }

      std::string const& name = tokens[i+1];
      phase_type phase{phase_type::UNKNOWN};
      if ( tokens[i+2] == "INV" )
      {
        phase = phase_type::INV;
      }
      else if ( tokens[i+2] == "NONINV" )
      {
        phase = phase_type::NONINV;
      }
      else
      {
        if ( tokens[i+2] != "UNKNOWN" )
        {
          if ( diag )
          {
            diag->report( diag_id::ERR_GENLIB_PIN_PHASE ).add_argument( tokens[i+1] );
          }
        }
      }

      double const input_load = std::stod( tokens[i+3] );
      double const max_load = std::stod( tokens[i+4] );
      double const rise_block_delay = std::stod( tokens[i+5] );
      double const rise_fanout_delay = std::stod( tokens[i+6] );
      double const fall_block_delay = std::stod( tokens[i+7] );
      double const fall_fanout_delay = std::stod( tokens[i+8] );

      pins.emplace_back( pin_spec{name,phase,input_load,max_load,rise_block_delay,rise_fanout_delay,fall_block_delay,fall_fanout_delay} );
    }

    if ( i != tokens.size() )
    {
      if ( diag )
      {
        diag->report( diag_id::ERR_GENLIB_FAILED ).add_argument( tokens[i] );
      }
      return false;
    }

    reader.on_gate( name, expression, area, pins );
    return true;
  }

protected:
  std::istream& in;
  genlib_reader const& reader;
  diagnostic_engine* diag;
}; /* genlib_parser */


/*! \brief Reader function for the GENLIB format.
 *
 * Reads GENLIB format from a stream and invokes a callback
 * method for each parsed primitive and each detected parse error.
 *
 * \param in Input stream
 * \param reader GENLIB reader with callback methods invoked for parsed primitives
 * \param diag An optional diagnostic engine with callback methods for parse errors
 * \return Success if parsing has been successful, or parse error if parsing has failed
 */
[[nodiscard]] inline return_code read_genlib( std::istream& in, const genlib_reader& reader, diagnostic_engine* diag = nullptr )
{
  genlib_parser parser( in, reader, diag );
  auto result = parser.run();
  if ( !result )
  {
    return return_code::parse_error;
  }
  else
  {
    return return_code::success;
  }
}

/*! \brief Reader function for the GENLIB format.
 *
 * Reads GENLIB format from a file and invokes a callback
 * method for each parsed primitive and each detected parse error.
 *
 * \param filename Name of the file
 * \param reader GENLIB reader with callback methods invoked for parsed primitives
 * \param diag An optional diagnostic engine with callback methods for parse errors
 * \return Success if parsing has been successful, or parse error if parsing has failed
 */
[[nodiscard]] inline return_code read_genlib( const std::string& filename, const genlib_reader& reader, diagnostic_engine* diag = nullptr )
{
  std::ifstream in( detail::word_exp_filename( filename ), std::ifstream::in );
  if ( !in.is_open() )
  {
    if ( diag )
    {
      diag->report( diag_id::ERR_FILE_OPEN ).add_argument( filename );
    }
    return return_code::parse_error;
  }
  else
  {
    auto const ret = read_genlib( in, reader, diag );
    in.close();
    return ret;
  }
}

} /* lorina */
