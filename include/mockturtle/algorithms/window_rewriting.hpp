/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
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
  \file window_rewriting.hpp
  \brief Window rewriting

  \author Heinz Riener
*/

#include "../utils/index_list.hpp"
#include "../utils/stopwatch.hpp"
#include "../utils/window_utils.hpp"
#include "../views/topo_view.hpp"
#include "../views/window_view.hpp"
#include "../utils/debugging_utils.hpp"

#include <abcresub/abcresub2.hpp>
#include <fmt/format.h>
#include <stack>

#pragma once

namespace mockturtle
{

struct window_rewriting_params
{
  uint64_t cut_size{6};
  uint64_t num_levels{5};

  bool filter_cyclic_substitutions{false};
}; /* window_rewriting_params */

struct window_rewriting_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{0};

  /*! \brief Time for constructing windows. */
  stopwatch<>::duration time_window{0};

  /*! \brief Time for optimizing windows. */
  stopwatch<>::duration time_optimize{0};

  /*! \brief Time for substituting. */
  stopwatch<>::duration time_substitute{0};

  /*! \brief Time for updating level information. */
  stopwatch<>::duration time_levels{0};

  /*! \brief Time for topological sorting. */
  stopwatch<>::duration time_topo_sort{0};

  /*! \brief Time for encoding index_list. */
  stopwatch<>::duration time_encode{0};

  /*! \brief Total number of calls to the resub. engine. */
  uint64_t num_substitutions{0};
  uint64_t num_restrashes{0};
  uint64_t num_windows{0};
  uint64_t gain{0};

  window_rewriting_stats operator+=( window_rewriting_stats const& other )
  {
    time_total += other.time_total;
    time_window += other.time_window;
    time_optimize += other.time_optimize;
    time_substitute += other.time_substitute;
    time_levels += other.time_levels;
    time_topo_sort += other.time_topo_sort;
    time_encode += other.time_encode;
    num_substitutions += other.num_substitutions;
    num_windows += other.num_windows;
    gain += other.gain;
    return *this;
  }

  void report() const
  {
    stopwatch<>::duration time_other =
      time_total - time_window - time_topo_sort - time_optimize - time_substitute - time_levels;

    fmt::print( "===========================================================================\n" );
    fmt::print( "[i] Windowing =  {:7.2f} ({:5.2f}%) (#win = {})\n",
                to_seconds( time_window ), to_seconds( time_window ) / to_seconds( time_total ) * 100, num_windows );
    fmt::print( "[i] Top.sort =   {:7.2f} ({:5.2f}%)\n", to_seconds( time_topo_sort ), to_seconds( time_topo_sort ) / to_seconds( time_total ) * 100 );
    fmt::print( "[i] Enc.list =   {:7.2f} ({:5.2f}%)\n", to_seconds( time_encode ), to_seconds( time_encode ) / to_seconds( time_total ) * 100 );
    fmt::print( "[i] Optimize =   {:7.2f} ({:5.2f}%) (#resubs = {}, est. gain = {})\n",
                to_seconds( time_optimize ), to_seconds( time_optimize ) / to_seconds( time_total ) * 100, num_substitutions, gain );
    fmt::print( "[i] Substitute = {:7.2f} ({:5.2f}%) (#hash upd. = {})\n",
                to_seconds( time_substitute ),
                to_seconds( time_substitute ) / to_seconds( time_total ) * 100,
                num_restrashes );
    fmt::print( "[i] Upd.levels = {:7.2f} ({:5.2f}%)\n", to_seconds( time_levels ), to_seconds( time_levels ) / to_seconds( time_total ) * 100 );
    fmt::print( "[i] Other =      {:7.2f} ({:5.2f}%)\n", to_seconds( time_other ), to_seconds( time_other ) / to_seconds( time_total ) * 100 );
    fmt::print( "---------------------------------------------------------------------------\n" );
    fmt::print( "[i] TOTAL =      {:7.2f}\n", to_seconds( time_total ) );
    fmt::print( "===========================================================================\n" );
  }
}; /* window_rewriting_stats */

