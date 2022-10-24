/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
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
  \file emap.hpp
  \brief An extended technology mapper

  \author Alessandro Tempia Calvino
*/

#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/static_truth_table.hpp>
#include <kitty/hash.hpp>

#include <fmt/format.h>

#include "../../networks/klut.hpp"
#include "../../utils/cuts.hpp"
#include "../../utils/node_map.hpp"
#include "../../utils/stopwatch.hpp"
#include "../../utils/tech_library.hpp"
#include "../../views/binding_view.hpp"
#include "../../views/depth_view.hpp"
#include "../../views/topo_view.hpp"
#include "../cleanup.hpp"
#include "../cut_enumeration.hpp"
#include "../cut_enumeration/tech_map_cut.hpp"
#include "../detail/mffc_utils.hpp"
#include "../detail/switching_activity.hpp"

namespace mockturtle
{

/*! \brief Parameters for emap.
 *
 * The data structure `emap_params` holds configurable parameters
 * with default arguments for `emap`.
 */
struct emap_params
{
  emap_params()
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

  /*! \brief Required time for delay optimization. */
  double required_time{ 0.0f };

  /*! \brief Skip delay round for area optimization. */
  bool skip_delay_round{ false };

  /*! \brief Number of rounds for area flow optimization. */
  uint32_t area_flow_rounds{ 1u };

  /*! \brief Number of rounds for exact area optimization. */
  uint32_t ela_rounds{ 2u };

  /*! \brief Number of rounds for exact switching power optimization. */
  uint32_t eswp_rounds{ 0u };

  /*! \brief Number of patterns for switching activity computation. */
  uint32_t switching_activity_patterns{ 2048u };

  /*! \brief Remove the cuts that are contained in others */
  bool remove_dominated_cuts{ true };

  /*! \brief Be verbose. */
  bool verbose{ false };
};

/*! \brief Statistics for emap.
 *
 * The data structure `emap_stats` provides data collected by running
 * `emap`.
 */
struct emap_stats
{
  /*! \brief Area result. */
  double area{ 0 };
  /*! \brief Worst delay result. */
  double delay{ 0 };
  /*! \brief Power result. */
  double power{ 0 };

  /*! \brief Runtime for covering. */
  stopwatch<>::duration time_mapping{ 0 };
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{ 0 };

  /*! \brief Cut enumeration stats. */
  cut_enumeration_stats cut_enumeration_st{};

  /*! \brief Delay and area stats for each round. */
  std::vector<std::string> round_stats{};

  /*! \brief Mapping error. */
  bool mapping_error{ false };

  void report() const
  {
    for ( auto const& stat : round_stats )
    {
      std::cout << stat;
    }
    std::cout << fmt::format( "[i] Area = {:>5.2f}; Delay = {:>5.2f};", area, delay );
    if ( power != 0 )
      std::cout << fmt::format( " Power = {:>5.2f};\n", power );
    else
      std::cout << "\n";
    std::cout << fmt::format( "[i] Mapping runtime = {:>5.2f} secs\n", to_seconds( time_mapping ) );
    std::cout << fmt::format( "[i] Total runtime   = {:>5.2f} secs\n", to_seconds( time_total ) );
  }
};

namespace detail
{

#pragma region cut set
template<unsigned NInputs>
struct cut_enumeration_emap_cut
{
  double delay{0};
  double flow{0};
  bool ignore{false};

  /* list of supergates matching the cut for positive and negative output phases */
  std::array<std::vector<supergate<NInputs>> const*, 2> supergates = { nullptr, nullptr };
  /* input negations, 0: pos, 1: neg */
  std::array<uint8_t, 2> negations{ 0, 0 };
};

enum class emap_cut_sort_type
{
  DELAY = 0,
  AREA = 1,
  NONE = 2
};

template<typename CutType, int MaxCuts>
class emap_cut_set
{
public:
  /*! \brief Standard constructor.
   */
  emap_cut_set()
  {
    clear();
  }

  /*! \brief Clears a cut set.
   */
  void clear()
  {
    _pcend = _pend = _pcuts.begin();
    auto pit = _pcuts.begin();
    for ( auto& c : _cuts )
    {
      *pit++ = &c;
    }
  }

  /*! \brief Adds a cut to the end of the set.
   *
   * This function should only be called to create a set of cuts which is known
   * to be sorted and irredundant (i.e., no cut in the set dominates another
   * cut).
   *
   * \param begin Begin iterator to leaf indexes
   * \param end End iterator (exclusive) to leaf indexes
   * \return Reference to the added cut
   */
  template<typename Iterator>
  CutType& add_cut( Iterator begin, Iterator end )
  {
    assert( _pend != _pcuts.end() );

    auto& cut = **_pend++;
    cut.set_leaves( begin, end );

    ++_pcend;
    return cut;
  }

  /*! \brief Checks whether cut is dominates by any cut in the set.
   *
   * \param cut Cut outside of the set
   */
  bool is_dominated( CutType const& cut ) const
  {
    return std::find_if( _pcuts.begin(), _pcend, [&cut]( auto const* other ) { return other->dominates( cut ); } ) != _pcend;
  }

  static bool sort_delay( CutType const& c1, CutType const& c2 )
  {
    constexpr auto eps{0.005f};
    if ( c1->data.delay < c2->data.delay - eps )
      return true;
    if ( c1->data.delay > c2->data.delay + eps )
      return false;
    if ( c1->data.flow < c2->data.flow - eps )
      return true;
    if ( c1->data.flow > c2->data.flow + eps )
      return false;
    return c1.size() < c2.size();
  }

  static bool sort_area( CutType const& c1, CutType const& c2 )
  {
    constexpr auto eps{0.005f};
    if ( c1->data.flow < c2->data.flow - eps )
      return true;
    if ( c1->data.flow > c2->data.flow + eps )
      return false;
    if ( c1.size() < c2.size() )
      return true;
    if ( c1.size() > c2.size() )
      return false;
    return c1->data.delay < c2->data.delay - eps;
  }

  /*! \brief Compare two cuts using sorting functions.
   *
   * This method compares two cuts using a sorting function.
   *
   * \param cut1 first cut.
   * \param cut2 second cut.
   * \param sort sorting function.
   */
  static bool compare( CutType const& cut1, CutType const& cut2, emap_cut_sort_type sort = emap_cut_sort_type::NONE  )
  {
    if ( sort == emap_cut_sort_type::DELAY )
    {
      return sort_delay( cut1, cut2 );
    }
    else if ( sort == emap_cut_sort_type::AREA )
    {
      return sort_area( cut1, cut2 );
    }
    else
    {
      return false;
    }
  }

  /*! \brief Inserts a cut into a set without checking dominance.
   *
   * This method will insert a cut into a set and maintain an order.  This
   * method doesn't remove the cuts that are dominated by `cut`.
   *
   * If `cut` is dominated by any of the cuts in the set, it will still be
   * inserted.  The caller is responsible to check whether `cut` is dominated
   * before inserting it into the set.
   *
   * \param cut Cut to insert.
   * \param sort Cut prioritization function.
   */
  void simple_insert( CutType const& cut, emap_cut_sort_type sort = emap_cut_sort_type::NONE )
  {
    /* insert cut in a sorted way */
    typename std::array<CutType*, MaxCuts>::iterator ipos = _pcuts.begin();

    if ( sort == emap_cut_sort_type::DELAY )
    {
      ipos = std::lower_bound( _pcuts.begin(), _pend, &cut, []( auto a, auto b ) { return sort_delay( *a, *b ); } );
    }
    else if ( sort == emap_cut_sort_type::AREA )
    {
      ipos = std::lower_bound( _pcuts.begin(), _pend, &cut, []( auto a, auto b ) { return sort_area( *a, *b ); } );
    }
    else /* NONE */
    {
       ipos == _pend;
    }

    /* too many cuts, we need to remove one */
    if ( _pend == _pcuts.end() )
    {
      /* cut to be inserted is worse than all the others, return */
      if ( ipos == _pend )
      {
        return;
      }
      else
      {
        /* remove last cut */
        --_pend;
        --_pcend;
      }
    }

    /* copy cut */
    auto& icut = *_pend;
    icut->set_leaves( cut.begin(), cut.end() );
    icut->data() = cut.data();

    if ( ipos != _pend )
    {
      auto it = _pend;
      while ( it > ipos )
      {
        std::swap( *it, *( it - 1 ) );
        --it;
      }
    }

    /* update iterators */
    _pcend++;
    _pend++;
  }

  /*! \brief Inserts a cut into a set.
   *
   * This method will insert a cut into a set and maintain an order.  Before the
   * cut is inserted into the correct position, it will remove all cuts that are
   * dominated by `cut`. Variable `skip0` tell to skip the dominance check on
   * cut zero.
   *
   * If `cut` is dominated by any of the cuts in the set, it will still be
   * inserted.  The caller is responsible to check whether `cut` is dominated
   * before inserting it into the set.
   *
   * \param cut Cut to insert.
   * \param skip0 Skip dominance check on cut zero.
   * \param sort Cut prioritization function.
   */
  void insert( CutType const& cut, bool skip0 = false, emap_cut_sort_type sort = emap_cut_sort_type::NONE )
  {
    auto begin = _pcuts.begin();

    if ( skip0 && _pend != _pcuts.begin() )
      ++begin;

    /* remove elements that are dominated by new cut */
    _pcend = _pend = std::stable_partition( begin, _pend, [&cut]( auto const* other ) { return !cut.dominates( *other ); } );

    /* insert cut in a sorted way */
    simple_insert( cut, sort );
  }

