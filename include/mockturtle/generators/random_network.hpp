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
  \file random_network.hpp
  \brief Generate random logic networks

  \author Heinz Riener
  \author Mathias Soeken
  \author Siang-Yun Lee
*/

#pragma once

#include "../networks/aig.hpp"
#include "../networks/xag.hpp"
#include "../networks/mig.hpp"

#include <percy/percy.hpp>
#include <random>
#include <algorithm>

namespace mockturtle
{

/*! \brief Parameters for random_network_generator.
 *
 * When this parameter object is used, random_network_generator
 * generates networks according to the specified number of PIs
 * and number of gates. After generating primary inputs and gates,
 * all nodes with no fanout become primary outputs. After generating
 * `num_networks_per_configuration` networks (i.e. `generate` being
 * called this number of times), the configuration (numbers of PIs
 * and gates) will be incremented by `num_pis_increment` and
 * `num_gates_increment`, respectively.
 */
struct random_network_generator_params_size
{
  /*! \brief Seed of the random generator. */
  uint64_t seed{0xcafeaffe};

  /*! \brief Number of networks of each configuration to generate
   * before increasing size. */
  uint32_t num_networks_per_configuration{100u};

  /*! \brief Number of PIs to start with. */
  uint32_t num_pis{4u};

  /*! \brief Number of gates to start with. */
  uint32_t num_gates{10u};

  /*! \brief Number of PIs to increment at each step. */
  uint32_t num_pis_increment{0u};

  /*! \brief Number of gates to increment at each step. */
  uint32_t num_gates_increment{0u};
}; /* random_network_generator_params_size */

/*! \brief Parameters for random_network_generator.
 *
 * When this parameter object is used, random_network_generator
 * generates networks according to a suggestion for the structure in
 * form of a list { l_1, ..., l_k }, where k denotes the number of
 * levels between primary inputs and primary outputs and each l_i, 1
 * <= i <= k, denotes a suggestion for the number of nodes at level
 * i.  //TODO
 */
struct random_network_generator_params_levels
{
  /*! \brief Seed of the random generator. */
  uint64_t seed{0xcafeaffe};

  /*! \brief Number of networks of each configuration to generate
   * before increasing size. */
  uint32_t num_networks_per_configuration{100u};

  /*! \brief Number of PIs. */
  uint32_t num_pis{20u};

  /*! \brief Number of levels to start with. */
  uint32_t num_levels{5u};

  /*! \brief Number of levels to increment at each step. */
  uint32_t num_levels_increment{0u};

  /*! \brief Average number of gates per level. */
  uint32_t ratio_gates_to_level{3u};

  /*! \brief Pool of suggested shapes. */
  std::vector<std::vector<uint32_t>> shapes = {{1u, 2u, 1u}};
}; /* random_network_generator_params_levels */

/*! \brief Parameters for random_network_generator.
 *
 * When this parameter object is used, random_network_generator
 * first enumerates all non-isomorphic connected partial DAGs of
 * `num_gates` vertices, randomly shuffles them, and then generates
 * `num_networks_per_configuration` random networks of each topology.
 * It is guaranteed that all possible topologies are generated for
 * the same number of times, provided that `generate` is called for
 * enough times. After all topologies have been generated, `num_gates`
 * is increased by 1 and the above steps are repeated.
 *
 * This method currently only supports generating 2-regular (i.e.,
 * each gate has two fanins), single-output DAGs.
 *
 * Guideline on how many iterations are needed to visit all topologies:
 * - `num_gates` = 2: 1 topology
 * - `num_gates` = 3: 3 topologies
 * - `num_gates` = 4: 10 topologies
 * - `num_gates` = 5: 49 topologies
 * - `num_gates` = 6: 302 topologies
 * - `num_gates` = 7: 2312 topologies
 * - `num_gates` = 8: 21218 topologies
 * - `num_gates` = 9: 228249 topologies
 *
 * For example, starting from `num_gates` = 3 and using
 * `num_networks_per_configuration` = 100, to generate networks
 * of all topologies with no more than 5 vertices, `generate`
 * should be called (3 + 10 + 49) * 100 = 6200 times.
 *
 * For each topology, a random network is concretized from the partial
 * DAG by randomly choosing a (#PIs/#inputs of DAG) ratio and allocating
 * the corresponding number of PIs, randomly choosing a PI to be connected
 * to each input of the partial DAG, and randomly deciding if each edge
 * should be complemented.
 */
struct random_network_generator_params_topology
{
  /*! \brief Seed of the random generator. */
  uint64_t seed{0xcafeaffe};

