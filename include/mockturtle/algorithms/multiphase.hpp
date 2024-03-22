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
  \file multiphase.hpp
  \brief Multiphase path balancing

  \author Rassul Bairamkulov
*/

#pragma once

#include <mockturtle/utils/misc.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/mph_view.hpp>

#include "../utils/stopwatch.hpp"

typedef uint32_t stage_t;

typedef mockturtle::klut_network klut;
typedef uint64_t node_t;

#define VERBOSE_PRINT(format, ...) if (ps.verbose) fmt::print(format, ##__VA_ARGS__)

const std::vector<std::string> GATE_TYPE { "PI", "AA", "AS", "SA", "T1" }; //, "PO"

template <uint8_t NUM_PHASES>
struct Path
{
  std::set<klut::signal> sources;   // AS/SA gates
  std::set<klut::signal> internals; // AA    gates
  std::set<klut::signal> targets;   // AS/SA gates
  Path(const std::set<klut::signal> & _sources, const std::set<klut::signal>& _internals, const std::set<klut::signal>& _targets)
    : sources(_sources), internals(_internals), targets(_targets) {}
  Path() : sources({}), internals({}), targets({}) {}
  
  void absorb(Path<NUM_PHASES> & other)
  {
    sources.insert(other.sources.begin(), other.sources.end());
    internals.insert(other.internals.begin(), other.internals.end());
    targets.insert(other.targets.begin(), other.targets.end());
  }

  void print() const
  {
    fmt::print(format());
  }

  std::string format() const
  {
    return fmt::format("Path from [{}]\n\tvia [{}]\n\tto [{}]\n", fmt::join(sources, ","), fmt::join(internals, ","), fmt::join(targets, ","));
  }

  std::vector<klut::signal> preds(const klut::signal & sig, const klut & ntk) const
  {
    if ( sources.count(sig) != 0)
    {
      return {};
    }

    std::vector<klut::signal> predecessors;

    ntk.foreach_fanin(sig, [&](const klut::signal & parent)
    {
      if ( internals.count(parent) != 0 || sources.count(parent) != 0 )
      {
        predecessors.push_back( parent );
      }
    });
    return predecessors;
  }

  std::string _kind( const klut::signal & node ) const
  {
    if ( targets.count(node) > 0 )  
    { 
      return "Target";  
    }
    else if ( internals.count(node) > 0 )  
    { 
      return "Internal";  
    }
    else if ( sources.count(node) > 0 )  
    { 
      return "Source";  
    }
    else 
    {
      throw;
    }
  }
};


/// @brief Structure representing the potential DFF location. Uniquely defined by fanin, fanout and stage
struct DFF_var 
{
  klut::signal fanin;
  klut::signal fanout;
  uint32_t stage;
  std::unordered_set<uint64_t> parent_hashes;

  DFF_var(klut::signal _fanin, klut::signal _fanout, uint32_t _stage, std::unordered_set<uint64_t> _parent_hashes = {})
      : fanin(_fanin), fanout(_fanout), stage(_stage), parent_hashes(_parent_hashes) {}

  DFF_var(const DFF_var& other)
      : fanin(other.fanin), fanout(other.fanout), stage(other.stage), parent_hashes(other.parent_hashes) {}

  DFF_var( klut::signal _index, uint32_t _stage, std::unordered_set<uint64_t> _parent_hashes = {} )
      : fanin( 0u ), fanout( _index ), stage( _stage ), parent_hashes( _parent_hashes ) {}

  DFF_var( uint64_t dff_hash, std::unordered_set<uint64_t> _parent_hashes = {} )
      : fanin( (uint64_t)dff_hash >> 40 ), fanout( (uint64_t)( dff_hash << 24 ) >> 40 ), stage( (uint64_t)( dff_hash & 0xFFFF ) ), parent_hashes( _parent_hashes ) {}

  std::string str() const
  {
    if ( fanin == 0 )
    {
      return fmt::format( "gate_{}_{}", fanout, stage );
    }

    return fmt::format("var_{}_{}_{}", fanin, fanout, stage);
  }
};

uint64_t dff_hash(klut::signal _fanin, klut::signal _fanout, uint32_t _stage)
{
  return ( (uint64_t)_fanin << 40 ) | ( (uint64_t)_fanout << 16 ) | _stage;
}

uint64_t dff_hash( DFF_var const& dff )
{
  return ( (uint64_t)dff.fanin << 40 ) | ( (uint64_t)dff.fanout << 16 ) | dff.stage;
}

/// @brief Enhanced map of the DFF variables for easy tracking of DFFs
struct DFF_registry
{
  phmap::flat_hash_map<uint64_t, DFF_var> variables;

  DFF_var & at(node_t _fanin, node_t _fanout, stage_t _stage)
  {
    return variables.at( dff_hash(_fanin, _fanout, _stage) );
  } 
  DFF_var & at(uint64_t _hash)
  {
    return variables.at( _hash );
  } 
  uint64_t add(node_t _fanin, node_t _fanout, stage_t _phase, std::unordered_set<uint64_t> _parent_hashes = {})
  {
    uint64_t _hash = dff_hash(_phase, _fanout, _fanin);
    DFF_var temp { _fanin, _fanout, _phase, _parent_hashes };
    variables.emplace(_hash, temp);
    return _hash;
  } 

