Network simulation
------------------

**Examples**

Simulate a Boolean input assignment.

.. code-block:: c++

   aig_network aig = ...;

   std::vector<bool> assignment( aig.num_pis() );
   assignment[0] = true;
   assignment[1] = false;
   assignment[2] = false;
   ...

   default_simulator<bool> sim( assignment );
   const auto values = simulate<bool>( aig, sim );

   ntk.foreach_po( [&]( auto const&, auto i ) {
     std::cout << fmt::format( "output {} has simulation value {}\n", i, values[i] );
   } );

Complete simulation with truth tables.

.. code-block:: c++

   aig_network aig = ...;

   default_simulator<kitty::dynamic_truth_table> sim( aig.num_pis() );
   const auto tts = simulate<>( aig, sim );

   ntk.foreach_po( [&]( auto const&, auto i ) {
     std::cout << fmt::format( "truth table of output {} is {}\n", i, kitty::to_hex( tts[i] ) );
   } );

.. doxygenfunction:: mockturtle::simulate