  /*! \brief Number of networks to generate for each topology. */
  uint32_t num_networks_per_configuration{100u};

  /*! \brief Number of gates to start with. */
  uint32_t num_gates{3u};

  /*! \brief Minimum ratio of (#PIs/#inputs of DAG).
   * Lower ratio makes more reconvergences. */
  float min_PI_ratio{0.5};

  /*! \brief Maximum ratio of (#PIs/#inputs of DAG).
   * Higher ratio makes it more likely to sample a full tree. */
  float max_PI_ratio{1.0};
}; /* random_network_generator_params_topology */

/*! \brief Parameters for random_network_generator.
 *
 * When this parameter object is used, random_network_generator
 * first enumerates all non-isomorphic connected partial DAGs with
 * numbers of vertices between `min_num_gates_component` and
 * `max_num_gates_component`. Random networks are generated by
 * randomly generating `num_components` DAGs of randomly-sampled
 * topologies (repeated topologies are allowed). The first topology
 * is concretized by connecting inputs to randomly-chosen PIs, and
 * the remaining ones are concretized by connecting inputs to PIs
 * or nodes in the previous components.
 *
 * After generating `num_networks_per_configuration` networks,
 * `num_components` is increased by 1.
 */
struct random_network_generator_params_composed
{
  /*! \brief Seed of the random generator. */
  uint64_t seed{0xcafeaffe};

  /*! \brief Number of networks to generate for each topology. */
  uint32_t num_networks_per_configuration{1000u};

  /*! \brief Minimum number of gates of the components. */
  uint32_t min_num_gates_component{3u};

  /*! \brief Maximum number of gates of the components. */
  uint32_t max_num_gates_component{5u};

  /*! \brief Number of components to start with. */
  uint32_t num_components{2u};

  /*! \brief Minimum ratio of (#PIs/#inputs of the first DAG).
   * Lower ratio makes more reconvergences. */
  float min_PI_ratio{0.5};

  /*! \brief Maximum ratio of (#PIs/#inputs of the first DAG).
   * Higher ratio makes it more likely to have independent sub-graphs. */
  float max_PI_ratio{1.0};
}; /* random_network_generator_params_composed */

namespace detail
{

template<typename Ntk>
struct create_gate_rule
{
  using signal = typename Ntk::signal;

  std::function<signal(Ntk&, std::vector<signal> const&)> func;
  uint32_t num_args;
};

} /* namespace detail */

/*! \brief Generates random logic networks
 *
 * Generate random logic networks with certain parameters.
 *
 * The constructor takes a vector of construction rules, which are
 * used in the algorithm to build the logic network. The constructor
 * also takes a parameter object, which can be of various types and
 * influences the way networks are generated.
 *
 * The function `generate` returns a random network and can be called
 * repeatedly, each time generating a different network.
 *
 */
template<typename Ntk, typename Params = random_network_generator_params_size>
class random_network_generator
{};

template<typename Ntk>
class random_network_generator<Ntk, random_network_generator_params_size>
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using rand_engine_t = std::mt19937;
  using rules_t = std::vector<typename detail::create_gate_rule<Ntk>>;
  using params_t = random_network_generator_params_size;

private: /* common data members */
  rules_t const _gens;
  params_t const _ps;
  rand_engine_t _rng;

public:
  explicit random_network_generator( rules_t const &gens, params_t ps = {} )
    : _gens( gens ), _ps( ps ), _rng( static_cast<rand_engine_t::result_type>( ps.seed ) ),
      _counter( 0u ), _num_pis( ps.num_pis ), _num_gates( ps.num_gates )
  {
  }

