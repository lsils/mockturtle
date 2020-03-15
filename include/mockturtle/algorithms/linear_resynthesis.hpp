/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
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
  \file linear_resynthesis.hpp
  \brief Resynthesize linear circuit

  \author Mathias Soeken
*/

#pragma once

#include <iostream>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../algorithms/cnf.hpp"
#include "../algorithms/simulation.hpp"
#include "../networks/xag.hpp"
#include "../utils/stopwatch.hpp"
#include "../traits.hpp"

#include <fmt/format.h>
#include <percy/solvers/bsat2.hpp>

namespace mockturtle
{

namespace detail
{

class linear_sum_simulator
{
public:
  std::vector<uint32_t> compute_constant( bool ) const { return {}; }
  std::vector<uint32_t> compute_pi( uint32_t index ) const { return {index}; }
  std::vector<uint32_t> compute_not( std::vector<uint32_t> const& value ) const
  {
    assert( false && "No NOTs in linear forms allowed" );
    std::abort();
    return value;
  }
};

class linear_matrix_simulator
{
public:
  linear_matrix_simulator( uint32_t num_inputs ) : num_inputs_( num_inputs ) {}

  std::vector<bool> compute_constant( bool ) const { return std::vector<bool>( num_inputs_, false ); }
  std::vector<bool> compute_pi( uint32_t index ) const
  {
    std::vector<bool> row( num_inputs_, false );
    row[index] = true;
    return row;
  }
  std::vector<bool> compute_not( std::vector<bool> const& value ) const
  {
    assert( false && "No NOTs in linear forms allowed" );
    std::abort();
    return value;
  }

private:
  uint32_t num_inputs_;
};

class linear_xag : public xag_network
{
public:
  linear_xag( xag_network const& xag ) : xag_network( xag ) {}

  template<typename Iterator>
  iterates_over_t<Iterator, std::vector<uint32_t>>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    (void)end;

    assert( n != 0 && !is_pi( n ) );

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];

    auto set1 = *begin++;
    auto set2 = *begin++;

    if ( c1.index < c2.index )
    {
      assert( false );
      std::abort();
      return {};
    }
    else
    {
      std::vector<uint32_t> result;
      auto it1 = set1.begin();
      auto it2 = set2.begin();

      while ( it1 != set1.end() && it2 != set2.end() )
      {
        if ( *it1 < *it2 )
        {
          result.push_back( *it1++ );
        }
        else if ( *it1 > *it2 )
        {
          result.push_back( *it2++ );
        }
        else
        {
          ++it1;
          ++it2;
        }
      }

      if ( it1 != set1.end() )
      {
        std::copy( it1, set1.end(), std::back_inserter( result ) );
      }
      else if ( it2 != set2.end() )
      {
        std::copy( it2, set2.end(), std::back_inserter( result ) );
      }

      return result;
    }
  }

  template<typename Iterator>
  iterates_over_t<Iterator, std::vector<bool>>
  compute( node const& n, Iterator begin, Iterator end ) const
  {
    (void)end;

    assert( n != 0 && !is_pi( n ) );

    auto const& c1 = _storage->nodes[n].children[0];
    auto const& c2 = _storage->nodes[n].children[1];

    auto set1 = *begin++;
    auto set2 = *begin++;

    if ( c1.index < c2.index )
    {
      assert( false );
      std::abort();
      return {};
    }
    else
    {
      std::vector<bool> result( set1.size() );
      std::transform( set1.begin(), set1.end(), set2.begin(), result.begin(), std::not_equal_to<bool>{} );
      return result;
    }
  }
};

struct pair_hash
{
  template<class T1, class T2>
  std::size_t operator()( std::pair<T1, T2> const& p ) const
  {
    return std::hash<T1>()( p.first ) ^ std::hash<T2>()( p.second );
  }
};

template<class Ntk>
struct linear_resynthesis_paar_impl
{
public:
  using index_pair_t = std::pair<uint32_t, uint32_t>;

  linear_resynthesis_paar_impl( Ntk const& xag ) : xag( xag ) {}

