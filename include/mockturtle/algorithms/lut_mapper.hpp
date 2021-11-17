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
  \file lut_mapper.hpp
  \brief Mapper

  \author Alessandro Tempia Calvino
*/

#pragma once

#include <cstdint>
#include <limits>

#include <fmt/format.h>

#include "../networks/klut.hpp"
#include "../utils/node_map.hpp"
#include "../utils/stopwatch.hpp"
#include "../utils/tech_library.hpp"
#include "../views/topo_view.hpp"
#include "../views/mapping_view.hpp"
#include "cut_enumeration.hpp"
#include "cut_enumeration/lut_delay_cut.hpp"
#include "cut_enumeration/mf_cut.hpp"
#include "detail/mffc_utils.hpp"
// #include "collapse_mapped.hpp"


namespace mockturtle
{

/*! \brief Parameters for map.
 *
 * The data structure `map_params` holds configurable parameters
 * with default arguments for `map`.
 */
struct lut_map_params
{
  lut_map_params()
  {
    cut_enumeration_ps.cut_limit = 49;
    cut_enumeration_ps.minimize_truth_table = true;
  }

  /*! \brief Parameters for cut enumeration
   *
   * The default cut limit is 49. By default,
   * truth table minimization is performed.
   */
  cut_enumeration_params cut_enumeration_ps{};

  /*! \brief Required depth for depth relaxation. */
  uint32_t required_delay{ 0u };

  /*! \brief Skip depth round for size optimization. */
  bool skip_delay_round{ false };

  /*! \brief Number of rounds for area flow optimization. */
  uint32_t area_flow_rounds{ 1u };

  /*! \brief Number of rounds for exact area optimization. */
  uint32_t ela_rounds{ 2u };

  /*! \brief Be verbose. */
  bool verbose{ false };
};

/*! \brief Statistics for mapper.
 *
 * The data structure `map_stats` provides data collected by running
 * `map`.
 */
struct lut_map_stats
{
  /*! \brief Area result. */
  uint32_t area{ 0 };
  /*! \brief Worst delay result. */
  uint32_t delay{ 0 };

  /*! \brief Runtime for covering. */
  stopwatch<>::duration time_mapping{ 0 };
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{ 0 };

  /*! \brief Cut enumeration stats. */
  cut_enumeration_stats cut_enumeration_st{};

  /*! \brief Depth and size stats for each round. */
  std::vector<std::string> round_stats{};

  void report() const
  {
    for ( auto const& stat : round_stats )
    {
      std::cout << stat;
    }
    std::cout << fmt::format( "[i] Area = {:8d}; Delay = {:8d};\n", area, delay );
    std::cout << fmt::format( "[i] Mapping runtime = {:>5.2f} secs\n", to_seconds( time_mapping ) );
    std::cout << fmt::format( "[i] Total runtime   = {:>5.2f} secs\n", to_seconds( time_total ) );
  }
};

namespace detail
{

struct node_lut
{
  /* best cut index for both phases */
  uint32_t best_cut; /* TODO: remove */

  /* arrival time at node output */
  uint32_t arrival;
  /* required time at node output */
  uint32_t required;
  /* area of the best matches */
  uint32_t area;

  /* number of references in the cover 0: pos, 1: neg, 2: pos+neg */
  uint32_t map_refs;
  /* references estimation */
  float est_refs;
  /* area flow */
  float flows;
};

template<class Ntk, unsigned CutSize, bool StoreFunction, typename CutData>
class lut_map_impl
{
public:
  using network_cuts_t = fast_network_cuts<Ntk, CutSize, StoreFunction, CutData>;
  using cut_t = typename network_cuts_t::cut_t;

public:
  explicit lut_map_impl( Ntk& ntk, lut_map_params const& ps, lut_map_stats& st )
      : ntk( ntk ),
        ps( ps ),
        st( st ),
        node_match( ntk.size() ),
        cuts( fast_cut_enumeration<Ntk, CutSize, StoreFunction, CutData>( ntk, ps.cut_enumeration_ps, &st.cut_enumeration_st ) )
  {}

