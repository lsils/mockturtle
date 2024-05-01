Extended technology mapping
---------------------------

**Header:** ``mockturtle/algorithms/emap.hpp``

The command `emap` stands for extended mapper. It supports large
library cells, of more than 6 inputs, and can perform matching using 3
different methods: Boolean, pattern, or hybrid. The current version
can map to 2-output gates, such as full adders and half adders,
and provides a 2x speedup in mapping time compared to command `map`
for similar or better quality. Similarly, to `map`, the implementation
is independent of the underlying graph representation.
Additionally, `emap` supports "don't touch" white boxes (gates).

Command `emap` can return the mapped network in two formats.
Command `emap` returns a `cell_view<block_network>` that supports
multi-output cells. Command `emap_klut` returns a `binding_view<klut_network>`
similarly as command `map`.

The following example shows how to perform delay-oriented technology mapping
from an and-inverter graph using large cells up to 9 inputs:

.. code-block:: c++

   aig_network aig = ...;

   /* read cell library in genlib format */
   std::vector<gate> gates;
   std::ifstream in( ... );
   lorina::read_genlib( in, genlib_reader( gates ) )
   tech_library<9> tech_lib( gates );

   /* perform technology mapping */
   cell_view<block_network> res = emap<9>( aig, tech_lib );

The next example performs area-oriented graph mapping using multi-output cells:

.. code-block:: c++

   aig_network aig = ...;

   /* read cell library in genlib format */
   std::vector<gate> gates;
   std::ifstream in( ... );
   lorina::read_genlib( in, genlib_reader( gates ) )
   tech_library tech_lib( gates );

   /* perform technology mapping */
   emap_params ps;
   ps.area_oriented_mapping = true;
   ps.map_multioutput = true;
   cell_view<block_network> res = emap( aig, tech_lib, ps );

In this case, `emap` is used to return a `block_network`, which can respresent multi-output
cells as single nodes. Alternatively, also `emap_klut` can be used but multi-output cells
would be reporesented by single-output nodes.

The maximum number of cuts stored for each node is limited to 20.
To increase this limit, change `max_cut_num` in `emap`.

You can set the inputs arrival time and output required times using the parameters `arrival_times`
and `required times`. Moreover, it is possible to ask for a required time relaxation. For instance,
if we want to map a network with an increase of 10% over its minimal delay, we can set
`relax_required` to 10.

For further details and usage scenarios of `emap`, such as white boxes, please check the
related tests.

**Parameters and statistics**

.. doxygenstruct:: mockturtle::emap_params
   :members:

.. doxygenstruct:: mockturtle::emap_stats
   :members:

**Algorithm**

.. doxygenfunction:: mockturtle::emap(Ntk const&, tech_library<NInputs, Configuration> const&, emap_params const&, emap_stats*)
.. doxygenfunction:: mockturtle::emap_klut(Ntk const&, tech_library<NInputs, Configuration> const&, emap_params const&, emap_stats*)
.. doxygenfunction:: mockturtle::emap_node_map(Ntk const&, tech_library<NInputs, Configuration> const&, emap_params const&, emap_stats*)
.. doxygenfunction:: mockturtle::emap_load_mapping(Ntk&)


Technology mapping and network conversion
-----------------------------------------

**Header:** ``mockturtle/algorithms/mapper.hpp``

A versatile mapper that supports technology mapping and graph mapping
(optimized network conversion). The mapper is independent of the
underlying graph representation. Hence, it supports generic subject
graph representations (e.g., AIG, and MIG) and a generic target
representation (e.g. cell library, XAG, XMG). The mapper aims at finding a
good mapping with respect to delay, area, and switching power.

The mapper uses a library (hash table) to facilitate Boolean matching.
For technology mapping, it needs `tech_library` while for graph mapping
it needs `exact_library`. For technology mapping, the generation of both NP- and
P-configurations of gates are supported. Generally, it is convenient to use
NP-configurations for small or medium size cell libraries. For bigger libraries,
P-configurations should perform better. You can test both the configurations to
see which one has the best run time. For graph mapping, NPN classification
is used instead.

