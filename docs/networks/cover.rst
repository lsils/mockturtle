Cover Network
-------------

**Header:** ``mockturtle/networks/cover.hpp``
This header file defines a data structure of type `cover_network`. 
This data structure stores the information contained in a `.blif` file before converting it into another type of graph.
Any node in the network corresponds to a boolean function, and the number of input signals to each node is variable.
Each node stores a function in the `.blif` formalism, i.e., either in terms of its onset or its offset.
In both cases, the information stored inside a node appears as a vector of cubes, where a bit set to (0) 1 indicates that the literal (does not) have 
to be negated while defining the offset or the onset. 

The following example shows how to store a 5-inputs OR in a cover network and to map it into an AIG.

.. code-block:: c++

    std::string file{
      ".model monitorBLIF\n"
      ".inputs a1 a2 a3 a4 a5 \n"
      ".outputs y\n"
      ".names a1 a2 a3 a4 a5 y\n"
      "1---- 1\n"
      "-1--- 1\n"
      "--1-- 1\n"
      "---1- 1\n"
      "----1 1\n"
      ".end\n" };

    std::istringstream in( file );
    auto result = lorina::read_blif( in, blif_reader( cover ) );

    cover_network cover;
    aig_network aig;
    convert_cover_to_graph( aig, cover );

    auto const sim_aig = ( simulate<kitty::static_truth_table<5u>>( aig )[0]._bits ); 
    auto const sim_aig = ( simulate<kitty::static_truth_table<5u>>( cover )[0]._bits );

The last two simulations yield the same result.

This data structure allows for a custom node definition. 
There are three ways of doing it:

* Define the cubes ( clauses ) of the onset ( offset ).
* Use a truth table.
* Use the available nodes creation functions.
The following code shows three ways of creating a majority node. 

.. code-block:: c++

    cover_network cover;

    const auto a = cover.create_pi();
    const auto b = cover.create_pi();
    const auto c = cover.create_pi();

    kitty::cube _X00 = kitty::cube( "-00" );
    kitty::cube _0X0 = kitty::cube( "0-0" );
    kitty::cube _00X = kitty::cube( "00-" );

    std::vector<kitty::cube> maj_offset = { _X00, _0X0, _00X };
    std::pair<std::vector<kitty::cube>, bool> cover_maj = std::make_pair( maj_offset, false );

    const auto f1 = cover.create_cover_node( { a, b, c }, cover_maj );

    kitty::dynamic_truth_table tt_maj( 3u );
    kitty::create_from_hex_string( tt_maj, "e8" );
    const auto f2 = cover.create_cover_node( { a, b, c }, tt_maj );

    const auto f3 = cover.create_maj( a, b, c );
Creating the node by specifying the cover requires the introduction of a vector of cubes and a boolean.
If the boolean is true, the cover is related to the onset. Otherwise, it specifies the offset.

It is worth pointing out that the latches-manipulation is a direct adaptation of the `k-LUT` implementation.
The tests performed on the current version are not sufficient to guarantee its stability.
Hence, the interested user should be aware that, in the current state, this data structure primarily aims at tasks involving the mapping of combinatorial logic from `.blif` format to a network type such as AIG, XAG, MIG, or XMG.

