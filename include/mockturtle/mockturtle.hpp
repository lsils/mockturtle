/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
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
#include "algorithms/cleanup.hpp"
#include "algorithms/collapse_mapped.hpp"
#include "algorithms/cut_enumeration.hpp"
#include "algorithms/cut_enumeration/gia_cut.hpp"
#include "algorithms/cut_enumeration/mf_cut.hpp"
#include "algorithms/cut_rewriting.hpp"
#include "algorithms/lut_mapping.hpp"
#include "algorithms/mig_algebraic_rewriting.hpp"
#include "algorithms/node_resynthesis.hpp"
#include "algorithms/node_resynthesis/akers.hpp"
#include "algorithms/node_resynthesis/mig_npn.hpp"
#include "algorithms/reconv_cut.hpp"
#include "algorithms/refactoring.hpp"
#include "algorithms/resubstitution.hpp"
#include "algorithms/simulation.hpp"
#include "generators/arithmetic.hpp"
#include "io/aiger_reader.hpp"
#include "io/bench_reader.hpp"
#include "io/verilog_reader.hpp"
#include "io/write_bench.hpp"
#include "networks/aig.hpp"
#include "networks/klut.hpp"
#include "networks/mig.hpp"
#include "networks/xag.hpp"
#include "networks/xmg.hpp"
#include "utils/cuts.hpp"
#include "utils/mixed_radix.hpp"
#include "utils/node_map.hpp"
#include "utils/progress_bar.hpp"
#include "utils/stopwatch.hpp"
#include "utils/truth_table_cache.hpp"
#include "views/cut_view.hpp"
#include "views/depth_view.hpp"
#include "views/immutable_view.hpp"
#include "views/mapping_view.hpp"
#include "views/mffc_view.hpp"
#include "views/fanout_view.hpp"
#include "views/topo_view.hpp"