The following example shows how to perform delay-oriented technology mapping
from an and-inverter graph using the default settings:

.. code-block:: c++

   aig_network aig = ...;

   /* read cell library in genlib format */
   std::vector<gate> gates;
   std::ifstream in( ... );
   lorina::read_genlib( in, genlib_reader( gates ) )
   tech_library tech_lib( gates );

   /* perform technology mapping */
   binding_view<klut_network> res = map( aig, tech_lib );

The mapped network is returned as a `binding_view` that extends a k-LUT network.
Each k-LUT abstracts a cell and the view contains the binding information.

The next example performs area-oriented graph mapping from AIG to MIG
using a NPN resynthesis database of structures:

.. code-block:: c++

   aig_network aig = ...;
   
   /* load the npn database in the library */
   mig_npn_resynthesis resyn{ true };
   exact_library<mig_network> exact_lib( resyn );

   /* perform graph mapping */
   map_params ps;
   ps.skip_delay_round = true;
   ps.required_time = std::numeric_limits<double>::max();
   mig_network res = map( aig, exact_lib, ps );

For graph mapping, we suggest reading the network directly in the
target graph representation if possible (e.g. read an AIG as a MIG)
since the mapping often leads to better results in this setting.

For technology mapping of sequential networks, a dedicated command
`seq_map` should be called. Only in case of graph mapping, the
command `map` can be used on sequential networks.

The following example shows how to perform delay-oriented technology
mapping from a sequential and-inverter graph:

.. code-block:: c++

   sequential<aig_network> aig = ...;

   /* read cell library in genlib format */
   std::vector<gate> gates;
   std::ifstream in( ... );
   lorina::read_genlib( in, genlib_reader( gates ) )
   tech_library tech_lib( gates );

   /* perform technology mapping */
   using res_t = binding_view<sequential<klut_network>>;
   res_t res = seq_map( aig, tech_lib );

The next example performs area-oriented graph mapping from a 
sequential AIG to a sequential MIG using a NPN resynthesis
database of structures:

.. code-block:: c++

   sequential<aig_network> aig = ...;
   
   /* load the npn database in the library */
   mig_npn_resynthesis resyn{ true };
   exact_library<sequential<mig_network>> exact_lib( resyn );

   /* perform graph mapping */
   map_params ps;
   ps.skip_delay_round = true;
   ps.required_time = std::numeric_limits<double>::max();
   sequential<mig_network> res = map( aig, exact_lib, ps );

The newest version of `map` for graph mapping or rewriting can
leverage satisfiability don't cares:

.. code-block:: c++

   aig_network aig = ...;
   
   /* load the npn database in the library and compute don't care classes */
   mig_npn_resynthesis resyn{ true };
   exact_library_params lps;
   lps.compute_dc_classes = true;
   exact_library<mig_network> exact_lib( resyn, lps );

   /* perform area-oriented rewriting */
   map_params ps;
   ps.skip_delay_round = true;
   ps.required_time = std::numeric_limits<double>::max();
   ps.use_dont_cares = true;
   mig_network res = map( aig, exact_lib, ps );

As a default setting, cut enumeration minimizes the truth tables.
This helps improving the results but slows down the computation.
We suggest to keep it always true. Anyhow, for a faster mapping,
set the truth table minimization parameter to false.
The maximum number of cuts stored for each node is limited to 49.
To increase this limit, change `max_cut_num` in `fast_network_cuts`.

**Parameters and statistics**

.. doxygenstruct:: mockturtle::map_params
   :members:

.. doxygenstruct:: mockturtle::map_stats
   :members:

**Algorithm**

.. doxygenfunction:: mockturtle::map(Ntk const&, tech_library<NInputs, Configuration> const&, map_params const&, map_stats*)
.. doxygenfunction:: mockturtle::map(Ntk&, exact_library<NtkDest, NInputs> const&, map_params const&, map_stats*)