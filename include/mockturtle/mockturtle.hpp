/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
*/

#pragma once

#include "mockturtle/algorithms/aig_resub.hpp"
#include "mockturtle/algorithms/akers_synthesis.hpp"
#include "mockturtle/algorithms/aqfp/aqfp_assumptions.hpp"
#include "mockturtle/algorithms/aqfp/aqfp_db.hpp"
#include "mockturtle/algorithms/aqfp/aqfp_fanout_resyn.hpp"
#include "mockturtle/algorithms/aqfp/aqfp_node_resyn.hpp"
#include "mockturtle/algorithms/aqfp/aqfp_resynthesis.hpp"
#include "mockturtle/algorithms/aqfp/buffer_insertion.hpp"
#include "mockturtle/algorithms/aqfp/buffer_verification.hpp"
#include "mockturtle/algorithms/aqfp/detail/dag.hpp"
#include "mockturtle/algorithms/aqfp/detail/dag_cost.hpp"
#include "mockturtle/algorithms/aqfp/detail/dag_gen.hpp"
#include "mockturtle/algorithms/aqfp/detail/dag_util.hpp"
#include "mockturtle/algorithms/aqfp/detail/db_builder.hpp"
#include "mockturtle/algorithms/aqfp/detail/db_utils.hpp"
#include "mockturtle/algorithms/aqfp/detail/npn_cache.hpp"
#include "mockturtle/algorithms/aqfp/detail/partial_dag.hpp"
#include "mockturtle/algorithms/aqfp/mig_algebraic_rewriting_splitters.hpp"
#include "mockturtle/algorithms/aqfp/mig_resub_splitters.hpp"
#include "mockturtle/algorithms/balancing.hpp"
#include "mockturtle/algorithms/balancing/esop_balancing.hpp"
#include "mockturtle/algorithms/balancing/sop_balancing.hpp"
#include "mockturtle/algorithms/balancing/utils.hpp"
#include "mockturtle/algorithms/bi_decomposition.hpp"
#include "mockturtle/algorithms/cell_window.hpp"
#include "mockturtle/algorithms/circuit_validator.hpp"
#include "mockturtle/algorithms/cleanup.hpp"
#include "mockturtle/algorithms/cnf.hpp"
#include "mockturtle/algorithms/collapse_mapped.hpp"
#include "mockturtle/algorithms/cut_enumeration.hpp"
#include "mockturtle/algorithms/cut_enumeration/cnf_cut.hpp"
#include "mockturtle/algorithms/cut_enumeration/exact_map_cut.hpp"
#include "mockturtle/algorithms/cut_enumeration/gia_cut.hpp"
#include "mockturtle/algorithms/cut_enumeration/mf_cut.hpp"
#include "mockturtle/algorithms/cut_enumeration/spectr_cut.hpp"
#include "mockturtle/algorithms/cut_enumeration/tech_map_cut.hpp"
#include "mockturtle/algorithms/cut_rewriting.hpp"
#include "mockturtle/algorithms/decomposition.hpp"
#include "mockturtle/algorithms/detail/database_generator.hpp"
#include "mockturtle/algorithms/detail/mffc_utils.hpp"
#include "mockturtle/algorithms/detail/minmc_xags.hpp"
#include "mockturtle/algorithms/detail/resub_utils.hpp"
#include "mockturtle/algorithms/detail/switching_activity.hpp"
#include "mockturtle/algorithms/dont_cares.hpp"
#include "mockturtle/algorithms/dsd_decomposition.hpp"
#include "mockturtle/algorithms/equivalence_checking.hpp"
#include "mockturtle/algorithms/equivalence_classes.hpp"
#include "mockturtle/algorithms/exact_mc_synthesis.hpp"
#include "mockturtle/algorithms/exorcism.hpp"
#include "mockturtle/algorithms/experimental/boolean_optimization.hpp"
#include "mockturtle/algorithms/experimental/sim_resub.hpp"
#include "mockturtle/algorithms/experimental/window_resub.hpp"
#include "mockturtle/algorithms/extract_linear.hpp"
#include "mockturtle/algorithms/functional_reduction.hpp"
#include "mockturtle/algorithms/gates_to_nodes.hpp"
#include "mockturtle/algorithms/klut_to_graph.hpp"
#include "mockturtle/algorithms/linear_resynthesis.hpp"
#include "mockturtle/algorithms/lut_mapping.hpp"
#include "mockturtle/algorithms/mapper.hpp"
#include "mockturtle/algorithms/mig_algebraic_rewriting.hpp"
#include "mockturtle/algorithms/mig_resub.hpp"
#include "mockturtle/algorithms/miter.hpp"
#include "mockturtle/algorithms/network_fuzz_tester.hpp"
#include "mockturtle/algorithms/node_resynthesis.hpp"
#include "mockturtle/algorithms/node_resynthesis/akers.hpp"
#include "mockturtle/algorithms/node_resynthesis/bidecomposition.hpp"
#include "mockturtle/algorithms/node_resynthesis/cached.hpp"
#include "mockturtle/algorithms/node_resynthesis/composed.hpp"
#include "mockturtle/algorithms/node_resynthesis/davio.hpp"
#include "mockturtle/algorithms/node_resynthesis/direct.hpp"
#include "mockturtle/algorithms/node_resynthesis/dsd.hpp"
#include "mockturtle/algorithms/node_resynthesis/exact.hpp"
#include "mockturtle/algorithms/node_resynthesis/mig_npn.hpp"
#include "mockturtle/algorithms/node_resynthesis/null.hpp"
#include "mockturtle/algorithms/node_resynthesis/shannon.hpp"
#include "mockturtle/algorithms/node_resynthesis/traits.hpp"
#include "mockturtle/algorithms/node_resynthesis/xag_minmc.hpp"
#include "mockturtle/algorithms/node_resynthesis/xag_minmc2.hpp"
#include "mockturtle/algorithms/node_resynthesis/xag_npn.hpp"
#include "mockturtle/algorithms/node_resynthesis/xmg3_npn.hpp"
#include "mockturtle/algorithms/node_resynthesis/xmg_npn.hpp"
#include "mockturtle/algorithms/pattern_generation.hpp"
#include "mockturtle/algorithms/reconv_cut.hpp"
#include "mockturtle/algorithms/refactoring.hpp"
#include "mockturtle/algorithms/resubstitution.hpp"
#include "mockturtle/algorithms/resyn_engines/aig_enumerative.hpp"
#include "mockturtle/algorithms/resyn_engines/mig_enumerative.hpp"
#include "mockturtle/algorithms/resyn_engines/mig_resyn.hpp"
#include "mockturtle/algorithms/resyn_engines/xag_resyn.hpp"
#include "mockturtle/algorithms/satlut_mapping.hpp"
#include "mockturtle/algorithms/sim_resub.hpp"
#include "mockturtle/algorithms/simulation.hpp"
#include "mockturtle/algorithms/window_rewriting.hpp"
#include "mockturtle/algorithms/xag_optimization.hpp"
#include "mockturtle/algorithms/xag_resub_withDC.hpp"
#include "mockturtle/algorithms/xmg_algebraic_rewriting.hpp"
#include "mockturtle/algorithms/xmg_optimization.hpp"
#include "mockturtle/algorithms/xmg_resub.hpp"
#include "mockturtle/generators/arithmetic.hpp"
#include "mockturtle/generators/control.hpp"
#include "mockturtle/generators/legacy.hpp"
#include "mockturtle/generators/majority.hpp"
#include "mockturtle/generators/majority_n.hpp"
#include "mockturtle/generators/modular_arithmetic.hpp"
#include "mockturtle/generators/random_network.hpp"
#include "mockturtle/generators/self_dualize.hpp"
#include "mockturtle/generators/sorting.hpp"
#include "mockturtle/io/aiger_reader.hpp"
#include "mockturtle/io/bench_reader.hpp"
#include "mockturtle/io/blif_reader.hpp"
#include "mockturtle/io/bristol_reader.hpp"
#include "mockturtle/io/dimacs_reader.hpp"
#include "mockturtle/io/genlib_reader.hpp"
#include "mockturtle/io/pla_reader.hpp"
#include "mockturtle/io/serialize.hpp"
#include "mockturtle/io/super_reader.hpp"
#include "mockturtle/io/verilog_reader.hpp"
#include "mockturtle/io/write_aiger.hpp"
#include "mockturtle/io/write_bench.hpp"
#include "mockturtle/io/write_blif.hpp"
#include "mockturtle/io/write_dimacs.hpp"
#include "mockturtle/io/write_dot.hpp"
#include "mockturtle/io/write_patterns.hpp"
#include "mockturtle/io/write_verilog.hpp"
#include "mockturtle/networks/abstract_xag.hpp"
#include "mockturtle/networks/aig.hpp"
#include "mockturtle/networks/aqfp.hpp"
#include "mockturtle/networks/buffered.hpp"
#include "mockturtle/networks/detail/foreach.hpp"
#include "mockturtle/networks/events.hpp"
#include "mockturtle/networks/klut.hpp"
#include "mockturtle/networks/mig.hpp"
#include "mockturtle/networks/storage.hpp"
#include "mockturtle/networks/xag.hpp"
#include "mockturtle/networks/xmg.hpp"
#include "mockturtle/properties/aqfpcost.hpp"
#include "mockturtle/properties/mccost.hpp"
#include "mockturtle/properties/migcost.hpp"
#include "mockturtle/properties/xmgcost.hpp"
#include "mockturtle/traits.hpp"
#include "mockturtle/utils/algorithm.hpp"
#include "mockturtle/utils/cost_functions.hpp"
#include "mockturtle/utils/cuts.hpp"
#include "mockturtle/utils/debugging_utils.hpp"
#include "mockturtle/utils/hash_functions.hpp"
#include "mockturtle/utils/include/percy.hpp"
#include "mockturtle/utils/index_list.hpp"
#include "mockturtle/utils/json_utils.hpp"
#include "mockturtle/utils/mixed_radix.hpp"
#include "mockturtle/utils/name_utils.hpp"
#include "mockturtle/utils/network_cache.hpp"
#include "mockturtle/utils/network_utils.hpp"
#include "mockturtle/utils/node_map.hpp"
#include "mockturtle/utils/progress_bar.hpp"
#include "mockturtle/utils/stopwatch.hpp"
#include "mockturtle/utils/string_utils.hpp"
#include "mockturtle/utils/super_utils.hpp"
#include "mockturtle/utils/tech_library.hpp"
#include "mockturtle/utils/truth_table_cache.hpp"
#include "mockturtle/utils/truth_table_utils.hpp"
#include "mockturtle/utils/window_utils.hpp"
#include "mockturtle/views/binding_view.hpp"
#include "mockturtle/views/cnf_view.hpp"
#include "mockturtle/views/color_view.hpp"
#include "mockturtle/views/cut_view.hpp"
#include "mockturtle/views/depth_view.hpp"
#include "mockturtle/views/fanout_limit_view.hpp"
#include "mockturtle/views/fanout_view.hpp"
#include "mockturtle/views/immutable_view.hpp"
#include "mockturtle/views/mapping_view.hpp"
#include "mockturtle/views/mffc_view.hpp"
#include "mockturtle/views/names_view.hpp"
#include "mockturtle/views/topo_view.hpp"
#include "mockturtle/views/window_view.hpp"