  /*! \brief Replaces a cut of the set.
   *
   * This method replaces the cut at position `index` in the set by `cut`
   * and maintains the cuts order. The function does not check whether
   * index is in the valid range.
   *
   * \param index Index of the cut to replace.
   * \param cut Cut to insert.
   */
  void replace( uint32_t index, CutType const& cut )
  {
    *_pcuts[index] = cut;
  }

  /*! \brief Begin iterator (constant).
   *
   * The iterator will point to a cut pointer.
   */
  auto begin() const { return _pcuts.begin(); }

  /*! \brief End iterator (constant). */
  auto end() const { return _pcend; }

  /*! \brief Begin iterator (mutable).
   *
   * The iterator will point to a cut pointer.
   */
  auto begin() { return _pcuts.begin(); }

  /*! \brief End iterator (mutable). */
  auto end() { return _pend; }

  /*! \brief Number of cuts in the set. */
  auto size() const { return _pcend - _pcuts.begin(); }

  /*! \brief Returns reference to cut at index.
   *
   * This function does not return the cut pointer but dereferences it and
   * returns a reference.  The function does not check whether index is in the
   * valid range.
   *
   * \param index Index
   */
  auto const& operator[]( uint32_t index ) const { return *_pcuts[index]; }

  /*! \brief Returns the best cut, i.e., the first cut.
   */
  auto const& best() const { return *_pcuts[0]; }

  /*! \brief Updates the best cut.
   *
   * This method will set the cut at index `index` to be the best cut.  All
   * cuts before `index` will be moved one position higher.
   *
   * \param index Index of new best cut
   */
  void update_best( uint32_t index )
  {
    auto* best = _pcuts[index];
    for ( auto i = index; i > 0; --i )
    {
      _pcuts[i] = _pcuts[i - 1];
    }
    _pcuts[0] = best;
  }

  /*! \brief Resize the cut set, if it is too large.
   *
   * This method will resize the cut set to `size` only if the cut set has more
   * than `size` elements.  Otherwise, the size will remain the same.
   */
  void limit( uint32_t size )
  {
    if ( std::distance( _pcuts.begin(), _pend ) > static_cast<long>( size ) )
    {
      _pcend = _pend = _pcuts.begin() + size;
    }
  }

  /*! \brief Prints a cut set. */
  friend std::ostream& operator<<( std::ostream& os, emap_cut_set const& set )
  {
    for ( auto const& c : set )
    {
      os << *c << "\n";
    }
    return os;
  }

private:
  std::array<CutType, MaxCuts> _cuts;
  std::array<CutType*, MaxCuts> _pcuts;
  typename std::array<CutType*, MaxCuts>::const_iterator _pcend{_pcuts.begin()};
  typename std::array<CutType*, MaxCuts>::iterator _pend{_pcuts.begin()};
};
#pragma endregion

template<unsigned NInputs>
struct node_match_flex
{
  /* best gate match for positive and negative output phases */
  supergate<NInputs> const* best_supergate[2] = { nullptr, nullptr };
  /* fanin pin phases for both output phases */
  uint8_t phase[2];
  /* best cut index for both phases */
  uint32_t best_cut[2];
  /* node is mapped using only one phase */
  bool same_match{ false };

  /* arrival time at node output */
  double arrival[2];
  /* required time at node output */
  double required[2];
  /* area of the best matches */
  float area[2];

  /* number of references in the cover 0: pos, 1: neg, 2: pos+neg */
  uint32_t map_refs[3];
  /* references estimation */
  float est_refs[3];
  /* area flow */
  float flows[3];
};

template<class Ntk, unsigned CutSize, unsigned NInputs, classification_type Configuration>
class emap_impl
{
public:
  static constexpr uint32_t max_cut_num = 250;
  using cut_t = cut_type<true, cut_enumeration_emap_cut<NInputs>>;
  using cut_set_t = emap_cut_set<cut_t, max_cut_num>;
  using cut_merge_t = typename std::array<cut_set_t*, Ntk::max_fanin_size + 1>;
  using klut_map = std::unordered_map<uint32_t, std::array<signal<klut_network>, 2>>;
  using tt_cache = truth_table_cache<kitty::static_truth_table<CutSize>>;

public:
  explicit emap_impl( Ntk const& ntk, tech_library<NInputs, Configuration> const& library, emap_params const& ps, emap_stats& st )
      : ntk( ntk ),
        library( library ),
        ps( ps ),
        st( st ),
        node_match( ntk.size() ),
        switch_activity( ps.eswp_rounds ? switching_activity( ntk, ps.switching_activity_patterns ) : std::vector<float>( 0 ) ),
        cuts( ntk.size() )
  {
    std::tie( lib_inv_area, lib_inv_delay, lib_inv_id ) = library.get_inverter_info();
    std::tie( lib_buf_area, lib_buf_delay, lib_buf_id ) = library.get_buffer_info();

    kitty::static_truth_table<CutSize> zero, proj;
    kitty::create_nth_var( proj, 0u );

    truth_tables.insert( zero );
    truth_tables.insert( proj );
  }

  explicit emap_impl( Ntk const& ntk, tech_library<NInputs, Configuration> const& library, std::vector<float> const& switch_activity, emap_params const& ps, emap_stats& st )
      : ntk( ntk ),
        library( library ),
        ps( ps ),
        st( st ),
        node_match( ntk.size() ),
        switch_activity( switch_activity ),
        cuts( ntk.size() )
  {
    std::tie( lib_inv_area, lib_inv_delay, lib_inv_id ) = library.get_inverter_info();
    std::tie( lib_buf_area, lib_buf_delay, lib_buf_id ) = library.get_buffer_info();

    kitty::static_truth_table<CutSize> zero, proj;
    kitty::create_nth_var( proj, 0u );

    truth_tables.insert( zero );
    truth_tables.insert( proj );
  }

  binding_view<klut_network> run()
  {
    stopwatch t( st.time_mapping );

    auto [res, old2new] = initialize_map_network();

    /* compute and save topological order */
    top_order.reserve( ntk.size() );
    topo_view<Ntk>( ntk ).foreach_node( [this]( auto n ) {
      top_order.push_back( n );
    } );

    /* compute cuts, matches, and initial mapping */
    if ( !ps.skip_delay_round )
    {
      if ( !compute_mapping_match<false>() )
      {
        return res;
      }
    }
    else
    {
      if ( !compute_mapping_match<true>() )
      {
        return res;
      }
    }

    /* compute mapping using global area flow */
    while ( iteration < ps.area_flow_rounds + 1 )
    {
      compute_required_time();
      if ( !compute_mapping<true>() )
      {
        return res;
      }
    }

    /* compute mapping using exact area */
    while ( iteration < ps.ela_rounds + ps.area_flow_rounds + 1 )
    {
      compute_required_time();
      if ( !compute_mapping_exact<false>() )
      {
        return res;
      }
    }

    /* compute mapping using exact switching activity estimation */
    while ( iteration < ps.eswp_rounds + ps.ela_rounds + ps.area_flow_rounds + 1 )
    {
      compute_required_time();
      if ( !compute_mapping_exact<true>() )
      {
        return res;
      }
    }

    /* insert buffers for POs driven by PIs */
    insert_buffers();

    /* generate the output network */
    finalize_cover( res, old2new );

    return res;
  }

private:
  template<bool DO_AREA>
  bool compute_mapping_match()
  {
    for ( auto const& n : top_order )
    {
      auto const index = ntk.node_to_index( n );
      auto& node_data = node_match[index];

      node_data.est_refs[0] = node_data.est_refs[1] = node_data.est_refs[2] = static_cast<float>( ntk.fanout_size( n ) );
      node_data.map_refs[0] = node_data.map_refs[1] = node_data.map_refs[2] = ntk.fanout_size( n );
      node_data.required[0] = node_data.required[1] = std::numeric_limits<double>::max();

      if ( ntk.is_constant( n ) )
      {
        /* all terminals have flow 1.0 */
        node_data.flows[0] = node_data.flows[1] = node_data.flows[2] = 0.0f;
        node_data.arrival[0] = node_data.arrival[1] = 0.0f;
        add_zero_cut( index );
        match_constants( index );
        continue;
      }
      else if ( ntk.is_pi( n ) )
      {
        /* all terminals have flow 1.0 */
        node_data.flows[0] = node_data.flows[1] = node_data.flows[2] = 0.0f;
        node_data.arrival[0] = 0.0f;
        /* PIs have the negative phase implemented with an inverter */
        node_data.arrival[1] = lib_inv_delay;
        add_unit_cut( index );
        continue;
      }

      /* compute cuts for node */
      if constexpr ( Ntk::min_fanin_size == 2 && Ntk::max_fanin_size == 2 )
      {
        merge_cuts2<DO_AREA>( n );
      }
      else
      {
        merge_cuts<DO_AREA>( n );
      }

      /* match positive phase */
      match_phase<DO_AREA>( n, 0u );

      /* match negative phase */
      match_phase<DO_AREA>( n, 1u );

      /* try to drop one phase */
      match_drop_phase<DO_AREA, false>( n, 0 );
    }

    double area_old = area;
    bool success = set_mapping_refs<false>();

    /* round stats */
    if ( ps.verbose )
    {
      std::stringstream stats{};
      float area_gain = 0.0f;

      if ( iteration != 1 )
        area_gain = float( ( area_old - area ) / area_old * 100 );

      if constexpr ( DO_AREA )
      {
        stats << fmt::format( "[i] AreaFlow : Delay = {:>12.2f}  Area = {:>12.2f}  {:>5.2f} %\n", delay, area, area_gain );
      }
      else
      {
        stats << fmt::format( "[i] Delay    : Delay = {:>12.2f}  Area = {:>12.2f}  {:>5.2f} %\n", delay, area, area_gain );
      }
      st.round_stats.push_back( stats.str() );
    }

    return success;
  }