  Ntk run()
  {
    xag.foreach_pi( [&]( auto const& ) {
      signals.push_back( dest.create_pi() );
    } );

    extract_linear_equations();

    while ( !occurrance_to_pairs.empty() )
    {
      const auto p = *( occurrance_to_pairs.back().begin() );
      replace_one_pair( p );
    }

    xag.foreach_po( [&]( auto const& f, auto i ) {
      if ( linear_equations[i].empty() )
      {
        dest.create_po( dest.get_constant( xag.is_complemented( f ) ) );
      }
      else
      {
        assert( linear_equations[i].size() == 1u );
        dest.create_po( signals[linear_equations[i].front()] ^ xag.is_complemented( f ) );
      }
    } );

    return dest;
  }

private:
  void extract_linear_equations()
  {
    occurrance_to_pairs.resize( 1u );

    linear_xag lxag{xag};
    linear_equations = simulate<std::vector<uint32_t>>( lxag, linear_sum_simulator{} );

    for ( auto o = 0u; o < linear_equations.size(); ++o )
    {
      const auto& lin_eq = linear_equations[o];
      for ( auto j = 1u; j < lin_eq.size(); ++j )
      {
        for ( auto i = 0u; i < j; ++i )
        {
          const auto p = std::make_pair( lin_eq[i], lin_eq[j] );
          pairs_to_output[p].push_back( o );
          add_pair( p );
        }
      }
    }
  }

  void add_pair( index_pair_t const& p )
  {
    if ( auto it = pair_to_occurrance.find( p ); it != pair_to_occurrance.end() )
    {
      // found another time
      const auto occ = it->second;
      occurrance_to_pairs[occ - 1u].erase( p );
      if ( occurrance_to_pairs.size() <= occ + 1u )
      {
        occurrance_to_pairs.resize( occ + 1u );
      }
      occurrance_to_pairs[occ].insert( p );
      it->second++;
    }
    else
    {
      // first time found
      pair_to_occurrance[p] = 1u;
      occurrance_to_pairs[0u].insert( p );
    }
  }

  void remove_all_pairs( index_pair_t const& p )
  {
    auto it = pair_to_occurrance.find( p );
    const auto occ = it->second;
    pair_to_occurrance.erase( it );
    occurrance_to_pairs[occ - 1u].erase( p );
    while ( !occurrance_to_pairs.empty() && occurrance_to_pairs.back().empty() )
    {
      occurrance_to_pairs.pop_back();
    }
    pairs_to_output.erase( p );
  }

  void remove_one_pair( index_pair_t const& p, uint32_t output )
  {
    auto it = pair_to_occurrance.find( p );
    const auto occ = it->second;
    occurrance_to_pairs[occ - 1u].erase( p );
    if ( occ > 1u )
    {
      occurrance_to_pairs[occ - 2u].insert( p );
    }
    it->second--;
    pairs_to_output[p].erase( std::remove( pairs_to_output[p].begin(), pairs_to_output[p].end(), output ), pairs_to_output[p].end() );
  }

  void replace_one_pair( index_pair_t const& p )
  {
    const auto [a, b] = p;
    auto c = signals.size();
    signals.push_back( dest.create_xor( signals[a], signals[b] ) );

    /* update data structures */
    for ( auto o : pairs_to_output[p] )
    {
      auto& leq = linear_equations[o];
      leq.erase( std::remove( leq.begin(), leq.end(), a ), leq.end() );
      leq.erase( std::remove( leq.begin(), leq.end(), b ), leq.end() );
      for ( auto i : leq )
      {
        remove_one_pair( {std::min( i, a ), std::max( i, a )}, o );
        remove_one_pair( {std::min( i, b ), std::max( i, b )}, o );
        add_pair( {i, c} );
        pairs_to_output[{i, c}].push_back( o );
      }
      leq.push_back( c );
    }
    remove_all_pairs( p );
  }

  void print_linear_matrix()
  {
    for ( auto const& le : linear_equations )
    {
      auto it = le.begin();
      for ( auto i = 0u; i < signals.size(); ++i )
      {
        if ( it != le.end() && *it == i )
        {
          std::cout << " 1";
          it++;
        }
        else
        {
          std::cout << " 0";
        }
      }
      assert( it == le.end() );
      std::cout << "\n";
    }
  }

private:
  Ntk const& xag;
  Ntk dest;
  std::vector<signal<Ntk>> signals;
  std::vector<std::vector<uint32_t>> linear_equations;
  std::vector<std::unordered_set<index_pair_t, pair_hash>> occurrance_to_pairs;
  std::unordered_map<index_pair_t, uint32_t, pair_hash> pair_to_occurrance;
  std::unordered_map<index_pair_t, std::vector<uint32_t>, pair_hash> pairs_to_output;
};

} // namespace detail

