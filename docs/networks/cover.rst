cover network
-------------

**Header:** ``mockturtle/networks/cover.hpp``
This header file defines a data structure of type `cover_network`. 
This data structure can be used to store the information contained in a `.blif` file, before to convert it into another type of graph.
Any node in the network corresponds to a boolean function and the number of input signals to each node is variable.
Each node function is expressed in the `.blif` formalism, which allows to represent any function either in terms of its ON set or in terms of its OFF set.
In both the cases, the information stored inside a node appears as a vector of cubes, where a bit set to (0) 1 indicates that the literal (doesn't) have 
to be negated while defining the OFF set or the ON set.  

The following example shows how to resynthesize a `k`-LUT network into an AIG, a XAG, a MIG and a XMG.

.. code-block:: c++

   /* derive some cover network */
    const cover_network cover = ...;

   /* define the destination networks */
    aig_network aig;
    xag_network xag;
    mig_network mig;
    xmg_network xmg;

    /* inline conversion of the cover network into the desired data structure */
    convert_covers_to_graph( cover_ntk, aig );
    convert_covers_to_graph( cover_ntk, xag ); 

    /* out of line conversion of the cover network into the desired data structure */
    mig = convert_covers_to_graph<mig_network>( cover_ntk );
    xmg = convert_covers_to_graph<xmg_network>( cover_ntk );

It is worth pointing out that the latches-manipulation has been implemented by adapting the `k-LUT` implementation.
The tests performed on the current version are not sufficient to guarantee its stability and the interested user should 
be aware of the fact that this data structure is stable only for tasks involving the mapping of cominatorial logic from `.blif` format to 
a network type such as AIG, XAG, MIG or XMG.
Furthermore, the tests have been performed by verifying the proper functioning of the mapped network with respect to a graph implementing the same
boolean function. A direct symulation method of the `cover_network` type is missing in the current implementation.
