From cover to graph
----------------

**Header:** ``mockturtle/io/cover_to_graph.hpp``
This header file defines a function to convert a network of type `network_cover` into a
new graph, of type `Ntk`. The new data structure can be chosen to be of type AIG, XAG, MIG or XMG.
Any node of the cover network is defined by specifying either the ON set or the OFF set of the node function.
Therefore, this converter function computes the Sum Of Product (SOP) or the Product Of Sum (POS) associated to a node.
The information of the decomposition to be performed is contained in the boolean `is_sop`, which is true when the SOP representation is to be performed.
This is done by exploiting the helper functions in the `cover_to_graph_converter` class:
 - `recursive_or`           : recursively splits the input signals in twice and redursively perform a binary or;
 - `recursive_and`          : recursively splits the input signals in twice and redursively perform a binary and;
 - `convert_cube_to_graph`  : depending on a boolean value, compute the product or the sum of the literals defined by the cube given as input;
 - `convert_cover_to_graph` : depending on a boolean value, compute the SOP or the POS of the covers stored in a node;
 - `run`                    : map primary inputs, primary outputs and the nodes into a proper representation in the destination network.
Note that it is at the level of the `convert_cube_to_graph` that the `.blif` formalism is exploited:
 - ON set  : the cube corresponds to an AND function in between the literals. Each literal should (not) be negated if the bit indicated in the cube is 1 (0);
 - OFF set : the cube corresponds to an OR function in between the literals. Each literal should (not) be negated if the bit indicated in the cube is 0 (1);

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

