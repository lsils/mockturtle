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
  \file truth.hpp
  \brief Implements TRUTH parser

  \author Andrea Costamagna
*/

#pragma once

#include "common.hpp"
#include "detail/utils.hpp"
#include "diagnostics.hpp"
#include <iostream>
#include <optional>

namespace lorina
{

/*! \brief A reader visitor for the TRUTH format.
 *
 * Callbacks for the TRUTH ( iwls2022 contest benchmarks ) format.
 */
class truth_reader
{

public:
  /*! \brief Callback method for parsed input.
   *
   * \param no parameters needed
   */
  virtual void on_input() const
  {
  }

  /*! \brief Callback method for parsed output.
   *
   * \param name Output name
   */
  virtual void on_output( const std::string& tt_binary_string ) const
  {
    (void)tt_binary_string;
  }
};

/*! \brief Reader function for the TRUTH format.
 *
 * Reads TRUTH format from a stream and invokes a callback
 * method for defining the network. The truth format describes the network
 * as a truth table for each output node. 
 *
 * \param in Input stream
 * \param reader A TRUTH reader with callback methods invoked for parsed primitives
 * \param diag An optional diagnostic engine with callback methods for parse errors
 * \return Success if parsing has been successful, or parse error if parsing has failed
 */
[[nodiscard]] inline return_code read_truth( std::istream& in, const truth_reader& reader, diagnostic_engine* diag = nullptr )
{
  return_code result = return_code::success;
  std::vector<uint64_t> sizes;
  std::vector<std::string> tt_binary_strings;
  detail::foreach_line_in_file_escape( in, [&]( std::string line ) {
    redo:
      sizes.push_back( line.size() );
      tt_binary_strings.push_back( line );
      return true;
  } );

  float num_inputs = log2( tt_binary_strings[0].size() );

  if ( ceil( num_inputs ) != floor( num_inputs ) )
    return return_code::parse_error;

  if ( tt_binary_strings.size() > 1 ) /* check that all kluts have the same size */
  {
    for ( auto i = 1u; i < tt_binary_strings.size(); ++i )
    {
      if ( sizes[0] != sizes[i] )
        return return_code::parse_error;
      else
        result = return_code::success;
    }
  }
  for ( uint64_t i{ 0 }; i < num_inputs; ++i )
    reader.on_input();

  for ( uint64_t i{ 0 }; i < tt_binary_strings.size(); ++i )
    reader.on_output( tt_binary_strings[i] );

  return result;
}

/*! \brief Reader function for TRUTH format.
 *
 * Reads binary TRUTH format from a file and invokes a callback
 * method for each parsed primitive and each detected parse error.
 *
 * \param filename Name of the file
 * \param reader A TRUTH reader with callback methods invoked for parsed primitives
 * \param diag An optional diagnostic engine with callback methods for parse errors
 * \return Success if parsing has been successful, or parse error if parsing has failed
 */
[[nodiscard]] inline return_code read_truth( const std::string& filename, const truth_reader& reader, diagnostic_engine* diag = nullptr )
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
    auto const ret = read_truth( in, reader, diag );
    in.close();
    return ret;
  }
}

} // namespace lorina
