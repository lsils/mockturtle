/* mockturtle: C++ logic network library
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
  \file mockturtle.hpp
  \brief Main header file for mockturtle

  \author Mathias Soeken
*/

#pragma once

#include "traits.hpp"
#include "algorithms/akers_synthesis.hpp"
#include "algorithms/cut_enumeration.hpp"
#include "generators/arithmetic.hpp"
#include "io/aiger_reader.hpp"
#include "io/bench_reader.hpp"
#include "io/write_bench.hpp"
#include "networks/aig.hpp"
#include "networks/klut.hpp"
#include "networks/mig.hpp"
#include "utils/cuts.hpp"
#include "utils/mixed_radix.hpp"
#include "utils/truth_table_cache.hpp"
#include "views/immutable_view.hpp"
#include "views/topo_view.hpp"
