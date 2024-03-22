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

#include <string>
#include <vector>
#include <set>
#include <cstdio>
#include <filesystem>
#include <thread>

#include <fmt/format.h>
#include <lorina/aiger.hpp>
#include <lorina/blif.hpp>
#include <lorina/genlib.hpp>
#include <lorina/bench.hpp>
#include <mockturtle/algorithms/rsfq/rsfq_network_conversion.hpp>
#include <mockturtle/algorithms/rsfq/rsfq_path_balancing.hpp>

#include <mockturtle/algorithms/multiphase.hpp>

#include <mockturtle/algorithms/mapper.hpp>
// #include <mockturtle/algorithms/nodes.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/retiming.hpp>
// #include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/io/bench_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/binding_view.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/rsfq_view.hpp>

#include <mockturtle/views/mph_view.hpp>

#include <mockturtle/algorithms/functional_reduction.hpp>
#include <mockturtle/algorithms/klut_to_graph.hpp>

// mockturtle/algorithms/mig_algebraic_rewriting.hpp

// #include <mockturtle/io/genlib_utils.hpp>

// #include <mockturtle/utils/GNM_global.hpp> // GNM global is stored here

#include <mockturtle/utils/misc.hpp>

#include <experiments.hpp>

constexpr uint8_t NUM_PHASES { 7u };
constexpr uint8_t NUM_VARS { 4u };

typedef mockturtle::mph_view<mockturtle::klut_network, NUM_PHASES> mph_klut;
typedef uint64_t node_t;
typedef mockturtle::klut_network klut;

constexpr std::array<int,12> COSTS_SUNMAGNETICS = {7, 9, 8, 8, 12, 8, 999, 999, 999, 8, 3, 0};

constexpr std::array<int, 12> COSTS_MAP = COSTS_SUNMAGNETICS;


// std::tuple<mockturtle::binding_view<klut>, mockturtle::map_stats> map_wo_pb 
template <typename Ntk>
std::tuple<mph_klut, mockturtle::map_stats> map_wo_pb 
( 
  const Ntk & input_ntk, 
  const mockturtle::tech_library<4u, mockturtle::classification_type::p_configurations> & tech_lib, 
  const bool area_oriented,
  const phmap::flat_hash_map<std::string, uint8_t> gate_types,
  const bool verbose = false
)
{
  mockturtle::map_params ps;
  ps.verbose = verbose;
  ps.cut_enumeration_ps.minimize_truth_table = true;
  ps.cut_enumeration_ps.cut_limit = 24;
  ps.cut_enumeration_ps.verbose = true;
  // ps.buffer_pis = false;
  if (area_oriented)
  {
      ps.skip_delay_round = true;
      ps.required_time = std::numeric_limits<float>::max();
  }
  mockturtle::map_stats st;
  mockturtle::binding_view<klut> res = map( input_ntk, tech_lib, ps, &st );

  mph_klut mph_ntk { res, gate_types };
  
  return std::make_tuple( mph_ntk, st );
}
  // res.foreach_gate([&] (const auto & n)
  // {
  //   fmt::print("{}: {} \n", n, res.get_binding( n ).name);
  // });


std::tuple<int, phmap::flat_hash_map<unsigned int, unsigned int>, std::string>  cpsat_macro_opt(const std::string & cfg_name, uint8_t n_phases) 
{
  std::string command = fmt::format("{} {} {} {}", PYTHON_EXECUTABLE, PYTHON_PHASE_ASSIGNMENT, n_phases, cfg_name);
  
  std::string pattern = "Objective value: (\\d+)";

  // Run the command and capture its output
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) 
  {
    std::cerr << "Error running the command." << std::endl;
    throw;
  }

  char buffer[128];
  std::string output;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) 
  {
    output += buffer;
  }
  fmt::print(output);

  int result = pclose(pipe);
  if (result == -1) 
  {
    std::cerr << "Error closing the command pipe." << std::endl;
    throw;
  }

  // Parse output
  std::istringstream iss(output);

  std::string line;
  std::string solve_status;
  int objective_value;
  phmap::flat_hash_map<unsigned int, unsigned int> key_value_pairs;

  while (std::getline(iss, line))
  {
    if (line.find("Solve status:") != std::string::npos) 
    {
      if (line.find("OPTIMAL") || line.find("FEASIBLE"))
      {
        solve_status = "SUCCESS";
        break;
      }
      else
      {
        solve_status = "UNKNOWN";
        return {0, {}, ""};
      }
    }
  }

  // Parse the second line (Objective value)
  std::getline(iss, line);
  if (line.find("Objective value: ") != std::string::npos) 
  {
      objective_value = std::stoi(line.substr(17));
  } 
  else 
  {
      // Handle missing or incorrect format for objective value
      std::cerr << "Error: Objective value not found or invalid format." << std::endl;
      return {0, {}, ""};
  }

  // Parse the key-value pairs in subsequent lines
  while (std::getline(iss, line)) 
  {
      std::istringstream line_stream(line);
      unsigned int key, value;
      char colon;
      if (line_stream >> key >> colon >> value && colon == ':') 
      {
          key_value_pairs[key] = value;
      } 
      else 
      {
          // Handle incorrect format for key-value pairs
          std::cerr << "Error: Invalid format for key-value pairs." << std::endl;
          return {0, {}, ""};
      }
  }

  return {objective_value, key_value_pairs, solve_status};
}

