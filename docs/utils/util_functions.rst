Utility functions
-----------------

Manipulate windows with network data types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/utils/network_utils.hpp``

.. doxygenfunction:: mockturtle::clone_subnetwork( Ntk const&, std::vector<typename Ntk::node> const&, std::vector<typename Ntk::signal> const&, std::vector<typename Ntk::node> const&, SubNtk& )

.. doxygenfunction:: mockturtle::insert_ntk( Ntk&, BeginIter, EndIter, SubNtk const&, Fn&& )