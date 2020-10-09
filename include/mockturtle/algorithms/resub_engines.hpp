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

namespace mockturtle
{
#define two_cares 0

template<class TT, bool use_top_down = true>
class mig_resub_engine
{
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
    uint32_t id; /* ( id - divisors.size() ) / 2 = its position in maj_nodes */
    std::vector<uint32_t> fanins; /* ids of its three fanins */

    /* only used in top-down */
    std::vector<TT> fanin_functions = std::vector<TT>();
    TT care = TT();
    expansion_position parent = expansion_position();
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

    if constexpr ( use_top_down )
    {
      return top_down_approach();
    }
    else
    {
      return bottom_up_approach();
    }
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

  std::optional<std::vector<uint32_t>> top_down_approach()
  {
    maj_nodes.reserve( size_limit );
    /* topmost node: care is const1 */
    TT const const1 = divisors.at( 0u ) | divisors.at( 1u );
  #if two_cares
    top_down_approach_try_one( const1, const1 );
  #else
    top_down_approach_try_one( const1 );
  #endif
    std::cout<<"first node built.\n";
    maj_nodes.emplace_back( maj_node{uint32_t( divisors.size() ), {max_i, max_j, max_k}, {divisors.at( max_i ), divisors.at( max_j ), divisors.at( max_k )}, const1} );
    if ( kitty::is_const0( ~kitty::ternary_majority( divisors.at( max_i ), divisors.at( max_j ), divisors.at( max_k ) ) ) )
    {
      std::cout<<"care bits all fulfilled.\n";
      /* 1-resub */
      std::vector<uint32_t> index_list;
      index_list.emplace_back( maj_nodes.at( 0u ).fanins.at( 0u ) );
      index_list.emplace_back( maj_nodes.at( 0u ).fanins.at( 1u ) );
      index_list.emplace_back( maj_nodes.at( 0u ).fanins.at( 2u ) );
      index_list.emplace_back( maj_nodes.at( 0u ).id );
      return index_list;
    }
    else
    {
      leaves.emplace_back( expansion_position{0, 0} );
      leaves.emplace_back( expansion_position{0, 1} );
      leaves.emplace_back( expansion_position{0, 2} );
    }

    while ( leaves.size() != 0u && maj_nodes.size() < size_limit )
    {
      // TODO: Choose a best leaf to expand
      expansion_position node_position = leaves.back();
      leaves.pop_back();

      maj_node& parent_node = maj_nodes.at( node_position.parent_position );
      uint32_t const& fi = node_position.fanin_num;
      TT const& original_function = parent_node.fanin_functions.at( fi );

      if ( parent_node.fanins.at( fi ) >= divisors.size() ) /* already expanded */
      {
        continue;
      }

    #if two_cares
      TT const care1 = parent_node.care & ~sibling_func( parent_node, fi, 1 );
      TT const care2 = parent_node.care & ~sibling_func( parent_node, fi, 2 );
      TT const care = care1 | care2;
      uint64_t const original_score = score( original_function, care1, care2 );
    #else
      TT const care = parent_node.care & ~( sibling_func( parent_node, fi, 1 ) & sibling_func( parent_node, fi, 2 ) );
      uint64_t const original_score = score( original_function, care );
    #endif

      if ( fulfilled( original_function, care ) ) /* already fulfilled */
      {
        continue;
      }

    #if two_cares
      top_down_approach_try_one( care1, care2, node_position );
    #else
      top_down_approach_try_one( care, node_position );
    #endif
      
      auto const current_function = kitty::ternary_majority( divisors.at( max_i ), divisors.at( max_j ), divisors.at( max_k ) );
      std::cout<<"resulting function: "; kitty::print_binary(current_function); std::cout<<"\n";

    #if two_cares
      auto new_score = score( current_function, care1, care2 );
    #else
      auto new_score = score( current_function, care );
    #endif

      if ( new_score < original_score )
      {
        std::cout << "get worse. (" << new_score << ")\n";
        continue;
      }

      if ( new_score == original_score )
      {
        if ( kitty::count_ones( current_function & parent_node.care ) >= kitty::count_ones( original_function & parent_node.care ) &&
             current_function != original_function )
        {
          std::cout<<"score stays the same but covers more care bits of parent node, or also the same but different function.\n";
        }
        else
        {
          continue;
        }
      }

      /* construct the new node */
      auto new_id = maj_nodes.back().id + 2u;
      maj_nodes.emplace_back( maj_node{new_id, {max_i, max_j, max_k}, {divisors.at( max_i ), divisors.at( max_j ), divisors.at( max_k )}, care, node_position} );
      update_fanin( parent_node, fi, new_id, current_function );

      if ( kitty::is_const0( ~current_function & care ) )
      {
        std::cout<<"care bits all fulfilled.\n";
        if ( node_fulfilled( maj_nodes.at( 0u ) ) )
        {
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
          std::vector<uint32_t> index_list;
          // TODO: translate
          return index_list;
        }
      }
      else
      {
        std::cout<<"improved but still not fulfilling all care bits, add children to queue.\n";
        leaves.emplace_back( expansion_position{int32_t( maj_nodes.size() - 1), 0} );
        leaves.emplace_back( expansion_position{int32_t( maj_nodes.size() - 1), 1} );
        leaves.emplace_back( expansion_position{int32_t( maj_nodes.size() - 1), 2} );
      }
    }
    return std::nullopt;
  }
  
