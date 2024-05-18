/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2024  EPFL
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
  \file read_genlib.hpp
  \brief Read genlib command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <vector>

#include <alice/alice.hpp>
#include <lorina/genlib.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/utils/tech_library.hpp>

namespace alice
{

class read_genlib_command : public alice::command
{
public:
  explicit read_genlib_command( const environment::ptr& env )
      : command( env,
                 "Reads a cell library from a genlib file." )
  {
    opts.add_option( "--filename,filename", filename,
                     "File to read in genlib format" )
        ->required();
    add_flag( "--large,-l", "Toggles loading large cells with more than 6 inputs [default=true]" );
    add_flag( "--multi,-m", "Toggles loading multi-output cells [default=true]" );
    add_flag( "--sym,-y", "Toggles filter symmetries (faster mapping) [default=false]" );
    add_flag( "--size,-s", "Toggles load minimum size cells only [default=true]" );
    add_flag( "--dom,-d", "Toggles removing dominated cells [default=true]" );
    add_flag( "--xms,-x", "Toggles using multi-output cells for single-output mapping [default=false]" );
  }

protected:
  void execute()
  {
    bool large = true, multi = true, sym = false, size = true, dom = true, xms = false;

    tech_library_store& lib = store<tech_library_store>().extend();

    std::vector<mockturtle::gate> gates;
    lorina::return_code result = lorina::read_genlib( filename,
                                                      mockturtle::genlib_reader( gates ) );
    if ( result != lorina::return_code::success )
    {
      env->err() << "[e] " << "Unable to read genlib file " << filename << "\n";
      return;
    }

    mockturtle::tech_library_params ps;
    ps.load_large_gates = large ^ is_set( "large" );
    ps.load_multioutput_gates = multi ^ is_set( "multi" );
    ps.ignore_symmetries = sym ^ is_set( "sym" );
    ps.load_minimum_size_only = size ^ is_set( "sym" );
    ps.remove_dominated_gates = dom ^ is_set( "dom" );
    ps.load_multioutput_gates_single = xms ^ is_set( "xms" );
    ps.verbose = true;

    lib = std::make_shared<mockturtle::tech_library<9u, mockturtle::classification_type::np_configurations>>( gates, ps );
    lib->set_library_name( filename );
  }

private:
  std::string filename{};
};

ALICE_ADD_COMMAND( read_genlib, "I/O" );

} // namespace alice