  template<bool DO_AREA>
  void merge_cuts2( node<Ntk> const& n )
  {
    auto index = ntk.node_to_index( n );
    auto& node_data = node_match[index];
    emap_cut_sort_type sort = emap_cut_sort_type::DELAY;

    if constexpr ( DO_AREA )
    {
      sort = emap_cut_sort_type::AREA;
    }

    /* compute cuts */
    const auto fanin = 2;
    uint32_t pairs{1};
    ntk.foreach_fanin( ntk.index_to_node( index ), [this, &pairs]( auto child, auto i ) {
      lcuts[i] = &cuts[ntk.node_to_index( ntk.get_node( child ) )];
      pairs *= static_cast<uint32_t>( lcuts[i]->size() );
    } );
    lcuts[2] = &cuts[index];
    auto& rcuts = *lcuts[fanin];

    cut_t new_cut;
    std::vector<cut_t const*> vcuts( fanin );

    for ( auto const& c1 : *lcuts[0] )
    {
      for ( auto const& c2 : *lcuts[1] )
      {
        if ( !c1->merge( *c2, new_cut, CutSize ) )
        {
          continue;
        }

        if ( ps.remove_dominated_cuts && rcuts.is_dominated( new_cut ) )
        {
          continue;
        }

        /* compute function */
        vcuts[0] = c1;
        vcuts[1] = c2;
        new_cut->func_id = compute_truth_table( index, vcuts, new_cut );

        /* match cut and compute data */
        compute_cut_data<DO_AREA>( new_cut, n );

        if ( ps.remove_dominated_cuts )
          rcuts.insert( new_cut, false, sort );
        else
          rcuts.simple_insert( new_cut, sort );
      }
    }

    cuts_total += rcuts.size();

    /* limit the maximum number of cuts */
    rcuts.limit( ps.cut_enumeration_ps.cut_limit );

    /* add trivial cut */
    if ( rcuts.size() > 1 || ( *rcuts.begin() )->size() > 1 )
    {
      add_unit_cut( index );
    }
  }

  template<bool DO_AREA>
  void merge_cuts( node<Ntk> const& n )
  {
    auto index = ntk.node_to_index( n );
    auto& node_data = node_match[index];
    emap_cut_sort_type sort = emap_cut_sort_type::AREA;
    cut_t best_cut;

    if constexpr ( DO_AREA )
    {
      sort = emap_cut_sort_type::AREA;
    }

    /* compute cuts */
    uint32_t pairs{1};
    std::vector<uint32_t> cut_sizes;
    ntk.foreach_fanin( ntk.index_to_node( index ), [this, &pairs, &cut_sizes]( auto child, auto i ) {
      lcuts[i] = &cuts[ntk.node_to_index( ntk.get_node( child ) )];
      cut_sizes.push_back( static_cast<uint32_t>( lcuts[i]->size() ) );
      pairs *= cut_sizes.back();
    } );
    const auto fanin = cut_sizes.size();
    lcuts[fanin] = &cuts[index];
    auto& rcuts = *lcuts[fanin];

    if ( fanin > 1 && fanin <= ps.cut_enumeration_ps.fanin_limit )
    {
      cut_t new_cut, tmp_cut;

      std::vector<cut_t const*> vcuts( fanin );

      foreach_mixed_radix_tuple( cut_sizes.begin(), cut_sizes.end(), [&]( auto begin, auto end ) {
        auto it = vcuts.begin();
        auto i = 0u;
        while ( begin != end )
        {
          *it++ = &( ( *lcuts[i++] )[*begin++] );
        }

        if ( !vcuts[0]->merge( *vcuts[1], new_cut, CutSize ) )
        {
          return true; /* continue */
        }

        for ( i = 2; i < fanin; ++i )
        {
          tmp_cut = new_cut;
          if ( !vcuts[i]->merge( tmp_cut, new_cut, CutSize ) )
          {
            return true; /* continue */
          }
        }

        if ( ps.remove_dominated_cuts && rcuts.is_dominated( new_cut ) )
        {
          return true; /* continue */
        }

        new_cut->func_id = compute_truth_table( index, vcuts, new_cut );

        /* match cut and compute data */
        compute_cut_data<DO_AREA>( new_cut, n );

        if ( ps.remove_dominated_cuts )
          rcuts.insert( new_cut, false, sort );
        else
          rcuts.simple_insert( new_cut, sort );

        return true;
      } );

      /* limit the maximum number of cuts */
      rcuts.limit( ps.cut_enumeration_ps.cut_limit );
    }
    else if ( fanin == 1 )
    {
      for ( auto const& cut : *lcuts[0] ) {
        cut_t new_cut = *cut;

        new_cut->func_id = compute_truth_table( index, {cut}, new_cut );

        /* match cut and compute data */
        compute_cut_data<DO_AREA>( new_cut, n );

        if ( ps.remove_dominated_cuts )
          rcuts.insert( new_cut, false, sort );
        else
          rcuts.simple_insert( new_cut, sort );
      }

      /* limit the maximum number of cuts */
      rcuts.limit( ps.cut_enumeration_ps.cut_limit );
    }

    cuts_total += rcuts.size();

    add_unit_cut( index );
  }

  template<bool DO_AREA>
  bool compute_mapping()
  {
    for ( auto const& n : top_order )
    {
      if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
      {
        continue;
      }

      /* match positive phase */
      match_phase<DO_AREA>( n, 0u );

      /* match negative phase */
      match_phase<DO_AREA>( n, 1u );

      /* try to drop one phase */
      match_drop_phase<DO_AREA, false>( n, 0 );
    }

    double area_old = area;
    bool success = set_mapping_refs<false>();

    /* round stats */
    if ( ps.verbose )
    {
      std::stringstream stats{};
      float area_gain = 0.0f;

      if ( iteration != 1 )
        area_gain = float( ( area_old - area ) / area_old * 100 );

      if constexpr ( DO_AREA )
      {
        stats << fmt::format( "[i] AreaFlow : Delay = {:>12.2f}  Area = {:>12.2f}  {:>5.2f} %\n", delay, area, area_gain );
      }
      else
      {
        stats << fmt::format( "[i] Delay    : Delay = {:>12.2f}  Area = {:>12.2f}  {:>5.2f} %\n", delay, area, area_gain );
      }
      st.round_stats.push_back( stats.str() );
    }

    return success;
  }

  template<bool SwitchActivity>
  bool compute_mapping_exact()
  {
    for ( auto const& n : top_order )
    {
      if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
        continue;

      auto index = ntk.node_to_index( n );
      auto& node_data = node_match[index];

      /* recursively deselect the best cut shared between
       * the two phases if in use in the cover */
      if ( node_data.same_match && node_data.map_refs[2] != 0 )
      {
        if ( node_data.best_supergate[0] != nullptr )
          cut_deref<SwitchActivity>( cuts[index][node_data.best_cut[0]], n, 0u );
        else
          cut_deref<SwitchActivity>( cuts[index][node_data.best_cut[1]], n, 1u );
      }

      /* match positive phase */
      match_phase_exact<SwitchActivity>( n, 0u );

      /* match negative phase */
      match_phase_exact<SwitchActivity>( n, 1u );

      /* try to drop one phase */
      match_drop_phase<true, true>( n, 0 );
    }

    double area_old = area;
    bool success = set_mapping_refs<true>();

    /* round stats */
    if ( ps.verbose )
    {
      float area_gain = float( ( area_old - area ) / area_old * 100 );
      std::stringstream stats{};
      if constexpr ( SwitchActivity )
        stats << fmt::format( "[i] Switching: Delay = {:>12.2f}  Area = {:>12.2f}  {:>5.2f} %\n", delay, area, area_gain );
      else
        stats << fmt::format( "[i] Area     : Delay = {:>12.2f}  Area = {:>12.2f}  {:>5.2f} %\n", delay, area, area_gain );
      st.round_stats.push_back( stats.str() );
    }

    return success;
  }