  void run()
  {
    stopwatch t( st.time_mapping );

    /* compute and save topological order */
    top_order.reserve( ntk.size() );
    topo_view<Ntk>( ntk ).foreach_node( [this]( auto n ) {
      top_order.push_back( n );
    } );

    /* init the data structure */
    init_nodes();

    /* compute mapping for depth */
    if ( !ps.skip_delay_round )
    {
      compute_mapping<false>();
    }

    /* compute mapping using global area flow */
    while ( iteration < ps.area_flow_rounds + 1 )
    {
      compute_required_time();
      compute_mapping<true>();
    }

    /* compute mapping using exact area */
    while ( iteration < ps.ela_rounds + ps.area_flow_rounds + 1 )
    {
      compute_required_time();
      compute_mapping_exact();
    }

    /* generate the output network */
    derive_mapping();
  }

private:
  void init_nodes()
  {
    ntk.foreach_node( [this]( auto const& n, auto ) {
      const auto index = ntk.node_to_index( n );
      auto& node_data = node_match[index];

      node_data.est_refs = static_cast<float>( ntk.fanout_size( n ) );

      if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      {
        /* all terminals have flow 1.0 */
        node_data.flows = 0.0f;
        node_data.arrival = 0.0f;
      }
    } );
  }

  template<bool DO_AREA>
  void compute_mapping()
  {
    for ( auto const& n : top_order )
    {
      if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      {
        continue;
      }

      compute_best_cut<DO_AREA>( n );
    }

    uint32_t area_old = area;
    set_mapping_refs<false>();

    /* round stats */
    if ( ps.verbose )
    {
      std::stringstream stats{};
      float area_gain = 0.0f;

      if ( iteration != 1 )
        area_gain = ( static_cast<float>( area_old - area ) / area_old * 100 );

      if constexpr ( DO_AREA )
      {
        stats << fmt::format( "[i] AreaFlow : Delay = {:8d}  Area = {:8d}  {:>5.2f} %\n", delay, area, area_gain );
      }
      else
      {
        stats << fmt::format( "[i] Delay    : Delay = {:8d}  Area = {:8d}  {:>5.2f} %\n", delay, area, area_gain );
      }
      st.round_stats.push_back( stats.str() );
    }
  }

  void compute_mapping_exact()
  {
    for ( auto const& n : top_order )
    {
      if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
        continue;

      compute_best_cut_exact( n );
    }

    uint32_t area_old = area;
    set_mapping_refs<true>();

    /* round stats */
    if ( ps.verbose )
    {
      float area_gain = ( static_cast<float>( area_old - area ) / area_old * 100 );
      std::stringstream stats{};
      stats << fmt::format( "[i] Area     : Delay = {:8d}  Area = {:8d}  {:>5.2f} %\n", delay, area, area_gain );
      st.round_stats.push_back( stats.str() );
    }
  }

  template<bool ELA>
  void set_mapping_refs()
  {
    const auto coef = 1.0f / ( 2.0f + ( iteration + 1 ) * ( iteration + 1 ) );

    if constexpr ( !ELA )
    {
      for ( auto i = 0u; i < node_match.size(); ++i )
      {
        node_match[i].map_refs = 0u;
      }
    }

    /* compute the current worst delay and update the mapping refs */
    delay = 0;
    ntk.foreach_po( [this]( auto s ) {
      const auto index = ntk.node_to_index( ntk.get_node( s ) );

      delay = std::max( delay, node_match[index].arrival );

      if constexpr ( !ELA )
      {
        node_match[index].map_refs++;
      }
    } );

    /* compute current area and update mapping refs in top-down order */
    area = 0;
    for ( auto it = top_order.rbegin(); it != top_order.rend(); ++it )
    {
      const auto index = ntk.node_to_index( *it );
      auto& node_data = node_match[index];

      /* skip constants and PIs */
      if ( ntk.is_constant( *it ) || ntk.is_pi( *it ) )
      {
        continue;
      }

      /* continue if not referenced in the cover */
      if ( node_match[index].map_refs == 0u )
        continue;

      if constexpr ( !ELA )
      {
        for ( auto const leaf : cuts.cuts( index )[0] )
        {
          node_match[leaf].map_refs++;
        }
      }
      ++area;
    }

    /* blend estimated references */
    for ( auto i = 0u; i < ntk.size(); ++i )
    {
      node_match[i].est_refs = coef * node_match[i].est_refs + ( 1.0f - coef ) * std::max( 1.0f, static_cast<float>( node_match[i].map_refs ) );
    }

    ++iteration;
  }