/*! \brief Linear circuit resynthesis (Paar's algorithm)
 *
 * This algorithm works on an XAG that is only composed of XOR gates.  It
 * extracts a matrix representation of the linear output equations and
 * resynthesizes them in a greedy manner by always substituting the most
 * frequent pair of variables using the computed function of an XOR gate.
 *
 * Reference: [C. Paar, IEEE Int'l Symp. on Inf. Theo. (1997), page 250]
 */
template<typename Ntk>
Ntk linear_resynthesis_paar( Ntk const& xag )
{
  static_assert( std::is_same_v<typename Ntk::base_type, xag_network>, "Ntk is not XAG-like" );

  return detail::linear_resynthesis_paar_impl<Ntk>( xag ).run();
}

struct exact_linear_synthesis_params
{
  /*! \brief Upper bound on number of XOR gates. If used, best solution is found decreasing */
  std::optional<uint32_t> upper_bound{};

  /*! \brief Conflict limit for SAT solving (default 0 = no limit). */
  int conflict_limit{0};

  /*! \brief Be verbose. */
  bool verbose{false};

  /*! \brief Be very verbose (debug messages). */
  bool very_verbose{false};
};

struct exact_linear_synthesis_stats
{
  /*! \brief Total time. */
  stopwatch<>::duration time_total{0};

  /*! \brief Time for SAT solving. */
  stopwatch<>::duration time_solving{0};

  /*! \brief Prints report. */
  void report() const
  {
    fmt::print( "[i] total time   = {:>5.2f} secs\n", to_seconds( time_total ) );
    fmt::print( "[i] solving time = {:>5.2f} secs\n", to_seconds( time_solving ) );
  }
};

namespace detail
{

template<class Ntk>
struct exact_linear_synthesis_impl
{
  exact_linear_synthesis_impl( std::vector<std::vector<bool>> const& linear_matrix, exact_linear_synthesis_params const& ps, exact_linear_synthesis_stats& st )
      : ps_( ps ),
        st_( st )
  {
    if ( ps_.very_verbose )
    {
      fmt::print( "[i] input matrix =\n" );
      debug_matrix( linear_matrix );
    }

    /* check matrix for trivial entries */
    for ( auto j = 0u; j < linear_matrix.size(); ++j )
    {
      const auto& row = linear_matrix[j];
      n_ = row.size();

      auto cnt = 0u;
      auto idx = 0u;
      for ( auto i = 0u; i < row.size(); ++i )
      {
        if ( row[i] )
        {
          idx = i;
          if ( ++cnt == 2u )
          {
            break;
          }
        }
      }
      if ( cnt == 0u )
      {
        /* constant 0 is encoded as input n */
        trivial_pos_.emplace_back( j, n_ );
      }
      else if ( cnt == 1u )
      {
        trivial_pos_.emplace_back( j, idx );
      }
      else
      {
        linear_matrix_.push_back( row );
      }
    }

    m_ = linear_matrix_.size();

    if ( ps_.very_verbose )
    {
      fmt::print( "[i] problem matrix =\n" );
      debug_matrix( linear_matrix_ );
      fmt::print( "\n[i] trivial POs =\n" );
      for ( auto const& [j, i] : trivial_pos_ )
      {
        if ( i == n_ )
        {
          fmt::print( "f{} = 0\n", j );
        }
        else
        {
          fmt::print( "f{} = x{}\n", j, i );
        }
      }
    }
  }

  Ntk run()
  {
    return ps_.upper_bound ? run_decreasing() : run_increasing();
  }

private:
  Ntk run_increasing()
  {
    k_ = m_;
    while ( true )
    {
      if ( ps_.verbose )
      {
        fmt::print( "[i] try to find a solution with {} steps, solving time so far = {:.2f} secs\n", k_, to_seconds( st_.time_solving ) );
      }
      percy::bsat_wrapper solver;
      ensure_row_size2( solver );
      ensure_connectivity( solver );
      ensure_outputs( solver );
      const auto res = call_with_stopwatch( st_.time_solving, [&]() { return solver.solve( ps_.conflict_limit ); } );
      if ( res == percy::success )
      {
        if ( ps_.very_verbose )
        {
          debug_solution( solver );
        }
        return extract_solution( solver );
      }
      ++k_;
    }
  }