  template<bool ELA>
  bool set_mapping_refs()
  {
    if constexpr ( !ELA )
    {
      for ( auto i = 0u; i < node_match.size(); ++i )
      {
        node_match[i].map_refs[0] = node_match[i].map_refs[1] = node_match[i].map_refs[2] = 0u;
      }
    }

    /* compute the current worst delay and update the mapping refs */
    delay = 0.0f;
    ntk.foreach_po( [this]( auto s ) {
      const auto index = ntk.node_to_index( ntk.get_node( s ) );

      if ( ntk.is_complemented( s ) )
        delay = std::max( delay, node_match[index].arrival[1] );
      else
        delay = std::max( delay, node_match[index].arrival[0] );

      if constexpr ( !ELA )
      {
        node_match[index].map_refs[2]++;
        if ( ntk.is_complemented( s ) )
          node_match[index].map_refs[1]++;
        else
          node_match[index].map_refs[0]++;
      }
    } );

    /* compute current area and update mapping refs in top-down order */
    area = 0.0f;
    for ( auto it = top_order.rbegin(); it != top_order.rend(); ++it )
    {
      const auto index = ntk.node_to_index( *it );
      auto& node_data = node_match[index];

      /* skip constants and PIs */
      if ( ntk.is_constant( *it ) )
      {
        if ( node_match[index].map_refs[2] > 0u )
        {
          /* if used and not available in the library launch a mapping error */
          if ( node_data.best_supergate[0] == nullptr && node_data.best_supergate[1] == nullptr )
          {
            std::cerr << "[i] MAP ERROR: technology library does not contain constant gates, impossible to perform mapping" << std::endl;
            st.mapping_error = true;
            return false;
          }
        }
        continue;
      }
      else if ( ntk.is_pi( *it ) )
      {
        if ( node_match[index].map_refs[1] > 0u )
        {
          /* Add inverter area over the negated fanins */
          area += lib_inv_area;
        }
        continue;
      }

      /* continue if not referenced in the cover */
      if ( node_match[index].map_refs[2] == 0u )
        continue;

      unsigned use_phase = node_data.best_supergate[0] == nullptr ? 1u : 0u;

      if ( node_data.best_supergate[use_phase] == nullptr )
      {
        /* Library is not complete, mapping is not possible */
        std::cerr << "[i] MAP ERROR: technology library is not complete, impossible to perform mapping" << std::endl;
        st.mapping_error = true;
        return false;
      }

      if ( node_data.same_match || node_data.map_refs[use_phase] > 0 )
      {
        if constexpr ( !ELA )
        {
          auto const& best_cut = cuts[index][node_data.best_cut[use_phase]];
          auto ctr = 0u;

          for ( auto const leaf : best_cut )
          {
            node_match[leaf].map_refs[2]++;
            if ( ( node_data.phase[use_phase] >> ctr++ ) & 1 )
              node_match[leaf].map_refs[1]++;
            else
              node_match[leaf].map_refs[0]++;
          }
        }
        area += node_data.area[use_phase];
        if ( node_data.same_match && node_data.map_refs[use_phase ^ 1] > 0 )
        {
          area += lib_inv_area;
        }
      }

      /* invert the phase */
      use_phase = use_phase ^ 1;

      /* if both phases are implemented and used */
      if ( !node_data.same_match && node_data.map_refs[use_phase] > 0 )
      {
        if constexpr ( !ELA )
        {
          auto const& best_cut = cuts[index][node_data.best_cut[use_phase]];
          auto ctr = 0u;
          for ( auto const leaf : best_cut )
          {
            node_match[leaf].map_refs[2]++;
            if ( ( node_data.phase[use_phase] >> ctr++ ) & 1 )
              node_match[leaf].map_refs[1]++;
            else
              node_match[leaf].map_refs[0]++;
          }
        }
        area += node_data.area[use_phase];
      }
    }

    /* blend estimated references */
    for ( auto i = 0u; i < ntk.size(); ++i )
    {
      node_match[i].est_refs[2] = ( 2.0 * node_match[i].est_refs[2] + 1.0f * node_match[i].map_refs[2] ) / 3.0;
      node_match[i].est_refs[1] = ( 2.0 * node_match[i].est_refs[1] + 1.0f * node_match[i].map_refs[1] ) / 3.0;
      node_match[i].est_refs[0] = ( 2.0 * node_match[i].est_refs[0] + 1.0f * node_match[i].map_refs[0] ) / 3.0;
    }

    ++iteration;
    return true;
  }

  void compute_required_time()
  {
    for ( auto i = 0u; i < node_match.size(); ++i )
    {
      node_match[i].required[0] = node_match[i].required[1] = std::numeric_limits<double>::max();
    }

    /* return in case of `skip_delay_round` */
    if ( iteration == 0 )
      return;

    auto required = delay;

    if ( ps.required_time != 0.0f )
    {
      /* Global target time constraint */
      if ( ps.required_time < delay - epsilon )
      {
        if ( !ps.skip_delay_round && iteration == 1 )
          std::cerr << fmt::format( "[i] MAP WARNING: cannot meet the target required time of {:.2f}", ps.required_time ) << std::endl;
      }
      else
      {
        required = ps.required_time;
      }
    }

    /* set the required time at POs */
    ntk.foreach_po( [&]( auto const& s ) {
      const auto index = ntk.node_to_index( ntk.get_node( s ) );
      if ( ntk.is_complemented( s ) )
        node_match[index].required[1] = required;
      else
        node_match[index].required[0] = required;
    } );

    /* propagate required time to the PIs */
    for ( auto it = top_order.rbegin(); it != top_order.rend(); ++it )
    {
      if ( ntk.is_pi( *it ) || ntk.is_constant( *it ) )
        break;

      const auto index = ntk.node_to_index( *it );

      if ( node_match[index].map_refs[2] == 0 )
        continue;

      auto& node_data = node_match[index];

      unsigned use_phase = node_data.best_supergate[0] == nullptr ? 1u : 0u;
      unsigned other_phase = use_phase ^ 1;

      assert( node_data.best_supergate[0] != nullptr || node_data.best_supergate[1] != nullptr );
      assert( node_data.map_refs[0] || node_data.map_refs[1] );

      /* propagate required time over the output inverter if present */
      if ( node_data.same_match && node_data.map_refs[other_phase] > 0 )
      {
        node_data.required[use_phase] = std::min( node_data.required[use_phase], node_data.required[other_phase] - lib_inv_delay );
      }

      if ( node_data.same_match || node_data.map_refs[use_phase] > 0 )
      {
        auto ctr = 0u;
        auto best_cut = cuts[index][node_data.best_cut[use_phase]];
        auto const& supergate = node_data.best_supergate[use_phase];
        for ( auto leaf : best_cut )
        {
          auto phase = ( node_data.phase[use_phase] >> ctr ) & 1;
          node_match[leaf].required[phase] = std::min( node_match[leaf].required[phase], node_data.required[use_phase] - supergate->tdelay[ctr] );
          ++ctr;
        }
      }

      if ( !node_data.same_match && node_data.map_refs[other_phase] > 0 )
      {
        auto ctr = 0u;
        auto best_cut = cuts[index][node_data.best_cut[other_phase]];
        auto const& supergate = node_data.best_supergate[other_phase];
        for ( auto leaf : best_cut )
        {
          auto phase = ( node_data.phase[other_phase] >> ctr ) & 1;
          node_match[leaf].required[phase] = std::min( node_match[leaf].required[phase], node_data.required[other_phase] - supergate->tdelay[ctr] );
          ++ctr;
        }
      }
    }
  }