  void compute_required_time()
  {
    for ( auto i = 0u; i < node_match.size(); ++i )
    {
      node_match[i].required = UINT32_MAX;
    }

    /* return in case of `skip_delay_round` */
    if ( iteration == 0 )
      return;

    auto required = delay;

    if ( ps.required_delay != 0 )
    {
      /* Global target time constraint */
      if ( ps.required_delay < delay )
      {
        if ( !ps.skip_delay_round && iteration == 1 )
          std::cerr << fmt::format( "[i] MAP WARNING: cannot meet the target required time of {:.2f}", ps.required_delay ) << std::endl;
      }
      else
      {
        required = ps.required_delay;
      }
    }

    /* set the required time at POs */
    ntk.foreach_po( [&]( auto const& s ) {
      const auto index = ntk.node_to_index( ntk.get_node( s ) );
      node_match[index].required = required;
    } );

    /* propagate required time to the PIs */
    for ( auto it = top_order.rbegin(); it != top_order.rend(); ++it )
    {
      if ( ntk.is_pi( *it ) || ntk.is_constant( *it ) )
        continue;

      const auto index = ntk.node_to_index( *it );

      if ( node_match[index].map_refs == 0 )
        continue;

      for ( auto leaf : cuts.cuts( index )[0] )
      {
        node_match[leaf].required = std::min( node_match[leaf].required, node_match[index].required - 1 );
      }
    }
  }

  template<bool DO_AREA>
  void compute_best_cut( node<Ntk> const& n )
  {
    uint32_t best_arrival = UINT32_MAX;
    double best_area_flow = std::numeric_limits<float>::max();
    uint32_t best_size = UINT32_MAX;
    uint8_t best_cut = 0u;
    uint8_t cut_index = 0u;
    auto index = ntk.node_to_index( n );

    auto& node_data = node_match[index];

    /* foreach cut */
    for ( auto& cut : cuts.cuts( index ) )
    {
      /* ignore trivial cut */
      if ( cut->size() == 1 && ( *cut->begin() ) == index )
      {
        ++cut_index;
        continue;
      }

      uint32_t worst_arrival = 0;
      double flow = 0;

      for ( auto leaf : *cut )
      {
        worst_arrival = std::max( worst_arrival, node_match[leaf].arrival + 1 );
        flow += node_match[leaf].flows;
      }

      double area_local = 1 + flow;

      if constexpr ( DO_AREA )
      {
        if ( worst_arrival > node_data.required )
        {
          ++cut_index;
          continue;
        }
      }

      if ( compare_map<DO_AREA>( worst_arrival, best_arrival, area_local, best_area_flow, cut->size(), best_size ) )
      {
        best_arrival = worst_arrival;
        best_area_flow = area_local;
        best_size = cut->size();
        best_cut = cut_index;
      }
      ++cut_index;
    }

    node_data.flows = best_area_flow / node_data.est_refs;
    node_data.arrival = best_arrival;
    node_data.best_cut = best_cut;

    if ( best_cut != 0 )
    {
      cuts.cuts( index ).update_best( best_cut );
    }
  }

