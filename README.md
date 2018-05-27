[![Build Status](https://travis-ci.org/lsils/mockturtle.svg?branch=master)](https://travis-ci.org/lsils/mockturtle)
[![Documentation Status](https://readthedocs.org/projects/mockturtle/badge/?version=latest)](http://mockturtle.readthedocs.io/en/latest/?badge=latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# mockturtle

<img src="https://cdn.rawgit.com/lsils/mockturtle/master/mockturtle.svg" width="64" height="64" align="left" style="margin-right: 12pt" />
mockturtle is a C++-17 logic network library.  It provides several logic
network implementations (such as And-inverter graphs, Majority-inverter graphs,
and *k* LUT networks), and generic algorithms for logic synthesis and logic
optimization.

[Read the full documentation.](http://mockturtle.readthedocs.io/en/latest/?badge=latest)

## Example

The following code snippet reads an AIG from an Aiger file, enumerates all cuts
and prints them for each node.

```c++
#include <mockturtle/mockturtle.hpp>
#include <lorina/aiger.hpp>

aig_network aig;
lorina::read_aiger( "file.aig", mockturtle::aiger_reader( aig ) );

const auto cuts = cut_enumeration( aig );
aig.foreach_node( [&]( auto node ) {
  std::cout << cuts.cuts( aig.node_to_index( node ) ) << "\n";
} );
```

## EPFL logic sythesis libraries

mockturtle is part of the [EPFL logic synthesis](https://lsi.epfl.ch/page-138455-en.html) libraries.  The other libraries and several examples on how to use and integrate the libraries can be found in the [logic synthesis tool showcase](https://github.com/lsils/lstools-showcase).

