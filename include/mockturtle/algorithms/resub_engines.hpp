/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2020  EPFL
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
  \file resub_engines.hpp
  \brief Implements generalized resubstitution engine(s).

  \author Siang-Yun Lee
*/

#pragma once

#include <kitty/kitty.hpp>

#include <vector>
#include <unordered_map>

namespace mockturtle
{

template<class TT>
class mig_resub_engine_bottom_up
{
  struct maj_node
  {
    uint32_t id; /* ( id - divisors.size() ) / 2 = its position in maj_nodes */
    std::vector<uint32_t> fanins; /* ids of its three fanins */
  };

public:
  explicit mig_resub_engine_bottom_up( uint64_t num_divisors, uint64_t max_num_divisors = 50ul )
    : max_num_divisors( max_num_divisors ), counter( 2u ), divisors( ( num_divisors + 1 ) * 2 ), scores( ( num_divisors + 1 ) * 2 )
  { }

  template<class node_type, class truth_table_storage_type>
  void add_root( node_type const& node, truth_table_storage_type const& tts )
  {
    divisors.at( 0u ) = ~tts[node]; // const 0 XNOR target = ~target
    divisors.at( 1u ) = tts[node]; // const 1 XNOR target = target
    num_bits = tts[node].num_bits();
  }

  template<class node_type, class truth_table_storage_type>
  void add_divisor( node_type const& node, truth_table_storage_type const& tts )
  {
    assert( tts[node].num_bits() == num_bits );
    divisors.at( counter++ ) = tts[node] ^ divisors.at( 0u ); // XNOR target = XOR ~target
    divisors.at( counter++ ) = ~tts[node] ^ divisors.at( 0u );
  }

  template<class iterator_type, class truth_table_storage_type>
  void add_divisors( iterator_type begin, iterator_type end, truth_table_storage_type const& tts )
  { 
    assert( counter == 2u );
    while ( begin != end )
    {
      add_divisor( *begin, tts );
      ++begin;
    }
  }

  std::optional<std::vector<uint32_t>> compute_function( uint32_t num_inserts )
  {
    uint64_t max_score = 0u;
    max_i = 0u;
    for ( auto i = 0u; i < divisors.size(); ++i )
    {
      scores.at( i ) = kitty::count_ones( divisors.at( i ) );
      if ( scores.at( i ) > max_score )
      {
        max_score = scores.at( i );
        max_i = i;
        if ( max_score == num_bits )
        {
          break;
        }
      }
    }
    /* 0-resub (including constants) */
    if ( max_score == num_bits )
    {
      return std::vector<uint32_t>( {max_i} );
    }

    if ( num_inserts == 0u )
    {
      return std::nullopt;
    }
    size_limit = num_inserts;

    return bottom_up_approach();
  }

private:
  std::optional<std::vector<uint32_t>> bottom_up_approach()
  {
    maj_nodes.emplace_back( maj_node{uint32_t( divisors.size() ), {max_i}} );
    TT const& function_i = divisors.at( max_i );
    max_i = divisors.size();
    return bottom_up_approach_rec( function_i );
  }