  template<bool DO_AREA>
  void match_phase( node<Ntk> const& n, uint8_t phase )
  {
    double best_arrival = std::numeric_limits<double>::max();
    double best_area_flow = std::numeric_limits<double>::max();
    float best_area = std::numeric_limits<float>::max();
    uint32_t best_size = UINT32_MAX;
    uint8_t best_cut = 0u;
    uint8_t best_phase = 0u;
    uint8_t cut_index = 0u;
    auto index = ntk.node_to_index( n );

    auto& node_data = node_match[index];
    supergate<NInputs> const* best_supergate = node_data.best_supergate[phase];

    /* recompute best match info */
    if ( best_supergate != nullptr )
    {
      auto const& cut = cuts[index][node_data.best_cut[phase]];

      best_phase = node_data.phase[phase];
      best_arrival = 0.0f;
      best_area_flow = best_supergate->area + cut_leaves_flow( cut, n, phase );
      best_area = best_supergate->area;
      best_cut = node_data.best_cut[phase];
      best_size = cut.size();

      auto ctr = 0u;
      for ( auto l : cut )
      {
        double arrival_pin = node_match[l].arrival[( best_phase >> ctr ) & 1] + best_supergate->tdelay[ctr];
        best_arrival = std::max( best_arrival, arrival_pin );
        ++ctr;
      }
    }

    /* foreach cut */
    for ( auto& cut : cuts[index] )
    {
      /* trivial cuts or not matched cuts */
      if ( ( *cut )->data.ignore )
      {
        ++cut_index;
        continue;
      }

      auto const& supergates = ( *cut )->data.supergates;
      auto const negation = ( *cut )->data.negations[phase];

      if ( supergates[phase] == nullptr )
      {
        ++cut_index;
        continue;
      }

      /* match each gate and take the best one */
      for ( auto const& gate : *supergates[phase] )
      {
        uint8_t gate_polarity = gate.polarity ^ negation;
        node_data.phase[phase] = gate_polarity;
        double area_local = gate.area + cut_leaves_flow( *cut, n, phase );
        double worst_arrival = 0.0f;

        auto ctr = 0u;
        for ( auto l : *cut )
        {
          double arrival_pin = node_match[l].arrival[( gate_polarity >> ctr ) & 1] + gate.tdelay[ctr];
          worst_arrival = std::max( worst_arrival, arrival_pin );
          ++ctr;
        }

        if constexpr ( DO_AREA )
        {
          if ( worst_arrival > node_data.required[phase] + epsilon )
            continue;
        }

        if ( compare_map<DO_AREA>( worst_arrival, best_arrival, area_local, best_area_flow, cut->size(), best_size ) )
        {
          best_arrival = worst_arrival;
          best_area_flow = area_local;
          best_size = cut->size();
          best_cut = cut_index;
          best_area = gate.area;
          best_phase = gate_polarity;
          best_supergate = &gate;
        }
      }

      ++cut_index;
    }

    node_data.flows[phase] = best_area_flow;
    node_data.arrival[phase] = best_arrival;
    node_data.area[phase] = best_area;
    node_data.best_cut[phase] = best_cut;
    node_data.phase[phase] = best_phase;
    node_data.best_supergate[phase] = best_supergate;
  }

  template<bool SwitchActivity>
  void match_phase_exact( node<Ntk> const& n, uint8_t phase )
  {
    double best_arrival = std::numeric_limits<double>::max();
    float best_exact_area = std::numeric_limits<float>::max();
    float best_area = std::numeric_limits<float>::max();
    uint32_t best_size = UINT32_MAX;
    uint8_t best_cut = 0u;
    uint8_t best_phase = 0u;
    uint8_t cut_index = 0u;
    auto index = ntk.node_to_index( n );

    auto& node_data = node_match[index];
    supergate<NInputs> const* best_supergate = node_data.best_supergate[phase];

    /* recompute best match info */
    if ( best_supergate != nullptr )
    {
      auto const& cut = cuts[index][node_data.best_cut[phase]];

      best_phase = node_data.phase[phase];
      best_arrival = 0.0f;
      best_area = best_supergate->area;
      best_cut = node_data.best_cut[phase];
      best_size = cut.size();

      auto ctr = 0u;
      for ( auto l : cut )
      {
        double arrival_pin = node_match[l].arrival[( best_phase >> ctr ) & 1] + best_supergate->tdelay[ctr];
        best_arrival = std::max( best_arrival, arrival_pin );
        ++ctr;
      }

      /* if cut is implemented, remove it from the cover */
      if ( !node_data.same_match && node_data.map_refs[phase] )
      {
        best_exact_area = cut_deref<SwitchActivity>( cuts[index][best_cut], n, phase );
      }
      else
      {
        best_exact_area = cut_ref<SwitchActivity>( cuts[index][best_cut], n, phase );
        cut_deref<SwitchActivity>( cuts[index][best_cut], n, phase );
      }
    }

    /* foreach cut */
    for ( auto& cut : cuts[index] )
    {
      /* trivial cuts or not matched cuts */
      if ( ( *cut )->data.ignore )
      {
        ++cut_index;
        continue;
      }

      auto const& supergates = ( *cut )->data.supergates;
      auto const negation = ( *cut )->data.negations[phase];

      if ( supergates[phase] == nullptr )
      {
        ++cut_index;
        continue;
      }

      /* match each gate and take the best one */
      for ( auto const& gate : *supergates[phase] )
      {
        uint8_t gate_polarity = gate.polarity ^ negation;
        node_data.phase[phase] = gate_polarity;
        node_data.area[phase] = gate.area;
        float area_exact = cut_ref<SwitchActivity>( *cut, n, phase );
        cut_deref<SwitchActivity>( *cut, n, phase );
        double worst_arrival = 0.0f;

        auto ctr = 0u;
        for ( auto l : *cut )
        {
          double arrival_pin = node_match[l].arrival[( gate_polarity >> ctr ) & 1] + gate.tdelay[ctr];
          worst_arrival = std::max( worst_arrival, arrival_pin );
          ++ctr;
        }

        if ( worst_arrival > node_data.required[phase] + epsilon )
          continue;

        if ( compare_map<true>( worst_arrival, best_arrival, area_exact, best_exact_area, cut->size(), best_size ) )
        {
          best_arrival = worst_arrival;
          best_exact_area = area_exact;
          best_area = gate.area;
          best_size = cut->size();
          best_cut = cut_index;
          best_phase = gate_polarity;
          best_supergate = &gate;
        }
      }

      ++cut_index;
    }

    node_data.flows[phase] = best_exact_area;
    node_data.arrival[phase] = best_arrival;
    node_data.area[phase] = best_area;
    node_data.best_cut[phase] = best_cut;
    node_data.phase[phase] = best_phase;
    node_data.best_supergate[phase] = best_supergate;

    if ( !node_data.same_match && node_data.map_refs[phase] )
    {
      best_exact_area = cut_ref<SwitchActivity>( cuts[index][best_cut], n, phase );
    }
  }

  template<bool DO_AREA, bool ELA>
  void match_drop_phase( node<Ntk> const& n, float required_margin_factor )
  {
    auto index = ntk.node_to_index( n );
    auto& node_data = node_match[index];

    /* compute arrival adding an inverter to the other match phase */
    double worst_arrival_npos = node_data.arrival[1] + lib_inv_delay;
    double worst_arrival_nneg = node_data.arrival[0] + lib_inv_delay;
    bool use_zero = false;
    bool use_one = false;

    /* only one phase is matched */
    if ( node_data.best_supergate[0] == nullptr )
    {
      set_match_complemented_phase( index, 1, worst_arrival_npos );
      if constexpr ( ELA )
      {
        if ( node_data.map_refs[2] )
          cut_ref<false>( cuts[index][node_data.best_cut[1]], n, 1 );
      }
      return;
    }
    else if ( node_data.best_supergate[1] == nullptr )
    {
      set_match_complemented_phase( index, 0, worst_arrival_nneg );
      if constexpr ( ELA )
      {
        if ( node_data.map_refs[2] )
          cut_ref<false>( cuts[index][node_data.best_cut[0]], n, 0 );
      }
      return;
    }

    /* try to use only one match to cover both phases */
    if constexpr ( !DO_AREA )
    {
      /* if arrival improves matching the other phase and inserting an inverter */
      if ( worst_arrival_npos < node_data.arrival[0] + epsilon )
      {
        use_one = true;
      }
      if ( worst_arrival_nneg < node_data.arrival[1] + epsilon )
      {
        use_zero = true;
      }
    }
    else
    {
      /* check if both phases + inverter meet the required time */
      use_zero = worst_arrival_nneg < ( node_data.required[1] + epsilon - required_margin_factor * lib_inv_delay );
      use_one = worst_arrival_npos < ( node_data.required[0] + epsilon - required_margin_factor * lib_inv_delay );
    }

    /* condition on not used phases, evaluate a substitution during exact area recovery */
    if constexpr ( ELA )
    {
      if ( iteration != 0 )
      {
        if ( node_data.map_refs[0] == 0 || node_data.map_refs[1] == 0 )
        {
          /* select the used match */
          auto phase = 0;
          auto nphase = 0;
          if ( node_data.map_refs[0] == 0 )
          {
            phase = 1;
            use_one = true;
            use_zero = false;
          }
          else
          {
            nphase = 1;
            use_one = false;
            use_zero = true;
          }
          /* select the not used match instead if it leads to area improvement and doesn't violate the required time */
          if ( node_data.arrival[nphase] + lib_inv_delay < node_data.required[phase] + epsilon )
          {
            auto size_phase = cuts[index][node_data.best_cut[phase]].size();
            auto size_nphase = cuts[index][node_data.best_cut[nphase]].size();

            if ( compare_map<DO_AREA>( node_data.arrival[nphase] + lib_inv_delay, node_data.arrival[phase], node_data.flows[nphase] + lib_inv_area, node_data.flows[phase], size_nphase, size_phase ) )
            {
              /* invert the choice */
              use_zero = !use_zero;
              use_one = !use_one;
            }
          }
        }
      }
    }

    if ( ( !use_zero && !use_one ) )
    {
      /* use both phases */
      node_data.flows[0] = node_data.flows[0] / node_data.est_refs[0];
      node_data.flows[1] = node_data.flows[1] / node_data.est_refs[1];
      node_data.flows[2] = node_data.flows[0] + node_data.flows[1];
      node_data.same_match = false;
      return;
    }

    /* use area flow as a tiebreaker */
    if ( use_zero && use_one )
    {
      auto size_zero = cuts[index][node_data.best_cut[0]].size();
      auto size_one = cuts[index][node_data.best_cut[1]].size();
      if ( compare_map<DO_AREA>( worst_arrival_nneg, worst_arrival_npos, node_data.flows[0], node_data.flows[1], size_zero, size_one ) )
        use_one = false;
      else
        use_zero = false;
    }

    if ( use_zero )
    {
      if constexpr ( ELA )
      {
        /* set cut references */
        if ( !node_data.same_match )
        {
          /* dereference the negative phase cut if in use */
          if ( node_data.map_refs[1] > 0 )
            cut_deref<false>( cuts[index][node_data.best_cut[1]], n, 1 );
          /* reference the positive cut if not in use before */
          if ( node_data.map_refs[0] == 0 && node_data.map_refs[2] )
            cut_ref<false>( cuts[index][node_data.best_cut[0]], n, 0 );
        }
        else if ( node_data.map_refs[2] )
          cut_ref<false>( cuts[index][node_data.best_cut[0]], n, 0 );
      }
      set_match_complemented_phase( index, 0, worst_arrival_nneg );
    }
    else
    {
      if constexpr ( ELA )
      {
        /* set cut references */
        if ( !node_data.same_match )
        {
          /* dereference the positive phase cut if in use */
          if ( node_data.map_refs[0] > 0 )
            cut_deref<false>( cuts[index][node_data.best_cut[0]], n, 0 );
          /* reference the negative cut if not in use before */
          if ( node_data.map_refs[1] == 0 && node_data.map_refs[2] )
            cut_ref<false>( cuts[index][node_data.best_cut[1]], n, 1 );
        }
        else if ( node_data.map_refs[2] )
          cut_ref<false>( cuts[index][node_data.best_cut[1]], n, 1 );
      }
      set_match_complemented_phase( index, 1, worst_arrival_npos );
    }
  }

