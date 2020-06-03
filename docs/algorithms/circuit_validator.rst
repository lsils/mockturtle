Functional equivalence of circuit nodes
--------------

**Header:** ``mockturtle/algorithms/circuit_validator.hpp``

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
         std::cout << "f1 and f2 have different values under PI assignment: " << v.cex[0] << v.cex[1] << "\n";
      }
   }

   circuit_validator<aig_network>::gate::fanin fi1;
   fi1.idx = 0; fi1.inv = true;
   circuit_validator<aig_network>::gate::fanin fi2;
   fi2.idx = 1; fi2.inv = true;
   circuit_validator<aig_network>::gate g;
   g.fanins = {fi1, fi2};
   g.type = circuit_validator::gate_type::AND;

   result = v.validate( f3, {aig.get_node( f1 ), aig.get_node( f2 )}, {g}, true );
   if ( result && *result )
   {
     std::cout << "f3 is equivalent to NOT(NOT f1 AND NOT f2)\n";
   }

**Validate with existing signals**

.. doxygenfunction:: mockturtle::circuit_validator::validate( signal const&, signal const& )
.. doxygenfunction:: mockturtle::circuit_validator::validate( node const&, signal const& )
.. doxygenfunction:: mockturtle::circuit_validator::validate( signal const&, bool )
.. doxygenfunction:: mockturtle::circuit_validator::validate( node const&, bool )

**Validate with non-existing circuit**

.. doxygenstruct:: mockturtle::circuit_validator::gate
   :members: fanins, type
.. doxygenstruct:: mockturtle::circuit_validator::gate::fanin
   :members: idx, inv

.. doxygenfunction:: mockturtle::circuit_validator::validate( signal const&, std::vector<node> const&, std::vector<gate> const&, bool )
.. doxygenfunction:: mockturtle::circuit_validator::validate( node const&, std::vector<node> const&, std::vector<gate> const&, bool )
.. doxygenfunction:: mockturtle::circuit_validator::validate( signal const&, iterator_type, iterator_type, std::vector<gate> const&, bool )
.. doxygenfunction:: mockturtle::circuit_validator::validate( node const&, iterator_type, iterator_type, std::vector<gate> const&, bool )

**Updating**
.. doxygenfunction:: mockturtle::circuit_validator::add_node
.. doxygenfunction:: mockturtle::circuit_validator::update
