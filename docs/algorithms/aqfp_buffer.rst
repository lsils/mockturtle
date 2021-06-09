AQFP buffer optimization
------------------------

**Header:** ``mockturtle/algorithms/aqfp/aqfp_buffer.hpp``

Technology assumptions
~~~~~~~~~~~~~~~~~~~~~~

.. doxygenstruct:: mockturtle::aqfp_buffer_params
   :members:

Buffer counting and optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenclass:: mockturtle::aqfp_buffer
   :members:

Buffered network data structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/networks/buffered.hpp``

Two buffered network types are implemented: `buffered_aig_network` and `buffered_mig_network`.
They inherit from `aig_network` and `mig_network`, respectively, and are supplemented with representations for buffers.


Verification of buffered networks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/algorithms/aqfp/aqfp_buffer.hpp``

.. doxygenfunction:: mockturtle::verify_aqfp_buffer

Simulation of buffered networks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/algorithms/simulation.hpp``

.. doxygenfunction:: mockturtle::simulate_buffered