  std::optional<std::vector<uint32_t>> bottom_up_approach_rec( TT const& function_i )
  {
    /* THINK: Should we consider reusing newly-built nodes (nodes in maj_nodes) in addition to divisors? */

    /* the second fanin: 2 * #newly-covered-bits + 1 * #cover-again-bits */
    uint64_t max_score = 0u;
    max_j = 0u;
    auto const not_covered_by_i = ~function_i;
    for ( auto j = 0u; j < divisors.size(); ++j )
    {
      auto const covered_by_j = divisors.at( j );
      scores.at( j ) = kitty::count_ones( covered_by_j ) + kitty::count_ones( not_covered_by_i & covered_by_j );
      if ( scores.at( j ) > max_score && (j >> 1) != (max_i >> 1) )
      {
        max_score = scores.at( j );
        max_j = j;
      }
    }
    maj_nodes.back().fanins.emplace_back( max_j );

    /* the third fanin: only care about the disagreed bits */
    max_score = 0u;
    max_k = 0u;
    auto const disagree_in_ij = function_i ^ divisors.at( max_j );
    for ( auto k = 0u; k < divisors.size(); ++k )
    {      
      scores.at( k ) = kitty::count_ones( divisors.at( k ) & disagree_in_ij );
      if ( scores.at( k ) > max_score && (k >> 1) != (max_i >> 1) && (k >> 1) != (max_j >> 1) )
      {
        max_score = scores.at( k );
        max_k = k;
      }
    }
    maj_nodes.back().fanins.emplace_back( max_k );

    auto const current_function = kitty::ternary_majority( function_i, divisors.at( max_j ), divisors.at( max_k ) );
    if ( kitty::is_const0( ~current_function ) )
    {
      std::vector<uint32_t> index_list;
      for ( auto& n : maj_nodes )
      {
        index_list.emplace_back( n.fanins.at( 0u ) );
        index_list.emplace_back( n.fanins.at( 1u ) );
        index_list.emplace_back( n.fanins.at( 2u ) );
      }
      index_list.emplace_back( maj_nodes.back().id );
      return index_list;
    }
    else if ( maj_nodes.size() < size_limit )
    {
      maj_nodes.emplace_back( maj_node{maj_nodes.back().id + 2u, {maj_nodes.back().id}} );
      return bottom_up_approach_rec( current_function );
    }
    else
    {
      return std::nullopt;
    }
  }

private:
  uint64_t max_num_divisors;
  uint64_t counter;
  uint32_t size_limit;
  uint32_t num_bits;

  uint32_t max_i, max_j, max_k;

  std::vector<TT> divisors;
  std::vector<uint64_t> scores;
  std::vector<maj_node> maj_nodes;

}; /* mig_resub_engine_bottom_up */

template<class TT>
class mig_resub_engine
{
  bool p = 0; // verbose printing
  /*! \brief Internal data structure */
  struct expansion_position
  {
    int32_t parent_position = -1; // maj_nodes.at( ... )
    int32_t fanin_num = -1; // 0, 1, 2

    bool operator==( expansion_position const& e ) const
    {
      return parent_position == e.parent_position && fanin_num == e.fanin_num;
    }
  };

  struct maj_node
  {
    uint32_t id; /* maj_nodes.at( id - divisors.size() ) */
    std::vector<uint32_t> fanins; /* ids of its three fanins */

    std::vector<TT> fanin_functions = std::vector<TT>();
    TT care = TT();
    expansion_position parent = expansion_position();
  };

  struct simple_maj
  {
    std::vector<uint32_t> fanins; /* ids of divisors */
    TT function; /* resulting function */
  };

public:
  explicit mig_resub_engine( uint64_t num_divisors, uint64_t max_num_divisors = 50ul )
    : max_num_divisors( max_num_divisors ), counter( 2u ), divisors( ( num_divisors + 1 ) * 2 ), scores( ( num_divisors + 1 ) * 2 )
  { }

  template<class node_type, class truth_table_storage_type>
  void add_root( node_type const& node, truth_table_storage_type const& tts )
  {
    divisors.at( 0u ) = ~tts[node]; // const 0 XNOR target = ~target
    divisors.at( 1u ) = tts[node]; // const 1 XNOR target = target
    num_bits = tts[node].num_bits();
  }

  template<class node_type, class truth_table_storage_type>
  void add_divisor( node_type const& node, truth_table_storage_type const& tts )
  {
    assert( tts[node].num_bits() == num_bits );
    divisors.at( counter++ ) = tts[node] ^ divisors.at( 0u ); // XNOR target = XOR ~target
    divisors.at( counter++ ) = ~tts[node] ^ divisors.at( 0u );
  }

  template<class iterator_type, class truth_table_storage_type>
  void add_divisors( iterator_type begin, iterator_type end, truth_table_storage_type const& tts )
  { 
    assert( counter == 2u );
    while ( begin != end )
    {
      add_divisor( *begin, tts );
      ++begin;
    }
  }

  std::optional<std::vector<uint32_t>> compute_function( uint32_t num_inserts )
  {
    for ( auto i = 0u; i < divisors.size(); ++i )
    {
      if (p) { std::cout<<"["<<i<<"] "; kitty::print_binary( divisors.at( i ) ); std::cout << "\n"; }
      if ( kitty::is_const0( ~divisors.at( i ) ) )
      {
        /* 0-resub (including constants) */
        return std::vector<uint32_t>( {i} );
      }
    }

    if ( num_inserts == 0u )
    {
      return std::nullopt;
    }
    size_limit = num_inserts;

    return top_down_approach();
  }

private:
  std::optional<std::vector<uint32_t>> top_down_approach()
  {
    maj_nodes.reserve( size_limit );
    /* topmost node: care is const1 */
    TT const const1 = divisors.at( 0u ) | divisors.at( 1u );
    simple_maj const top_node = expand_one( const1 );
    
    if ( kitty::is_const0( ~top_node.function ) )
    {
      /* 1-resub */
      std::vector<uint32_t> index_list;
      index_list.emplace_back( top_node.fanins.at( 0u ) );
      index_list.emplace_back( top_node.fanins.at( 1u ) );
      index_list.emplace_back( top_node.fanins.at( 2u ) );
      index_list.emplace_back( divisors.size() );
      return index_list;
    }

    std::vector<maj_node> copy;
    for ( int32_t i = 0; i < 3; ++i )
    {
      if (p) { std::cout<<"=== try expand "<<i<<" first ===\n"; }
      maj_nodes.clear();
      maj_nodes.emplace_back( maj_node{uint32_t( divisors.size() ), top_node.fanins, {divisors.at( top_node.fanins[0] ), divisors.at( top_node.fanins[1] ), divisors.at( top_node.fanins[2] )}, const1} );

      TT const care = ~( divisors.at( top_node.fanins[sibling_index( i, 1 )] ) & divisors.at( top_node.fanins[sibling_index( i, 2 )] ) );
      if ( evaluate_one( care, divisors.at( top_node.fanins[i] ), expansion_position{0, i} ) )
      {
        /* 2-resub */
        copy = maj_nodes;
        break;
      }

      leaves.clear();
      back_up.clear();
      leaves.emplace_back( expansion_position{0, (int32_t)sibling_index( i, 1 )} );
      leaves.emplace_back( expansion_position{0, (int32_t)sibling_index( i, 2 )} );
      if ( !refine() )
      {
        continue;
      }

      if ( copy.size() == 0u || maj_nodes.size() < copy.size() )
      {
        copy = maj_nodes;
      }
    }

    if ( copy.size() == 0u )
    {
      return std::nullopt;
    }
    std::vector<uint32_t> index_list;
    std::unordered_map<uint32_t, uint32_t> id_map;
    for ( auto i = 0u; i < copy.size(); ++i )
    {
      auto& n = copy.at( copy.size() - i - 1u );
      for ( auto j = 0u; j < 3u; ++j )
      {
        if ( n.fanins.at( j ) < divisors.size() )
        {
          index_list.emplace_back( n.fanins.at( j ) );
        }
        else
        {
          auto mapped = id_map.find( n.fanins.at( j ) );
          assert( mapped != id_map.end() );
          index_list.emplace_back( mapped->second );
        }
        id_map[n.id] = divisors.size() + i * 2;
      }
    }
    index_list.emplace_back( ( id_map.find( copy.at( 0u ).id ) )->second );
    return index_list;
  }

  bool refine()
  {
    while ( ( leaves.size() != 0u || back_up.size() != 0u ) && maj_nodes.size() < size_limit )
    {
      if ( leaves.size() == 0u )
      {
        leaves = back_up;
        back_up.clear();
        first_round = false;
      }

      uint32_t max_mismatch = 0u;
      uint32_t pos = 0u;
      for ( int32_t i = 0; (unsigned)i < leaves.size(); ++i )
      {
        maj_node& parent_node = maj_nodes.at( leaves[i].parent_position );
        uint32_t const& fi = leaves[i].fanin_num;
        TT const& original_function = parent_node.fanin_functions.at( fi );

        if ( parent_node.fanins.at( fi ) >= divisors.size() ) /* already expanded */
        {
          leaves.erase( leaves.begin() + i );
          --i;
          continue;
        }

        TT const care = parent_node.care & ~( sibling_func( parent_node, fi, 1 ) & sibling_func( parent_node, fi, 2 ) );
        if ( fulfilled( original_function, care ) /* already fulfilled */
             || care == parent_node.care /* probably cannot improve */
           )
        {
          leaves.erase( leaves.begin() + i );
          --i;
          continue;
        }

        uint32_t const mismatch = count_ones( care & ~original_function );
        if ( mismatch > max_mismatch )
        {
          pos = i;
          max_mismatch = mismatch;
        }
      }
      if ( leaves.size() == 0u )
      {
        break;
      }
      expansion_position node_position = leaves.at( pos );
      leaves.erase( leaves.begin() + pos );

      maj_node& parent_node = maj_nodes.at( node_position.parent_position );
      uint32_t const& fi = node_position.fanin_num;
      TT const& original_function = parent_node.fanin_functions.at( fi );
      TT const care = parent_node.care & ~( sibling_func( parent_node, fi, 1 ) & sibling_func( parent_node, fi, 2 ) );

      if ( evaluate_one( care, original_function, node_position ) )
      {
        return true;
      }
    }
    return false;
  }

  bool evaluate_one( TT const& care, TT const& original_function, expansion_position const& node_position )
  {
    maj_node& parent_node = maj_nodes.at( node_position.parent_position );
    uint32_t const& fi = node_position.fanin_num;

    simple_maj const new_node = expand_one( care, node_position );
    uint64_t const original_score = score( original_function, care );
    uint64_t const new_score = score( new_node.function, care );
    if ( new_score < original_score )
    {
      if (p) { std::cout << "get worse. (" << new_score << ")\n"; }
      return false;
    }

    if ( new_score == original_score )
    {
      if (p) { std::cout<<"score stays the same.\n"; }
      if ( kitty::count_ones( new_node.function & parent_node.care ) > kitty::count_ones( original_function & parent_node.care ) )
      {
        if (p) { std::cout<<"...but covers more care bits of parent node.\n"; }
      }
      else if ( kitty::count_ones( new_node.function & parent_node.care ) == kitty::count_ones( original_function & parent_node.care ) && new_node.function != original_function )
      {
        if (p) { std::cout<<"...and also covers the same # of care bits of parent node, but the function is different.\n"; }
        if ( first_round )
        {
          back_up.emplace_back( node_position );
          return false;
        }
      }
      else
      {
        return false;
      }
    }

    /* construct the new node */
    uint32_t const new_id = maj_nodes.size() + divisors.size();
    maj_nodes.emplace_back( maj_node{new_id, new_node.fanins, {divisors.at( new_node.fanins[0] ), divisors.at( new_node.fanins[1] ), divisors.at( new_node.fanins[2] )}, care, node_position} );
    update_fanin( parent_node, fi, new_id, new_node.function );

    if ( kitty::is_const0( ~new_node.function & care ) )
    {
      if (p) { std::cout<<"care bits all fulfilled.\n"; }
      if ( node_fulfilled( maj_nodes.at( 0u ) ) )
      {
        if (p) {
          std::cout<<"\n======== solution found =========\n";
          for ( auto i = 0u; i < maj_nodes.size(); ++i )
          {
            std::cout<<"[node "<<i<<"] id = " <<maj_nodes[i].id<< "\n";
            for ( auto j = 0u; j < 3u; ++j )
            {
              std::cout<<"["<<std::setw(3)<<maj_nodes[i].fanins[j]<<"] ";
              kitty::print_binary(maj_nodes[i].fanin_functions[j]);
              std::cout<<"\n";
            }
            std::cout<<"      --------\n      ";
            kitty::print_binary(kitty::ternary_majority(maj_nodes[i].fanin_functions[0], maj_nodes[i].fanin_functions[1], maj_nodes[i].fanin_functions[2]));
            std::cout<<"\n\n";
          }
        }
        return true;
      }
    }
    else
    {
      if (p) { std::cout<<"improved but still not fulfilling all care bits, add children to queue.\n"; }
      leaves.emplace_back( expansion_position{int32_t( maj_nodes.size() - 1 ), 0} );
      leaves.emplace_back( expansion_position{int32_t( maj_nodes.size() - 1 ), 1} );
      leaves.emplace_back( expansion_position{int32_t( maj_nodes.size() - 1 ), 2} );
    }
    return false;
  }
  