  inline void set_match_complemented_phase( uint32_t index, uint8_t phase, double worst_arrival_n )
  {
    auto& node_data = node_match[index];
    auto phase_n = phase ^ 1;
    node_data.same_match = true;
    node_data.best_supergate[phase_n] = nullptr;
    node_data.best_cut[phase_n] = node_data.best_cut[phase];
    node_data.phase[phase_n] = node_data.phase[phase];
    node_data.arrival[phase_n] = worst_arrival_n;
    node_data.area[phase_n] = node_data.area[phase];
    node_data.flows[phase] = node_data.flows[phase] / node_data.est_refs[2];
    node_data.flows[phase_n] = node_data.flows[phase];
    node_data.flows[2] = node_data.flows[phase];
  }

  void match_constants( uint32_t index )
  {
    auto& node_data = node_match[index];

    kitty::static_truth_table<NInputs> zero_tt;
    auto const supergates_zero = library.get_supergates( zero_tt );
    auto const supergates_one = library.get_supergates( ~zero_tt );

    /* Not available in the library */
    if ( supergates_zero == nullptr && supergates_one == nullptr )
    {
      return;
    }
    /* if only one is available, the other is obtained using an inverter */
    if ( supergates_zero != nullptr )
    {
      node_data.best_supergate[0] = &( ( *supergates_zero )[0] );
      node_data.arrival[0] = node_data.best_supergate[0]->tdelay[0];
      node_data.area[0] = node_data.best_supergate[0]->area;
      node_data.phase[0] = 0;
    }
    if ( supergates_one != nullptr )
    {
      node_data.best_supergate[1] = &( ( *supergates_one )[0] );
      node_data.arrival[1] = node_data.best_supergate[1]->tdelay[0];
      node_data.area[1] = node_data.best_supergate[1]->area;
      node_data.phase[1] = 0;
    }
    else
    {
      node_data.same_match = true;
      node_data.arrival[1] = node_data.arrival[0] + lib_inv_delay;
      node_data.area[1] = node_data.area[0] + lib_inv_area;
      node_data.phase[1] = 1;
    }
    if ( supergates_zero == nullptr )
    {
      node_data.same_match = true;
      node_data.arrival[0] = node_data.arrival[1] + lib_inv_delay;
      node_data.area[0] = node_data.area[1] + lib_inv_area;
      node_data.phase[0] = 1;
    }
  }

  inline double cut_leaves_flow( cut_t const& cut, node<Ntk> const& n, uint8_t phase )
  {
    double flow{ 0.0f };
    auto const& node_data = node_match[ntk.node_to_index( n )];

    uint8_t ctr = 0u;
    for ( auto leaf : cut )
    {
      uint8_t leaf_phase = ( node_data.phase[phase] >> ctr++ ) & 1;
      // if ( node_match[leaf].same_match )
      // {
      //   if ( node_match[leaf].map_refs[2] > 0 )
          // flow += node_match[leaf].flows[2];
      //   else
      //     flow += node_match[leaf].flows[2] * node_match[leaf].est_refs[2];
      // }
      // else
      // {
      //   if ( node_match[leaf].map_refs[leaf_phase] > 0 )
           flow += node_match[leaf].flows[leaf_phase];
      //   else
      //     flow += node_match[leaf].flows[leaf_phase] * node_match[leaf].est_refs[leaf_phase];
      // }
    }

    return flow;
  }

  template<bool SwitchActivity>
  float cut_ref( cut_t const& cut, node<Ntk> const& n, uint8_t phase )
  {
    auto const& node_data = node_match[ntk.node_to_index( n )];
    float count;

    if constexpr ( SwitchActivity )
      count = switch_activity[ntk.node_to_index( n )];
    else
      count = node_data.area[phase];

    uint8_t ctr = 0;
    for ( auto leaf : cut )
    {
      /* compute leaf phase using the current gate */
      uint8_t leaf_phase = ( node_data.phase[phase] >> ctr++ ) & 1;

      if ( ntk.is_constant( ntk.index_to_node( leaf ) ) )
      {
        continue;
      }
      else if ( ntk.is_pi( ntk.index_to_node( leaf ) ) )
      {
        /* reference PIs, add inverter cost for negative phase */
        if ( leaf_phase == 1u )
        {
          if ( node_match[leaf].map_refs[1]++ == 0u )
          {
            if constexpr ( SwitchActivity )
              count += switch_activity[leaf];
            else
              count += lib_inv_area;
          }
        }
        else
        {
          ++node_match[leaf].map_refs[0];
        }
        continue;
      }

      if ( node_match[leaf].same_match )
      {
        /* Add inverter area if not present yet and leaf node is implemented in the opposite phase */
        if ( node_match[leaf].map_refs[leaf_phase]++ == 0u && node_match[leaf].best_supergate[leaf_phase] == nullptr )
        {
          if constexpr ( SwitchActivity )
            count += switch_activity[leaf];
          else
            count += lib_inv_area;
        }
        /* Recursive referencing if leaf was not referenced */
        if ( node_match[leaf].map_refs[2]++ == 0u )
        {
          count += cut_ref<SwitchActivity>( cuts[leaf][node_match[leaf].best_cut[leaf_phase]], ntk.index_to_node( leaf ), leaf_phase );
        }
      }
      else
      {
        ++node_match[leaf].map_refs[2];
        if ( node_match[leaf].map_refs[leaf_phase]++ == 0u )
        {
          count += cut_ref<SwitchActivity>( cuts[leaf][node_match[leaf].best_cut[leaf_phase]], ntk.index_to_node( leaf ), leaf_phase );
        }
      }
    }
    return count;
  }

  template<bool SwitchActivity>
  float cut_deref( cut_t const& cut, node<Ntk> const& n, uint8_t phase )
  {
    auto const& node_data = node_match[ntk.node_to_index( n )];
    float count;

    if constexpr ( SwitchActivity )
      count = switch_activity[ntk.node_to_index( n )];
    else
      count = node_data.area[phase];

    uint8_t ctr = 0;
    for ( auto leaf : cut )
    {
      /* compute leaf phase using the current gate */
      uint8_t leaf_phase = ( node_data.phase[phase] >> ctr++ ) & 1;

      if ( ntk.is_constant( ntk.index_to_node( leaf ) ) )
      {
        continue;
      }
      else if ( ntk.is_pi( ntk.index_to_node( leaf ) ) )
      {
        /* dereference PIs, add inverter cost for negative phase */
        if ( leaf_phase == 1u )
        {
          if ( --node_match[leaf].map_refs[1] == 0u )
          {
            if constexpr ( SwitchActivity )
              count += switch_activity[leaf];
            else
              count += lib_inv_area;
          }
        }
        else
        {
          --node_match[leaf].map_refs[0];
        }
        continue;
      }

      if ( node_match[leaf].same_match )
      {
        /* Add inverter area if it is used only by the current gate and leaf node is implemented in the opposite phase */
        if ( --node_match[leaf].map_refs[leaf_phase] == 0u && node_match[leaf].best_supergate[leaf_phase] == nullptr )
        {
          if constexpr ( SwitchActivity )
            count += switch_activity[leaf];
          else
            count += lib_inv_area;
        }
        /* Recursive dereferencing */
        if ( --node_match[leaf].map_refs[2] == 0u )
        {
          count += cut_deref<SwitchActivity>( cuts[leaf][node_match[leaf].best_cut[leaf_phase]], ntk.index_to_node( leaf ), leaf_phase );
        }
      }
      else
      {
        --node_match[leaf].map_refs[2];
        if ( --node_match[leaf].map_refs[leaf_phase] == 0u )
        {
          count += cut_deref<SwitchActivity>( cuts[leaf][node_match[leaf].best_cut[leaf_phase]], ntk.index_to_node( leaf ), leaf_phase );
        }
      }
    }
    return count;
  }

