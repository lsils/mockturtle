/* mockturtle: C++ logic network library
 * Copyright (C) 2018  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file traits.hpp
  \brief Type traits and checkers for the network interface

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>

namespace mockturtle
{

static_assert( false, "file interface.hpp cannot be included, it's only used for documentation" );

class network final
{
public:
  /*! \brief Type representing a node.
   *
   * A ``node`` is a node in the logic network.  It could be a constant, a
   * primary input or a logic gate.
   */
  struct node
  {
  };

  /*! \brief Type representing a signal.
   *
   * A ``signal`` can be seen as a pointer to a node, or an outgoing edge of
   * a node towards its parants.  Depending on the kind of logic network, it
   * may carry additional information such as a complement attribute.
   */
  struct signal
  {
  };

  /*! \brief Type representing the storage.
   *
   * A ``storage`` is some container that can contain all data necessary to
   * store the logic network.  It can constructed outside of the logic network
   * and passed as a reference to the constructor.  It may be shared among
   * several logic networks.  A `std::shared_ptr<T>` is a convenient data
   * structure to hold a storage in a logic network.
   */
  struct storage
  {
  };

  network();

  explicit network( storage s );

#pragma region Primary I / O and constants
  /*! \brief Gets constant value represented by network.
   *
   * A constant node is the only node that must be created when initializing
   * the network.  For this reason, this method has constant access and is not
   * called `create_constant`.
   *
   * \param value Constant value
   */
  signal get_constant( bool value ) const;

  /*! \brief Creates a primary input in the network.
   *
   * Each created primary input is stored in a node and contributes to the size
   * of the network.
   *
   * \param name Optional name for the input
   */
  signal create_pi( std::string const& name = {} );

  /*! \brief Creates a primary output in the network.
   *
   * A primary output is not stored in terms of a node, and it also does not
   * contribute to the size of the network.  A primary output is created for a
   * signal in the network and it is possible that multiple primary outputs
   * point to the same signal.
   *
   * \param s Signal that drives the created primary output
   * \param name Optional name for the output
   */
  void create_po( signal const& s, std::string const& name = {} );

  /*! \brief Checks whether a node is a constant node. */
  bool is_constant( node const& n ) const;

  /*! \brief Checks whether a node is a primary input. */
  bool is_pi( node const& n ) const;
#pragma endregion

#pragma region Create unary functions
  /*! \brief Creates signal that computes ``f``.
   *
   * This method is not required to create a gate in the network.  A network
   * implementation can also just return ``f``.
   *
   * \param f Child signal
   */
  signal create_buf( signal const& f );

  /*! \brief Creates a signal that inverts ``f``.
   *
   * This method is not required to create a gate in the network.  If a network
   * supports complemented attributes on signals, it can just return the
   * complemented signal ``f``.
   *
   * \param f Child signal
   */
  signal create_not( signal const& f );
#pragma endregion

