blif reader
--------------

**Header:** ``mockturtle/io/blif_reader.hpp``
This header file defined the `blif_reader` to be parsed to the `read_blif` function.
There are two possibilities depending on the way the blif information is to be stored:
 - truth table based : the covers are converted into truth tables and a network is generated from them;
 - cover based       : the covers are considered directly for the network generation.  
Two network types admitting the two representations are `klut` and `cover`, respectively. The way a blif file can be read
in these network format is described in the following code

.. code-block:: c++

    /* read a blif file into a k-LUT network */
    klut_network klut;
    lorina::read_blif( "file.blif", blif_reader( klut ) );
      
    /* read a blif file into a cover network */  
    cover_network cover;
    lorina::read_blif( "file.blif", blif_reader( cover ) );