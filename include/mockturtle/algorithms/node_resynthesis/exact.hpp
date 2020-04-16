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
  \file exact.hpp
  \brief Replace with exact synthesis result

  \author Mathias Soeken
*/

#pragma once

#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>
#include <kitty/hash.hpp>
#include <kitty/print.hpp>
#include <kitty/traits.hpp>
#include <percy/percy.hpp>

#include "../../networks/aig.hpp"
#include "../../networks/xmg.hpp"
#include "../../networks/klut.hpp"

namespace mockturtle
{

struct exact_resynthesis_params
{
  using cache_map_t = std::unordered_map<kitty::dynamic_truth_table, percy::chain, kitty::hash<kitty::dynamic_truth_table>>;
  using cache_t = std::shared_ptr<cache_map_t>;

  using blacklist_cache_map_t = std::unordered_map<kitty::dynamic_truth_table, int32_t, kitty::hash<kitty::dynamic_truth_table>>;
  using blacklist_cache_t = std::shared_ptr<blacklist_cache_map_t>;
  
  cache_t cache;
  blacklist_cache_t blacklist_cache;

  bool add_alonce_clauses{true};
  bool add_colex_clauses{true};
  bool add_lex_clauses{false};
  bool add_lex_func_clauses{true};
  bool add_nontriv_clauses{true};
  bool add_noreapply_clauses{true};
  bool add_symvar_clauses{true};
  int conflict_limit{0};

  percy::SolverType solver_type = percy::SLV_BSAT2;

  percy::EncoderType encoder_type = percy::ENC_SSV;

  percy::SynthMethod synthesis_method = percy::SYNTH_STD;
};

/*! \brief Resynthesis function based on exact synthesis.
 *
 * This resynthesis function can be passed to ``node_resynthesis``,
 * ``cut_rewriting``, and ``refactoring``.  The given truth table will be
 * resynthized in terms of an optimum size `k`-LUT network, where `k` is
 * specified as input to the constructor.  In order to guarantee a reasonable
 * runtime, `k` should be 3 or 4.
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      const klut_network klut = ...;

      exact_resynthesis<klut_network> resyn( 3 );
      klut = cut_rewriting( klut, resyn );
   \endverbatim
 *
 * A cache can be passed as second parameter to the constructor, which will
 * store optimum networks for all functions for which resynthesis is invoked
 * for.  The cache can be used to retrieve the computed network, which reduces
 * runtime.
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      const klut_network klut = ...;

      exact_resynthesis_params ps;
      ps.cache = std::make_shared<exact_resynthesis_params::cache_map_t>();
      exact_resynthesis<klut_network> resyn( 3, ps );
      klut = cut_rewriting( klut, resyn );

   The underlying engine for this resynthesis function is percy_.

   .. _percy: https://github.com/lsils/percy
   \endverbatim
 *
 */
template<class Ntk = klut_network>
class exact_resynthesis
{
public:
  explicit exact_resynthesis( uint32_t fanin_size = 3u, exact_resynthesis_params const& ps = {} )
      : _fanin_size( fanin_size ),
        _ps( ps )
  {
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( Ntk& ntk, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn ) const
  {
    operator()( ntk, function, function.construct(), begin, end, fn );
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( Ntk& ntk, kitty::dynamic_truth_table const& function, kitty::dynamic_truth_table const& dont_cares, LeavesIterator begin, LeavesIterator end, Fn&& fn ) const
  {
    if ( static_cast<uint32_t>( function.num_vars() ) <= _fanin_size )
    {
      fn( ntk.create_node( std::vector<signal<Ntk>>( begin, end ), function ) );
      return;
    }

    percy::spec spec;
    spec.fanin = _fanin_size;
    spec.verbosity = 0;
    spec.add_alonce_clauses = _ps.add_alonce_clauses;
    spec.add_colex_clauses = _ps.add_colex_clauses;
    spec.add_lex_clauses = _ps.add_lex_clauses;
    spec.add_lex_func_clauses = _ps.add_lex_func_clauses;
    spec.add_nontriv_clauses = _ps.add_nontriv_clauses;
    spec.add_noreapply_clauses = _ps.add_noreapply_clauses;
    spec.add_symvar_clauses = _ps.add_symvar_clauses;
    spec.conflict_limit = _ps.conflict_limit;
    spec[0] = function;
    bool with_dont_cares{false};
    if ( !kitty::is_const0( dont_cares ) )
    {
      spec.set_dont_care( 0, dont_cares );
      with_dont_cares = true;
    }

    auto c = [&]() -> std::optional<percy::chain> {
      if ( !with_dont_cares && _ps.cache )
      {
        const auto it = _ps.cache->find( function );
        if ( it != _ps.cache->end() )
        {
          return it->second;
        }
      }
      else if ( !with_dont_cares && _ps.blacklist_cache )
      {
        const auto it = _ps.blacklist_cache->find( function );
        if ( it != _ps.blacklist_cache->end() && _ps.conflict_limit >= it->second )
        {
          return std::nullopt;
        }
      }

      percy::chain c;
      if ( const auto result = percy::synthesize( spec, c, _ps.solver_type,
                                             _ps.encoder_type,
                                             _ps.synthesis_method );
           result != percy::success )
      {
        if ( _ps.blacklist_cache )
        {
          ( *_ps.blacklist_cache )[function] = result == percy::timeout ? _ps.conflict_limit : 0;
        }
        return std::nullopt;
      }
      c.denormalize();
      if ( !with_dont_cares && _ps.cache )
      {
        ( *_ps.cache )[function] = c;
      }
      return c;
    }();

    if ( !c )
    {
      return;
    }

    std::vector<signal<Ntk>> signals( begin, end );

    for ( auto i = 0; i < c->get_nr_steps(); ++i )
    {
      std::vector<signal<Ntk>> fanin;
      for ( const auto& child : c->get_step( i ) )
      {
        fanin.emplace_back( signals[child] );
      }
      signals.emplace_back( ntk.create_node( fanin, c->get_operator( i ) ) );
    }

    fn( signals.back() );
  }

private:
  uint32_t _fanin_size{3u};
  exact_resynthesis_params _ps;
};

/*! \brief Resynthesis function based on exact synthesis for AIGs.
 *
 * This resynthesis function can be passed to ``node_resynthesis``,
 * ``cut_rewriting``, and ``refactoring``.  The given truth table will be
 * resynthesized in terms of an optimum size AIG network.
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      aig_network aig = ...;

      exact_aig_resynthesis<aig_network> resyn;
      aig = cut_rewriting( aig, resyn );
   \endverbatim
 *
 * A cache can be passed as second parameter to the constructor, which will
 * store optimum networks for all functions for which resynthesis is invoked
 * for.  The cache can be used to retrieve the computed network, which reduces
 * runtime.
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      aig_network aig = ...;

      exact_resynthesis_params ps;
      ps.cache = std::make_shared<exact_resynthesis_params::cache_map_t>();
      exact_aig_resynthesis<aig_network> resyn( false, ps );
      aig = cut_rewriting( aig, resyn );

   The underlying engine for this resynthesis function is percy_.

   .. _percy: https://github.com/lsils/percy
   \endverbatim
 *
 */
template<class Ntk = aig_network>
class exact_aig_resynthesis
{
public:
  explicit exact_aig_resynthesis( bool _allow_xor = false, exact_resynthesis_params const& ps = {} )
      : _allow_xor( _allow_xor ),
        _ps( ps )
  {
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( Ntk& ntk, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn ) const
  {
    operator()( ntk, function, function.construct(), begin, end, fn );
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( Ntk& ntk, kitty::dynamic_truth_table const& function, kitty::dynamic_truth_table const& dont_cares, LeavesIterator begin, LeavesIterator end, Fn&& fn ) const
  {
    // TODO: special case for small functions (up to 2 variables)?

    percy::spec spec;
    if ( !_allow_xor )
    {
      spec.set_primitive( percy::AIG );
    }
    spec.fanin = 2;
    spec.verbosity = 0;
    spec.add_alonce_clauses = _ps.add_alonce_clauses;
    spec.add_colex_clauses = _ps.add_colex_clauses;
    spec.add_lex_clauses = _ps.add_lex_clauses;
    spec.add_lex_func_clauses = _ps.add_lex_func_clauses;
    spec.add_nontriv_clauses = _ps.add_nontriv_clauses;
    spec.add_noreapply_clauses = _ps.add_noreapply_clauses;
    spec.add_symvar_clauses = _ps.add_symvar_clauses;
    spec.conflict_limit = _ps.conflict_limit;
    if ( _lower_bound )
    {
      spec.initial_steps = *_lower_bound;
    }
    spec[0] = function;
    bool with_dont_cares{false};
    if ( !kitty::is_const0( dont_cares ) )
    {
      spec.set_dont_care( 0, dont_cares );
      with_dont_cares = true;
    }

    auto c = [&]() -> std::optional<percy::chain> {
      if ( !with_dont_cares && _ps.cache )
      {
        const auto it = _ps.cache->find( function );
        if ( it != _ps.cache->end() )
        {
          return it->second;
        }
      }

      percy::chain c;
      if ( const auto result = percy::synthesize( spec, c, _ps.solver_type,
                                                  _ps.encoder_type,
                                                  _ps.synthesis_method );
           result != percy::success )
      {
        return std::nullopt;
      }
      if ( !with_dont_cares && _ps.cache )
      {
        ( *_ps.cache )[function] = c;
      }
      return c;
    }();

    if ( !c )
    {
      return;
    }

    std::vector<signal<Ntk>> signals( begin, end );
    for ( auto i = 0; i < c->get_nr_steps(); ++i )
    {
      auto c1 = signals[c->get_step( i )[0]];
      auto c2 = signals[c->get_step( i )[1]];
      switch ( c->get_operator( i )._bits[0] )
      {
      default:
        std::cerr << "[e] unsupported operation " << kitty::to_hex( c->get_operator( i ) ) << "\n";
        assert( false );
        break;
      case 0x8:
        signals.emplace_back( ntk.create_and( c1, c2 ) );
        break;
      case 0x4:
        signals.emplace_back( ntk.create_and( !c1, c2 ) );
        break;
      case 0x2:
        signals.emplace_back( ntk.create_and( c1, !c2 ) );
        break;
      case 0xe:
        signals.emplace_back( !ntk.create_and( !c1, !c2 ) );
        break;
      case 0x6:
        signals.emplace_back( ntk.create_xor( c1, c2 ) );
        break;
      }
    }

    fn( c->is_output_inverted( 0 ) ? !signals.back() : signals.back() );
  }

  void set_bounds( std::optional<uint32_t> const& lower_bound, std::optional<uint32_t> const& upper_bound )
  {
    _lower_bound = lower_bound;
    _upper_bound = upper_bound;
  }

private:
  bool _allow_xor = false;
  exact_resynthesis_params _ps;

  std::optional<uint32_t> _lower_bound;
  std::optional<uint32_t> _upper_bound;
};

struct exact_xmg_resynthesis_params
{
  uint32_t num_candidates{10u};
};

/*! \brief Resynthesis function based on exact synthesis for XMGs.
 *
 * This resynthesis function can be passed to ``node_resynthesis``,
 * ``cut_rewriting``, and ``refactoring``.  The given truth table will be
 * resynthesized in terms of an optimum size XMG network.
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      xmg_network aig = ...;

      exact_xmg_resynthesis<xmg_network> resyn;
      xmg = cut_rewriting( xmg, resyn );
   \endverbatim
 *
 *
   The underlying engine for this resynthesis function is percy_.

   .. _percy: https://github.com/lsils/percy
   \endverbatim
 *
 */
template<class Ntk = xmg_network>
class exact_xmg_resynthesis
{
public:
  explicit exact_xmg_resynthesis( exact_xmg_resynthesis_params const& ps = {} )
    : ps( ps )
  {
  }

  template<typename LeavesIterator, typename TT, typename Fn>
  void operator()( Ntk& ntk, TT const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn ) const
  {
    static_assert( kitty::is_complete_truth_table<TT>::value, "Truth table must be complete" );

    using signal = mockturtle::signal<Ntk>;

    percy::chain chain;
    percy::spec spec;
    spec.verbosity = 0;
    spec.fanin = 3;

    /* specify local normalized gate primitives */
    kitty::dynamic_truth_table const0{3};
    kitty::dynamic_truth_table a{3};
    kitty::dynamic_truth_table b{3};
    kitty::dynamic_truth_table c{3};
    kitty::create_nth_var( a, 0 );
    kitty::create_nth_var( b, 1 );
    kitty::create_nth_var( c, 2 );

    spec.add_primitive( const0 );
    spec.add_primitive( a );
    spec.add_primitive( b );
    spec.add_primitive( c );
    spec.add_primitive( kitty::ternary_majority(  a,  b,  c ) );
    spec.add_primitive( kitty::ternary_majority( ~a,  b,  c ) );
    spec.add_primitive( kitty::ternary_majority(  a, ~b,  c ) );
    spec.add_primitive( kitty::ternary_majority(  a,  b, ~c ) );
    spec.add_primitive( a ^ b ^ c );

    percy::bsat_wrapper solver;
    percy::ssv_encoder encoder(solver);

    spec[0] = kitty::is_normal( function ) ? function : ~function;

    for ( auto i = 0u; i < ps.num_candidates; ++i )
    {
      auto const result = percy::next_struct_solution( spec, chain, solver, encoder );
      if ( result != percy::success )
        break;

      assert( result == percy::success );

      auto const sim = chain.simulate();
      assert( chain.simulate()[0] == spec[0] );

      std::vector<signal> signals( begin, end );
      for ( auto i = 0; i < chain.get_nr_steps(); ++i )
      {
        auto const c1 = signals[chain.get_step( i )[0]];
        auto const c2 = signals[chain.get_step( i )[1]];
        auto const c3 = signals[chain.get_step( i )[2]];

        switch( chain.get_operator( i )._bits[0] )
        {
        case 0x00:
          signals.emplace_back( ntk.get_constant( false ) );
          break;

        case 0xe8:
          signals.emplace_back( ntk.create_maj( c1,  c2,  c3 ) );
          break;

        case 0xd4:
          signals.emplace_back( ntk.create_maj( !c1,  c2,  c3 ) );
          break;

        case 0xb2:
          signals.emplace_back( ntk.create_maj( c1,  !c2,  c3 ) );
          break;

        case 0x8e:
          signals.emplace_back( ntk.create_maj( c1,  c2,  !c3 ) );
          break;

        case 0x96:
          signals.emplace_back( ntk.create_xor3( c1,  c2,  c3 ) );
          continue;

        default:
          std::cerr << "[e] unsupported operation " << kitty::to_hex( chain.get_operator( i ) ) << "\n";
          assert( false );
          break;
        }
      }

      fn( chain.is_output_inverted( 0 ) ? !signals.back() : signals.back(), chain.get_nr_steps() );
    }
  }

protected:
  exact_xmg_resynthesis_params const& ps;
}; /* exact_xmg_resynthesis */

} /* namespace mockturtle */