// Function to read unordered_map from CSV file
phmap::flat_hash_map<std::string, int> readCSV(const std::string& filename) 
{
  std::ifstream infile(filename);             // Open the input file stream
  phmap::flat_hash_map<std::string, int> map;   // Create the unordered_map
  
  std::string line;
  std::getline(infile, line);                 // Ignore the header row

  // Read each subsequent row and add the key-value pair to the unordered_map
  while (std::getline(infile, line)) 
  {
    std::stringstream ss(line);
    std::string key;
    int value;
    std::getline(ss, key, ',');
    ss >> value;
    map[key] = value;
  }
  infile.close(); // Close the input file stream
  return map;
}

int cpsat_ortools(const std::string & cfg_name) 
{
  std::string command = fmt::format("{} {} {}", PYTHON_EXECUTABLE, PYTHON_DFF_PLACEMENT, cfg_name);
  std::string pattern = "Objective value: (\\d+)";

  // Run the command and capture its output
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) 
  {
    std::cerr << "Error running the command." << std::endl;
    return -1;
  }

  char buffer[128];
  std::string output;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) 
  {
    output += buffer;
  }
  fmt::print(output);

  int result = pclose(pipe);
  if (result == -1) 
  {
    std::cerr << "Error closing the command pipe." << std::endl;
    return -1;
  }

  // Use regex to find the objective value in the output
  std::regex regex(pattern);
  std::smatch match;
  if (std::regex_search(output, match, regex) && match.size() > 1) 
  {
    std::string value_str = match[1];
    return std::stoi(value_str);
  } 
  else 
  {
    std::cerr << "Objective value not found in the output." << std::endl;
    return -1;
  }
}

int main()
{
  using namespace experiments;
  using namespace mockturtle;

  fmt::print( "[i] processing technology library\n" );

  // library to map to technology
  std::vector<gate> gates;
  std::ifstream inputFile( "/Users/brainkz/Documents/GitHub/mockturtle_latest/experiments/cell_libraries/CONNECT.genlib" );
  if ( lorina::read_genlib( inputFile, genlib_reader( gates ) ) != lorina::return_code::success ) { return 1; }
  mockturtle::tech_library_params tps; // tps.verbose = true;
  tech_library<NUM_VARS, mockturtle::classification_type::p_configurations> tech_lib( gates, tps );

  phmap::flat_hash_map<std::string, uint8_t> gate_types = { {"AND2_SA", SA_GATE},  {"OR2_SA", AA_GATE},  {"XOR2_AS", AS_GATE},  {"NOT_AS", AS_GATE},  {"DFF_AS", AS_GATE} };

  #pragma region benchmark_parsing
    // *** BENCHMARKS OF INTEREST ***

    // auto benchmarks1 = all_benchmarks( experiments::int2float |  experiments::priority | experiments::voter  | experiments::c432 | experiments::c880 | experiments::c1908  | experiments::c3540  | experiments::c1355 );
    auto benchmarks1 = all_benchmarks( 
      int2float |  
      // priority | 
      // voter  | 
      // c432 | 
      // c880 | 
      // c1908 | 
      // c3540 | 
      // c1355 |
      0u
      );

  #pragma endregion

  // *** START PROCESSING BECNHMARKS ***
  for ( auto const& benchmark : benchmarks1 )
  {
    fmt::print( "[i] processing {}\n", benchmark );

    // *** LOAD NETWORK INTO MIG ***
    mockturtle::mig_network ntk_original;
    fmt::print("USING THE AIGER READER\n");
    if ( lorina::read_aiger( benchmark_path( benchmark ), aiger_reader( ntk_original ) ) != lorina::return_code::success )
    {
      fmt::print("Failed to read {}\n", benchmark);
      continue;
    }

    std::chrono::high_resolution_clock::time_point mapping_start = std::chrono::high_resolution_clock::now();

    // *** MAP, NO NEED FOR RETIMING/PATH BALANCING AT THIS TIME ***
    fmt::print("Started mapping {}\n", benchmark);
    auto _result = map_wo_pb(ntk_original, tech_lib, false, gate_types, false); 
    auto mapped_ntk = std::get<0>(_result);
    auto mapper_stats = std::get<1>(_result);
    fmt::print("Finished mapping {}\n", benchmark);
    // std::cout << typeid(mapped_ntk).name() << '\n';

    // *** DECOMPOSE COMPOUND GATES INTO PRIMITIVES, REMOVE DFFS, REPLACE OR GATES WITH CB WHERE POSSIBLE ***
    // mapped_ntk.foreach_gate([&] (const auto & n)
    // {
    //   fmt::print("{}: {} | {}\n", n, mapped_ntk.get_type( n ), mapped_ntk.get_stage( n ));
    // });
    multiphase_balancing_params ps;
    ps.verbose = true;
    auto st = multiphase_balancing<mph_klut, NUM_PHASES> ( mapped_ntk, ps );
    st.report();

    return 0;
  }
}