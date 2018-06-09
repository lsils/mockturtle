/* kitty: C++ truth table library
 * Copyright (C) 2017-2018  EPFL
 * Copyright (C) 2017-2018  University of Victoria
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
  \file spectral.hpp
  \brief Implements methods for spectral classification

  Original implementation by D. Michael Miller (University of
  Victoria, BC, Canada).  Modified C++ implementation and integration
  into kitty by Mathias Soeken.

  \author D. Michael Miller
  \author Mathias Soeken
*/

#pragma once

#include "bit_operations.hpp"

#include <iomanip>
#include <unordered_map>

namespace kitty
{

namespace detail
{
struct spectral_operation
{
  enum class kind : uint16_t
  {
    none,
    permutation,
    input_negation,
    output_negation,
    spectral_translation,
    disjoint_translation
  };

  spectral_operation() = default;
  explicit spectral_operation( kind _kind, uint16_t _var1 = 0, uint16_t _var2 = 0 ) : _kind( _kind ), _var1( _var1 ), _var2( _var2 ) {}

  kind _kind{kind::none};
  uint16_t _var1{0};
  uint16_t _var2{0};
};

inline void fast_hadamard_transform( std::vector<int32_t>& s, bool reverse = false )
{
  unsigned k{};
  int t{};

  for ( auto m = 1u; m < s.size(); m <<= 1u )
  {
    for ( auto i = 0u; i < s.size(); i += ( m << 1u ) )
    {
      for ( auto j = i, p = k = i + m; j < p; ++j, ++k )
      {
        t = s[j];
        s[j] += s[k];
        s[k] = t - s[k];
      }
    }
  }

  if ( reverse )
  {
    for ( auto i = 0u; i < s.size(); ++i )
    {
      s[i] /= static_cast<int>( s.size() );
    }
  }
}

class spectrum
{
public:
  spectrum() = delete;
  ~spectrum() = default;

  spectrum( const spectrum& other ) : _s( std::begin( other._s ), std::end( other._s ) ) {}
  spectrum( spectrum&& other ) noexcept : _s( std::move( other._s ) ) {}

  spectrum& operator=( const spectrum& other )
  {
    if ( this != &other )
    {
      _s = other._s;
    }
    return *this;
  }

  spectrum& operator=( spectrum&& other ) noexcept
  {
    _s = std::move( other._s );
    return *this;
  }

private:
  explicit spectrum( std::vector<int32_t> _s ) : _s( std::move( _s ) ) {}

public:
  template<typename TT>
  static spectrum from_truth_table( const TT& tt )
  {
    std::vector<int32_t> _s( tt.num_bits(), 1 );
    for_each_one_bit( tt, [&_s]( auto bit ) { _s[bit] = -1; } );
    fast_hadamard_transform( _s );
    return spectrum( _s );
  }

  template<typename TT>
  void to_truth_table( TT& tt ) const
  {
    auto copy = _s;
    fast_hadamard_transform( copy, true );

    clear( tt );
    for ( auto i = 0u; i < copy.size(); ++i )
    {
      if ( copy[i] == -1 )
      {
        set_bit( tt, i );
      }
    }
  }

  auto permutation( unsigned i, unsigned j )
  {
    spectral_operation op( spectral_operation::kind::permutation, i, j );

    for ( auto k = 0u; k < _s.size(); ++k )
    {
      if ( ( k & i ) > 0 && ( k & j ) == 0 )
      {
        std::swap( _s[k], _s[k - i + j] );
      }
    }

    return op;
  }

  auto input_negation( unsigned i )
  {
    spectral_operation op( spectral_operation::kind::input_negation, i );
    for ( auto k = 0u; k < _s.size(); ++k )
    {
      if ( ( k & i ) > 0 )
      {
        _s[k] = -_s[k];
      }
    }

    return op;
  }

  auto output_negation()
  {
    for ( auto& coeff : _s )
    {
      coeff = -coeff;
    }
    return spectral_operation( spectral_operation::kind::output_negation );
  }

  auto spectral_translation( int i, int j )
  {
    spectral_operation op( spectral_operation::kind::spectral_translation, i, j );

    for ( auto k = 0u; k < _s.size(); ++k )
    {
      if ( ( k & i ) > 0 && ( k & j ) == 0 )
      {
        std::swap( _s[k], _s[k + j] );
      }
    }

    return op;
  }

  auto disjoint_translation( int i )
  {
    spectral_operation op( spectral_operation::kind::disjoint_translation, i );

    for ( auto k = 0u; k < _s.size(); ++k )
    {
      if ( ( k & i ) > 0 )
      {
        std::swap( _s[k], _s[k - i] );
      }
    }

    return op;
  }