namespace detail
{

template<typename Ntk>
bool is_contained_in_tfi_recursive( Ntk const& ntk, typename Ntk::node const& node, typename Ntk::node const& n )
{
  if ( ntk.color( node ) == ntk.current_color() )
  {
    return false;
  }
  ntk.paint( node );

  if ( n == node )
  {
    return true;
  }

  bool found = false;
  ntk.foreach_fanin( node, [&]( typename Ntk::signal const& fi ){
    if ( is_contained_in_tfi_recursive( ntk, ntk.get_node( fi ), n ) )
    {
      found = true;
      return false;
    }
    return true;
  });

  return found;
}

} /* namespace detail */

template<typename Ntk>
bool is_contained_in_tfi( Ntk const& ntk, typename Ntk::node const& node, typename Ntk::node const& n )
{
  /* do not even build the TFI, but just search for the node */
  ntk.new_color();
  return detail::is_contained_in_tfi_recursive( ntk, node, n );
}

namespace detail
{

template<class Ntk>
class window_rewriting_impl
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  explicit window_rewriting_impl( Ntk& ntk, window_rewriting_params const& ps, window_rewriting_stats& st )
    : ntk( ntk )
    , ps( ps )
    , st( st )
  {
    auto const update_level_of_new_node = [&]( const auto& n ) {
      ntk.resize_levels();
      update_node_level( n );
    };

    auto const update_level_of_existing_node = [&]( node const& n, const auto& old_children ) {
      (void)old_children;
      ntk.resize_levels();
      update_node_level( n );
    };

    auto const update_level_of_deleted_node = [&]( node const& n ) {
      assert( ntk.fanout_size( n ) == 0u );
      ntk.set_level( n, -1 );
    };

    ntk._events->on_add.emplace_back( update_level_of_new_node );
    ntk._events->on_modified.emplace_back( update_level_of_existing_node );
    ntk._events->on_delete.emplace_back( update_level_of_deleted_node );
  }

