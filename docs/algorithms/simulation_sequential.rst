Sequential simulation
---------------------

**Header:** ``mockturtle/algorithms/simulation_sequential.hpp``

``simulate`` evaluates the combinational logic of a network exactly once and has no notion of a register.  On a sequential network it never assigns the register outputs at all, so every value in their fanout cone is meaningless.

``simulate_sequential`` runs the network over a number of clock cycles instead.  Every register starts at its reset value, the combinational logic is evaluated once per cycle, the primary outputs are recorded, and the register inputs are latched into the register outputs for the next cycle.

It returns a ``simulate_sequential_result``, which carries two traces indexed by clock cycle: ``outputs[cycle][index]`` is the value primary output ``index`` took in that cycle, and ``states[cycle][index]`` the value register ``index`` held while that cycle was evaluated.  The state trace is one entry longer than the output trace, because simulating *n* cycles crosses *n + 1* state boundaries -- ``reset_state()`` is the one the run started from and ``final_state()`` the one it ended in.

**Examples**

A design with no primary inputs runs off its reset state alone.  A 4-bit LFSR seeded with ``0b0001`` walks through all 15 of its non-zero states:

.. code-block:: c++

   sequential<aig_network> lfsr = ...;

   auto const result = simulate_sequential<bool>( lfsr, 15, default_simulator<bool>( std::vector<bool>{} ) );

   for ( auto const& outputs : result.outputs )
   {
     std::cout << outputs[0];
   }

The state trace answers what the registers were doing while that happened, and where they ended up:

.. code-block:: c++

   for ( auto const& state : result.states )
   {
     std::cout << fmt::format( "{}\n", fmt::join( state, "" ) );
   }

   assert( result.final_state() == result.reset_state() );  // a full period

Primary inputs that change from one cycle to the next are supplied by ``stimulus_simulator``, which holds one assignment vector per cycle and repeats its last one for the rest of the run:

.. code-block:: c++

   sequential<aig_network> shift_register = ...;

   /* a single 1 on the input, then silence */
   stimulus_simulator sim( { { true }, { false } } );

   auto const result = simulate_sequential<bool>( shift_register, 6, sim );

Any simulator that works with ``simulate`` works here too, holding its assignment for the whole run.  Truth tables, for instance, give the outputs of each cycle as a function of the primary inputs:

.. code-block:: c++

   auto const result = simulate_sequential<kitty::dynamic_truth_table>(
       ntk, 3, default_simulator<kitty::dynamic_truth_table>( ntk.num_pis() ) );

**Result**

.. doxygenstruct:: mockturtle::simulate_sequential_result
   :members:

**Parameters**

.. doxygenstruct:: mockturtle::simulate_sequential_params
   :members:

**Simulators**

.. doxygenclass:: mockturtle::stimulus_simulator

**Algorithm**

.. doxygenfunction:: mockturtle::simulate_sequential
