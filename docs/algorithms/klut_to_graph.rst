From k-LUT to graph
----------------

**Header:** ``mockturtle/algorithms/node_resynthesis/klut_to_graph.hpp``
This header file defines a wrapper function for resynthesizing a k-LUT network (type `NtkSrc`) into a
new graph (of type `NtkDest`). The new data structure can be of type AIG, XAG, MIG or XMG.
First the function attempts a Disjoint Support Decomposition (DSD), branching the network into subnetworks. 
As soon as DSD can no longer be done, there are two possibilities depending on the dimensionality of the subnetwork to be resynthesized.
On the one hand, if the size of the associated support is lower or equal than 4, the solution can be recovered by exploiting the mapping of the subnetwork to its NPN-class. 
On the other hand, if the support size is higher than 4, A Shannon decomposition is performed, branching the network in further subnetworks with reduced support.
Finally, once the threshold value of 4 is reached, the NPN mapping completes the graph definition.

The following example shows how to resynthesize a `k`-LUT network into an AIG, a XAG, a MIG and a XMG.

.. code-block:: c++

   /* derive some k-LUT */
   const klut_network klut = ...;

   /* define the destination networks */
   aig_network aig;
   xag_network xag;
   mig_network mig;
   xmg_network xmg;

   /* node resynthesis */
   aig = convert_klut_to_graph<aig_network>( klut );
   xag = convert_klut_to_graph<xag_network>( klut );
   mig = convert_klut_to_graph<mig_network>( klut );
   xmg = convert_klut_to_graph<xmg_network>( klut );

Parameters and statistics
~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenstruct:: mockturtle::node_resynthesis_params
   :members:

.. doxygenstruct:: mockturtle::node_resynthesis_stats
   :members:

Algorithm
~~~~~~~~~

.. doxygenfunction:: mockturtle::node_resynthesis(NtkSource const&, ResynthesisFn&&, node_resynthesis_params const&, node_resynthesis_stats*)

.. doxygenfunction:: mockturtle::node_resynthesis(NtkDest&, NtkSource const&, ResynthesisFn&&, node_resynthesis_params const&, node_resynthesis_stats*)

