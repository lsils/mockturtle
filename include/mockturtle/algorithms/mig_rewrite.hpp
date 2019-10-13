/*!
 \file mig_rewrite.hpp
 \brief MIG transformation rules
 
 \author Dewmini Sudara
 */

#pragma once

#include <map>
#include <vector>

#include "fmt/format.h"
#include "kitty/dynamic_truth_table.hpp"
#include "mockturtle/algorithms/simulation.hpp"
#include "mockturtle/generators/majority.hpp"
#include "mockturtle/networks/mig.hpp"

namespace mockturtle
{

/*! \brief Compute the truth table for signal, given the truth table map of nodes.
     *
     */
template<typename T, typename Ntk>
inline T truth_table_of( signal<Ntk> x, const node_map<T, Ntk>& tt )
{
  return x.complement ? ~tt[x.index] : tt[x.index];
}

/*! \brief Get a list of primary inputs in a given ntk.
     *
     */
template<typename Ntk>
std::vector<node<Ntk>> get_pis( const Ntk& ntk )
{
  std::vector<node<Ntk>> pis;
  ntk.foreach_pi( [&pis]( auto const& f ) {
    pis.push_back( f );
  } );
  return pis;
}

/*! \brief Get a list of primary outputs in a given ntk.
     *
     */
template<typename Ntk>
std::vector<signal<Ntk>> get_pos( const Ntk& ntk )
{
  std::vector<signal<Ntk>> pos;
  ntk.foreach_po( [&pos]( auto const& f ) {
    pos.push_back( f );
  } );
  return pos;
}

/*! \brief Get a list of children of a given node in a given ntk.
     *
     */
template<typename Ntk>
std::vector<signal<Ntk>> get_children( const Ntk& ntk, node<Ntk> parent )
{
  std::vector<signal<Ntk>> children;
  ntk.foreach_fanin( parent, [&children]( auto const& f ) {
    children.push_back( f );
  } );
  return children;
}

namespace detail
{

template<typename Ntk, typename T>
signal<Ntk> replace_in_subgraph_rec( Ntk& ntk, signal<Ntk> root_sig, signal<Ntk> old_sig, signal<Ntk> new_sig, T& cache )
{
  if ( root_sig == old_sig )
    return new_sig;
  if ( root_sig == !old_sig )
    return !new_sig;

  auto root_node = ntk.get_node( root_sig );
  // find the new signal for root node
  if ( cache.count( root_node ) == 0 )
  {
    signal<Ntk> result;
    if ( ntk.is_constant( root_node ) || ntk.is_pi( root_node ) )
      result = ntk.make_signal( root_node );
    else
    {
      auto c = get_children( ntk, root_node );
      result = ntk.create_maj(
          replace_in_subgraph_rec( ntk, c[0], old_sig, new_sig, cache ),
          replace_in_subgraph_rec( ntk, c[1], old_sig, new_sig, cache ),
          replace_in_subgraph_rec( ntk, c[2], old_sig, new_sig, cache ) );
    }
    cache[root_node] = result;
  }
  return ntk.is_complemented( root_sig ) ? !cache[root_node] : cache[root_node];
}
} // namespace detail

/*! \brief Construct a new subgraph for root_sig by replacing all occurrences of old_sig by new_sig.
     *
     */
template<typename Ntk>
signal<Ntk> replace_in_subgraph( Ntk& ntk, signal<Ntk> root_sig, signal<Ntk> old_sig, signal<Ntk> new_sig )
{
  std::map<node<Ntk>, signal<Ntk>> cache;
  return detail::replace_in_subgraph_rec( ntk, root_sig, old_sig, new_sig, cache );
}

/* distributivity */
template<typename Ntk>
struct distributivity
{
  /**
         * dir = FWD means <xu<yvz>> => <<xuy>v<xuz>>
         * dir = BWD means <<xuy>v<xuz>> => <xu<yvz>>
         */

  enum class direction
  {
    FWD,
    BWD
  };
  node<Ntk> n;
  signal<Ntk> x, u, y, v, z;
  direction dir;

  distributivity( node<Ntk> n, signal<Ntk> x, signal<Ntk> u, signal<Ntk> y, signal<Ntk> v, signal<Ntk> z, direction dir ) : n( n ), x( x ), u( u ), y( y ), v( v ), z( z ), dir( dir )
  {
  }

