Change Log
==========

v0.1 (not yet released)
-----------------------

* Initial network interface
  `#1 <https://github.com/lsils/mockturtle/pull/1>`_ `#61 <https://github.com/lsils/mockturtle/pull/61>`_ `#96 <https://github.com/lsils/mockturtle/pull/96>`_ `#99 <https://github.com/lsils/mockturtle/pull/99>`_
* Network implementations:
    - AIG network (`aig_network`) `#1 <https://github.com/lsils/mockturtle/pull/1>`_ `#62 <https://github.com/lsils/mockturtle/pull/62>`_
    - MIG network (`mig_network`) `#4 <https://github.com/lsils/mockturtle/pull/4>`_
    - k-LUT network (`klut_network`) `#1 <https://github.com/lsils/mockturtle/pull/1>`_
    - XOR-majority graph (`xmg_network`) `#47 <https://github.com/lsils/mockturtle/pull/47>`_
    - XOR-and graph (`xag_network`) `#79 <https://github.com/lsils/mockturtle/pull/79>`_
* Algorithms:
    - Cut enumeration (`cut_enumeration`) `#2 <https://github.com/lsils/mockturtle/pull/2>`_
    - LUT mapping (`lut_mapping`) `#7 <https://github.com/lsils/mockturtle/pull/7>`_
    - Akers synthesis (`akers_synthesis`) `#9 <https://github.com/lsils/mockturtle/pull/9>`_
    - Create LUT network from mapped network (`collapse_mapped_network`) `#13 <https://github.com/lsils/mockturtle/pull/13>`_
    - MIG algebraic depth rewriting (`mig_algebraic_depth_rewriting`) `#16 <https://github.com/lsils/mockturtle/pull/16>`_ `#58 <https://github.com/lsils/mockturtle/pull/58>`_
    - Cleanup dangling nodes (`cleanup_dangling`) `#16 <https://github.com/lsils/mockturtle/pull/16>`_
    - Node resynthesis (`node_resynthesis`) `#17 <https://github.com/lsils/mockturtle/pull/17>`_
    - Reconvergency-driven cut computation (`reconv_cut`) `#24 <https://github.com/lsils/mockturtle/pull/24>`_
    - Simulate networks (`simulate`) `#25 <https://github.com/lsils/mockturtle/pull/25>`_
    - Simulate node values (`simulate_nodes`) `#28 <https://github.com/lsils/mockturtle/pull/28>`_
    - Cut rewriting (`cut_rewriting`) `#31 <https://github.com/lsils/mockturtle/pull/31>`_
    - Refactoring (`refactoring`) `#34 <https://github.com/lsils/mockturtle/pull/34>`_
    - Exact resynthesis for node resynthesis, cut rewriting, and refactoring `#46 <https://github.com/lsils/mockturtle/pull/46>`_ `#71 <https://github.com/lsils/mockturtle/pull/71>`_
    - Boolean resubstitution (`resubstitution`) `#50 <https://github.com/lsils/mockturtle/pull/50>`_ `#54 <https://github.com/lsils/mockturtle/pull/54>`_ `#82 <https://github.com/lsils/mockturtle/pull/82>`_
    - Compute satisfiability don't cares (`satisfiability_dont_cares`) `#70 <https://github.com/lsils/mockturtle/pull/70>`_
    - Compute observability don't cares (`observability_dont_cares`) `#82 <https://github.com/lsils/mockturtle/pull/82>`_
    - Optimum XMG resynthesis for node resynthesis, cut rewriting, and refactoring `#86 <https://github.com/lsils/mockturtle/pull/86>`_
    - XMG algebraic depth rewriting (`xmg_algebraic_depth_rewriting`) `#86 <https://github.com/lsils/mockturtle/pull/86>`_
    - Convert gate-based networks to node-based networks (`gates_to_nodes`) `#90 <https://github.com/lsils/mockturtle/pull/90>`_
    - Direct resynthesis of functions into primitives (`direct_resynthesis`) `#90 <https://github.com/lsils/mockturtle/pull/90>`_