  void run()
  {
    stopwatch t( st.time_total );

    create_window_impl windowing( ntk );
    uint32_t const size = ntk.size();
    for ( uint32_t n = 0u; n < std::min( size, ntk.size() ); ++n )
    {
      if ( ntk.is_constant( n ) || ntk.is_ci( n ) || ntk.is_dead( n ) )
      {
        continue;
      }

      if ( const auto w = call_with_stopwatch( st.time_window, [&]() { return windowing.run( n, ps.cut_size, ps.num_levels ); } ) )
      {
        ++st.num_windows;

        auto topo_win = call_with_stopwatch( st.time_topo_sort, ( [&](){
          window_view win( ntk, w->inputs, w->outputs, w->nodes );
          topo_view topo_win{win};
          return topo_win;
        }) );

        abc_index_list il;
        call_with_stopwatch( st.time_encode, [&]() {
          encode( il, topo_win );
        } );

        auto il_opt = optimize( il );
        if ( !il_opt )
        {
          continue;
        }

        std::vector<signal> signals;
        for ( auto const& i : w->inputs )
        {
          signals.push_back( ntk.make_signal( i ) );
        }

        std::vector<signal> outputs;
        topo_win.foreach_co( [&]( signal const& o ){
          outputs.push_back( o );
        });

        uint32_t counter{0};
        ++st.num_substitutions;

        std::list<std::pair<node, signal>> substitutions;
        insert( ntk, std::begin( signals ), std::end( signals ), *il_opt,
                [&]( signal const& _new )
                {
                  assert( !ntk.is_dead( ntk.get_node( _new ) ) );
                  auto const _old = outputs.at( counter++ );
                  if ( _old == _new )
                  {
                    return true;
                  }

                  /* ensure that _old is not in the TFI of _new */
                  // assert( !is_contained_in_tfi( ntk, ntk.get_node( _new ), ntk.get_node( _old ) ) );
                  if ( ps.filter_cyclic_substitutions && is_contained_in_tfi( ntk, ntk.get_node( _new ), ntk.get_node( _old ) ) )
                  {
                    std::cout << "undo resubstitution " << ntk.get_node( _old ) << std::endl;
                    if ( ntk.fanout_size( ntk.get_node( _new ) ) == 0u )
                    {
                      ntk.take_out_node( ntk.get_node( _new ) );
                    }
                    return false;
                  }

                  substitutions.emplace_back( std::make_pair( ntk.get_node( _old ), ntk.is_complemented( _old ) ? !_new : _new ) );
                  return true;
                });

        substitute_nodes( substitutions );

        /* recompute levels and depth */
        // call_with_stopwatch( st.time_levels, [&]() { ntk.update_levels(); } );
        update_depth();

        /* ensure that no dead nodes are reachable */
        assert( count_reachable_dead_nodes( ntk ) == 0u );

        /* ensure that the network structure is still acyclic */
        assert( network_is_acylic( ntk ) );

        /* ensure that the levels and depth is correct */
        assert( check_network_levels( ntk ) );

        /* update internal data structures in windowing */
        windowing.resize( ntk.size() );
      }
    }

    /* ensure that no dead nodes are reachable */
    assert( count_reachable_dead_nodes( ntk ) == 0u );
  }

private:
  /* optimize an index_list and return the new list */
  std::optional<abc_index_list> optimize( abc_index_list const& il, bool verbose = false )
  {
    stopwatch t( st.time_optimize );

    int *raw = ABC_CALLOC( int, il.size() + 1u );
    uint64_t i = 0;
    for ( auto const& v : il.raw() )
    {
      raw[i++] = v;
    }
    raw[1] = 0; /* fix encoding */

    abcresub::Abc_ResubPrepareManager( 1 );
    int *new_raw = nullptr;
    int num_resubs = 0;
    uint64_t new_entries = abcresub::Abc_ResubComputeWindow( raw, ( il.size() / 2u ), 1000, -1, 0, 0, 0, 0, &new_raw, &num_resubs );
    abcresub::Abc_ResubPrepareManager( 0 );

    if ( verbose )
    {
      fmt::print( "Performed resub {} times.  Reduced {} nodes.\n",
                  num_resubs, new_entries > 0 ? ( ( il.size() / 2u ) - new_entries ) : 0 );
    }
    st.gain += new_entries > 0 ? ( ( il.size() / 2u ) - new_entries ) : 0;

    if ( raw )
    {
      ABC_FREE( raw );
    }

    if ( new_entries > 0 )
    {
      std::vector<uint32_t> values;
      for ( uint32_t i = 0; i < 2*new_entries; ++i )
      {
        values.push_back( new_raw[i] );
      }
      values[1u] = 1; /* fix encoding */
      if ( new_raw )
      {
        ABC_FREE( new_raw );
      }
      return abc_index_list( values, il.num_pis() );
    }
    else
    {
      assert( new_raw == nullptr );
      return std::nullopt;
    }
  }