  signal<Ntk> apply_to( Ntk& ntk )
  {
    if ( dir == direction::FWD )
    {
      return ntk.create_maj( ntk.create_maj( x, u, y ), v, ntk.create_maj( x, u, z ) );
    }
    else
    {
      return ntk.create_maj( x, u, ntk.create_maj( y, v, z ) );
    }
  }
};

namespace detail
{

template<typename Ntk>
void get_bwd_distributivities( const Ntk& ntk, node<Ntk> n, signal<Ntk> v_sig, signal<Ntk> xuy_sig, signal<Ntk> xuz_sig, std::vector<distributivity<Ntk>>& result )
{
  auto xuy = ntk.get_node( xuy_sig );
  auto xuz = ntk.get_node( xuz_sig );
  if ( ntk.is_maj( xuy ) && ntk.is_maj( xuz ) )
  {
    auto xuy_c = get_children( ntk, xuy );
    auto xuz_c = get_children( ntk, xuz );
    for ( auto i = 0; i < 3; i++ )
    {
      xuy_c[i] = xuy_c[i] ^ xuy_sig.complement;
      xuz_c[i] = xuz_c[i] ^ xuz_sig.complement;
    }
    for ( auto i = 0; i < 3; i++ )
    {
      auto x_sig = xuy_c[i];
      auto u_sig = xuy_c[( i + 1 ) % 3];
      auto y_sig = xuy_c[( i + 2 ) % 3];
      for ( auto j = 0; j < 3; j++ )
      {
        for ( auto k = 0; k < 3; k++ )
        {
          if ( j != k && xuz_c[j] == x_sig && xuz_c[k] == u_sig )
          {
            auto z_sig = xuz_c[3 - j - k];
            result.push_back( distributivity<Ntk>( n, x_sig, u_sig, y_sig, v_sig, z_sig, distributivity<Ntk>::direction::BWD ) );
          }
        }
      }
    }
  }
}
} // namespace detail

/*! \brief Find and return all possible backward distributivities for a given node n in network ntk.
     *
     */
template<typename Ntk>
std::vector<distributivity<Ntk>> get_bwd_distributivities( const Ntk& ntk, node<Ntk> n )
{
  std::vector<distributivity<Ntk>> res;
  if ( ntk.is_maj( n ) )
  {
    auto c = get_children( ntk, n );
    // for all cyclic permutations
    for ( auto i = 0; i < 3; i++ )
    {
      detail::get_bwd_distributivities( ntk, n, c[i], c[( i + 1 ) % 3], c[( i + 2 ) % 3], res );
    }
  }
  return res;
}

namespace detail
{

template<typename Ntk>
void get_fwd_distributivities( const Ntk& ntk, node<Ntk> n, signal<Ntk> x_sig, signal<Ntk> u_sig, signal<Ntk> yvz_sig, std::vector<distributivity<Ntk>>& result )
{
  node<Ntk> yvz = ntk.get_node( yvz_sig );
  if ( ntk.is_maj( yvz ) )
  {
    auto yvz_c = get_children( ntk, yvz );
    for ( auto i = 0; i < 3; i++ )
    {
      yvz_c[i] = yvz_c[i] ^ yvz_sig.complement;
    }
    for ( auto i = 0; i < 3; i++ )
    {
      result.push_back( distributivity<Ntk>( n, x_sig, u_sig, yvz_c[i], yvz_c[( i + 1 ) % 3], yvz_c[( i + 2 ) % 3], distributivity<Ntk>::direction::FWD ) );
    }
  }
}
} // namespace detail

/*! \brief Find and return all possible forward distributivities for a given node n in network ntk.
     *
     */
template<typename Ntk>
std::vector<distributivity<Ntk>> get_fwd_distributivities( const Ntk& ntk, node<Ntk> n )
{
  std::vector<distributivity<Ntk>> res;
  if ( ntk.is_maj( n ) )
  {
    std::vector<signal<Ntk>> c = get_children( ntk, n );
    // for all cyclic permutations
    for ( auto i = 0; i < 3; i++ )
    {
      detail::get_fwd_distributivities( ntk, n, c[i], c[( i + 1 ) % 3], c[( i + 2 ) % 3], res );
    }
  }
  return res;
}

/* associativity */
template<typename Ntk>
struct associativity
{
  /*
         * source structure n = <xu<yuz>>
         * target structure <<xuy>uz>
         */
  node<Ntk> n;
  signal<Ntk> x, u, y, z;