* Views:
    - Visit nodes in topological order (`topo_view`) `#3 <https://github.com/lsils/mockturtle/pull/3>`_
    - Disable structural modifications to network (`immutable_view`) `#3 <https://github.com/lsils/mockturtle/pull/3>`_
    - View for mapped networks (`mapping_view`) `#7 <https://github.com/lsils/mockturtle/pull/7>`_
    - View compute depth and node levels (`depth_view`) `#16 <https://github.com/lsils/mockturtle/pull/16>`_
    - Cut view (`cut_view`) `#20 <https://github.com/lsils/mockturtle/pull/20>`_
    - Access fanout of a node (`fanout_view`) `#27 <https://github.com/lsils/mockturtle/pull/27>`_ `#49 <https://github.com/lsils/mockturtle/pull/49>`_
    - Compute MFFC of a node (`mffc_view`) `#34 <https://github.com/lsils/mockturtle/pull/34>`_
    - Compute window around a node (`window_view`) `#41 <https://github.com/lsils/mockturtle/pull/41>`_
* I/O:
    - Read AIGER files using *lorina* (`aiger_reader`) `#6 <https://github.com/lsils/mockturtle/pull/6>`_
    - Read BENCH files using *lorina* (`bench_reader`) `#6 <https://github.com/lsils/mockturtle/pull/6>`_
    - Write networks to BENCH files (`write_bench`) `#10 <https://github.com/lsils/mockturtle/pull/10>`_
    - Read Verilog files using *lorina* (`verilog_reader`) `#40 <https://github.com/lsils/mockturtle/pull/40>`_
    - Write networks to Verilog files (`write_verilog`) `#65 <https://github.com/lsils/mockturtle/pull/65>`_
    - Read PLA files using *lorina* (`pla_reader`) `#97 <https://github.com/lsils/mockturtle/pull/97>`_
* Generators for arithmetic circuits:
    - Carry ripple adder (`carry_ripple_adder`) `#5 <https://github.com/lsils/mockturtle/pull/5>`_
    - Carry ripple subtractor (`carry_ripple_subtractor`) `#32 <https://github.com/lsils/mockturtle/pull/32>`_
    - Carry ripple multiplier (`carry_ripple_multiplier`) `#45 <https://github.com/lsils/mockturtle/pull/45>`_
    - Modular adder (`modular_adder_inplace`) `#43 <https://github.com/lsils/mockturtle/pull/43>`_
    - Modular subtractor (`modular_subtractor_inplace`) `#43 <https://github.com/lsils/mockturtle/pull/43>`_
    - Modular multiplication (`modular_multiplication_inplace`) `#48 <https://github.com/lsils/mockturtle/pull/48>`_
    - 2k-to-k multiplexer (`mux_inplace`) `#43 <https://github.com/lsils/mockturtle/pull/43>`_
    - Zero padding (`zero_extend`) `#48 <https://github.com/lsils/mockturtle/pull/48>`_
    - Random logic networks for AIGs and MIGs (`random_logic_generator`) `#68 <https://github.com/lsils/mockturtle/pull/68>`_
* Utility data structures: `truth_table_cache`, `cut`, `cut_set`, `node_map`, `progress_bar`, `stopwatch`
    - Truth table cache (`truth_table_cache`) `#1 <https://github.com/lsils/mockturtle/pull/1>`_
    - Cuts (`cut` and `cut_set`) `#2 <https://github.com/lsils/mockturtle/pull/2>`_
    - Container to associate values to nodes (`node_map`) `#13 <https://github.com/lsils/mockturtle/pull/13>`_ `#76 <https://github.com/lsils/mockturtle/pull/76>`_
    - Progress bar (`progress_bar`) `#30 <https://github.com/lsils/mockturtle/pull/30>`_
    - Tracking time of computations (`stopwatch`, `call_with_stopwatch`, `make_with_stopwatch`) `#35 <https://github.com/lsils/mockturtle/pull/35>`_