  std::string str(uint64_t hash, bool negated = false) const
  {
    const DFF_var & dff = variables.at(hash);
    if (negated)
    {
      return fmt::format( "var_{}_{}_{}.Not()", dff.fanin, dff.fanout, dff.stage );
    }
    return fmt::format( "var_{}_{}_{}", dff.fanin, dff.fanout, dff.stage );
  }
};



template <uint8_t NUM_PHASES>
struct Chain
{
  std::deque<std::vector<uint64_t>> sections;

  Chain(): sections({}) {}
  Chain( const uint64_t head ): sections({ { head } }) {}
  Chain( const std::deque<std::vector<uint64_t>> _sections ): sections(_sections) {}
  Chain( const Chain & _other ): sections(_other.sections) {}

  bool append(const uint64_t dff_hash, DFF_registry &DFF_REG)
  {
    DFF_var & dff = DFF_REG.at( dff_hash );

    std::vector<uint64_t> & head_section = sections.back();
    uint64_t & head_hash = head_section.back();
    DFF_var & head_dff = DFF_REG.at( head_hash );
    if (dff.stage == head_dff.stage) // add to the same section
    {
      head_section.push_back( dff_hash );
      return false;
    }
    else
    {
      // assert( head_dff.phase - dff.phase == 1 );
      sections.push_back( { dff_hash } );
      if (sections.size() > NUM_PHASES)
      {
        sections.pop_front();
      }
      return true;
    }
  }
};

template <uint8_t NUM_PHASES>
void write_chains(const std::vector<Chain<NUM_PHASES>> & chains, DFF_registry & DFF_REG, const std::vector<uint64_t> & required_SA_DFFs, const std::string cfg_name, bool verbose = false)
{
  std::ofstream spec_file (cfg_name);

  for (const auto & chain : chains)
  {
    std::vector<std::string> vars_bucket;
    for (const std::vector<uint64_t> & section : chain.sections)
    {
      std::vector<std::string> vars;
      for (uint64_t hash : section)
      {
        vars.push_back(DFF_REG.str( hash ));
      }
      vars_bucket.push_back(fmt::format(vars.size()>1?"({})":"{}", fmt::join(vars, "+")));
      if (vars.size() > 1)
      {
        spec_file << fmt::format("PHASE,{}\n", fmt::join(vars, ","));
      }
    }
    std::reverse(vars_bucket.begin(), vars_bucket.end());
    if (vars_bucket.size() == NUM_PHASES)
    {
      spec_file << fmt::format("BUFFER,{}\n", fmt::join(vars_bucket, ","));
    }
  }

  for (const uint64_t & hash : required_SA_DFFs)
  {
    spec_file << fmt::format("SA_REQUIRED,{}\n", DFF_REG.str( hash ));
  }
}


namespace mockturtle
{
 
/*! \brief Parameters for multiphase_balancing.
 *
 * The data structure `multiphase_balancing_params` holds configurable parameters with
 * default arguments for `multiphase_balancing`.
 */
struct multiphase_balancing_params
{
  /*! \brief equalize the epochs of the POs. */
  bool balance_pos { true };

  /*! \brief Maximum time for CP-SAT-based phase assignment. */
  float phase_assignment_max_time  { 600.0 };

  /*! \brief Maximum time for CP-SAT -based DFF insertion. */
  float dff_insertion_max_time  { 10.0 };

  /*! \brief Be verbose. */
  bool verbose{ false };
};

/*! \brief Statistics for multiphase balancing.
 *
 * The data structure `multiphase_balancing_stats` provides data collected by running
 * `multiphase_balancing`.
 */
struct multiphase_balancing_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{ 0 };

  // /*! \brief Number of added DFFs. */
  uint64_t num_added_dffs { 0u };

  void report( bool show_time_mis = true ) const
  {
    fmt::print( "[i] total time     = {:>5.2f} secs\n", to_seconds( time_total ) );
    fmt::print( "[i] inserted DFFs : {}\n", num_added_dffs );
  }
};

template <class Ntk, uint8_t NUM_PHASES>
class multiphase_balancing_impl
{
private:
  Ntk ntk;
  multiphase_balancing_params ps;
  multiphase_balancing_stats & st;
public:
  // multiphase_balancing_impl( mockturtle::mph_view<Ntk, NUM_PHASES> _ntk, multiphase_balancing_params _ps, multiphase_balancing_stats _st): ntk(_ntk), ps(_ps), st(_st) {};
  multiphase_balancing_impl( Ntk _ntk, multiphase_balancing_params _ps, multiphase_balancing_stats & _st): ntk(_ntk), ps(_ps), st(_st) {};

  std::string run_command(const std::string & command)
  {
      // Run the command and capture its output
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) 
    {
      throw std::runtime_error("Error running the command.");
    }

    char buffer[128];
    std::stringstream output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) 
    {
      output << buffer;
    }
    
    if (pclose(pipe) == -1) 
    {
      throw std::runtime_error("Error closing the command pipe.");
    }

    VERBOSE_PRINT("{}\n", output.str());

    return output.str();
  }

  void write_klut_specs( const std::string & filename )
  {
    std::ofstream spec_file( filename );

    spec_file << "PI";
    ntk.foreach_pi( [&]( const auto & node ){
      spec_file << "," << node;
    } );
    spec_file << "\n";

    ntk.foreach_gate( [&]( auto const& n ) 
    {
      std::vector<klut::node> n_fanins;
      ntk.foreach_fanin( n, [&n_fanins]( auto const& ni ) 
      {
        n_fanins.push_back( ni );
      } );

      spec_file << fmt::format( "{0},{1},{2}\n", n, ntk.get_type(n), fmt::join( n_fanins, "|" ) );
      // spec_file << fmt::format( "{0},{1},{2}\n", ntk.node_to_index( n ), ntk.get_type(n), fmt::join( n_fanins, "|" ) );
      return true;
    } );
  }

  /// @brief Use CP-SAT solver to assign phase to each node while minimizing the expected number of DFFs
  /// @param cfg_name - filename of the config
  /// @return a tuple of [expected number of DFFs, phase assignment, success flag]
  std::tuple<int, std::string> cpsat_macro_opt(const std::string & cfg_name) 
  {
    std::string command = fmt::format("{} {} {} {} {}", PYTHON_EXECUTABLE, PYTHON_PHASE_ASSIGNMENT, NUM_PHASES, cfg_name, ps.phase_assignment_max_time);

    const std::string output = run_command(command);

    VERBOSE_PRINT("RUNNING CPSAT:\n{}\n", output);

    // Parse output
    std::string pattern = "Objective value: (\\d+)";

    std::istringstream iss(output);
    std::string line;
    std::string solve_status;
    int expected_nDFF;
    // phmap::flat_hash_map<unsigned int, unsigned int> key_value_pairs;

    while (std::getline(iss, line))
    {
      if (line.find("Solve status:") != std::string::npos) 
      {
        if (line.find("OPTIMAL") != std::string::npos || line.find("FEASIBLE") != std::string::npos)
        {
          solve_status = "SUCCESS";
          break;
        }
        else
        {
          throw std::runtime_error("Error: Invalid solve status (OPTIMAL or FEASIBLE status required.)");
        }
      }
    }

    if (solve_status.empty())
    {
        throw std::runtime_error("Error: Solve status not found.");
    }

    // Parse the second line (Objective value)
    if (!std::getline(iss, line) || line.find("Objective value: ") == std::string::npos)
    {
      throw std::runtime_error("Error: Objective value not found or in invalid format.");
    }
    try
    {
      expected_nDFF = std::stoi(line.substr(17));
    } 
    catch (const std::exception& e) // Catch stoi parsing errors
    {
      throw std::runtime_error("Error parsing objective value: " + std::string(e.what()));
    }

    // Parse the key-value pairs in subsequent lines
    while (std::getline(iss, line)) 
    {
      std::istringstream line_stream(line);
      unsigned int node_idx, stage;
      char colon;
      if (!(line_stream >> node_idx >> colon >> stage) || colon != ':') 
      {
        throw std::runtime_error("Error: Invalid format for key-value pairs.");
      }
      ntk.set_stage(node_idx, stage);
    }

    return {expected_nDFF, solve_status};
  }


  /// @brief Explicit insertion of the splitters as late as posible to maximize sharing. Creates a simple chain (only needed for assigning the stages)
  void splitter_ntk_insertion()
  {
    // Lambda function for comparing the phases of two signals in the network.
    auto phase_comp = [&](const klut::signal & a, const klut::signal & b)
    {
      if      (ntk.is_constant(a)) { return true;  }
      else if (ntk.is_constant(b)) { return false; }
      return ntk.get_stage(a) < ntk.get_stage(b);
    };

    // Create a view of the network that provides access to fanout information.
    auto ntk_fo = fanout_view<klut>(ntk);

    auto init_size = ntk.size();
    // For each node in the fanout view:
    ntk_fo.foreach_node([&](const klut::signal & node)
    {
      // if ( ntk_fo.is_dangling( node ) || ntk_fo.is_constant(node) )
      // {
      //   return;
      // }

      // Get the number of fanouts for the current node.
      uint32_t fo_size{ 0u };
      std::vector<klut::signal> fanouts;
      fanouts.reserve(fo_size);
      ntk_fo.foreach_fanout( node, [&]( auto const& fo_node ) {
        // if ( !ntk_fo.is_dangling( fo_node ) )
        // {
          ++fo_size;
          fanouts.push_back( fo_node );
        // }
      } );

      // Fix the fanout count (bugged fanouts_size()?)
      ntk._storage->nodes[node].data[0].h1 = fo_size;
      
      VERBOSE_PRINT("\t[NODE {}] FANOUT SIZE = {}\n", node, fo_size);
      // If the current node is a constant or it has fanout ≤ 1, skip to the next node.
      if ( fo_size <= 1 )
      {
        return;
      }
      VERBOSE_PRINT("Processing node {:>5d} out of {:>5d}\r", node, init_size);

      // Fix the fanout count (bugged fanouts_size()?)
      ntk._storage->nodes[node].data[0].h1 = fanouts.size();

      // Sort the fanouts using the phase comparison function.
      std::sort(fanouts.begin(), fanouts.end(), phase_comp);
      VERBOSE_PRINT("\t[NODE {}] SORTED FANOUTS:\n", node);
      if (ps.verbose) 
      {
        printVector(fanouts, 2);
      }

      // Create [fo_size - 1] splitter nodes.
      klut::signal last_spl = node;
      std::vector<klut::signal> splitters;
      splitters.reserve( fanouts.size() - 1 );
      for (auto it = fanouts.begin(); it < fanouts.end() - 1; ++it)
      {
        VERBOSE_PRINT("\t\t[NODE {}] LAST SPL: {}\n", node, last_spl);

        // Create a new splitter node connected to 'last_spl'.
        VERBOSE_PRINT("\t\t[NODE {}] CREATING SPL FOR {}\n", node, *it);
        VERBOSE_PRINT("\t\t[NODE {}] LAST_SPL {} FANOUT BEFORE: {}\n", node, last_spl, ntk.fanout_size(last_spl));
        const klut::signal spl = ntk.explicit_buffer(last_spl, AA_GATE);
        ntk.set_stage_type(spl, ntk.get_stage(*it), AA_GATE);
        VERBOSE_PRINT("\t\t[NODE {}] CREATED SPL {}\n", node, spl);
        VERBOSE_PRINT("\t\t[NODE {}] LAST_SPL {} FANOUT AFTER: {}\n", node, last_spl, ntk.fanout_size(last_spl));
        VERBOSE_PRINT("\t\t[NODE {}] SPL {} FANIN: {}\n", node, spl, ntk.fanin_size(spl));

        // auto n = ntk._storage->nodes[*it];
        // auto& preds = n.children;

        // Update the connections to reflect the splitter's presence.
        VERBOSE_PRINT("\t\t[NODE {}, LAST_SPL {}] UPDATING CONNECTIONS\n", node, last_spl);
        // for (auto& pred : preds)
        for (auto pred_it = ntk._storage->nodes[*it].children.begin();
                  pred_it < ntk._storage->nodes[*it].children.end(); pred_it++ )
        {
          // if ( ntk.is_dangling( pred_it->data ) )
          // {
          //   continue;
          // }

          VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] PRED {}\n", node, last_spl, pred_it->data);
          if (pred_it->data == node)
          {
            VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] RECORDING OLD PREDS\n", node, last_spl);

            VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] PREDS BEFORE\n", node, last_spl);
            pred_it->data = spl;                             // Replace the connection with the splitter.
            VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] PREDS AFTER\n", node, last_spl);

            VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] SPL {} FANOUT BEFORE: {}\n", node, last_spl, spl, static_cast<int>(ntk._storage->nodes[spl].data[0].h1));
            VERBOSE_PRINT("\t\t\t\t Call: {}\n", ntk.fanout_size(spl));
            ntk._storage->nodes[spl].data[0].h1++;  // Increment fan-out of the splitter.
            VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] SPL {} FANOUT  AFTER: {}\n", node, last_spl, spl, static_cast<int>(ntk._storage->nodes[spl].data[0].h1));
            VERBOSE_PRINT("\t\t\t\t Call: {}\n", ntk.fanout_size(spl));

            VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] NODE {} FANOUT BEFORE: {}\n", node, last_spl, node, static_cast<int>(ntk._storage->nodes[node].data[0].h1));
            VERBOSE_PRINT("\t\t\t\t Call: {}\n", ntk.fanout_size(node));
            ntk._storage->nodes[node].data[0].h1--; // Decrement fan-out of the current node.
            VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] NODE {} FANOUT  AFTER: {}\n", node, last_spl, node, static_cast<int>(ntk._storage->nodes[node].data[0].h1));
            VERBOSE_PRINT("\t\t\t\t Call: {}\n", ntk.fanout_size(node));
            
            // splitter has only one fanin, can break the loop
            break;
          }
        }
        last_spl = spl;
        splitters.push_back(spl);
      }

      // Process the last fanout.
      // auto& preds = ntk._storage->nodes[fanouts.back()].children;
      VERBOSE_PRINT("\t\t[NODE {}, LAST_SPL {}] UPDATING CONNECTIONS TO LAST FANOUT {}\n", node, last_spl, fanouts.back());
      // for (auto& pred : preds)
      for (auto pred_it = ntk._storage->nodes[fanouts.back()].children.begin();
                pred_it < ntk._storage->nodes[fanouts.back()].children.end(); pred_it++ )
      {
        // if ( ntk.is_dangling( pred_it->data ) )
        // {
        //   continue;
        // }

        if (pred_it->data == node)
        {
          VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] RECORDING OLD PREDS\n", node, last_spl);

          VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] PREDS BEFORE\n", node, last_spl);
          // for (const auto& entry : ntk._storage->nodes[fanouts.back()].children)  { VERBOSE_PRINT("\t\t\t\t{}\n", entry.data); }
          pred_it->data = last_spl;                            // Replace the connection with the last splitter.
          VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] PREDS AFTER\n", node, last_spl);

          VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] SPL {} FANOUT BEFORE: {}\n", node, last_spl, last_spl, static_cast<int>(ntk._storage->nodes[last_spl].data[0].h1));
          VERBOSE_PRINT("\t\t\t\t Call: {}\n", ntk.fanout_size(last_spl));
          ntk._storage->nodes[last_spl].data[0].h1++; // Increment fan-out of the last splitter.
          VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] SPL {} FANOUT  AFTER: {}\n", node, last_spl, last_spl, static_cast<int>(ntk._storage->nodes[last_spl].data[0].h1));
          VERBOSE_PRINT("\t\t\t\t Call: {}\n", ntk.fanout_size(last_spl));

          VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] NODE {} FANOUT BEFORE: {}\n", node, last_spl, node, static_cast<int>(ntk._storage->nodes[node].data[0].h1));
          VERBOSE_PRINT("\t\t\t\t Call: {}\n", ntk.fanout_size(node));
          ntk._storage->nodes[node].data[0].h1--;     // Decrement fan-out of the current node.
          VERBOSE_PRINT("\t\t\t[NODE {}, LAST_SPL {}] NODE {} FANOUT  AFTER: {}\n", node, last_spl, node, static_cast<int>(ntk._storage->nodes[node].data[0].h1));
          VERBOSE_PRINT("\t\t\t\t Call: {}\n", ntk.fanout_size(node));
        }
      }

      // debug printing
      if (ps.verbose)
      {
        auto fo_ntk { fanout_view( ntk ) };
        for (const auto & node : fanouts)
        {
          VERBOSE_PRINT("\t\t\t node: {}\n", node);
          
          fo_ntk.foreach_fanin( node, [&](const klut::signal & fi_node)
          {
            // if ( fo_ntk.is_dangling( fi_node ) )
            // {
            //   return;
            // }

            VERBOSE_PRINT("\t\t\t\t fanin : {}\n", fi_node);
          });
          fo_ntk.foreach_fanout( node, [&](const klut::signal & fo_node)
          {
            // if ( fo_ntk.is_dangling( fo_node ) )
            // {
            //   return;
            // }

            VERBOSE_PRINT("\t\t\t\t fanout: {}\n", fo_node);
          });
        }
        for (const auto & node : splitters)
        {
          VERBOSE_PRINT("\t\t\t spl : {}\n", node);
          
          fo_ntk.foreach_fanin( node, [&](const klut::signal & fi_node)
          {
            // if ( fo_ntk.is_dangling( fi_node ) )
            // {
            //   return;
            // }

            VERBOSE_PRINT("\t\t\t\t fanin : {}\n", fi_node);
          });
          fo_ntk.foreach_fanout( node, [&](const klut::signal & fo_node)
          {
            // if ( fo_ntk.is_dangling( fo_node ) )
            // {
            //   return;
            // }

            VERBOSE_PRINT("\t\t\t\t fanout: {}\n", fo_node);
          });
        }
      }
      // Ensure that the current node's fan-out count is now 1 (since all other fanouts have been replaced by splitters).
      assert(ntk._storage->nodes[node].data[0].h1 == 1);
    });
  }

  std::vector<Path<NUM_PHASES>> extract_paths() const
  {
    VERBOSE_PRINT("\t[i] ENTERED FUNCTION extract_paths\n");
    std::vector<Path<NUM_PHASES>> paths;

    ntk.foreach_node([&](const klut::signal & fo_node)
    {
      VERBOSE_PRINT("\t\t[i] PROCESSING NODE {}\n", fo_node);
      if (ntk.is_constant(fo_node) || ntk.is_pi(fo_node))
      {
        VERBOSE_PRINT("\t\t\t[NODE {}] the node is CONSTANT/PI\n", fo_node);
        return;
      }
      if ( ntk.get_type(fo_node) == AA_GATE ) 
      {
        VERBOSE_PRINT("\t\t\t[NODE {}] the node is AA, skipping\n", fo_node);
        return;
      }

      // at this point, the node should be AS/SA/T1
      VERBOSE_PRINT("\t\t[NODE {}] the node is AS/SA/T1, continuing...\n", fo_node);

      ntk.foreach_fanin(fo_node, [&](const klut::signal & fi_node)
      {
        VERBOSE_PRINT("\t\t\t[NODE {}] processing fanin {}\n", fo_node, fi_node);
        // Create a path object with only a target
        Path<NUM_PHASES> node_path;
        node_path.targets.emplace( fo_node );

        std::vector<klut::signal> stack { fi_node };
        
        VERBOSE_PRINT("\t\t\t[NODE {}][FANIN {}] created stack\n", fo_node, fi_node);
        
        std::set<klut::signal> seen;
        while (!stack.empty())
        {
          VERBOSE_PRINT("\t\t\t[NODE {}][FANIN {}] stack contents:\n", fo_node, fi_node);
          if (ps.verbose) 
          {
            printVector(stack, 4);
          }

          const klut::signal & n = stack.back();
          stack.pop_back();

          VERBOSE_PRINT("\t\t\t[NODE {}][FANIN {}]: Analyzing node {}\n", fo_node, fi_node, n);

          // A constant does not have any effect on the DFF placement, we can skip it
          if ( ntk.is_constant( n ) )
          {
            continue;
          }        
          const auto n_type = ntk.get_type(n);
          // Found a source of the path, add to sources, do not continue traversal
          if ( ntk.is_pi(n) || n_type == AS_GATE || n_type == SA_GATE || n_type == T1_GATE )
          {
            VERBOSE_PRINT("\t\t\t[NODE {}][FANIN {}]: node {} is a source \n", fo_node, fi_node, n);
            node_path.sources.emplace( n );
          }
          // Found AA gate, add to internal nodes, add parents for further traversal
          else if ( n_type == AA_GATE )
          {
            VERBOSE_PRINT("\t\t\t[NODE {}][FANIN {}]: node is INTERNAL adding fanins \n", fo_node, fi_node, n);
            node_path.internals.emplace( n );

            ntk.foreach_fanin(n, [&](const klut::signal & sig){
              stack.push_back( sig );
            });
          }
          else
          {
            VERBOSE_PRINT("\t\t\t[NODE {}][FANIN {}]: Signal {}: {} is not recognized \n", fo_node, fi_node, n, GATE_TYPE.at( n_type ));
            throw "Unsupported case";
          }
          seen.emplace( n );
        }

        // Identify overlapping paths
        std::vector<size_t> to_merge;
        for (size_t i = 0u; i < paths.size(); ++i)
        {
          Path<NUM_PHASES> & known_paths = paths[i];
          // merge if there are sources in common
          if( haveCommonElements( known_paths.sources, node_path.sources) )
          {
            to_merge.push_back(i);
          }
        }
        // Merge overlapping paths into the path object and remove the path object
        // Iterating in reverse order to preserve order
        for (auto it = to_merge.rbegin(); it != to_merge.rend(); ++it) 
        {
          auto idx = *it;
          node_path.absorb(paths[idx]);
          paths.erase(paths.begin() + idx);
        }
        paths.push_back(node_path);
      });
    });
    return paths;
  }

  /// @brief Create binary variables for DFF placement in a given path
  /// @param path - a path object to insert DFFs into
  /// @param NR - unordered_map of NtkNode objects 
  /// @param verbose - prints debug messages if set to *true*
  /// @return 
  std::tuple<DFF_registry, uint64_t, std::vector<uint64_t>> generate_dff_vars(const Path<NUM_PHASES> & path)
  {
    DFF_registry DFF_REG;
    std::vector<uint64_t> required_SA_DFFs;

    std::vector<std::tuple<klut::signal, uint64_t>> stack;
    for (const klut::signal & tgt : path.targets)
    {
      stack.emplace_back(tgt, 0);
    }
    VERBOSE_PRINT("[DFF] Target nodes: {}\n", fmt::join(path.targets, ","));

    auto precalc_ndff = 0u;
    
    while (!stack.empty())
    { 
      // no structured bindings due to the C++17 bug 
      auto _node_tuple = stack.back();
      const klut::signal fo_node          = std::get<0>(_node_tuple);
      const uint64_t earliest_child_hash  = std::get<1>(_node_tuple);
      stack.pop_back();
      const auto [fo_stage, fo_type] = ntk.get_stage_type(fo_node);

      // AS gates and T1 gate are clocked, so one needs to start one stage earlier
      uint32_t latest_sigma = fo_stage - (fo_type == AS_GATE || fo_type == T1_GATE);
      VERBOSE_PRINT("[DFF] Analyzing child: {}({})[{}]\n", GATE_TYPE.at(fo_type), fo_node, (int)fo_stage);

      ntk.foreach_fanin(fo_node, [&](const klut::signal & fi_node)
      {
        const auto [fi_stage, fi_type] = ntk.get_stage_type(fi_node);
        /* if fi_node is an AA gate, it is allowed to put a DFF at the phase that fi_node is assigned to */
        uint32_t earliest_sigma = fi_stage + (fi_type != AA_GATE);

        VERBOSE_PRINT("\t[DFF] Analyzing parent: {}({})[{}]\n", GATE_TYPE.at(fi_type), fi_node, (int)fi_stage);

        // check if the chain is straight - #DFF is just floor(delta-phase), no need to create the dff vars
        if (fo_type != AA_GATE && fi_type != AA_GATE)
        {
          // special case when an AS gate feeds directly into SA gate
          if (fo_stage == fi_stage)
          {
            VERBOSE_PRINT("\t[DFF] Straight chain: AS{} -> SA{}\n", fi_node, fo_node);
            // do nothing, no additional DFFs needed
            assert(fo_type == SA_GATE && fi_type == AS_GATE && ntk.fanout_size(fi_node) == 1);
          }
          else
          {
            VERBOSE_PRINT("\t[DFF] Straight chain: {}[{}] -> {}[{}]\n", GATE_TYPE.at(fi_type), (int)fi_stage, GATE_TYPE.at(fo_type), (int)fo_stage);
            // straight chain, just floor the difference!
            precalc_ndff += (fo_stage - fi_stage - 1)/NUM_PHASES + (fo_type == SA_GATE); //extra DFF before SA gate
          }
          return;
        }

        VERBOSE_PRINT("\t[DFF] Non-straight chain: {}[{}] -> {}[{}]\n", GATE_TYPE.at(fi_type), (int)fi_stage, GATE_TYPE.at(fo_type), (int)fo_stage);
        std::vector<uint64_t> out_hashes;
        VERBOSE_PRINT("\tAdding new DFFs [reg size = {}]\n", DFF_REG.variables.size());

        for (stage_t stage = earliest_sigma; stage <= latest_sigma; ++stage)
        {
          uint64_t new_hash = DFF_REG.add(fi_node, fo_node, stage);
          out_hashes.push_back(new_hash);
          VERBOSE_PRINT("\tAdded new DFFs at phase {} [reg size = {}]\n", stage, DFF_REG.variables.size());
        }
        VERBOSE_PRINT("\tConnecting new DFFs\n");
        for (auto i = 1u; i < out_hashes.size(); ++i)
        {
          DFF_var & dff = DFF_REG.at( out_hashes[i] );
          dff.parent_hashes.emplace(out_hashes[i-1]);
        }
        if (fo_type == SA_GATE)
        {
          // ensure that the SA gate is placed at least one stage after the fanin 
          //(to leave space for a DFF)
          assert( !out_hashes.empty() );
          // The last DFF in the chain is required
          required_SA_DFFs.push_back(out_hashes.back());
        }
        // if there are DFFs, the earliest_hash is the first hash in the chain
        // otherwise, it is the earliest_hash of the previous chain
        uint64_t earliest_hash = (out_hashes.empty()) ? earliest_child_hash : out_hashes.front();
        // if the node is internal, connect with the fanout phase
        if (fo_type == AA_GATE && !out_hashes.empty() && earliest_hash != 0 && earliest_child_hash != 0)
        {
          DFF_var & child_dff = DFF_REG.at( earliest_child_hash );
          VERBOSE_PRINT("\tPrior node is {}[{}]\n", child_dff.str(), (int)child_dff.stage); 
          // assert(child_dff.fanin == fo_id);
          child_dff.parent_hashes.emplace( out_hashes.back() );
        }
        if (fi_type == AA_GATE)
        {
          stack.emplace_back( fi_node, earliest_hash );
          VERBOSE_PRINT("\tEmplacing {}({})[{}], {}\n", GATE_TYPE.at(fi_type), fi_node, (int)fi_stage, (earliest_hash!=0)?DFF_REG.at(earliest_hash).str():"");
        }
      });
    }
    return std::make_tuple(DFF_REG, precalc_ndff, required_SA_DFFs);
  }

  std::vector<Chain<NUM_PHASES>> generate_chains(const Path<NUM_PHASES> & path,  DFF_registry & DFF_REG)
  {
    std::vector<Chain<NUM_PHASES>> out_chains; 
    std::vector<Chain<NUM_PHASES>> stack;
    
    VERBOSE_PRINT("[i]: Starting extraction of chains \n");
    // get the fanouts of the path
    for (const auto & [hash, dff]: DFF_REG.variables)
    {
      const auto [fo_stage, fo_type] = ntk.get_stage_type(dff.fanout);
      auto fanout_stage = fo_stage - ( fo_type == AS_GATE );
      auto it = std::find(path.targets.begin(), path.targets.end(), dff.fanout);
      if (it != path.targets.end() && ( ( fanout_stage < 0 ) || ( ( fanout_stage >= 0 ) && ( dff.stage >= static_cast<uint32_t>( fanout_stage ) ) ) ))
      {
        stack.emplace_back( hash );
      }
    }
    
    // Move along the path in DFS order and record all chains
    while (!stack.empty())
    {
      VERBOSE_PRINT("[i] Stack size is {} \n", stack.size());
      auto chain = stack.back();
      stack.pop_back();

      // get the earliest DFF
      VERBOSE_PRINT("\t[i] The chain has {} sections\n", chain.sections.size());
      uint64_t hash = chain.sections.back().back();
      DFF_var & dff = DFF_REG.at( hash );

      VERBOSE_PRINT("\t\t[i] The DFF {} has {} parents\n", DFF_REG.at( hash ).str(),  dff.parent_hashes.size() );

      bool returned_current_chain = false;
      for (const uint64_t parent_hash : dff.parent_hashes)
      {
        auto chain_copy = chain; 
        VERBOSE_PRINT("\t\t[i] Advancing towards fanin {}\n", DFF_REG.at( parent_hash ).str() );
        bool status = chain_copy.append(parent_hash, DFF_REG);
        VERBOSE_PRINT((status) ? "\t\t\tAdded new section!\n" :"\t\t\tExtended existing section!\n"  );
        VERBOSE_PRINT("\t\t\tThe new length is {}\n", chain_copy.sections.size() );
        
        stack.push_back( chain_copy );
        if (status && !returned_current_chain && chain_copy.sections.size() == NUM_PHASES)
        {
          VERBOSE_PRINT("\t\tAdding the chain to the output\n");
          out_chains.push_back(chain);
          returned_current_chain = true;
        }
      }
    }
    return out_chains;
  }

  std::tuple<int, int, int> parse_var(const std::string& s, char delimiter = '_') 
  {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) 
    {
      tokens.push_back(token);
    }
    return std::make_tuple(std::stoi(tokens[1]), std::stoi(tokens[2]), std::stoi(tokens[3]));
  } 

  /// @brief Place DFFs within the multiphase network using the CP-SAT solver in Python OR-Tools
  /// @param cfg_name - name of the config file
  /// @return 
  std::pair<int, 
    phmap::flat_hash_map<klut::signal, phmap::flat_hash_map<klut::signal, std::vector<uint32_t>>>
  > 
  cpsat_ortools(const std::string & cfg_filename) 
  {
    std::string command = fmt::format("{} {} {} {}", PYTHON_EXECUTABLE, PYTHON_DFF_PLACEMENT, cfg_filename, ps.dff_insertion_max_time);

    const std::string output = run_command(command);

    // Use regex to find the objective value in the output
    std::string pattern = "Objective value: (\\d+)";
    std::regex regex(pattern);
    std::smatch match;

    // Flag to start recording lines after finding the objective value
    bool record_lines = false;

    int obj_value ;

    // Split output into lines
    std::istringstream iss(output);
    std::string line;
    phmap::flat_hash_map<klut::signal, phmap::flat_hash_map<klut::signal, std::vector<uint32_t>>> DFFs;
    while (std::getline(iss, line)) {
      if (record_lines) 
      {
        // Record or process the line
        auto [fanin, fanout, stage] = parse_var(line);

        VERBOSE_PRINT("Detected {} {} {}\n",fanin, fanout, stage);
        DFFs[fanin][fanout].push_back(stage);
      }

      if (std::regex_search(line, match, regex) && match.size() > 1) {
        std::string value_str = match[1];
        obj_value = std::stoi(value_str);
        record_lines = true; // Start recording lines after finding the value_str
      }
    }

    if (!record_lines) {
      throw std::runtime_error("Objective value not found in the output.");
    }

    // Assuming you want to return the objective value as an integer
    // Note: This conversion and return should be done within the regex match condition
    // if you only care about the first occurrence.
    return std::make_pair(obj_value, DFFs);
  }

  void insert_dffs(const klut::signal fanin, const klut::signal fanout, const std::vector<uint32_t> stages)
  {
    // std::sort(stages.begin(), stages.end());

    klut::signal current_fanin = fanin;
    for (auto stage : stages)
    {
      klut::signal dff = ntk.explicit_buffer(current_fanin, AS_GATE);
      ntk.set_stage(dff, stage);
      VERBOSE_PRINT("Created DFF {} between {} and {}\n", dff, current_fanin, fanout);
      current_fanin = dff;
    }
    for (auto pred_it = ntk._storage->nodes[fanout].children.begin();
              pred_it < ntk._storage->nodes[fanout].children.end(); ++pred_it )
    {
      if (pred_it->data == fanin)
      {
        pred_it->data = current_fanin;
        VERBOSE_PRINT("\tFanin of {} changed from {} to {}\n", fanout, fanin, current_fanin);
        ntk._storage->nodes[current_fanin].data[0].h1++; // Increment fan-out of the DFF.
        ntk._storage->nodes[fanin].data[0].h1--; // Decrement fan-out of the original fanin.
      }
    }
  }

  void process_path(std::vector<int> & local_num_dff, const int idx, const Path<NUM_PHASES>& path, const std::string cfg_name = "CPSAT_CFG")
  {
    VERBOSE_PRINT("\tAnalyzing the path\n");

    // *** Create binary variables
    auto [DFF_REG, precalc_ndff, required_SA_DFFs] = generate_dff_vars(path);
    local_num_dff[idx] += precalc_ndff;
    VERBOSE_PRINT("\t\t\t\t[i]: Precalculated {} DFFs\n", precalc_ndff);

    // *** Generate constraints
    std::vector<Chain<NUM_PHASES>> chains = generate_chains(path, DFF_REG);
    VERBOSE_PRINT("\tCreated {} chains\n", chains.size());

    // *** If there's anything that needs optimization
    if (!chains.empty())
    {
      std::string cfg_filename = fmt::format("{}_{}.csv", cfg_name, idx);
      write_chains<NUM_PHASES>(chains, DFF_REG, required_SA_DFFs, cfg_filename);
      auto [num_dff, DFFs] = cpsat_ortools(cfg_filename);
      // VERBOSE_PRINT("OR Tools optimized to {} DFF\n", num_dff);
      local_num_dff[idx] += num_dff;
      VERBOSE_PRINT("\t\t\t\t[i] total CPSAT #DFF = {}\n", local_num_dff[idx]);


      // insert DFF along the path
      for (auto [fanin, inner_map] : DFFs)
      {
        for (auto [fanout, stages] : inner_map)
        {
          insert_dffs(fanin, fanout, stages);
        }
      }
    }
  }

  void run(const std::string cfg_filename = "/tmp/ilp_cfg.csv")
  {
    stopwatch t( st.time_total );

    // First, perform the macro-optimization (assign a phase to each gate)
    VERBOSE_PRINT("\tWriting config {}\n", cfg_filename);
    write_klut_specs(cfg_filename);
    auto [obj_val, status] = cpsat_macro_opt(cfg_filename);

    splitter_ntk_insertion();

    std::vector<Path<NUM_PHASES>> paths = extract_paths();

    std::vector<int> local_num_dff(paths.size());
    int idx = 0;
    for (const Path<NUM_PHASES> & path : paths)
    {
      process_path(std::ref(local_num_dff), idx, std::ref(path));
      idx++;
    }

    st.num_added_dffs = std::accumulate(local_num_dff.begin(), local_num_dff.end(), 0);

    if (ps.balance_pos)
    {
      // determine the latest PO stage
      uint64_t max_stage { 0u };
      ntk.foreach_po([&](const klut::signal & node)
      {
        max_stage = (max_stage > ntk.get_stage(node))? max_stage:ntk.get_stage(node);
      });
      // equialize the PO epochs
      ntk.foreach_po([&](const klut::signal & node)
      {
        st.num_added_dffs += (max_stage - ntk.get_stage(node)) / NUM_PHASES;
      });
    }

    VERBOSE_PRINT("Placed a total of {} DFFs\n", st.num_added_dffs);
  }

private:
};


template<class Ntk, uint8_t NUM_PHASES>
multiphase_balancing_stats multiphase_balancing(Ntk & ntk, multiphase_balancing_params const & ps = {})
{

  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );

  static_assert( has_foreach_pi_v<Ntk>, "NtkSource does not implement the foreach_pi method" );
  static_assert( has_foreach_node_v<Ntk>, "NtkSource does not implement the foreach_node method" );
  static_assert( has_foreach_fanin_v<Ntk>, "NtkSource does not implement the foreach_fanin method" );
  static_assert( has_foreach_po_v<Ntk>, "NtkSource does not implement the foreach_po method" );

  multiphase_balancing_stats st;
  multiphase_balancing_impl<Ntk, NUM_PHASES> ( ntk, ps, st ).run();
  return st;
}

} // namespace mockturtle