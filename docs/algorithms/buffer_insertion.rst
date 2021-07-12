AQFP buffer insertion and verification
--------------------------------------

**Header:** ``mockturtle/algorithms/aqfp/buffer_insertion.hpp``

Technology assumptions
~~~~~~~~~~~~~~~~~~~~~~

.. doxygenstruct:: mockturtle::aqfp_assumptions
   :members:

Buffer insertion algorithms
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenclass:: mockturtle::buffer_insertion
   :members:

Parameters
~~~~~~~~~~

.. doxygenstruct:: mockturtle::buffer_insertion_params
   :members:

Buffered network data structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/networks/buffered.hpp``

Two buffered network types are implemented: `buffered_aig_network` and `buffered_mig_network`.
They inherit from `aig_network` and `mig_network`, respectively, and are supplemented with representations for buffers.


Verification of buffered networks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/algorithms/aqfp/buffer_verification.hpp``

.. doxygenfunction:: mockturtle::verify_aqfp_buffer( Ntk const&, aqfp_assumptions const& )

.. doxygenfunction:: mockturtle::schedule_buffered_network( Ntk const& ntk, aqfp_assumptions const& ps )

.. doxygenfunction:: mockturtle::verify_aqfp_buffer( Ntk const&, aqfp_assumptions const&, node_map<uint32_t, Ntk> const& )


Simulation of buffered networks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/algorithms/simulation.hpp``

.. doxygenfunction:: mockturtle::simulate_buffered