  void substitute_nodes( std::list<std::pair<node, signal>> substitutions )
  {
    stopwatch t( st.time_substitute );

    auto clean_substitutions = [&]( node const& n )
    {
      substitutions.erase( std::remove_if( std::begin( substitutions ), std::end( substitutions ),
                                           [&]( auto const& s ){
                                             if ( s.first == n )
                                             {
                                               node const nn = ntk.get_node( s.second );
                                               if ( ntk.is_dead( nn ) )
                                                 return true;

                                               /* deref fanout_size of the node */
                                               if ( ntk.fanout_size( nn ) > 0 )
                                               {
                                                 ntk.decr_fanout_size( nn );
                                               }
                                               /* remove the node if it's fanout_size becomes 0 */
                                               if ( ntk.fanout_size( nn ) == 0 )
                                               {
                                                 ntk.take_out_node( nn );
                                               }
                                               /* remove substitution from list */
                                               return true;
                                             }
                                             return false; /* keep */
                                           } ),
                           std::end( substitutions ) );
    };

    /* register event to delete substitutions if their right-hand side
       nodes get deleted */
    ntk._events->on_delete.push_back( clean_substitutions );

    /* increment fanout_size of all signals to be used in
       substitutions to ensure that they will not be deleted */
    for ( const auto& s : substitutions )
    {
      ntk.incr_fanout_size( ntk.get_node( s.second ) );
    }

    while ( !substitutions.empty() )
    {
      auto const [old_node, new_signal] = substitutions.front();
      substitutions.pop_front();

      // for ( auto index = 1u; index < _storage->nodes.size(); ++index )
      const auto parents = ntk.fanout( old_node );
      for ( auto index : parents )
      {
        /* skip CIs and dead nodes */
        if ( ntk.is_dead( index ) )
          continue;

        /* skip nodes that will be deleted */
        if ( std::find_if( std::begin( substitutions ), std::end( substitutions ),
                           [&index]( auto s ){ return s.first == index; } ) != std::end( substitutions ) )
          continue;

        /* replace in node */
        if ( const auto repl = ntk.replace_in_node( index, old_node, new_signal ); repl )
        {
          ntk.incr_fanout_size( ntk.get_node( repl->second ) );
          substitutions.emplace_back( *repl );
        }
      }

      /* replace in outputs */
      ntk.replace_in_outputs( old_node, new_signal );

      /* replace in substitutions */
      for ( auto& s : substitutions )
      {
        if ( ntk.get_node( s.second ) == old_node )
        {
          s.second = ntk.is_complemented( s.second ) ? !new_signal : new_signal;
          ntk.incr_fanout_size( ntk.get_node( new_signal ) );
        }
      }

      /* finally remove the node: note that we never decrement the
         fanout_size of the old_node. instead, we remove the node and
         reset its fanout_size to 0 knowing that it must be 0 after
         substituting all references. */
      assert( !ntk.is_dead( old_node ) );
      ntk.take_out_node( old_node );

      /* decrement fanout_size when released from substitution list */
      ntk.decr_fanout_size( ntk.get_node( new_signal ) );
    }

    ntk._events->on_delete.pop_back();
  }

  /* recursively update the node levels and the depth of the critical path */
  void update_node_level( node const& n )
  {
    uint32_t const curr_level = ntk.level( n );

    uint32_t max_level = 0;
    ntk.foreach_fanin( n, [&]( const auto& f ) {
      auto const p = ntk.get_node( f );
      auto const fanin_level = ntk.level( p );
      if ( fanin_level > max_level )
      {
        max_level = fanin_level;
      }
    } );
    ++max_level;

    if ( curr_level != max_level )
    {
      ntk.set_level( n, max_level );
      ntk.foreach_fanout( n, [&]( const auto& p ) {
        if ( !ntk.is_dead( p ) )
        {
          update_node_level( p );
        }
      } );
    }
  }

  void update_depth()
  {
    uint32_t max_level{0};
    ntk.foreach_co( [&]( signal s ){
      assert( !ntk.is_dead( ntk.get_node( s ) ) );
      if ( ntk.level( ntk.get_node( s ) ) > max_level )
      {
        max_level = ntk.level( ntk.get_node( s ) );
      }
    });

    if ( ntk.depth() != max_level )
    {
      ntk.set_depth( max_level );
    }
  }

private:
  Ntk& ntk;
  window_rewriting_params ps;
  window_rewriting_stats& st;
}; /* window_rewriting_impl */

} /* detail */

template<class Ntk>
void window_rewriting( Ntk& ntk, window_rewriting_params const& ps = {}, window_rewriting_stats* pst = nullptr )
{
  window_rewriting_stats st;
  detail::window_rewriting_impl<Ntk>( ntk, ps, st ).run();
  if ( pst )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */
