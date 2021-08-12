Utility data structures
-----------------------

Truth table cache
~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/utils/truth_table_cache.hpp``

.. doc_overview_table:: classmockturtle_1_1truth__table__cache
   :column: Method

   truth_table_cache
   insert
   operator[]
   size

.. doxygenclass:: mockturtle::truth_table_cache
   :members:

Node map
~~~~~~~~

**Header:** ``mockturtle/utils/node_map.hpp``

.. doc_overview_table:: classmockturtle_1_1node__map
   :column: Method

   node_map
   operator[]
   reset

.. doxygenclass:: mockturtle::node_map
   :members:

.. doxygenfunction:: mockturtle::initialize_copy_network

Tech Library
~~~~~~~~~~~~

**Header:** ``mockturtle/utils/tech_library.hpp``

.. doc_overview_table:: classmockturtle_1_1tech__library
   :column: Method

   get_supergates
   get_inverter_info
   max_gate_size
   get_gates

.. doxygenclass:: mockturtle::tech_library
   :members:

Exact Library
~~~~~~~~~~~~~

**Header:** ``mockturtle/utils/tech_library.hpp``

.. doc_overview_table:: classmockturtle_1_1exact__library
   :column: Method

   get_supergates
   get_database
   get_inverter_info

.. doxygenclass:: mockturtle::exact_library
   :members:

Super Utils
~~~~~~~~~~~

**Header:** ``mockturtle/utils/super_utils.hpp``

.. doc_overview_table:: classmockturtle_1_1super__utils
   :column: Method

   get_super_library
   get_standard_library_size

.. doxygenclass:: mockturtle::super_utils
   :members:

Cuts
~~~~

**Header:** ``mockturtle/utils/cuts.hpp``

.. doc_overview_table:: classmockturtle_1_1cut
   :column: Method

   operator=
   set_leaves
   signature
   size
   begin
   end
   operator->
   data
   subsumes
   merge

.. doxygenclass:: mockturtle::cut
   :members:

Cut sets
~~~~~~~~

**Header:** ``mockturtle/utils/cuts.hpp``

.. doc_overview_table:: classmockturtle_1_1cut__set
   :column: Method

   cut_set
   clear
   add_cut
   is_subsumed
   insert
   begin
   end
   size
   operator[]
   best
   update_best
   limit
   operator<<

.. doxygenclass:: mockturtle::cut_set
   :members:

.. _index_list:

Index List
~~~~~~~~~~

**Header:** ``mockturtle/utils/index_list.hpp``

.. doxygenstruct:: mockturtle::abc_index_list
.. doxygenfunction:: mockturtle::encode( abc_index_list&, Ntk const& )
.. doxygenfunction:: mockturtle::insert( Ntk&, BeginIter, EndIter, abc_index_list const&, Fn&& )
.. doxygenfunction:: mockturtle::to_index_list_string( abc_index_list const& )

.. doxygenstruct:: mockturtle::mig_index_list
.. doxygenfunction:: mockturtle::encode( mig_index_list&, Ntk const& )
.. doxygenfunction:: mockturtle::insert( Ntk&, BeginIter, EndIter, mig_index_list const&, Fn&& )
.. doxygenfunction:: mockturtle::to_index_list_string( mig_index_list const& )

.. doxygenstruct:: mockturtle::xag_index_list
.. doxygenfunction:: mockturtle::encode( xag_index_list<separate_header>&, Ntk const& )
.. doxygenfunction:: mockturtle::insert( Ntk&, BeginIter, EndIter, xag_index_list<separate_header> const&, Fn&& )
.. doxygenfunction:: mockturtle::to_index_list_string( xag_index_list<true> const& )

.. doxygenfunction:: mockturtle::decode( Ntk&, IndexList const& )
.. doxygenclass:: mockturtle::aig_index_list_enumerator

Stopwatch
~~~~~~~~~

**Header:** ``mockturtle/utils/stopwatch.hpp``

.. doc_overview_table:: classmockturtle_1_1stopwatch
   :column: Method

   stopwatch
   ~stopwatch

.. doxygenclass:: mockturtle::stopwatch
   :members:

.. doxygenfunction:: mockturtle::call_with_stopwatch

.. doxygenfunction:: mockturtle::make_with_stopwatch

.. doxygenfunction:: mockturtle::to_seconds

Progress bar
~~~~~~~~~~~~

**Header:** ``mockturtle/utils/progress_bar.hpp``

.. doc_overview_table:: classmockturtle_1_1progress__bar
   :column: Method

   progress_bar
   ~progress_bar
   operator()
   done

.. doxygenclass:: mockturtle::progress_bar
   :members:
