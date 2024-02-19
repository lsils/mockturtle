Rewrite
-------

**Header:** ``mockturtle/algorithms/rewrite.hpp``

The following example shows how to rewrite an MIG using precomputed optimum
networks.  In this case the maximum number of variables for a node function is
4.

.. code-block:: c++

   /* derive some MIG */
   mig_network mig = ...;

   /* rewrite */
   mig_npn_resynthesis resyn{ true };
   exact_library_params eps;
   eps.np_classification = false;
   exact_library<mig_network> exact_lib( resyn, eps );

It is possible to change the cost function of nodes in rewrite.  Here is
an example, in which the cost function only accounts for AND gates in a network,
which corresponds to the multiplicative complexity of a function.

.. code-block:: c++

   template<class Ntk>
   struct mc_cost
   {
     uint32_t operator()( Ntk const& ntk, node<Ntk> const& n ) const
     {
       return ntk.is_and( n ) ? 1 : 0;
     }
   };

   SomeResynthesisClass resyn;
   exact_library_params eps;
   eps.np_classification = false;
   exact_library<xag_network> exact_lib( resyn, eps );
   rewrite<decltype( Ntk ), decltype( exact_lib ), mc_cost>( ntk, exact_lib );

Rewrite supports also satisfiability don't cares:

.. code-block:: c++
   
   /* derive some MIG */
   mig_network mig = ...;

   /* rewrite */
   mig_npn_resynthesis resyn{ true };
   exact_library_params eps;
   eps.np_classification = false;
   eps.compute_dc_classes = true;
   exact_library<mig_network> exact_lib( resyn, eps );

   /* rewrite */
   rewrite_params ps;
   ps.use_dont_cares = true;
   rewrite( mig, exact_lib, ps );

Parameters and statistics
~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenstruct:: mockturtle::rewrite_params
   :members:

.. doxygenstruct:: mockturtle::rewrite_stats
   :members:

Algorithm
~~~~~~~~~

.. doxygenfunction:: mockturtle::rewrite

Rewriting functions
~~~~~~~~~~~~~~~~~~~

One can use resynthesis functions with a pre-computed database and process
them using :ref:`exact_library`, see :ref:`node_resynthesis_functions`.