  Ntk generate()
  {
    assert( _num_pis > 0 );
    assert( _num_gates > 0 );

    std::vector<signal> fs;
    Ntk ntk;

    /* generate constant */
    fs.emplace_back( ntk.get_constant( false ) );

    /* generate pis */
    for ( auto i = 0u; i < _num_pis; ++i )
    {
      fs.emplace_back( ntk.create_pi() );
    }

    /* generate gates */
    std::uniform_int_distribution<int> rule_dist( 0, static_cast<int>( _gens.size() - 1u ) );

    auto gate_counter = ntk.num_gates();
    while ( gate_counter < _num_gates )
    {
      auto const r = _gens.at( rule_dist( _rng ) );

      std::uniform_int_distribution<int> dist( 0, static_cast<int>( fs.size() - 1 ) );
      std::vector<signal> args;
      for ( auto i = 0u; i < r.num_args; ++i )
      {
        auto const a_compl = dist( _rng ) & 1;
        auto const a = fs.at( dist( _rng ) );
        args.emplace_back( a_compl ? !a : a );
      }

      auto const g = r.func( ntk, args );
      if ( ntk.num_gates() > gate_counter )
      {
        fs.emplace_back( g );
        ++gate_counter;
      }

      assert( ntk.num_gates() == gate_counter );
    }

    /* generate pos */
    ntk.foreach_node( [&]( auto const& n ){
      if ( ntk.fanout_size( n ) == 0u )
      {
        ntk.create_po( ntk.make_signal( n ) );
      }
    });

    assert( ntk.num_pis() == _num_pis );
    assert( ntk.num_gates() == _num_gates );

    if ( ++_counter >= _ps.num_networks_per_configuration )
    {
      _counter = 0;
      _num_gates += _ps.num_gates_increment;
      _num_pis += _ps.num_pis_increment;
    }

    return ntk;
  }

private:
  uint32_t _counter;
  uint32_t _num_pis;
  uint32_t _num_gates;
}; /* random_network_generator<Ntk, random_network_generator_params_size> */

template<typename Ntk>
class random_network_generator<Ntk, random_network_generator_params_levels>
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using rand_engine_t = std::mt19937;
  using rules_t = std::vector<typename detail::create_gate_rule<Ntk>>;
  using params_t = random_network_generator_params_levels;

private: /* common data members */
  rules_t const _gens;
  params_t const _ps;
  rand_engine_t _rng;

public:
  explicit random_network_generator( rules_t const &gens, params_t ps = {} )
    : _gens( gens ), _ps( ps ), _rng( static_cast<rand_engine_t::result_type>( ps.seed ) )
  {
  }

  Ntk generate()
  {
    #if 0
    assert( num_inputs > 0 );
    assert( num_outputs > 0 );
    assert( num_gates_per_level.size() >= 0u );

    uint64_t const num_levels{num_gates_per_level.size() + 2u};
    std::vector<std::vector<signal>> levels( num_levels );

    Ntk ntk;

    /* generate constant */
    levels[0].emplace_back( ntk.get_constant( false ) );

    /* generate pis */
    while ( ntk.num_pis() < num_inputs )
    {
      levels[0].emplace_back( ntk.create_pi() );
    }

    /* generate gates */
    std::uniform_int_distribution<uint32_t> rule_dist( 0, _gens.size() - 1u );

    uint64_t gate_counter{ntk.num_gates()};
    uint64_t curr_level{0u};

    while ( ++curr_level < num_levels )
    {
      uint64_t iterations{0};
      uint64_t const num_expected_gates{curr_level == num_levels - 1 ? num_outputs : num_gates_per_level[curr_level - 1u]};
      while ( levels[curr_level].size() < num_expected_gates && iterations++ < std::max( uint64_t(3ul*num_expected_gates), uint64_t(50ul) ) )
      {
        /* select a rule */
        auto const rule = _gens.at( rule_dist( _rng ) );

        /* select an element */
        std::uniform_int_distribution<uint32_t> dist( 0, levels[curr_level - 1].size() - 1 );
        std::vector<signal> args;
        while ( args.size() < rule.num_args )
        {
          bool const s_compl = dist( _rng ) & 1;
          signal const s = levels[curr_level - 1].at( dist( _rng ) );
          args.emplace_back( s_compl ? !s : s );
        }

        /* apply rule */
        signal const gate = rule.func( ntk, args );
        if ( ntk.num_gates() > gate_counter )
        {
          levels[curr_level].emplace_back( gate );
          ++gate_counter;
        }
      }

      /* update num_gates_per_level */
      if ( curr_level < num_levels - 1 )
      {
        num_gates_per_level[curr_level - 1u] = levels[curr_level].size();
      }
    }

    /* generate pos */
    for ( auto& po : levels[num_levels - 1] )
    {
      ntk.create_po( po );
    }

    while ( ntk.num_pos() < num_outputs )
    {
      ntk.create_po( ntk.get_constant( false ) );
    }

    assert( ntk.num_pis() == num_inputs );
    assert( ntk.num_pos() == num_outputs );

    return ntk;
    #endif
    return Ntk();
  }
}; /* random_network_generator<Ntk, random_network_generator_params_levels> */

