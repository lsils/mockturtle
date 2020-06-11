Functional equivalence of circuit nodes
---------------------------------------

**Header:** ``mockturtle/algorithms/circuit_validator.hpp``

This class can be used to validate potential circuit optimization choices. It checks the functional equivalence of a circuit node with an existing or non-existing signal with SAT, with optional consideration of observability don't-care (ODC).

If more advanced SAT validation is needed, one could consider using ``cnf_view`` instead, which also constructs the CNF clauses of circuit nodes.

**Example**

The following code shows how to check functional equivalence of a root node to signals existing in the network, or created with nodes within the network. If not, get the counter example.

.. code-block:: c++

   /* derive some AIG (can be AIG, XAG, MIG, or XMG) */
   aig_network aig;
   auto const a = aig.create_pi();
   auto const b = aig.create_pi();
   auto const f1 = aig.create_and( !a, b );
   auto const f2 = aig.create_and( a, !b );
   auto const f3 = aig.create_or( f1, f2 );

   circuit_validator v( aig );

   auto result = v.validate( f1, f2 );
   /* result is an optional, which is nullopt if SAT conflict limit was exceeded */
   if ( result )
   {
      if ( *result )
      {
         std::cout << "f1 and f2 are functionally equivalent\n";
      }
      else
      {
         std::cout << "f1 and f2 have different values under PI assignment: ";
         std::cout << v.cex[0] << v.cex[1] << "\n";
      }
   }

   circuit_validator<aig_network>::gate::fanin gi1; gi1.idx = 0; gi1.inv = true;
   circuit_validator<aig_network>::gate::fanin gi2; gi2.idx = 1; gi2.inv = true;
   circuit_validator<aig_network>::gate g; g.fanins = {gi1, gi2};
   g.type = circuit_validator<aig_network>::gate_type::AND;

   circuit_validator<aig_network>::gate::fanin hi1; hi1.idx = 2; hi1.inv = false;
   circuit_validator<aig_network>::gate::fanin hi2; hi2.idx = 0; hi2.inv = false;
   circuit_validator<aig_network>::gate h; h.fanins = {hi1, hi2};
   h.type = circuit_validator<aig_network>::gate_type::AND;

   result = v.validate( f3, {aig.get_node( f1 ), aig.get_node( f2 )}, {g, h}, true );
   if ( result && *result )
   {
     std::cout << "f3 is equivalent to NOT((NOT f1 AND NOT f2) AND f1)\n";
   }

**Parameters**

.. doxygenstruct:: mockturtle::validator_params
   :members:

**Validate with existing signals**

.. doxygenfunction:: mockturtle::circuit_validator::validate( signal const&, signal const& )
.. doxygenfunction:: mockturtle::circuit_validator::validate( node const&, signal const& )
.. doxygenfunction:: mockturtle::circuit_validator::validate( signal const&, bool )
.. doxygenfunction:: mockturtle::circuit_validator::validate( node const&, bool )

**Validate with non-existing circuit**

.. doxygenfunction:: mockturtle::circuit_validator::validate( signal const&, std::vector<node> const&, std::vector<gate> const&, bool )
.. doxygenfunction:: mockturtle::circuit_validator::validate( node const&, std::vector<node> const&, std::vector<gate> const&, bool )
.. doxygenfunction:: mockturtle::circuit_validator::validate( signal const&, iterator_type, iterator_type, std::vector<gate> const&, bool )
.. doxygenfunction:: mockturtle::circuit_validator::validate( node const&, iterator_type, iterator_type, std::vector<gate> const&, bool )

.. doxygenstruct:: mockturtle::circuit_validator::gate
   :members: fanins, type
.. doxygenstruct:: mockturtle::circuit_validator::gate::fanin
   :members: idx, inv

**Updating**

.. doxygenfunction:: mockturtle::circuit_validator::update