  Ntk run_decreasing()
  {
    Ntk best;
    k_ = *ps_.upper_bound;
    while ( true )
    {
      if ( ps_.verbose )
      {
        fmt::print( "[i] try to find a solution with {} steps, solving time so far = {:.2f} secs\n", k_, to_seconds( st_.time_solving ) );
      }
      percy::bsat_wrapper solver;
      ensure_row_size2( solver );
      ensure_connectivity( solver );
      ensure_outputs( solver );
      const auto res = call_with_stopwatch( st_.time_solving, [&]() { return solver.solve( ps_.conflict_limit ); } );
      if ( res == percy::success )
      {
        if ( ps_.very_verbose )
        {
          debug_solution( solver );
        }
        best = extract_solution( solver );
        --k_;
      }
      else
      {
        return best;
      }
    }
  }

private:
  // 0 <= i <= k - 1
  // 0 <= j <= n - 1
  uint32_t b( uint32_t i, uint32_t j ) const
  {
    return 1 + i * n_ + j;
  }

  uint32_t num_bs() const
  {
    return k_ * n_;
  }

  // 0 <= i <= k - 1
  // 0 <= p <= i - 1
  uint32_t c( uint32_t i, uint32_t p ) const
  {
    return 1 + num_bs() + ( ( ( i - 1 ) * i ) / 2 ) + p;
  }

  uint32_t num_cs() const
  {
    return ( ( k_ - 1 ) * k_ ) / 2;
  }

  // 0 <= i <= k - 1
  // 0 <= j <= n + i - 1
  uint32_t b_or_c( uint32_t i, uint32_t j ) const
  {
    return j < n_ ? b( i, j ) : c( i, j - n_ );
  }

  // 0 <= l <= m - 1
  // 0 <= i <= k - 1
  uint32_t f( uint32_t l, uint32_t i ) const
  {
    return 1 + num_bs() + num_cs() + l * k_ + i;
  }

  uint32_t num_fs() const
  {
    return k_ * m_;
  }

  // 0 <= j <= n - 1
  // 0 <= i <= k - 1
  uint32_t psi( uint32_t j, uint32_t i ) const
  {
    return j * k_ + i;
  }

  void ensure_row_size2( percy::bsat_wrapper& solver ) const
  {
    for ( auto i = 0u; i < k_; ++i )
    {
      /* at least 2 */
      for ( auto cpl = 0u; cpl <= n_ + i; ++cpl )
      {
        std::vector<uint32_t> lits( n_ + i );
        for ( auto j = 0u; j < n_ + i; ++j )
        {
          lits[j] = make_lit( b_or_c( i, j ), cpl == j );
        }
        solver.add_clause( lits );
      }

      /* at most 2 */
      int plit[3];
      for ( auto j = 2u; j < n_ + i; ++j )
      {
        plit[0] = make_lit( b_or_c( i, j ), true );
        for ( auto jj = 1u; jj < j; ++jj )
        {
          plit[1] = make_lit( b_or_c( i, jj ), true );
          for ( auto jjj = 0u; jjj < jj; ++jjj )
          {
            plit[2] = make_lit( b_or_c( i, jjj ), true );
            solver.add_clause( plit, plit + 3 );
          }
        }
      }
    }
  }