  /* default values are just redundant values because they are not used for the first node */
#if two_cares
  void top_down_approach_try_one( TT const& care1, TT const& care2, expansion_position const& node_position = {} )
#else
  void top_down_approach_try_one( TT const& care, expansion_position const& node_position = {} )
#endif
  {
    std::cout << "\n\nexpanding node " << node_position.parent_position << " at fanin " << node_position.fanin_num << ".\ncare = ";
  #if two_cares
    kitty::print_binary( care1 ); std::cout<<" "; kitty::print_binary( care2 );
  #else
    kitty::print_binary( care );
  #endif

    std::cout << "\nchoosing first fanin:\n";
    /* the first fanin: cover most care bits */
    uint64_t max_score = 0u;
    max_i = 0u;
    for ( auto i = 0u; i < divisors.size(); ++i )
    {
    #if two_cares
      scores.at( i ) = kitty::count_ones( divisors.at( i ) & care1 ) + kitty::count_ones( divisors.at( i ) & care2 );
    #else
      scores.at( i ) = kitty::count_ones( divisors.at( i ) & care );
    #endif
      std::cout<<"["<<i<<"] "; kitty::print_binary( divisors.at( i ) ); std::cout << ": " << scores.at(i) << "\n";
      if ( scores.at( i ) > max_score )
      {
        max_score = scores.at( i );
        max_i = i;
      }
    }
    std::cout<<"===== chosen " << max_i <<"\n\nchoosing second fanin:\n";

    /* the second fanin: 2 * #newly-covered-bits + 1 * #cover-again-bits */
    max_score = 0u;
    max_j = 0u;
    auto const not_covered_by_i = ~divisors.at( max_i );
    for ( auto j = 0u; j < divisors.size(); ++j )
    {
    #if two_cares
      auto const covered_by_j1 = divisors.at( j ) & care1;
      auto const covered_by_j2 = divisors.at( j ) & care2;
      scores.at( j ) = kitty::count_ones( covered_by_j1 ) + kitty::count_ones( not_covered_by_i & covered_by_j1 ) + kitty::count_ones( covered_by_j2 ) + kitty::count_ones( not_covered_by_i & covered_by_j2 );
    #else
      auto const covered_by_j = divisors.at( j ) & care;
      scores.at( j ) = kitty::count_ones( covered_by_j ) + kitty::count_ones( not_covered_by_i & covered_by_j );
    #endif
      std::cout<<"["<<j<<"] "; kitty::print_binary( divisors.at( j ) ); std::cout << ": " << scores.at(j) << "\n";
      if ( scores.at( j ) > max_score && !same_divisor( j, max_i ) )
      {
        max_score = scores.at( j );
        max_j = j;
      }
    }
    std::cout<<"===== chosen " << max_j <<"\n\nchoosing third fanin:\n";

    /* the third fanin: 2 * #cover-never-covered-bits + 1 * #cover-covered-once-bits */
    max_score = 0u;
    max_k = 0u;
    auto const not_covered_by_j = ~divisors.at( max_j );
    for ( auto k = 0u; k < divisors.size(); ++k )
    {
    #if two_cares
      auto const covered_by_k1 = divisors.at( k ) & care1;
      auto const covered_by_k2 = divisors.at( k ) & care2;
      scores.at( k ) = kitty::count_ones( covered_by_k1 & not_covered_by_i ) + kitty::count_ones( covered_by_k1 & not_covered_by_j ) + kitty::count_ones( covered_by_k2 & not_covered_by_i ) + kitty::count_ones( covered_by_k2 & not_covered_by_j );
    #else
      auto const covered_by_k = divisors.at( k ) & care;
      scores.at( k ) = kitty::count_ones( covered_by_k & not_covered_by_i ) + kitty::count_ones( covered_by_k & not_covered_by_j );
    #endif
      std::cout<<"["<<k<<"] "; kitty::print_binary( divisors.at( k ) ); std::cout << ": " << scores.at(k) << "\n";
      if ( scores.at( k ) > max_score && !same_divisor( k, max_i ) && !same_divisor( k, max_j ) )
      {
        max_score = scores.at( k );
        max_k = k;
      }
    }
    std::cout<<"===== chosen " << max_k <<"\n";
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

#if two_cares
  uint64_t score( TT const& func, TT const& care1, TT const& care2 )
  {
    return kitty::count_ones( func & care1 ) + kitty::count_ones( func & care2 );
  }
#else
  uint64_t score( TT const& func, TT const& care )
  {
    return kitty::count_ones( func & care );
  }
#endif

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
    return ( id - divisors.size() ) / 2;
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

  uint32_t max_i, max_j, max_k;

  std::vector<TT> divisors;
  std::vector<uint64_t> scores;
  std::vector<maj_node> maj_nodes;

  /* pairs of (maj_nodes index, fanin number) */
  std::vector<expansion_position> leaves; // can convert to a queue
}; /* mig_resub_engine */

} /* namespace mockturtle */
