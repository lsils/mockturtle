Akers Synthesis 
---------------

The algorithm implements the method for 3-input majority-based logic synthesis proposed in 
“*Synthesis of combinational logic using three-input majority 
gates*” by S. B. Akers (1962). 

This method is implemented in the function :cpp:func:`mockturtle::akers_synthesis`. 
The inputs are two truth tables: (i) :math:`func`, which specifies the function to be synthesized,
and (ii) :math:`care`, where don't cares can be considered. [more to be said here?]
Both truth tables are given as :cpp:func:`kitty::dynamic_truth_tables`. 

The easiest way to run Akers' synthesis is by doing: 

.. code-block:: c++

   auto mig = akers_synthesis( mig, func, care);

Here, the function returns an mig, with as many inputs as the number of 
variables of func and one primary output. 

The synthesis method can also be run using already existing signals from an mig as inputs. 
As an example, consider an mig with 4 primary inputs (a, b, c, d) 
and two gates: 

.. code-block:: c++
   std::vector<mig_network::signal> gates;
   gates.push_back( mig.create_and( a, b ) );
   gates.push_back( mig.create_and( c, d ) );

   auto t = akers_synthesis( mig, func, care, gates.begin(), gates.end() );

The algorithm performs the synthesis of the function considering as inputs the two AND gates. 
Here, the output :cpp:func:`t`is a :cpp:func:`signal<mig_network>`. 