  associativity( node<Ntk> n, signal<Ntk> x, signal<Ntk> u, signal<Ntk> y, signal<Ntk> z ) : n( n ), x( x ), u( u ), y( y ), z( z )
  {
  }

  signal<Ntk> apply_to( Ntk& ntk )
  {
    return ntk.create_maj( ntk.create_maj( x, u, y ), u, z );
  }
};

namespace detail
{

template<typename Ntk>
void get_associativities( const Ntk& ntk, node<Ntk> n, signal<Ntk> x_sig, signal<Ntk> u_sig, signal<Ntk> yuz_sig, std::vector<associativity<Ntk>>& result )
{
  node<Ntk> yuz = ntk.get_node( yuz_sig );
  if ( ntk.is_maj( yuz ) )
  {
    // write yuz_sig as <y, u, z> (propagate inverters if necessary)
    auto c = get_children( ntk, yuz );
    if ( ntk.is_complemented( yuz_sig ) )
    {
      for ( auto i = 0; i < 3; i++ )
      {
        c[i] = !c[i];
      }
    }
    // find if u_sig is in yuz, otherwise no structural match
    for ( auto i = 0; i < 3; i++ )
    {
      if ( c[i] == u_sig )
      {
        // treat yuz[(i+1)%3] as y and yuz[(i+2)%3] as z
        result.push_back( associativity<Ntk>( n, x_sig, c[i], c[( i + 1 ) % 3], c[( i + 2 ) % 3] ) );
        // treat yuz[(i+2)%3] as y and yuz[(i+1)%3] as z
        result.push_back( associativity<Ntk>( n, x_sig, c[i], c[( i + 2 ) % 3], c[( i + 1 ) % 3] ) );
      }
    }
  }
}
} // namespace detail

/*! \brief Find and return all possible associativities for a given node n in network ntk.
     *
     */
template<typename Ntk>
std::vector<associativity<Ntk>> get_associativities( const Ntk& ntk, const node<Ntk> n )
{
  std::vector<associativity<Ntk>> res;
  if ( ntk.is_maj( n ) )
  {
    auto c = get_children( ntk, n );
    uint64_t ind[3] = {0, 1, 2};
    // for all permutations
    do
    {
      detail::get_associativities( ntk, n, c[ind[0]], c[ind[1]], c[ind[2]], res );
    } while ( std::next_permutation( ind, ind + 3 ) );
  }
  return res;
}

/* Relevance */

template<typename Ntk>
struct relevance
{
  /**
         * source n = <x,y,z>, target n = <x_{y/z'}, y, z>
         */

  node<Ntk> n;
  signal<Ntk> x, y, z;

  relevance( node<Ntk> n, signal<Ntk> x, signal<Ntk> y, signal<Ntk> z ) : n( n ), x( x ), y( y ), z( z )
  {
  }