  void insert_buffers()
  {
    if ( lib_buf_id != UINT32_MAX )
    {
      double area_old = area;
      bool buffers = false;

      ntk.foreach_po( [&]( auto const& f ) {
        auto const& n = ntk.get_node( f );
        if ( !ntk.is_constant( n ) && ntk.is_pi( n ) && !ntk.is_complemented( f ) )
        {
          area += lib_buf_area;
          delay = std::max( delay, node_match[ntk.node_to_index( n )].arrival[0] + lib_inv_delay );
          buffers = true;
        }
      } );

      /* round stats */
      if ( ps.verbose && buffers )
      {
        std::stringstream stats{};
        float area_gain = 0.0f;

        area_gain = float( ( area_old - area ) / area_old * 100 );

        stats << fmt::format( "[i] Buffering: Delay = {:>12.2f}  Area = {:>12.2f}  {:>5.2f} %\n", delay, area, area_gain );
        st.round_stats.push_back( stats.str() );
      }
    }
  }

  std::pair<binding_view<klut_network>, klut_map> initialize_map_network()
  {
    binding_view<klut_network> dest( library.get_gates() );
    klut_map old2new;

    old2new[ntk.node_to_index( ntk.get_node( ntk.get_constant( false ) ) )][0] = dest.get_constant( false );
    old2new[ntk.node_to_index( ntk.get_node( ntk.get_constant( false ) ) )][1] = dest.get_constant( true );

    ntk.foreach_pi( [&]( auto const& n ) {
      old2new[ntk.node_to_index( n )][0] = dest.create_pi();
    } );
    return { dest, old2new };
  }

  void finalize_cover( binding_view<klut_network>& res, klut_map& old2new )
  {
    for ( auto const& n : top_order )
    {
      auto index = ntk.node_to_index( n );
      auto const& node_data = node_match[index];

      /* add inverter at PI if needed */
      if ( ntk.is_constant( n ) )
      {
        if ( node_data.best_supergate[0] == nullptr && node_data.best_supergate[1] == nullptr )
          continue;
      }
      else if ( ntk.is_pi( n ) )
      {
        if ( node_data.map_refs[1] > 0 )
        {
          old2new[index][1] = res.create_not( old2new[n][0] );
          res.add_binding( res.get_node( old2new[index][1] ), lib_inv_id );
        }
        continue;
      }

      /* continue if cut is not in the cover */
      if ( node_data.map_refs[2] == 0u )
        continue;

      unsigned phase = ( node_data.best_supergate[0] != nullptr ) ? 0 : 1;

      /* add used cut */
      if ( node_data.same_match || node_data.map_refs[phase] > 0 )
      {
        create_lut_for_gate( res, old2new, index, phase );

        /* add inverted version if used */
        if ( node_data.same_match && node_data.map_refs[phase ^ 1] > 0 )
        {
          old2new[index][phase ^ 1] = res.create_not( old2new[index][phase] );
          res.add_binding( res.get_node( old2new[index][phase ^ 1] ), lib_inv_id );
        }
      }

      phase = phase ^ 1;
      /* add the optional other match if used */
      if ( !node_data.same_match && node_data.map_refs[phase] > 0 )
      {
        create_lut_for_gate( res, old2new, index, phase );
      }
    }

    /* create POs */
    ntk.foreach_po( [&]( auto const& f ) {
      if ( ntk.is_complemented( f ) )
      {
        res.create_po( old2new[ntk.node_to_index( ntk.get_node( f ) )][1] );
      }
      else if ( !ntk.is_constant( ntk.get_node( f ) ) && ntk.is_pi( ntk.get_node( f ) ) && lib_buf_id != UINT32_MAX )
      {
        /* create buffers for POs */
        static uint64_t _buf = 0x2;
        kitty::dynamic_truth_table tt_buf( 1 );
        kitty::create_from_words( tt_buf, &_buf, &_buf + 1 );
        const auto buf = res.create_node( { old2new[ntk.node_to_index( ntk.get_node( f ) )][0] }, tt_buf );
        res.create_po( buf );
        res.add_binding( res.get_node( buf ), lib_buf_id );
      }
      else
      {
        res.create_po( old2new[ntk.node_to_index( ntk.get_node( f ) )][0] );
      }
    } );

    /* write final results */
    st.area = area;
    st.delay = delay;
    if ( ps.eswp_rounds )
      st.power = compute_switching_power();
  }

  void create_lut_for_gate( binding_view<klut_network>& res, klut_map& old2new, uint32_t index, unsigned phase )
  {
    auto const& node_data = node_match[index];
    auto& best_cut = cuts[index][node_data.best_cut[phase]];
    auto const& gate = node_data.best_supergate[phase]->root;

    /* permutate and negate to obtain the matched gate truth table */
    std::vector<signal<klut_network>> children( gate->num_vars );

    auto ctr = 0u;
    for ( auto l : best_cut )
    {
      if ( ctr >= gate->num_vars)
        break;
      children[node_data.best_supergate[phase]->permutation[ctr]] = old2new[l][( node_data.phase[phase] >> ctr ) & 1];
      ++ctr;
    }

    if ( !gate->is_super )
    {
      /* create the node */
      auto f = res.create_node( children, gate->function );
      res.add_binding( res.get_node( f ), gate->root->id );

      /* add the node in the data structure */
      old2new[index][phase] = f;
    }
    else
    {
      /* supergate, create sub-gates */
      auto f = create_lut_for_gate_rec( res, *gate, children );

      /* add the node in the data structure */
      old2new[index][phase] = f;
    }
  }

  signal<klut_network> create_lut_for_gate_rec( binding_view<klut_network>& res, composed_gate<NInputs> const& gate, std::vector<signal<klut_network>> const& children )
  {
    std::vector<signal<klut_network>> children_local( gate.fanin.size() );

    auto i = 0u;
    for ( auto const fanin : gate.fanin )
    {
      if ( fanin->root == nullptr )
      {
        /* terminal condition */
        children_local[i] = children[fanin->id];
      }
      else
      {
        children_local[i] = create_lut_for_gate_rec( res, *fanin, children );
      }
      ++i;
    }

    auto f = res.create_node( children_local, gate.root->function );
    res.add_binding( res.get_node( f ), gate.root->id );
    return f;
  }

  template<bool DO_AREA>
  void compute_cut_data( cut_t& cut, node<Ntk> const& n )
  {
    // double delay{ 0 };
    // double flow = 1.0f;

    // for ( auto leaf : cut )
    // {
    //   const auto& best_leaf_cut = cuts[leaf][0];
    //   delay = std::max( delay, best_leaf_cut->data.delay );
    //   flow += best_leaf_cut->data.flow;
    // }

    // cut->data.delay = 1 + delay;
    // cut->data.flow = flow / ntk.fanout_size( n );
    // cut->data.ignore = false;

    double best_arrival = std::numeric_limits<float>::max();
    double best_area_flow = std::numeric_limits<float>::max();
    cut->data.delay = best_arrival;
    cut->data.flow = best_area_flow;
    cut->data.ignore = false;

    if ( cut.size() > NInputs )
    {
      /* Ignore cuts too big to be mapped using the library */
      cut->data.ignore = true;
      return;
    }

    const auto tt = truth_tables[cut->func_id];
    const auto fe = kitty::shrink_to<NInputs>( tt );
    auto fe_canon = fe;

    uint8_t negations_pos = 0;
    uint8_t negations_neg = 0;

    /* match positive polarity */
    if constexpr ( Configuration == classification_type::p_configurations )
    {
      auto canon = kitty::exact_n_canonization( fe );
      fe_canon = std::get<0>( canon );
      negations_pos = std::get<1>( canon );
    }
    auto const supergates_pos = library.get_supergates( fe_canon );

    /* match negative polarity */
    if constexpr ( Configuration == classification_type::p_configurations )
    {
      auto canon = kitty::exact_n_canonization( ~fe );
      fe_canon = std::get<0>( canon );
      negations_neg = std::get<1>( canon );
    }
    else
    {
      fe_canon = ~fe;
    }

    /* get gates */
    auto const supergates_neg = library.get_supergates( fe_canon );

    if ( supergates_pos != nullptr || supergates_neg != nullptr )
    {
      cut->data.supergates = {supergates_pos, supergates_neg};
      cut->data.negations = {negations_pos, negations_neg};
    }
    else
    {
      /* Ignore not matched cuts */
      cut->data.ignore = true;
      return;
    }

    // /* compute cut cost */
    auto& node_data = node_match[ntk.node_to_index( n )];

    /* get best delay - area for positive phase */
    if ( supergates_pos != nullptr )
    {
      for ( auto const& gate : *supergates_pos )
      {
        uint8_t gate_polarity = gate.polarity ^ negations_pos;
        node_data.phase[0] = gate_polarity;
        double area_local = gate.area + cut_leaves_flow( cut, n, 0 );
        double worst_arrival = 0.0f;

        auto ctr = 0u;
        for ( auto l : cut )
        {
          double arrival_pin = node_match[l].arrival[( gate_polarity >> ctr ) & 1] + gate.tdelay[ctr];
          worst_arrival = std::max( worst_arrival, arrival_pin );
          ++ctr;
        }

        if ( compare_map<DO_AREA>( worst_arrival, best_arrival, area_local, best_area_flow, 0, 0 ) )
        {
          best_arrival = worst_arrival;
          best_area_flow = area_local;
        }
      }
    }

    if ( supergates_neg != nullptr )
    {
      for ( auto const& gate : *supergates_neg )
      {
        uint8_t gate_polarity = gate.polarity ^ negations_neg;
        node_data.phase[1] = gate_polarity;
        double area_local = gate.area + cut_leaves_flow( cut, n, 1 );
        double worst_arrival = 0.0f;

        auto ctr = 0u;
        for ( auto l : cut )
        {
          double arrival_pin = node_match[l].arrival[( gate_polarity >> ctr ) & 1] + gate.tdelay[ctr];
          worst_arrival = std::max( worst_arrival, arrival_pin );
          ++ctr;
        }

        if ( compare_map<DO_AREA>( worst_arrival, best_arrival, area_local, best_area_flow, 0, 0 ) )
        {
          best_arrival = worst_arrival;
          best_area_flow = area_local;
        }
      }
    }

    cut->data.delay = best_arrival;
    cut->data.flow = best_area_flow;
  }