  void apply( const spectral_operation& op )
  {
    switch ( op._kind )
    {
    case spectral_operation::kind::none:
      assert( false );
      break;
    case spectral_operation::kind::permutation:
      permutation( op._var1, op._var2 );
      break;
    case spectral_operation::kind::input_negation:
      input_negation( op._var1 );
      break;
    case spectral_operation::kind::output_negation:
      output_negation();
      break;
    case spectral_operation::kind::spectral_translation:
      spectral_translation( op._var1, op._var2 );
      break;
    case spectral_operation::kind::disjoint_translation:
      disjoint_translation( op._var1 );
      break;
    }
  }

  inline auto operator[]( std::vector<int32_t>::size_type pos )
  {
    return _s[pos];
  }

  inline auto operator[]( std::vector<int32_t>::size_type pos ) const
  {
    return _s[pos];
  }

  inline auto size() const
  {
    return _s.size();
  }

  inline auto cbegin() const
  {
    return _s.cbegin();
  }

  inline auto cend() const
  {
    return _s.cend();
  }

  void print( std::ostream& os, const std::vector<uint32_t>& order ) const
  {
    os << std::setw( 4 ) << _s[order.front()];

    for ( auto it = order.begin() + 1; it != order.end(); ++it )
    {
      os << " " << std::setw( 4 ) << _s[*it];
    }
  }

private:
  std::vector<int32_t> _s;
};

inline std::vector<uint32_t> get_rw_coeffecient_order( uint32_t num_vars )
{
  auto size = uint32_t( 1 ) << num_vars;
  std::vector<uint32_t> map( size, 0u );
  auto p = std::begin( map ) + 1;

  for ( uint32_t i = 1u; i <= num_vars; ++i )
  {
    for ( uint32_t j = 1u; j < size; ++j )
    {
      if ( __builtin_popcount( j ) == static_cast<int>( i ) )
      {
        *p++ = j;
      }
    }
  }

  return map;
}

template<typename TT>
class miller_spectral_canonization_impl
{
public:
  explicit miller_spectral_canonization_impl( const TT& func )
      : func( func ),
        num_vars( func.num_vars() ),
        num_vars_exp( 1 << num_vars ),
        spec( spectrum::from_truth_table( func ) ),
        best_spec( spec ),
        transforms( 100u )
  {
  }

  template<typename Callback>
  TT run( Callback&& fn )
  {
    order = get_rw_coeffecient_order( num_vars );
    normalize();

    fn( best_transforms );

    TT tt = func.construct();
    spec.to_truth_table<TT>( tt );
    return tt;
  }

private:
  unsigned transformation_costs( const std::vector<spectral_operation>& transforms )
  {
    auto costs = 0u;
    for ( const auto& t : transforms )
    {
      costs += ( t._kind == spectral_operation::kind::permutation ) ? 3u : 1u;
    }
    return costs;
  }

  void closer( spectrum& lspec )
  {
    for ( auto i = 0u; i < lspec.size(); ++i )
    {
      const auto j = order[i];
      if ( lspec[j] == best_spec[j] )
      {
        continue;
      }
      if ( abs( lspec[j] ) > abs( best_spec[j] ) ||
           ( abs( lspec[j] ) == abs( best_spec[j] ) && lspec[j] > best_spec[j] ) )
      {
        update_best( lspec );
        return;
      }

      if ( abs( lspec[j] ) < abs( best_spec[j] ) ||
           ( abs( lspec[j] ) == abs( best_spec[j] ) && lspec[j] < best_spec[j] ) )
      {
        return;
      }
    }

    if ( transformation_costs( transforms ) < transformation_costs( best_transforms ) )
    {
      update_best( lspec );
    }
  }