  signal<Ntk> apply_to( Ntk& ntk )
  {
    return ntk.create_maj( replace_in_subgraph( ntk, x, y, !z ), y, z );
  }
};

template<typename Ntk>
std::vector<relevance<Ntk>> get_fwd_comp_assocs( const Ntk& ntk, node<Ntk> n )
{
  std::vector<relevance<Ntk>> result;
  if ( ntk.is_maj( n ) )
  {
    auto c = get_children( ntk, n );
    uint64_t ind[3] = {0, 1, 2};
    // for all permutations
    do
    {
      auto& x = c[ind[0]];
      auto x_node = ntk.get_node( x );
      if ( ntk.is_maj( x_node ) )
      {
        auto& y = c[ind[1]];
        auto& z = c[ind[2]];
        auto xc = get_children( ntk, x_node );
        for ( auto i = 0; i < 3; i++ )
        {
          if ( ( xc[i] ^ x.complement ) == y )
          {
            result.push_back( relevance<Ntk>( n, x, y, z ) );
            break;
          }
        }
      }
    } while ( std::next_permutation( ind, ind + 3 ) );
  }
  return result;
}

template<typename Ntk>
std::vector<relevance<Ntk>> get_bwd_comp_assocs( const Ntk& ntk, node<Ntk> n )
{
  std::vector<relevance<Ntk>> result;
  if ( ntk.is_maj( n ) )
  {
    auto c = get_children( ntk, n );
    uint64_t ind[3] = {0, 1, 2};
    // for all permutations
    do
    {
      auto& x = c[ind[0]];
      auto x_node = ntk.get_node( x );
      if ( ntk.is_maj( x_node ) )
      {
        auto& y = c[ind[1]];
        auto& z = c[ind[2]];
        auto xc = get_children( ntk, x_node );
        for ( auto i = 0; i < 3; i++ )
        {
          if ( ( xc[i] ^ x.complement ) == !y )
          {
            result.push_back( relevance<Ntk>( n, x, y, z ) );
            break;
          }
        }
      }
    } while ( std::next_permutation( ind, ind + 3 ) );
  }
  return result;
}

/*! \brief Find all possible relevance rule applications for a given node n in ntk.
     *
     */
template<typename Ntk>
std::vector<relevance<Ntk>> get_relevances( const Ntk& ntk, node<Ntk> n )
{
  std::vector<relevance<Ntk>> result;
  if ( ntk.is_maj( n ) )
  {
    auto c = get_children( ntk, n );
    uint64_t ind[3] = {0, 1, 2};
    // for all permutations
    do
    {
      if ( ntk.is_maj( ntk.get_node( c[ind[0]] ) ) )
      {
        result.push_back( relevance<Ntk>( n, c[ind[0]], c[ind[1]], c[ind[2]] ) );
      }
    } while ( std::next_permutation( ind, ind + 3 ) );
  }
  return result;
}

/* swapping */
template<typename Ntk>
struct swapping
{
  // n = <x <y v1 v2> <y w1 w2>>
  node<Ntk> n;
  signal<Ntk> x, y, v1, v2, w1, w2;

  swapping( node<Ntk> n, signal<Ntk> x, signal<Ntk> y, signal<Ntk> v1, signal<Ntk> v2, signal<Ntk> w1, signal<Ntk> w2 ) : n( n ), x( x ), y( y ), v1( v1 ), v2( v2 ), w1( w1 ), w2( w2 )
  {
  }

  signal<Ntk> apply_to( Ntk& ntk )
  {
    return ntk.create_maj( x, ntk.create_maj( y, w1, v2 ), ntk.create_maj( y, v1, w2 ) );
  }
};

template<typename Ntk>
bool is_swapping_possible( signal<Ntk> v1, signal<Ntk> w1, signal<Ntk> v2, signal<Ntk> w2, const node_map<kitty::dynamic_truth_table, Ntk>& tts )
{
  return kitty::is_const0( ( truth_table_of( v1, tts ) ^ truth_table_of( v2, tts ) ) & ( truth_table_of( w1, tts ) ^ truth_table_of( w2, tts ) ) );
}

namespace detail
{

template<typename Ntk>
void get_swappings( const Ntk& ntk, node<Ntk> n, signal<Ntk> x_sig, signal<Ntk> a_sig, signal<Ntk> b_sig, std::vector<swapping<Ntk>>& result, const node_map<kitty::dynamic_truth_table, Ntk>& tts )
{
  auto ac = get_children( ntk, ntk.get_node( a_sig ) );
  if ( ntk.is_complemented( a_sig ) )
  {
    for ( auto i = 0; i < 3; i++ )
    {
      ac[i] = !ac[i];
    }
  }
  auto bc = get_children( ntk, ntk.get_node( b_sig ) );
  if ( ntk.is_complemented( b_sig ) )
  {
    for ( auto i = 0; i < 3; i++ )
    {
      bc[i] = !bc[i];
    }
  }
  for ( auto i = 0; i < 3; i++ )
  {
    for ( auto j = 0; j < 3; j++ )
    {
      if ( ac[i] != bc[j] )
        continue;
      auto y_sig = ac[i];
      for ( auto v1 = 0; v1 < 3; v1++ )
      {
        if ( v1 == i )
          continue;
        auto w1 = 3 - i - v1;
        for ( auto v2 = 0; v2 < 3; v2++ )
        {
          if ( v2 == j )
            continue;
          auto w2 = 3 - j - v2;
          if ( is_swapping_possible( ac[v1], ac[w1], bc[v2], bc[w2], tts ) )
          {
            result.push_back( swapping<Ntk>( n, x_sig, y_sig, ac[v1], ac[w1], bc[v2], bc[w2] ) );
          }
        }
      }
    }
  }
}
} // namespace detail

/*! \brief Find and return all possible swappings for a given node n in  network ntk.
     *
     */
template<typename Ntk>
std::vector<swapping<Ntk>> get_swappings( const Ntk& ntk, const node<Ntk> n )
{
  default_simulator<kitty::dynamic_truth_table> sim( ntk.num_pis() );
  const auto tts = simulate_nodes<kitty::dynamic_truth_table>( ntk, sim );
  std::vector<swapping<Ntk>> res;
  if ( ntk.is_maj( n ) )
  {
    auto c = get_children( ntk, n );
    for ( auto i = 0; i < 3; i++ )
    {
      auto x = c[i];
      auto t1 = c[( i + 1 ) % 3];
      auto t2 = c[( i + 2 ) % 3];
      if ( ntk.is_maj( ntk.get_node( t1 ) ) && ntk.is_maj( ntk.get_node( t2 ) ) )
      {
        detail::get_swappings( ntk, n, x, t1, t2, res, tts );
      }
    }
  }
  return res;
}

/* Symmetry */
template<typename Ntk>
struct symmetry
{
  node<Ntk> n;
  signal<Ntk> x, y, z;
  signal<Ntk> replacee, replacer;