  void compute_best_cut_exact( node<Ntk> const& n )
  {
    uint32_t best_arrival = UINT32_MAX;
    uint32_t best_exact_area = UINT32_MAX;
    uint32_t best_size = UINT32_MAX;
    uint8_t best_cut = 0u;
    uint8_t cut_index = 0u;
    auto index = ntk.node_to_index( n );

    auto& node_data = node_match[index];

    /* recursively deselect the best cut shared between
     * the two phases if in use in the cover */
    if ( node_data.map_refs )
    {
      cut_deref( cuts.cuts( index )[0] );
    }

    /* foreach cut */
    for ( auto& cut : cuts.cuts( index ) )
    {
      /* ignore trivial cut */
      if ( cut->size() == 1 && ( *cut->begin() ) == index )
      {
        ++cut_index;
        continue;
      }

      uint32_t area_exact = cut_ref( *cut );
      cut_deref( *cut );
      uint32_t worst_arrival = 0;

      for ( auto l : *cut )
      {
        worst_arrival = std::max( worst_arrival, node_match[l].arrival + 1 );
      }

      if ( worst_arrival > node_data.required )
      {
        ++cut_index;
        continue;
      }

      if ( compare_map<true>( worst_arrival, best_arrival, area_exact, best_exact_area, cut->size(), best_size ) )
      {
        best_arrival = worst_arrival;
        best_exact_area = area_exact;
        best_size = cut->size();
        best_cut = cut_index;
      }

      ++cut_index;
    }

    node_data.flows = best_exact_area;
    node_data.arrival = best_arrival;

    if ( best_cut != 0 )
    {
      cuts.cuts( index ).update_best( best_cut );
    }

    if ( node_data.map_refs )
    {
      cut_ref( cuts.cuts( index )[0] );
    }
  }

  uint32_t cut_ref( cut_t const& cut )
  {
    uint32_t count = 1;

    for ( auto leaf : cut )
    {
      if ( ntk.is_pi( ntk.index_to_node( leaf ) ) || ntk.is_constant( ntk.index_to_node( leaf ) ) )
      {
        continue;
      }

      /* Recursive referencing if leaf was not referenced */
      if ( node_match[leaf].map_refs++ == 0u )
      {
        count += cut_ref( cuts.cuts( leaf )[0] );
      }
    }
    return count;
  }

  uint32_t cut_deref( cut_t const& cut )
  {
    uint32_t count = 1;

    for ( auto leaf : cut )
    {
      if ( ntk.is_pi( ntk.index_to_node( leaf ) ) || ntk.is_constant( ntk.index_to_node( leaf ) ) )
      {
        continue;
      }

      /* Recursive referencing if leaf was not referenced */
      if ( --node_match[leaf].map_refs == 0u )
      {
        count += cut_deref( cuts.cuts( leaf )[0] );
      }
    }
    return count;
  }

  void derive_mapping()
  {
    ntk.clear_mapping();

    for ( auto const& n : top_order )
    {
      if ( ntk.is_pi( n ) || ntk.is_constant( n ) )
        continue;

      const auto index = ntk.node_to_index( n );
      if ( node_match[index].map_refs == 0 )
        continue;

      std::vector<node<Ntk>> nodes;
      auto const& best_cut = cuts.cuts( index ).best();

      for ( auto const& l : best_cut )
      {
        nodes.push_back( ntk.index_to_node( l ) );
      }
      ntk.add_to_mapping( n, nodes.begin(), nodes.end() );

      if constexpr ( StoreFunction )
      {
        ntk.set_cell_function( n, cuts.truth_table( cuts.cuts( index ).best() ) );
      }
    }

    st.area = area;
    st.delay = delay;
  }

