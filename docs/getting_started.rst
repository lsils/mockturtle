Getting started
===============

mockturtle is a header-only C++-17 library. It can be compiled as a stand-alone
tool for direct usage, or be integrated in another project as a library.

Compilation requirements
------------------------

Support of C++ 17 standard is required to compile mockturtle.
We constantly test building mockturtle on Linux using Clang 11, GCC 7, GCC 9
and GCC 10, and on Mac OS using Clang 12, GCC 9 and GCC 10.  It also 
compiles on Windows using the C++ compiler in Visual Studio 2019.

Using mockturtle as a stand-alone tool
--------------------------------------

mockturtle can be compiled and used as a stand-alone logic synthesis tool.
Compilation configuration can be easily done with CMake. For example, to 
configure, compile and run the example code in our GitHub page::

  mkdir build
  cd build
  cmake ..
  make cut_enumeration
  ./examples/cut_enumeration

Please note that CMake version >= 3.8 is required. For a more interactive
interface, you could also use ``ccmake``.

If you experience that the system compiler does not suffice the requirements,
you can manually pass a compiler to CMake using::

  cmake -DCMAKE_CXX_COMPILER=/path/to/c++-compiler ..

To write your own code to run mockturtle algorithms, simply create a new cpp
file in the examples directory, and then re-configure with CMake::

  cd build
  cmake .

Then, a make target of your file name should become available.

For most of the algorithms, there is a corresponding experiment code in the 
experiments directory, which demonstrates how the algorithm can be called.
To compile the experiments, you need to turn on the ``MOCKTURTLE_EXPERIMENTS``
option in CMake::

  cmake -DMOCKTURTLE_EXPERIMENTS=ON ..

Note that many of the experiments check circuit equivalence with a system-command
call to ABC_. If you see ``abc: command not found``, you could either install ABC_
and add it to the PATH variable, or ignore this error as it does not affect the
operation of the algorithms.

.. _ABC: https://github.com/berkeley-abc/abc

Using mockturtle as a library in another project
------------------------------------------------

Being header-only, mockturtle can be easily integrated into existing and new projects.
Just add the include directory of mockturtle to your include directories, and simply
include mockturtle by

.. code-block:: c++

   #include <mockturtle/mockturtle.hpp>

Some external projects using mockturtle can be found on our showcase_ page.

.. _showcase: https://github.com/lsils/lstools-showcase#external-projects-using-the-epfl-logic-synthesis-libraries

Building tests
--------------

In order to run the tests, you need to init the submodules and enable tests
in CMake::

  mkdir build
  cd build
  cmake -DMOCKTURTLE_TEST=ON ..
  make run_tests
  ./test/run_tests

Debugging toolset
-----------------

When encountering bugs --- either a segmentation fault, or some algorithm does not
operate as expected --- some of the tools described in this section might be helpful
in figuring out the source of error.

Testcase minimizer
~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/algorithms/testcase_minimizer.hpp``

Often, the testcase that triggers the bug is big. The testcase minimizer helps reducing
the size of the testcase as much as possible, while keeping the buggy behavior triggered.
A minimized testcase not only facilitates debugging, but also enhances communication
between developers if the original testcase cannot be disclosed.

.. doxygenstruct:: mockturtle::testcase_minimizer_params
   :members:

.. doxygenclass:: mockturtle::testcase_minimizer

Fuzz testing
~~~~~~~~~~~~

**Header:** ``mockturtle/algorithms/network_fuzz_tester.hpp``

If minimizing the testcase is not successful, is too slow, or there is not an initial
testcase to start with, fuzz testing can help generating small testcases that triggers
unwanted behaviors.

.. doxygenstruct:: mockturtle::fuzz_tester_params
   :members:

.. doxygenclass:: mockturtle::network_fuzz_tester

Debugging utilities
~~~~~~~~~~~~~~~~~~~

**Header:** ``mockturtle/utils/debugging_utils.hpp``

Some utility functions are provided in this header file. They can be added as
assertions in algorithms to identify abnormal network operations, or be used
as the checks in testcase minimizer or fuzz testing.

Visualization
~~~~~~~~~~~~~

When the testcase is small enough, it becomes possible to visualize the network.
mockturtle supports writing out a network into the DOT format, which can then be
visualized using Graphviz. (:ref:`write_dot`)

Time machine
~~~~~~~~~~~~

Coming soon...


