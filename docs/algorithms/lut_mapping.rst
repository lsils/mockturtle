LUT mapping
-----------

**Header:** ``mockturtle/algorithms/lut_mapping.hpp``

LUT mapping with cut size :math:`k` partitions a logic network into mapped
nodes and unmapped nodes, where a mapped node :math:`n` is assigned some cut
:math:`(n, L)` such that the following conditions hold: i) each node that
drives a primary output is mapped, and ii) each leaf in the cut of a mapped
node is either a primary input or also mapped.  This ensures that the mapping
covers the whole logic network.  LUT mapping aims to find a good mapping with
respect to a cost function, e.g., a short critical path in the mapping or a
small number of mapped nodes.  The LUT mapping algorithm can assign a weight
to a cut for alternative cost functions.

The following example shows how to perform a LUT mapping to an And-inverter
graph using the default settings:

.. code-block:: c++

   aig_network aig = ...;
   mapping_view mapped_aig{aig};
   lut_mapping( mapped_aig );

Note that the AIG is wrapped into a `mapping_view` in order to equip the
network structure with the required mapping methods.

The next example is more complex.  It uses an MIG as underlying network
structure, changes the cut size :math:`k` to 8, and also computes the functions
for the cut of each mapped node: 

.. code-block:: c++

   mig_network mig = ...;
   mapped_view<mig_network, true> mapped_mig{mig};

   lut_mapping_params ps;
   ps.cut_enumeration_ps.cut_size = 8;
   lut_mapping<mapped_view<mig_network, true>, true>( mapped_mig );

Parameters and statistics
~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenstruct:: mockturtle::lut_mapping_params
   :members:

.. doxygenstruct:: mockturtle::lut_mapping_stats
   :members:

Algorithm
~~~~~~~~~

.. doxygenfunction:: mockturtle::lut_mapping
