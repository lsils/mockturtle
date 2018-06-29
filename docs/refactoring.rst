Refactoring
-----------

**Header:** ``mockturtle/algorithms/refactoring.hpp``

The following example shows how to refactor an MIG using the Akers synthesis
method.

.. code-block:: c++

   /* derive some MIG */
   mig_network mig = ...;

   /* node resynthesis */
   akers_resynthesis resyn;
   refactoring( mig, resyn );
   mig = cleanup_dangling( mig );

Parameters
~~~~~~~~~~

.. doxygenstruct:: mockturtle::refactoring_params
   :members:

Algorithm
~~~~~~~~~

.. doxygenfunction:: mockturtle::refactoring

Rewriting functions
~~~~~~~~~~~~~~~~~~~

One can use resynthesis functions that can be passed to `node_resynthesis`, see
:ref:`node_resynthesis_functions`.