  simple_maj expand_one( TT const& care, expansion_position const& node_position = {} )
  {
    if (p) { std::cout << "\n\nexpanding node " << node_position.parent_position << " at fanin " << node_position.fanin_num << ".\ncare = "; kitty::print_binary( care ); std::cout<<"\n"; }

    /* look up in computed_table */
    auto computed = computed_table.find( care );
    if ( computed != computed_table.end() )
    {
      //std::cout<<"cache hit!\n";
      return computed->second;
    }

    /* the first fanin: cover most care bits */
    uint64_t max_score = 0u;
    uint32_t max_i = 0u;
    for ( auto i = 0u; i < divisors.size(); ++i )
    {
      scores.at( i ) = kitty::count_ones( divisors.at( i ) & care );
      if ( scores.at( i ) > max_score )
      {
        max_score = scores.at( i );
        max_i = i;
      }
    }

    /* the second fanin: 2 * #newly-covered-bits + 1 * #cover-again-bits */
    max_score = 0u;
    uint32_t max_j = 0u;
    auto const not_covered_by_i = ~divisors.at( max_i );
    for ( auto j = 0u; j < divisors.size(); ++j )
    {
      auto const covered_by_j = divisors.at( j ) & care;
      scores.at( j ) = kitty::count_ones( covered_by_j ) + kitty::count_ones( not_covered_by_i & covered_by_j );
      if ( scores.at( j ) > max_score && !same_divisor( j, max_i ) )
      {
        max_score = scores.at( j );
        max_j = j;
      }
    }

    /* the third fanin: 2 * #cover-never-covered-bits + 1 * #cover-covered-once-bits */
    max_score = 0u;
    uint32_t max_k = 0u;
    auto const not_covered_by_j = ~divisors.at( max_j );
    for ( auto k = 0u; k < divisors.size(); ++k )
    {
      auto const covered_by_k = divisors.at( k ) & care;
      scores.at( k ) = kitty::count_ones( covered_by_k & not_covered_by_i ) + kitty::count_ones( covered_by_k & not_covered_by_j );
      if ( scores.at( k ) > max_score && !same_divisor( k, max_i ) && !same_divisor( k, max_j ) )
      {
        max_score = scores.at( k );
        max_k = k;
      }
    }

    if (p) { std::cout<<"resulting function: <"<<max_i<<", "<<max_j<<", "<<max_k<<"> = "; kitty::print_binary(kitty::ternary_majority( divisors.at( max_i ), divisors.at( max_j ), divisors.at( max_k ) )); std::cout<<"\n"; }
    computed_table[care] = simple_maj( {{max_i, max_j, max_k}, kitty::ternary_majority( divisors.at( max_i ), divisors.at( max_j ), divisors.at( max_k ) )} );
    return computed_table[care];
  }

private:
  bool same_divisor( uint32_t const i, uint32_t const j )
  {
    return ( i >> 1 ) == ( j >> 1 );
  }