template<typename Ntk>
class random_network_generator<Ntk, random_network_generator_params_topology>
{
public:
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;
  using rand_engine_t = std::mt19937;
  using rules_t = std::vector<typename detail::create_gate_rule<Ntk>>;
  using params_t = random_network_generator_params_topology;

private: /* common data members */
  rules_t const _gens;
  params_t const _ps;
  rand_engine_t _rng;

public:
  explicit random_network_generator( rules_t const &gens, params_t ps = {} )
    : _gens( gens ), _ps( ps ), _rng( static_cast<rand_engine_t::result_type>( ps.seed ) ),
      _counter( 0u ), _num_gates( ps.num_gates ), _ith_dag( 0u ),
      _rule_dist( 0, _gens.size() - 1u )
  {
  }

  Ntk generate()
  {
    if ( _counter == 0 )
    {
      if ( _ith_dag == 0 )
      {
        prepare_partial_dags();
      }
      auto num_inputs = _dags.at( _ith_dag ).nr_pi_fanins();
      uint32_t min_num_pis = std::ceil( _ps.min_PI_ratio * num_inputs );
      uint32_t max_num_pis = std::ceil( _ps.max_PI_ratio * num_inputs );
      _num_pis_dist = std::uniform_int_distribution<uint32_t>( min_num_pis, max_num_pis );
    }

    percy::partial_dag const& pd = _dags.at( _ith_dag );
    uint32_t num_pis = _num_pis_dist( _rng );
    std::uniform_int_distribution<uint32_t> pis_dist( 1u, num_pis );
    
    std::vector<signal> fs;
    Ntk ntk;

    /* generate constant */
    fs.emplace_back( ntk.get_constant( false ) );

    /* generate pis */
    for ( auto i = 0u; i < num_pis; ++i )
    {
      fs.emplace_back( ntk.create_pi() );
    }

    /* generate gates */
    pd.foreach_vertex( [&]( auto const& v, auto i ){
      uint32_t size_before = ntk.num_gates();
      signal g;

      do
      {
        auto const r = _gens.at( _rule_dist( _rng ) );
        std::vector<signal> args;

        for ( auto fi : v )
        {
          bool const inv = _rng() & 1;
          if ( fi == percy::FANIN_PI )
          {
            auto const& a = fs.at( pis_dist( _rng ) );
            args.emplace_back( inv ? !a : a );
          }
          else
          {
            assert( fi >= 1 && num_pis + fi < fs.size() );
            auto const& a = fs.at( num_pis + fi );
            args.emplace_back( inv ? !a : a );
          }
        }

        g = r.func( ntk, args );
      } while ( ntk.num_gates() == size_before );

      fs.emplace_back( g );
    });

    /* generate pos */
    ntk.foreach_gate( [&]( auto const& n ){
      if ( ntk.fanout_size( n ) == 0u )
      {
        ntk.create_po( ntk.make_signal( n ) );
      }
    });
    
    if ( ++_counter >= _ps.num_networks_per_configuration )
    {
      _counter = 0;
      if ( ++_ith_dag >= _dags.size() )
      {
        _ith_dag = 0;
        ++_num_gates;
      }
    }

    return ntk;
  }

private:
  void prepare_partial_dags()
  {
    using namespace percy;

    _dags.clear();
    partial_dag g;
    partial_dag_generator gen(_num_gates);
    std::set<std::vector<graph>> can_reprs;
    pd_iso_checker checker(_num_gates);

    gen.set_callback([&](partial_dag_generator* gen) {
        for (int i = 0; i < gen->nr_vertices(); i++) {
            g.set_vertex(i, gen->_js[i], gen->_ks[i]);
        }
        const auto can_repr = checker.crepr(g);
        const auto res = can_reprs.insert(can_repr);
        if (res.second)
            _dags.push_back(g);
    });
    gen.gen_type(partial_gen_type::GEN_COLEX);
    g.reset(2, _num_gates);
    gen.count_dags();

    std::shuffle( _dags.begin(), _dags.end(), _rng );
  }

private:
  uint32_t _counter;
  uint32_t _num_gates;
  std::vector<percy::partial_dag> _dags;
  uint32_t _ith_dag;
  std::uniform_int_distribution<uint32_t> _num_pis_dist, _rule_dist;
}; /* random_network_generator<Ntk, random_network_generator_params_topology> */

