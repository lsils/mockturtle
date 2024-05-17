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
  \file print_stats.hpp
  \brief Printing statistics command

  \author Alessandro tempia Calvino
*/

#pragma once

#include <alice/alice.hpp>

namespace alice
{

/* Writes a network to file */
class print_stats_command : public alice::command
{
public:
  explicit print_stats_command( const environment::ptr& env )
      : command( env,
                 "Prints the statistics of the current logic network." )
  {
    /* TODO: add flags for additional custom statistics */
  }

protected:
  void execute()
  {
    if ( !store<network_manager>().empty() )
    {
      env->out() << store<network_manager>().current().stats() << std::endl;
    }
    else
    {
      env->err() << "No logic network" << std::endl;
    }
  }
};

ALICE_ADD_COMMAND( print_stats, "Printing" );

} // namespace alice