#pragma region Create binary functions
  /*! \brief Creates a signal that computes the binary AND. */
  signal create_and( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary NAND. */
  signal create_nand( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary OR. */
  signal create_or( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary NOR. */
  signal create_nor( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary less-than.
   *
   * The signal is true if and only if ``f`` is 0 and ``g`` is 1.
   */
  signal create_lt( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary less-than-or-equal.
   *
   * The signal is true if and only if ``f`` is 0 or ``g`` is 1.
   */
  signal create_le( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary greater-than.
   *
   * The signal is true if and only if ``f`` is 1 and ``g`` is 0.
   */
  signal create_gt( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary greater-than-or-equal.
   *
   * The signal is true if and only if ``f`` is 1 or ``g`` is 0.
   */
  signal create_ge( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary XOR. */
  signal create_xor( signal const& f, signal const& g );

  /*! \brief Creates a signal that computes the binary XNOR. */
  signal create_xnor( signal const& f, signal const& g );
#pragma endregion

#pragma region Create ternary functions
  /*! \brief Creates a signal that computes the majority-of-3. */
  signal create_maj( signal const& f, signal const& g, signal const& h );

  /*! \brief Creates a signal that computes the if-then-else operation.
   *
   * \param cond Condition for ITE operator
   * \param f_then Then-case for ITE operator
   * \param f_else Else-case for ITE operator
   */
  signal create_ite( signal const& cond, signal const& f_then, signal const& f_else );
#pragma endregion

#pragma region Create arbitrary functions
  /*! \brief Creates node with arbitrary function.
   *
   * The number of variables in ``function`` must match the number of fanin
   * signals in ``fanin``.  ``fanin[0]`` will correspond to the
   * least-significant variable in ``function``.
   *
   * \param fanin Fan-in signals
   * \param function Truth table for node function
   */
  signal create_node( std::vector<signal> const& fanin, kitty::dynamic_truth_table const& function );

  /*! \brief Clones a node from another network of same type.
   *
   * This method can clone a node from a different network ``other``, which is
   * from the same type.  The node ``source`` is a node in the source network
   * ``other``, but the signals in ``fanin`` refer to signals in the target
   * network, which are assumed to be in the same order as in the source
   * network.
   *
   * \param other Other network of same type
   * \param source Node in ``other``
   * \param children Fan-in signals from the current network
   * \return New signal representing node in current network
   */
  signal clone_node( network const& other, node const& source, std::vector<signal> const& fanin );
#pragma endregion

#pragma region Restructuring
  /*! \brief Replaces one node in a network by another one.
   *
   * This method causes all nodes that have ``old_node`` as fanin to have
   * `new_node` as fanin instead.  Afterwards, the fan-out count of
   * ``old_node`` is guaranteed to be 0.
   *
   * It does not update custom values or visited flags of a node.
   *
   * \brief old_node Node to replace
   * \brief new_node Node to replace ``old_node`` with
   */
  void substitute_node( node const& old_node, node const& new_node );
#pragma endregion

#pragma region Structural properties
  /*! \brief Returns the number of nodes (incl. constants and PIs). */
  uint32_t size() const;

  /*! \brief Returns the number of PIs. */
  uint32_t num_pis() const;

  /*! \brief Returns the number of POs. */
  uint32_t num_pos() const;

  /*! \brief Returns the number of gates. 
   *
   * The return value is equal to the size of the network without the number
   * of constants and PIs.
   */
  uint32_t num_gates() const;

  /*! \brief Returns the fanin size of a node. */
  uint32_t fanin_size( node const& n ) const;

  /*! \brief Returns the fanout size of a node. */
  uint32_t fanout_size( node const& n ) const;
#pragma endregion

#pragma region Functional properties
  /*! \brief Returns the function of a node. */
  kitty::dynamic_truth_table node_function( node const& n ) const;
#pragma endregion

#pragma region Nodes and signals
  /*! \brief Get the node a signal is pointing to. */
  node get_node( signal const& f ) const;

  /*! \brief Check whether a signal is complemented.
   *
   * This method may also be provided by network implementations that do not
   * have complemented edges.  In this case, the method simply returns
   * ``false`` for each node.
   */
  bool is_complemented( signal const& f ) const;

  /*! \brief Returns the index of a node.
   *
   * The index of a node must be a unique for each node and must be between 0
   * (inclusive) and the size of a network (exclusive, value returned by
   * ``size()``).
   */
  uint32_t node_to_index( node const& n ) const;

  /*! \brief Returns the node for an index.
   *
   * This is the inverse function to ``node_to_index``.
   *
   * \param index A value between 0 (inclusive) and the size of the network
   *              (exclusive)
   */
  node index_to_node( uint32_t index ) const;
#pragma endregion

#pragma region Node and signal iterators
  /*! \brief Calls ``fn`` on every node in network.
   *
   * The order of nodes depends on the implementation and must not guarantee
   * topological order.  The paramater ``fn`` is any callable that must have
   * one of the following four signatures.
   * - ``void(node const&)``
   * - ``void(node const&, uint32_t)``
   * - ``bool(node const&)``
   * - ``bool(node const&, uint32_t)``
   *
   * If ``fn`` has two parameters, the second parameter is an index starting
   * from 0 and incremented in every iteration.  If ``fn`` returns a ``bool``,
   * then it can interrupt the iteration by returning ``false``.
   */
  template<typename Fn>
  void foreach_node( Fn&& fn ) const;

  /*! \brief Calls ``fn`` on every primary input noe in the network.
   *
   * The order is in the same order as primary inputs have been created with
   * ``create_pi``.  The paramater ``fn`` is any callable that must have one of
   * the following four signatures.
   * - ``void(node const&)``
   * - ``void(node const&, uint32_t)``
   * - ``bool(node const&)``
   * - ``bool(node const&, uint32_t)``
   *
   * If ``fn`` has two parameters, the second parameter is an index starting
   * from 0 and incremented in every iteration.  If ``fn`` returns a ``bool``,
   * then it can interrupt the iteration by returning ``false``.
   */
  template<typename Fn>
  void foreach_pi( Fn&& fn ) const;

  /*! \brief Calls ``fn`` on every primary output signal in the network.
   *
   * The order is in the same order as primary outputs have been created with
   * ``create_po``.  The function is called on the signal that is driving the
   * output and may occur more than once in the iteration, if it drives more
   * than one output. The paramater ``fn`` is any callable that must have one
   * of the following four signatures.
   * - ``void(signal const&)``
   * - ``void(signal const&, uint32_t)``
   * - ``bool(signal const&)``
   * - ``bool(signal const&, uint32_t)``
   *
   * If ``fn`` has two parameters, the second parameter is an index starting
   * from 0 and incremented in every iteration.  If ``fn`` returns a ``bool``,
   * then it can interrupt the iteration by returning ``false``.
   */
  template<typename Fn>
  void foreach_po( Fn&& fn ) const;

  /*! \brief Calls ``fn`` on every gate node in the network.
   *
   * Calls each node that is not constant and not a primary input.  The
   * paramater ``fn`` is any callable that must have one of the following four
   * signatures.
   * - ``void(node const&)``
   * - ``void(node const&, uint32_t)``
   * - ``bool(node const&)``
   * - ``bool(node const&, uint32_t)``
   *
   * If ``fn`` has two parameters, the second parameter is an index starting
   * from 0 and incremented in every iteration.  If ``fn`` returns a ``bool``,
   * then it can interrupt the iteration by returning ``false``.
   */
  template<typename Fn>
  void foreach_gate( Fn&& fn ) const;

  /*! \brief Calls ``fn`` on every fanin of a node.
   *
   * The order of the fanins is in the same order that was used to create the
   * node.  The paramater ``fn`` is any callable that must have one of the
   * following four signatures.
   * - ``void(signal const&)``
   * - ``void(signal const&, uint32_t)``
   * - ``bool(signal const&)``
   * - ``bool(signal const&, uint32_t)``
   *
   * If ``fn`` has two parameters, the second parameter is an index starting
   * from 0 and incremented in every iteration.  If ``fn`` returns a ``bool``,
   * then it can interrupt the iteration by returning ``false``.
   */
  template<typename Fn>
  void foreach_fanin( node const& n, Fn&& fn ) const;
#pragma endregion

#pragma region Simulate values
  /*! \brief Simulates arbitrary value on a node.
   *
   * This is a generic simulation method that can be implemented multiple times
   * for a network interface for different types.  One only needs to change the
   * implementation and change the value for the type parameter ``T``, which
   * indicates the element type of the iterators.
   *
   * Examples for simulation types are ``bool``,
   * ``kitty::dynamic_truth_table``, bit masks, or BDDs.
   *
   * The ``begin`` and ``end`` iterator point to values which are assumed to be
   * assigned to the fanin of the node.  Consequently, the distance from
   * ``begin`` to ``end`` must equal the fanin size of the node.
   *
   * \param n Node to simulate (used to retrieve the node function)
   * \param begin Begin iterator to simulation values of fanin
   * \param end End iterator to simulation values of fanin
   * \return Returns computed simulation value of type ``T``
   */
  template<typename Iterator>
  iterates_over_t<Iterator, T>
  compute( node const& n, Iterator begin, Iterator end ) const;
#pragma endregion

#pragma region Mapping
  /*! \brief Returns true, if network has a mapping. */
  bool has_mapping() const;

  /*! \brief Returns true, if node is mapped. */
  bool is_mapped( node const& n ) const;

  /*! \brief Clears a mapping. */
  void clear_mapping();

  /*! \brief Number of mapped nodes. */
  uint32_t num_luts() const;

  /*! \brief Adds a node to the mapping. */
  template<typename LeavesIterator>
  void add_to_mapping( node const& n, LeavesIterator begin, LeavesIterator end );

  /*! \brief Remove from mapping. */
  void remove_from_mapping( node const& n );

  /*! \brief Gets LUT function */
  kitty::dynamic_truth_table lut_function( node const& n );

  /*! \brief Sets LUT function. */
  void set_lut_function( node const& n, kitty::dynamic_truth_table const& function );

  /*! \brief Iterators over node's mapping fan-ins. */
  template<typename Fn>
  void foreach_lut_fanin( node const& n, Fn&& fn ) const;
#pragma endregion

#pragma region Custom node values
  /*! \brief Reset all values to 0. */
  void clear_values() const;

  /*! \brief Returns value of a node. */
  uint32_t value( node const& n ) const;

  /*! \brief Sets value of a node. */
  void set_value( node const& n, uint32_t value ) const;

  /*! \brief Increments value of a node and returns *previous* value. */
  uint32_t incr_value( node const& n ) const;

  /*! \brief Decrements value of a node and returns *new* value. */
  uint32_t decr_value( node const& n ) const;
#pragma endregion

#pragma region Visited flags
  /*! \brief Reset all visited values to 0. */
  void clear_visited() const;

  /*! \brief Returns the visited value of a node. */
  uint32_t visited( node const& n ) const;

  /*! \brief Sets the visited value of a node. */
  uint32_t set_visited( node const& n, uint32_t v ) const;
#pragma endregion

};

} /* namespace mockturtle */