/*! \brief Generates a random AIG network */
template<typename GenParams = random_network_generator_params_size>
auto random_aig_generator( GenParams ps = {} )
{
  using gen_t = random_network_generator<aig_network, GenParams>;
  using rule_t = typename detail::create_gate_rule<aig_network>;

  std::vector<rule_t> rules;
  rules.emplace_back( rule_t{[]( aig_network& aig, std::vector<aig_network::signal> const& vs ) -> aig_network::signal
    {
      assert( vs.size() == 2u );
      return aig.create_and( vs[0], vs[1] );
    }, 2u} );

  return gen_t( rules, ps );
}

/*! \brief Generates a random XAG network */
template<typename GenParams = random_network_generator_params_size>
auto random_xag_generator( GenParams ps = {} )
{
  using gen_t = random_network_generator<xag_network, GenParams>;
  using rule_t = typename detail::create_gate_rule<xag_network>;

  std::vector<rule_t> rules;
  rules.emplace_back( rule_t{[]( xag_network& xag, std::vector<xag_network::signal> const& vs ) -> xag_network::signal
    {
      assert( vs.size() == 2u );
      return xag.create_and( vs[0], vs[1] );
    }, 2u} );
  rules.emplace_back( rule_t{[]( xag_network& xag, std::vector<xag_network::signal> const& vs ) -> xag_network::signal
    {
      assert( vs.size() == 2u );
      return xag.create_xor( vs[0], vs[1] );
    }, 2u} );

  return gen_t( rules, ps );
}

/*! \brief Generates a random MIG network */
template<typename GenParams = random_network_generator_params_size>
auto default_random_mig_generator( GenParams ps = {} )
{
  using gen_t = random_network_generator<mig_network, GenParams>;
  using rule_t = typename detail::create_gate_rule<mig_network>;

  std::vector<rule_t> rules;
  rules.emplace_back( rule_t{[]( mig_network& mig, std::vector<mig_network::signal> const& vs ) -> mig_network::signal
    {
      assert( vs.size() == 3u );
      return mig.create_maj( vs[0], vs[1], vs[2] );
    }, 3u} );

  return gen_t( rules, ps );
}

/*! \brief Generates a random MIG network MAJ-, AND-, and OR-gates */
template<typename GenParams = random_network_generator_params_size>
auto mixed_random_mig_generator( GenParams ps = {} )
{
  using gen_t = random_network_generator<mig_network, GenParams>;
  using rule_t = typename detail::create_gate_rule<mig_network>;

  std::vector<rule_t> rules;
  rules.emplace_back( rule_t{[]( mig_network& mig, std::vector<mig_network::signal> const& vs ) -> mig_network::signal
    {
      assert( vs.size() == 3u );
      return mig.create_maj( vs[0], vs[1], vs[2] );
    }, 3u} );
  rules.emplace_back( rule_t{[]( mig_network& mig, std::vector<mig_network::signal> const& vs ) -> mig_network::signal
    {
      assert( vs.size() == 2u );
      return mig.create_and( vs[0], vs[1] );
    }, 2u} );
  rules.emplace_back( rule_t{[]( mig_network& mig, std::vector<mig_network::signal> const& vs ) -> mig_network::signal
    {
      assert( vs.size() == 2u );
      return mig.create_or( vs[0], vs[1] );
    }, 2u} );

  return gen_t( rules, ps );
}

} // namespace mockturtle