  bool fulfilled( TT const& func, TT const& care )
  {
    return kitty::is_const0( ~func & care );
  }

  bool node_fulfilled( maj_node const& node )
  {
    return fulfilled( kitty::ternary_majority( node.fanin_functions.at( 0u ), node.fanin_functions.at( 1u ), node.fanin_functions.at( 2u ) ), node.care );
  }

  uint64_t score( TT const& func, TT const& care )
  {
    return kitty::count_ones( func & care );
  }

  void update_fanin( maj_node& parent_node, uint32_t const fi, uint32_t const new_id, TT const& new_function )
  {
    parent_node.fanins.at( fi ) = new_id;
    parent_node.fanin_functions.at( fi ) = new_function;

    TT const& sibling_func1 = sibling_func( parent_node, fi, 1 );
    TT const& sibling_func2 = sibling_func( parent_node, fi, 2 );

    update_sibling( parent_node, fi, 1, new_function, sibling_func2 );
    update_sibling( parent_node, fi, 2, new_function, sibling_func1 );

    /* update grandparents */
    if ( parent_node.parent.parent_position != -1 ) /* not the topmost node */
    {
      update_fanin( grandparent( parent_node ), parent_node.parent.fanin_num, parent_node.id, kitty::ternary_majority( new_function, sibling_func1, sibling_func2 ) );
    }
  }

  void update_sibling( maj_node const& parent_node, uint32_t const fi, uint32_t const sibling_num, TT const& new_function, TT const& sibling_func )
  {
    uint32_t index = sibling_index( fi, sibling_num );
    uint32_t id = parent_node.fanins.at( index );
    /* update the `care`s of the siblings (if they are not divisors) */
    if ( id >= divisors.size() )
    {
      id_to_node( id ).care = parent_node.care & ~( new_function & sibling_func );
    }
    else /* or add these positions back to the expansion queue */
    {
      expansion_position new_pos{int32_t( id_to_pos( parent_node.id ) ), int32_t( index )};
      bool added = false;
      for ( auto& l : leaves )
      {
        if ( l == new_pos )
        {
          added = true;
          break;
        }
      }
      if ( !added )
      {
        leaves.emplace_back( new_pos );
      }
    }
  }

  inline maj_node& grandparent( maj_node const& parent_node )
  {
    return maj_nodes.at( parent_node.parent.parent_position );
  }

  inline uint32_t sibling_index( uint32_t const my_index, uint32_t const sibling_num )
  {
    return ( my_index + sibling_num ) % 3;
  }

  //inline uint32_t sibling_id( maj_node const& parent_node, uint32_t const my_index, uint32_t const sibling_num )
  //{
  //  return parent_node.fanins.at( sibling_index( my_index, sibling_num ) );
  //}

  inline TT const& sibling_func( maj_node const& parent_node, uint32_t const my_index, uint32_t const sibling_num )
  {
    return parent_node.fanin_functions.at( sibling_index( my_index, sibling_num ) );
  }

  inline uint32_t id_to_pos( uint32_t const id )
  {
    assert( id >= divisors.size() );
    return ( id - divisors.size() );
  }

  inline maj_node& id_to_node( uint32_t const id )
  { 
    return maj_nodes.at( id_to_pos( id ) );
  }

private:
  uint64_t max_num_divisors;
  uint64_t counter;
  uint32_t size_limit;
  uint32_t num_bits;

  std::vector<TT> divisors;
  std::vector<uint64_t> scores;
  std::vector<maj_node> maj_nodes; /* the really used nodes */
  std::unordered_map<TT, simple_maj, kitty::hash<TT>> computed_table; /* map from care to a simple_maj with divisors as fanins */

  std::vector<expansion_position> leaves;
  std::vector<expansion_position> back_up;
  bool first_round = true;
}; /* mig_resub_engine */

} /* namespace mockturtle */