  template<bool DO_AREA>
  inline bool compare_map( uint32_t arrival, uint32_t best_arrival, double area_flow, double best_area_flow, uint32_t size, uint32_t best_size )
  {
    if constexpr ( DO_AREA )
    {
      if ( area_flow < best_area_flow - epsilon )
      {
        return true;
      }
      else if ( area_flow > best_area_flow + epsilon )
      {
        return false;
      }
      else if ( arrival < best_arrival )
      {
        return true;
      }
      else if ( arrival > best_arrival )
      {
        return false;
      }
    }
    else
    {
      if ( arrival < best_arrival )
      {
        return true;
      }
      else if ( arrival > best_arrival )
      {
        return false;
      }
      else if ( area_flow < best_area_flow - epsilon )
      {
        return true;
      }
      else if ( area_flow > best_area_flow + epsilon )
      {
        return false;
      }
    }
    if ( size < best_size )
    {
      return true;
    }
    return false;
  }

private:
  Ntk& ntk;
  lut_map_params const& ps;
  lut_map_stats& st;

  uint32_t iteration{ 0 };       /* current mapping iteration */
  uint32_t delay{ 0 };          /* current delay of the mapping */
  uint32_t area{ 0 };           /* current area of the mapping */
  const float epsilon{ 0.005f }; /* epsilon */

  std::vector<node<Ntk>> top_order;
  std::vector<node_lut> node_match;
  network_cuts_t cuts;
};

} /* namespace detail */

/*! \brief LUT mapper.
 *
 * This function implements a LUT mapping algorithm. It is controlled by a
 * template argument `CutData` (defaulted to `cut_enumeration_tech_map_cut`).
 * The argument is similar to the `CutData` argument in `cut_enumeration`, which can
 * specialize the cost function to select priority cuts and store additional data.
 * The default argument gives priority firstly to the cut size, then delay, and lastly
 * to area flow. Thus, it is more suited for delay-oriented mapping.
 * The type passed as `CutData` must implement the following four fields:
 *
 * - `uint32_t delay`
 * - `float flow`
 * - `uint8_t match_index`
 * - `bool ignore`
 *
 * See `include/mockturtle/algorithms/cut_enumeration/cut_enumeration_tech_map_cut.hpp`
 * for one example of a CutData type that implements the cost function that is used in
 * the technology mapper.
 * 
 * The function takes the size of the cuts in the template parameter `CutSize`.
 *
 * The function returns a k-LUT network. Each LUT abstacts a gate of the technology library.
 *
 * **Required network functions:**
 * - `size`
 * - `is_pi`
 * - `is_constant`
 * - `node_to_index`
 * - `index_to_node`
 * - `get_node`
 * - `foreach_po`
 * - `foreach_node`
 * - `fanout_size`
 *
 * \param ntk Network
 * \param library Technology library
 * \param ps Mapping params
 * \param pst Mapping statistics
 * 
 * The implementation of this algorithm was inspired by the
 * mapping command ``map`` in ABC.
 */
template<class Ntk, unsigned CutSize = 4u, bool StoreFunction = false, typename CutData = cut_enumeration_lut_delay_cut>
void lut_map( Ntk& ntk, lut_map_params const& ps = {}, lut_map_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_size_v<Ntk>, "Ntk does not implement the size method" );
  static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
  static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
  static_assert( has_node_to_index_v<Ntk>, "Ntk does not implement the node_to_index method" );
  static_assert( has_index_to_node_v<Ntk>, "Ntk does not implement the index_to_node method" );
  static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
  static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
  static_assert( has_clear_mapping_v<Ntk>, "Ntk does not implement the clear_mapping method" );
  static_assert( has_add_to_mapping_v<Ntk>, "Ntk does not implement the add_to_mapping method" );

  lut_map_stats st;
  detail::lut_map_impl<Ntk, CutSize, StoreFunction, CutData> p( ntk, ps, st );
  p.run();

  st.time_total = st.time_mapping + st.cut_enumeration_st.time_total;
  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst != nullptr )
  {
    *pst = st;
  }
}

} /* namespace mockturtle */
