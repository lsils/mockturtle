.. _cost_aware_optimization:

Optimization customized cost
-------------------------

**Header:** ``mockturtle/algorithms/experimental/cost_aware_optimization.hpp``

This header file defines a function to optimize a network with a customized cost function. 

.. code-block:: c++

   /* derive some MIG */
   xag_network xag = ...;

   cost_aware_params ps;
   cost_aware_stats st;

   cost_aware_optimization( xag, and_cost<xag_network>(), ps, &st );
   xag = cleanup_dangling( xag );


Algorithm
~~~~~~~~~

.. doxygenfunction:: mockturtle::experimental::cost_aware_optimization