  void ensure_connectivity( percy::bsat_wrapper& solver ) const
  {
    xag_network pntk;
    std::vector<xag_network::signal> nodes( 1 + num_bs() + num_cs() + num_fs() );
    nodes.front() = pntk.get_constant( false );
    std::generate( nodes.begin() + 1u, nodes.end(), [&]() { return pntk.create_pi(); } );

    // psi function
    std::vector<xag_network::signal> psis( k_ * n_ );
    for ( auto i = 0u; i < k_; ++i )
    {
      for ( auto j = 0u; j < n_; ++j )
      {
        std::vector<xag_network::signal> xors( 1 + i );
        auto it = xors.begin();
        *it++ = nodes[b( i, j )];
        for ( auto p = 0u; p < i; ++p )
        {
          *it++ = pntk.create_and( nodes[c( i, p )], psis[psi( j, p )] );
        }
        psis[psi( j, i )] = pntk.create_nary_xor( xors );
      }
    }

    for ( auto l = 0u; l < m_; ++l )
    {
      for ( auto i = 0u; i < k_; ++i )
      {
        std::vector<xag_network::signal> ands( n_ );
        for ( auto j = 0u; j < n_; ++j )
        {
          ands[j] = pntk.create_xnor( psis[psi( j, i )], pntk.get_constant( linear_matrix_[l][j] ) );
        }
        pntk.create_po( pntk.create_or( pntk.create_not( nodes[f( l, i )] ), pntk.create_nary_and( ands ) ) );
      }
    }

    // No two steps are the same
    for ( auto i = 0u; i < k_; ++i )
    {
      for ( auto p = 0u; p < i; ++p )
      {
        std::vector<xag_network::signal> ors( n_ );
        for ( auto j = 0u; j < n_; ++j )
        {
          ors[j] = pntk.create_xor( psis[psi( j, p )], psis[psi( j, i )] );
        }
        pntk.create_po( pntk.create_nary_or( ors ) );
      }
    }

    auto outputs = generate_cnf( pntk, [&]( auto const& clause ) {
      solver.add_clause( clause );
    } );
    for ( int output : outputs )
    {
      solver.add_clause( &output, &output + 1 );
    }

    // at least 2 inputs in each compute form
    //for ( auto i = 0u; i < k_; ++i )
    //{
    //  /* at least 2 */
    //  for ( auto cpl = 0u; cpl <= n_; ++cpl )
    //  {
    //    std::vector<uint32_t> lits( n_ );
    //    for ( auto j = 0u; j < n_; ++j )
    //    {
    //      lits[j] = make_lit( psi( j, i ), cpl == j );
    //    }
    //    solver.add_clause( lits );
    //  }
    //}
  }

  void ensure_outputs( percy::bsat_wrapper& solver ) const
  {
    // each output covers at least one row
    for ( auto l = 0u; l < m_; ++l )
    {
      std::vector<uint32_t> lits( k_ );
      int plit[2];
      for ( auto i = 0u; i < k_; ++i )
      {
        lits[i] = make_lit( f( l, i ) );
        plit[0] = make_lit( f( l, i ), true );
        for ( auto ii = i + 1; ii < k_; ++ii )
        {
          plit[1] = make_lit( f( l, ii ), true );
          solver.add_clause( plit, plit + 2 );
        }
      }
      solver.add_clause( lits );
    }

    // at most one output (if no duplicates) per row
    //for ( auto i = 0u; i < k_; ++i )
    //{
    //  for ( auto l = 1u; l < m_; ++l )
    //  {
    //    int lits[2];
    //    lits[0] = make_lit( f( l, i ), true );
    //    for ( auto ll = 0u; ll < l; ++ll )
    //    {
    //      lits[1] = make_lit( f( ll, i ), true );
    //      solver.add_clause( lits, lits + 2 );
    //    }
    //  }
    //}
  }

  Ntk extract_solution( percy::bsat_wrapper& solver ) const
  {
    Ntk ntk;

    std::vector<signal<Ntk>> nodes( n_ );
    std::generate( nodes.begin(), nodes.end(), [&]() { return ntk.create_pi(); } );

    for ( auto i = 0u; i < k_; ++i )
    {
      std::array<signal<Ntk>, 2> children;
      auto it = children.begin();
      for ( auto j = 0u; j < n_ + i; ++j )
      {
        if ( solver.var_value( b_or_c( i, j ) ) )
        {
          *it++ = nodes[j];
        }
      }
      nodes.push_back( ntk.create_xor( children[0], children[1] ) );
    }

    auto it = trivial_pos_.begin();
    auto poctr = 0u;
    for ( auto l = 0u; l < m_; ++l )
    {
      while ( it != trivial_pos_.end() && it->first == poctr )
      {
        ntk.create_po( it->second == n_ ? ntk.get_constant( false ) : nodes[it->second] );
        poctr++;
        ++it;
      }

      for ( auto i = 0u; i < k_; ++i )
      {
        if ( solver.var_value( f( l, i ) ) )
        {
          ntk.create_po( nodes[n_ + i] );
          poctr++;
          break;
        }
      }
    }

    return ntk;
  }

private:
  void debug_matrix( std::vector<std::vector<bool>> const& matrix ) const
  {
    for ( auto const& row : matrix )
    {
      for ( auto b : row )
      {
        fmt::print( "{}", b ? '1' : '0' );
      }
      fmt::print( "\n" );
    }
  }