  void normalize_rec( spectrum& lspec, unsigned v )
  {
    if ( v == num_vars_exp ) /* leaf case */
    {
      /* invert function if necessary */
      if ( lspec[0u] < 0 )
      {
        insert( lspec.output_negation() );
      }
      /* invert any variable as necessary */
      for ( auto i = 1u; i < num_vars_exp; i <<= 1 )
      {
        if ( lspec[i] < 0 )
        {
          insert( lspec.input_negation( i ) );
        }
      }

      closer( lspec );
      return;
    }

    auto min = 0, max = 0;
    const auto p = std::accumulate( lspec.cbegin() + v, lspec.cend(),
                                    std::make_pair( min, max ),
                                    []( auto a, auto sv ) {
                                      return std::make_pair( std::min( a.first, abs( sv ) ), std::max( a.second, abs( sv ) ) );
                                    } );
    min = p.first;
    max = p.second;

    if ( max == 0 )
    {
      auto& spec2 = specs.at( num_vars_exp );
      spec2 = lspec;
      normalize_rec( spec2, num_vars_exp );
    }
    else
    {
      for ( auto i = 1u; i < lspec.size(); ++i )
      {
        auto j = order[i];
        if ( abs( lspec[j] ) != max )
        {
          continue;
        }

        /* k = first one bit in j starting from pos v */
        auto k = j & ~( v - 1 ); /* remove 1 bits until v */
        if ( k == 0 )
        {
          continue; /* are there bit left? */
        }
        k = k - ( k & ( k - 1 ) ); /* extract lowest bit */
        j ^= k;                    /* remove bit k from j */

        auto& spec2 = specs.at( v << 1 );
        spec2 = lspec;

        const auto save = transform_index;

        /* spectral translation to all other 1s in j */
        while ( j )
        {
          auto p = j - ( j & ( j - 1 ) );
          insert( spec2.spectral_translation( k, p ) );
          j ^= p;
        }

        if ( k != v )
        {
          insert( spec2.permutation( k, v ) );
        }

        normalize_rec( spec2, v << 1 );

        if ( v == 1 && min == max )
        {
          return;
        }
        transform_index = save;
      }
    }
  }

  void normalize()
  {
    /* find maximum absolute element index in spectrum (by order) */
    auto j = *std::max_element( order.cbegin(), order.cend(), [this]( auto p1, auto p2 ) { return abs( spec[p1] ) < abs( spec[p2] ); } );

    /* if max element is not the first element */
    if ( j )
    {
      auto k = j - ( j & ( j - 1 ) ); /* LSB of j */
      j ^= k;                         /* delete bit in j */

      while ( j )
      {
        auto p = j - ( j & ( j - 1 ) ); /* next LSB of j */
        j ^= p;                         /* delete bit in j */
        insert( spec.spectral_translation( k, p ) );
      }
      insert( spec.disjoint_translation( k ) );
    }

    for ( auto v = 1u; v <= num_vars_exp; v <<= 1 )
    {
      specs.insert( {v, spec} );
    }

    update_best( spec );
    normalize_rec( spec, 1 );
    spec = best_spec;
  }

  void insert( const spectral_operation& trans )
  {
    if ( transform_index >= transforms.size() )
    {
      transforms.resize( transforms.size() << 1 );
    }
    assert( transform_index < transforms.size() );
    transforms[transform_index++] = trans;
  }

  void update_best( const spectrum& lspec )
  {
    best_spec = lspec;
    best_transforms.resize( transform_index );
    std::copy( transforms.begin(), transforms.begin() + transform_index, best_transforms.begin() );
  }

  std::ostream& print_spectrum( std::ostream& os )
  {
    os << "[i]";

    for ( auto i = 0u; i < spec.size(); ++i )
    {
      auto j = order[i];
      if ( j > 0 && __builtin_popcount( order[i - 1] ) < __builtin_popcount( j ) )
      {
        os << " |";
      }
      os << " " << std::setw( 3 ) << spec[j];
    }
    return os << std::endl;
  }

private:
  const TT& func;

  unsigned num_vars;
  unsigned num_vars_exp;
  spectrum spec;
  spectrum best_spec;
  std::unordered_map<uint64_t, spectrum> specs;

  std::vector<uint32_t> order;
  std::vector<spectral_operation> transforms;
  std::vector<spectral_operation> best_transforms;
  unsigned transform_index{0u};
};

inline void exact_spectral_canonization_null_callback( const std::vector<spectral_operation>& operations )
{
  (void)operations;
}
} /* namespace detail */

/*! \brief Exact spectral canonization

  The function can be passed as second argument a callback that is called with a
  vector of spectral operations necessary to transform the input function into
  the representative.

  \param tt Truth table
  \param fn Callback to retrieve list of transformations (optional)
 */
template<typename TT, typename Callback = decltype( detail::exact_spectral_canonization_null_callback )>
inline TT exact_spectral_canonization( const TT& tt, Callback&& fn = detail::exact_spectral_canonization_null_callback )
{
  detail::miller_spectral_canonization_impl<TT> impl( tt );
  return impl.run( fn );
}

/*! \brief Print spectral representation of a function in RW order

  \param tt Truth table
  \param os Output stream
 */
template<typename TT>
inline void print_spectrum( const TT& tt, std::ostream& os = std::cout )
{
  const auto spectrum = detail::spectrum::from_truth_table( tt );
  spectrum.print( os, detail::get_rw_coeffecient_order( tt.num_vars() ) );
}

} /* namespace kitty */
