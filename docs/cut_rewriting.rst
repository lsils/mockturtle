Cut rewriting
-------------

**Header:** ``mockturtle/algorithms/cut_rewriting.hpp``

The following example shows how to rewrite an MIG using precomputed optimum
networks.  In this case the maximum number of variables for a node function is
4.

.. code-block:: c++

   /* derive some MIG */
   mig_network mig = ...;

   /* node resynthesis */
   mig_npn_resynthesis resyn;
   cut_rewriting_params ps;
   ps.cut_enumeration_ps.cut_size = 4;
   cut_rewriting( mig, resyn, ps );
   mig = cleanup_dangling( mig );

Parameters
~~~~~~~~~~

.. doxygenstruct:: mockturtle::cut_rewriting_params
   :members:

Algorithm
~~~~~~~~~

.. doxygenfunction:: mockturtle::cut_rewriting

Rewriting functions
~~~~~~~~~~~~~~~~~~~

One can use resynthesis functions that can be passed to `node_resynthesis`, see
:ref:`node_resynthesis_functions`.