  void debug_solution( percy::bsat_wrapper& solver ) const
  {
    for ( auto i = 0u; i < k_; ++i )
    {
      fmt::print( i == 0 ? "B =" : "   " );
      for ( auto j = 0u; j < n_; ++j )
      {
        fmt::print( " {}", solver.var_value( b( i, j ) ) );
      }
      fmt::print( i == 0 ? " C =" : "    " );
      for ( auto p = 0u; p < i; ++p )
      {
        fmt::print( " {}", solver.var_value( c( i, p ) ) );
      }
      fmt::print( std::string( 2 * ( k_ - i ), ' ' ) );
      fmt::print( i == 0u ? " F =" : "    " );
      for ( auto l = 0u; l < m_; ++l )
      {
        fmt::print( " {}", solver.var_value( f( l, i ) ) );
      }
      fmt::print( "\n" );
    }
  }

private:
  uint32_t n_{};
  uint32_t m_{0u};
  uint32_t k_{};
  std::vector<std::vector<bool>> linear_matrix_;
  std::vector<std::pair<uint32_t, uint32_t>> trivial_pos_;
  exact_linear_synthesis_params const& ps_;
  exact_linear_synthesis_stats& st_;
};

} // namespace detail

/*! \brief Extracts linear matrix from XOR-based XAG
 *
 * This algorithm can be used to extract the linear matrix represented by an
 * XAG that only contains XOR gates and no inverters at the outputs.  The matrix
 * can be passed as an argument to `exact_linear_synthesis`.
 */
template<class Ntk>
std::vector<std::vector<bool>> get_linear_matrix( Ntk const& ntk )
{
  static_assert( std::is_same_v<typename Ntk::base_type, xag_network>, "Ntk is not XAG-like" );

  detail::linear_matrix_simulator sim( ntk.num_pis() );
  return simulate<std::vector<bool>>( detail::linear_xag{ntk}, sim );
}

/*! \brief Optimum linear circuit synthesis (based on SAT)
 *
 * This algorithm creates an XAG that is only composed of XOR gates.  It is
 * given as input a linear matrix, represented as vector of bool-vectors.  The
 * size of the outer vector corresponds to the number of outputs, the size of
 * each inner vector must be the same and corresponds to the number of inputs.
 *
 * Reference: [C. Fuhs and P. Schneider-Kamp, SAT (2010), page 71-84]
 */
template<class Ntk>
Ntk exact_linear_synthesis( std::vector<std::vector<bool>> const& linear_matrix, exact_linear_synthesis_params const& ps = {}, exact_linear_synthesis_stats *pst = nullptr )
{
  static_assert( std::is_same_v<typename Ntk::base_type, xag_network>, "Ntk is not XAG-like" );

  exact_linear_synthesis_stats st;
  const auto xag = detail::exact_linear_synthesis_impl<Ntk>{linear_matrix, ps, st}.run();

  if ( ps.verbose )
  {
    st.report();
  }
  if ( pst )
  {
    *pst = st;
  }
  return xag;
}

/*! \brief Optimum linear circuit resynthesis (based on SAT)
 *
 * This algorithm extracts the linear matrix from an XAG that only contains of
 * XOR gates and no inversions and returns a new XAG that has the optimum number
 * of XOR gates to represent the same function.
 *
 * Reference: [C. Fuhs and P. Schneider-Kamp, SAT (2010), page 71-84]
 */
template<typename Ntk>
Ntk exact_linear_resynthesis( Ntk const& ntk, exact_linear_synthesis_params const& ps = {}, exact_linear_synthesis_stats *pst = nullptr )
{
  static_assert( std::is_same_v<typename Ntk::base_type, xag_network>, "Ntk is not XAG-like" );

  const auto linear_matrix = get_linear_matrix( ntk );
  return exact_linear_synthesis<Ntk>( linear_matrix, ps, pst );
}

} /* namespace mockturtle */