  /* compute positions of leave indices in cut `sub` (subset) with respect to
   * leaves in cut `sup` (super set).
   *
   * Example:
   *   compute_truth_table_support( {1, 3, 6}, {0, 1, 2, 3, 6, 7} ) = {1, 3, 4}
   */
  std::vector<uint8_t> compute_truth_table_support( cut_t const& sub, cut_t const& sup ) const
  {
    std::vector<uint8_t> support;
    support.reserve( sub.size() );

    auto itp = sup.begin();
    for ( auto i : sub )
    {
      itp = std::find( itp, sup.end(), i );
      support.push_back( static_cast<uint8_t>( std::distance( sup.begin(), itp ) ) );
    }

    return support;
  }

  void add_zero_cut( uint32_t index )
  {
    auto& cut = cuts[index].add_cut( &index, &index ); /* fake iterator for emptyness */

    cut->func_id = 0;
    cut->data.ignore = true;
  }

  void add_unit_cut( uint32_t index )
  {
    auto& cut = cuts[index].add_cut( &index, &index + 1 );

    cut->func_id = 2;
    cut->data.ignore = true;
  }

  uint32_t compute_truth_table( uint32_t index, std::vector<cut_t const*> const& vcuts, cut_t& res )
  {
    stopwatch t( st.cut_enumeration_st.time_truth_table );

    std::vector<kitty::static_truth_table<CutSize>> tt( vcuts.size() );
    auto i = 0;
    for ( auto const& cut : vcuts )
    {
      tt[i] = truth_tables[( *cut )->func_id];
      const auto supp = compute_truth_table_support( *cut, res );
      kitty::expand_inplace( tt[i], supp );
      ++i;
    }

    auto tt_res = ntk.compute( ntk.index_to_node( index ), tt.begin(), tt.end() );

    if ( ps.cut_enumeration_ps.minimize_truth_table )
    {
      const auto support = kitty::min_base_inplace( tt_res );
      if ( support.size() != res.size() )
      {
        std::vector<uint32_t> leaves_before( res.begin(), res.end() );
        std::vector<uint32_t> leaves_after( support.size() );

        auto it_support = support.begin();
        auto it_leaves = leaves_after.begin();
        while ( it_support != support.end() )
        {
          *it_leaves++ = leaves_before[*it_support++];
        }
        res.set_leaves( leaves_after.begin(), leaves_after.end() );
      }
    }

    return truth_tables.insert( tt_res );
  }

  template<bool DO_AREA>
  inline bool compare_map( double arrival, double best_arrival, double area_flow, double best_area_flow, uint32_t size, uint32_t best_size )
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
      if ( size < best_size )
      {
        return true;
      }
      else if ( size > best_size )
      {
        return false;
      }
      return arrival < best_arrival - epsilon;
    }
    else
    {
      if ( arrival < best_arrival - epsilon )
      {
        return true;
      }
      else if ( arrival > best_arrival + epsilon )
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
      return size < best_size;
    }
  }

  double compute_switching_power()
  {
    double power = 0.0f;

    for ( auto const& n : top_order )
    {
      const auto index = ntk.node_to_index( n );
      auto& node_data = node_match[index];

      if ( ntk.is_constant( n ) )
      {
        if ( node_data.best_supergate[0] == nullptr && node_data.best_supergate[1] == nullptr )
          continue;
      }
      else if ( ntk.is_pi( n ) )
      {
        if ( node_data.map_refs[1] > 0 )
          power += switch_activity[ntk.node_to_index( n )];
        continue;
      }

      /* continue if cut is not in the cover */
      if ( node_match[index].map_refs[2] == 0u )
        continue;

      unsigned phase = ( node_data.best_supergate[0] != nullptr ) ? 0 : 1;

      if ( node_data.same_match || node_data.map_refs[phase] > 0 )
      {
        power += switch_activity[ntk.node_to_index( n )];

        if ( node_data.same_match && node_data.map_refs[phase ^ 1] > 0 )
          power += switch_activity[ntk.node_to_index( n )];
      }

      phase = phase ^ 1;
      if ( !node_data.same_match && node_data.map_refs[phase] > 0 )
      {
        power += switch_activity[ntk.node_to_index( n )];
      }
    }

    return power;
  }

private:
  Ntk const& ntk;
  tech_library<NInputs, Configuration> const& library;
  emap_params const& ps;
  emap_stats& st;

  uint32_t iteration{ 0 };        /* current mapping iteration */
  double delay{ 0.0f };           /* current delay of the mapping */
  double area{ 0.0f };            /* current area of the mapping */
  const float epsilon{ 0.005f };  /* epsilon */

  /* lib inverter info */
  float lib_inv_area;
  float lib_inv_delay;
  uint32_t lib_inv_id;

  /* lib buffer info */
  float lib_buf_area;
  float lib_buf_delay;
  uint32_t lib_buf_id;

  std::vector<node<Ntk>> top_order;
  std::vector<node_match_flex<NInputs>> node_match;
  std::vector<float> switch_activity;

  /* cut computation */
  std::vector<cut_set_t> cuts;    /* compressed representation of cuts */
  cut_merge_t lcuts;              /* cut merger container */
  tt_cache truth_tables;          /* cut truth tables */
  uint32_t cuts_total{ 0 };       /* current computed cuts */
};

} /* namespace detail */

/*! \brief Technology mapping.
 *
 * This function implements a technology mapping algorithm.
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
 */
template<class Ntk, unsigned CutSize = 5u, unsigned NInputs, classification_type Configuration>
binding_view<klut_network> emap( Ntk const& ntk, tech_library<NInputs, Configuration> const& library, emap_params const& ps = {}, emap_stats* pst = nullptr )
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

  emap_stats st;
  detail::emap_impl<Ntk, CutSize, NInputs, Configuration> p( ntk, library, ps, st );
  auto res = p.run();

  st.time_total = st.time_mapping + st.cut_enumeration_st.time_total;
  if ( ps.verbose && !st.mapping_error )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }
  return res;
}


/*! \brief Technology node mapping.
 *
 * This function implements a simple technology mapping algorithm.
 * The algorithm maps each node to the first implementation in the technology library.
 * 
 * The input must be a binding_view with the gates correctly loaded.
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
 * - `has_binding`
 *
 * \param ntk Network
 *
 */
template<class Ntk>
void flex_node_map( Ntk& ntk )
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
  static_assert( has_has_binding_v<Ntk>, "Ntk does not implement the has_binding method" );


  /* build the library map */
  using lib_t = std::unordered_map<kitty::dynamic_truth_table, uint32_t, kitty::hash<kitty::dynamic_truth_table>>;
  lib_t tt_to_gate;

  for ( auto const& g : ntk.get_library() )
  {
    tt_to_gate[g.function] = g.id;
  }

  ntk.foreach_gate( [&]( auto const& n ) {
    if ( auto it = tt_to_gate.find( ntk.node_function( n ) ); it != tt_to_gate.end() )
    {
      ntk.add_binding( n, it->second );
    }
    else
    {
      std::cout << fmt::format( "[e] node mapping for node {} failed: no match in the tech library\n", ntk.node_to_index( n ) );
    }
  } );
}

} /* namespace mockturtle */