  symmetry( node<Ntk> n, signal<Ntk> x, signal<Ntk> y, signal<Ntk> z, signal<Ntk> replacee ) : n( n ), x( x ), y( y ), z( z ), replacee( replacee )
  {
  }

  symmetry( node<Ntk> n, signal<Ntk> x, signal<Ntk> y, signal<Ntk> z, signal<Ntk> replacee, signal<Ntk> replacer ) : n( n ), x( x ), y( y ), z( z ), replacee( replacee ), replacer( replacer )
  {
  }

  symmetry<Ntk> make_copy_for_replacer( signal<Ntk> replacer )
  {
    return symmetry( n, x, y, z, replacee, replacer );
  }

  signal<Ntk> apply_to( Ntk& ntk )
  {
    auto new_y = replace_in_subgraph( ntk, y, replacee, replacer );
    auto new_z = replace_in_subgraph( ntk, z, replacee, replacer );
    return ntk.create_maj( x, new_y, new_z );
  }
};

template<typename Ntk>
bool is_u_and_v_similar_wrt_candidate( const Ntk& ntk, signal<Ntk> u_sig, signal<Ntk> v_sig, node<Ntk> candidate )
{
  auto u = ntk.get_node( u_sig );
  auto v = ntk.get_node( v_sig );

  // trivially similar
  if ( u == candidate || v == candidate )
  {
    return u_sig == !v_sig;
  }
  if ( !ntk.is_maj( u ) && !ntk.is_maj( v ) )
  {
    return ( u_sig == v_sig );
  }

  // if one of the node is not a majority there is no structural match
  if ( !ntk.is_maj( u ) || !ntk.is_maj( v ) )
    return false;

  // now only the majority gates are considered in the following
  auto uc = get_children( ntk, u );
  auto vc = get_children( ntk, v );
  for ( int i = 0; i < 3; i++ )
  {
    uc[i] = uc[i] ^ u_sig.complement;
    vc[i] = vc[i] ^ v_sig.complement;
  }

  bool stat[3][3];
  for ( int i = 0; i < 3; i++ )
  {
    for ( int j = 0; j < 3; j++ )
    {
      stat[i][j] = is_u_and_v_similar_wrt_candidate( ntk, uc[i], vc[j], candidate );
    }
  }
  if ( stat[0][0] && stat[1][1] && stat[2][2] )
    return true;
  if ( stat[0][0] && stat[1][2] && stat[2][1] )
    return true;
  if ( stat[0][1] && stat[1][0] && stat[2][2] )
    return true;
  if ( stat[0][1] && stat[1][2] && stat[2][0] )
    return true;
  if ( stat[0][2] && stat[1][0] && stat[2][1] )
    return true;
  if ( stat[0][2] && stat[1][1] && stat[2][0] )
    return true;
  return false;
}

template<typename Ntk>
std::vector<node<Ntk>> get_replacee_candidate_nodes( const Ntk& ntk, signal<Ntk> u, signal<Ntk> v )
{
  std::vector<node<Ntk>> result;
  ntk.foreach_node( [&]( auto n ) {
    if ( is_u_and_v_similar_wrt_candidate( ntk, u, v, n ) )
    {
      result.push_back( n );
    }
  } );
  return result;
}

template<typename Ntk>
std::vector<node<Ntk>> get_replacee_candidate_pis( const Ntk& ntk, signal<Ntk> u, signal<Ntk> v )
{
  default_simulator<kitty::dynamic_truth_table> sim( ntk.num_pis() );
  const auto tts = simulate_nodes<kitty::dynamic_truth_table>( ntk, sim );
  kitty::dynamic_truth_table utt = truth_table_of( u, tts );
  kitty::dynamic_truth_table vtt = truth_table_of( v, tts );
  std::vector<node<Ntk>> result;
  if ( is_u_and_v_similar_wrt_candidate( ntk, u, v, ntk.get_node( ntk.get_constant( false ) ) ) )
  {
    result.push_back( ntk.get_node( ntk.get_constant( false ) ) );
  }
  auto pis = get_pis( ntk );
  for ( auto i = 0u; i < pis.size(); i++ )
  {
    auto uttc0 = kitty::cofactor0( utt, i );
    auto uttc1 = kitty::cofactor1( utt, i );
    auto vttc0 = kitty::cofactor0( vtt, i );
    auto vttc1 = kitty::cofactor1( vtt, i );
    if ( uttc0 == vttc1 && uttc1 == vttc0 )
    {
      result.push_back( pis[i] );
    }
  }
  return result;
}

template<typename Ntk>
void find_suitable_replacer( const Ntk& ntk, const std::vector<node<Ntk>>& replacee_list, signal<Ntk> root, std::map<std::pair<node<Ntk>, uint64_t>, uint64_t>& ht, std::unordered_map<node<Ntk>, bool>& visited )
{
  node<Ntk> root_node = ntk.get_node( root );

  if ( !ntk.is_maj( root_node ) || visited[root_node] )
    return;

  visited[root_node] = true;

  auto c = get_children( ntk, root_node );
  for ( auto& replacee : replacee_list )
  {
    for ( auto i = 0; i < 3; i++ )
    {
      if ( ntk.get_node( c[i] ) == replacee )
      {
        for ( auto j = 0; j < 3; j++ )
        {
          if ( j != i )
          {
            std::pair<node<Ntk>, uint64_t> temp = std::make_pair( replacee, c[j].data );
            ht[temp]++;
          }
        }
      }
    }
  }
  for ( int i = 0; i < 3; i++ )
  {
    find_suitable_replacer( ntk, replacee_list, c[i], ht, visited );
  }
}

template<typename Ntk>
std::vector<std::pair<node<Ntk>, signal<Ntk>>> find_suitable_replacee_replacer_pairs( const Ntk& ntk, const std::vector<node<Ntk>>& replacee_list, signal<Ntk> root_r )
{
  std::map<std::pair<node<Ntk>, uint64_t>, uint64_t> ht;
  std::unordered_map<node<Ntk>, bool> visited;

  find_suitable_replacer( ntk, replacee_list, root_r, ht, visited );

  unsigned long max = 0;
  std::vector<std::pair<node<Ntk>, signal<Ntk>>> result;
  for ( const auto& [key, value] : ht )
  {
    if ( value > max )
    {
      result.clear();
      max = value;
    }
    if ( value == max )
    {
      result.emplace_back( key.first, signal<Ntk>( key.second ) );
    }
  }
  return result;
}

/*! \brief Find and return all possible symmetries for a given node n in  network ntk.
     *
     */
template<typename Ntk>
std::vector<symmetry<Ntk>> get_symmetries( const Ntk& ntk, node<Ntk> n )
{
  std::vector<symmetry<Ntk>> result;
  if ( ntk.is_maj( n ) )
  {
    auto c = get_children( ntk, n );
    for ( auto i = 0; i < 3; i++ )
    {
      signal<Ntk> x = c[i];
      signal<Ntk> y = c[( i + 1 ) % 3];
      signal<Ntk> z = c[( i + 2 ) % 3];
      auto possible_replacees = get_replacee_candidate_pis( ntk, y, z );
      auto pairs = find_suitable_replacee_replacer_pairs( ntk, possible_replacees, y );
      for ( auto& pair : pairs )
      {
        result.push_back( symmetry<Ntk>( n, x, y, z, ntk.make_signal( pair.first ), pair.second ) );
      }
    }
  }
  return result;
}

/* majority-n opt substituition */
template<typename Ntk>
struct majn_substitution
{
  node<Ntk> n;
  std::vector<signal<Ntk>> signals;

  majn_substitution( node<Ntk> n, std::vector<signal<Ntk>> signals ) : n( n ), signals( signals )
  {
  }
};

/*! \brief Returns whether at least thresh bits out of the least significant num_vars bits of input_bits are high.
     *
     */
bool threshold( unsigned long input_bits, unsigned long thresh, unsigned long num_vars )
{
  unsigned long count = 0;
  for ( auto j = 0u; j < num_vars; j++ )
  {
    if ( input_bits & ( 1 << j ) )
      count++;
  }
  return ( count > thresh );
}

/*! \brief Check whether a given truth table represents the majority-n of some subset of inputs (or their complements)
     *
     */
template<typename Ntk>
std::vector<signal<Ntk>> maj_of( const kitty::dynamic_truth_table& tt )
{
  std::vector<signal<Ntk>> result;
  std::vector<signal<Ntk>> empty;
  unsigned long mask_cmp_vars = 0;
  unsigned long mask_dep_vars = 0;
  for ( int i = 0; i < tt.num_vars(); i++ )
  {
    if ( kitty::has_var( tt, i ) )
    {
      mask_dep_vars |= ( 1 << i );
      kitty::dynamic_truth_table c0 = kitty::cofactor0( tt, i );
      kitty::dynamic_truth_table c1 = kitty::cofactor1( tt, i );
      if ( kitty::implies( c0, c1 ) )
      {
        result.push_back( signal<Ntk>( i + 1, 0 ) );
      }
      else if ( kitty::implies( c1, c0 ) )
      {
        result.push_back( signal<Ntk>( i + 1, 1 ) );
        mask_cmp_vars |= ( 1 << i );
      }
      else
      {
        return empty;
      }
    }
  }
  for ( unsigned long i = 0; i < tt.num_bits(); i++ )
  {
    if ( kitty::get_bit( tt, i ) != threshold( ( i & mask_dep_vars ) ^ mask_cmp_vars, result.size() / 2, tt.num_vars() ) )
    {
      return empty;
    }
  }
  return result;
}

template<typename Ntk>
signal<Ntk> relabel_recursively( Ntk& ntk, const Ntk& opt_ntk, signal<Ntk> opt_root, const std::vector<signal<Ntk>>& sigs )
{
  signal<Ntk> result;
  // get the node for considered signal of the optimum network
  node<Ntk> opt_root_node = opt_ntk.get_node( opt_root );
  if ( opt_ntk.is_constant( opt_root_node ) )
  {
    result = ntk.make_signal( opt_root_node );
  }
  else if ( opt_ntk.is_pi( opt_root_node ) )
  {
    result = sigs[opt_root_node - 1];
  }
  else
  {
    // If the node is already explored, it is not cached in the current implementation.
    // Caching can be implemented to avoid repetitive calls.
    std::vector<signal<Ntk>> c = get_children( opt_ntk, opt_root_node );
    result = ntk.create_maj(
        relabel_recursively( ntk, opt_ntk, c[0], sigs ),
        relabel_recursively( ntk, opt_ntk, c[1], sigs ),
        relabel_recursively( ntk, opt_ntk, c[2], sigs ) );
  }
  // if opt root is complemented, complement the result before returning
  return result ^ opt_root.complement;
}

/*! \brief Replaces majority-n with optimum structure.
     *
     */
template<typename Ntk>
signal<Ntk> replace_with_opt_maj_n( Ntk& ntk, node<Ntk> node, const std::vector<signal<Ntk>>& sigs )
{
  Ntk opt_ntk;
  if ( sigs.size() == 5u )
  {
    std::array<signal<Ntk>, 5> pis;
    for ( uint64_t i = 0; i < 5; i++ )
      pis[i] = opt_ntk.create_pi();
    opt_ntk.create_po( majority5( opt_ntk, pis ) );
  }
  else if ( sigs.size() == 7u )
  {
    std::array<signal<Ntk>, 7> pis;
    for ( uint64_t i = 0; i < 7; i++ )
      pis[i] = opt_ntk.create_pi();
    opt_ntk.create_po( majority7( opt_ntk, pis ) );
  }
  else if ( sigs.size() == 9u )
  {
    std::array<signal<Ntk>, 9> pis;
    for ( uint64_t i = 0; i < 9; i++ )
      pis[i] = opt_ntk.create_pi();
    opt_ntk.create_po( majority9_12( opt_ntk, pis ) );
  }
  else
  {
    return ntk.make_signal( node );
  }
  return relabel_recursively( ntk, opt_ntk, get_pos( opt_ntk )[0], sigs );
}

/*! \brief Replaces majority-n nodes with their optimum structures in the subgraph of root.
     *
     */
template<typename Ntk>
signal<Ntk> replace_with_opt_maj_n( Ntk& ntk, signal<Ntk> root, const std::map<node<Ntk>, std::vector<signal<Ntk>>>& subs )
{
  node<Ntk> root_node = ntk.get_node( root );
  signal<Ntk> result;
  if ( ntk.is_constant( root_node ) )
  {
    result = ntk.make_signal( root_node );
  }
  else if ( ntk.is_pi( root_node ) )
  {
    result = ntk.make_signal( root_node );
  }
  else
  {
    if ( subs.count( root_node ) > 0 )
    {
      result = replace_with_opt_maj_n( ntk, root_node, subs.at( root_node ) );
    }
    else
    {
      std::vector<signal<Ntk>> c = get_children( ntk, root_node );
      result = ntk.create_maj(
          replace_with_opt_maj_n( ntk, c[0], subs ),
          replace_with_opt_maj_n( ntk, c[1], subs ),
          replace_with_opt_maj_n( ntk, c[2], subs ) );
    }
  }
  return result ^ root.complement;
}

/*! \brief Find majority-n substructures in ntk and replace them with the optimum majority-n network.
     *
     */
template<typename Ntk>
signal<Ntk> substitute_maj_n( Ntk& ntk, node<Ntk> root_node, uint64_t num_vars )
{
  if ( ( num_vars != 5u ) && ( num_vars != 7u ) && ( num_vars != 9u ) )
  {
    return ntk.make_signal( root_node );
  }

  std::vector<majn_substitution<Ntk>> subs;
  default_simulator<kitty::dynamic_truth_table> sim( ntk.num_pis() );
  node_map<kitty::dynamic_truth_table, Ntk> ttmap = simulate_nodes<kitty::dynamic_truth_table>( ntk, sim );

  // find all majority-n nodes
  ntk.foreach_node( [&]( auto n ) {
    if ( ntk.is_maj( n ) )
    {
      std::vector<signal<Ntk>> result = maj_of<Ntk>( ttmap[n] );
      if ( result.size() == num_vars )
      {
        subs.push_back( majn_substitution<Ntk>( n, result ) );
      }
    }
  } );

  if ( subs.size() > 1 )
  {
    // find a variable in first mig that is not in second mig, move it to first place
    for ( auto i = 0u; i < num_vars; i++ )
    {
      bool found = false;
      for ( auto j = 0u; j < num_vars; j++ )
      {
        if ( subs[0].signals[i] == subs[1].signals[j] )
        {
          found = true;
          break;
        }
      }
      if ( !found )
      {
        for ( auto j = i; j > 0; j-- )
        {
          std::swap( subs[0].signals[j], subs[0].signals[j - 1] );
        }
        break;
      }
    }
    // find a variable in second mig that is not in first mig, move it to first place
    for ( auto i = 0u; i < num_vars; i++ )
    {
      bool found = false;
      for ( auto j = 0u; j < num_vars; j++ )
      {
        if ( subs[1].signals[i] == subs[0].signals[j] )
        {
          found = true;
          break;
        }
      }
      if ( !found )
      {
        for ( auto j = i; j > 0; j-- )
        {
          std::swap( subs[1].signals[j], subs[1].signals[j - 1] );
        }
        break;
      }
    }
  }

  std::map<node<Ntk>, std::vector<signal<Ntk>>> subs_map;
  for ( auto i = 0u; i < subs.size(); i++ )
  {
    subs_map[subs[i].n] = subs[i].signals;
  }
  return replace_with_opt_maj_n( ntk, ntk.make_signal( root_node ), subs_map );
}

} /* namespace mockturtle */
