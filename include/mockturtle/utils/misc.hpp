
#pragma once

#include <iostream>
#include <string>
#include <set>
#include <fmt/format.h>

#define DEBUG_PRINT(format, ...) if (verbose) fmt::print(format, ##__VA_ARGS__)

// *** FILE PATHS
/* Gate costs are based on CONNECT library (from Japan) */
const std::string DATABASE_PATH { "../rsfq_tech_lib/LIBRARY_2023_06_27_CONNECT_CONSERVATIVE.genlib" } ;
/*The number of internal DFFs within each cell. 
Some of them are necessary not only for path balancing but also 
for synchronizing the pulses for AND gates. I include them 
in total DFF count */
const std::string NDFF_PATH { "../rsfq_tech_lib/nDFF_2023_06_27_CONNECT_CONSERVATIVE.csv" } ; 
const std::string LibEntry_file { "../rsfq_tech_lib/LibEntry_2023_06_27_CONNECT_CONSERVATIVE.csv" };

// Need to provide a valid Python executable
const std::string PYTHON_EXECUTABLE { "~/anaconda3/bin/python" }; // 

// Python script that runs OR-tools for phase assignment
// const std::string PYTHON_PHASE_ASSIGNMENT { "../python/multiphase/decomposed_ilp_max.py" };
const std::string PYTHON_PHASE_ASSIGNMENT { "../python/multiphase/phase_assignment.py" };


// Python script that runs OR-tools for DFF placement
const std::string PYTHON_DFF_PLACEMENT { "../python/multiphase/config_solver.py" };
const std::string PYTHON_DFF_PLACEMENT_UNION { "../python/multiphase/config_solver_union.py" };
// /Users/brainkz/Documents/GitHub/ortools_python/config_solver.py

// Folder containing OPENCORES benchmarks in BLIF format
const std::string OPENCORES_FOLDER { "../benchmarks/opencores" };

// Folder containing ISCAS89 benchmarks in AIG format 
const std::string ISCAS89_FOLDER { "../benchmarks/iscas89" };

// Path prefix for files containing the specifications of the global nodemap. This nodemap contains the implementations of the compound gates found during enumeration process
const std::string NODEMAP_PREFIX = "../GNM/x3"; //deprecated

const std::string NODEMAP_BINARY_PREFIX = "../GNM/GNM_global";


std::string repeatString(const std::string& str, int count) 
{
  std::string repeatedStr;
  for (int i = 0; i < count; ++i) 
  {
      repeatedStr += str;
  }
  return repeatedStr;
}

template <typename T1, typename T2>
inline auto generic_max(const T1& a, const T2& b) 
{
  return (a > b) ? a : b;
}

template <typename KeyType, typename ValueType>
void printUnorderedMap(const std::unordered_map<KeyType, ValueType>& map, const int indent_lvl = 0) 
{
  for (const auto& entry : map) 
  {
    fmt::print("{}Key: {} - Value: {}\n", repeatString("\t", indent_lvl), entry.first, entry.second);
  }
}

template <typename ValueType>
void printVector(const std::vector<ValueType>& vec, const int indent_lvl = 0) 
{
  for (const auto& entry : vec) 
  {
    fmt::print("{}{}\n", repeatString("\t", indent_lvl), entry);
  }
}

template <typename elem>
bool haveCommonElements(const std::set<elem>& set1, const std::set<elem>& set2) 
{
    for (const auto& element : set1) 
    {
        if (set2.count(element) > 0) 
        {
            return true;  // Found a common element
        }
    }
    return false;  // No common elements found
}

template <typename Key, typename Value>
Value getMaxValue(const std::unordered_map<Key, Value>& map) 
{
  if (map.empty()) 
  {
    std::cerr << "The map is empty." << std::endl;
    // Return a default-constructed Value or handle the error accordingly
    return Value();
  }
  auto max_entry = std::max_element(map.begin(), map.end(),
    [](const auto& lhs, const auto& rhs) 
    {
        return lhs.second < rhs.second;
    });
  return max_entry->second;
}

template <std::size_t ArrSize>
struct ArrayHash
{
  size_t operator()(const std::array<uint64_t, ArrSize>& arr) const
  {
    size_t seed = 0;
    for (uint64_t value : arr) 
    {
      seed ^= value + 0x9e3779b97f4a7c55 + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

// std::tuple<int, std::vector<gate>> readGenlibFromFile(const std::string& databasePath) {
//     std::vector<gate> gates;
//     int statusCode = 0;

//     std::ifstream inputFile(databasePath);
//     if (!inputFile.is_open()) {
//         std::cerr << "Error opening input file." << std::endl;
//         statusCode = 1;
//         return std::make_tuple(statusCode, gates);
//     }

//     if (lorina::read_genlib(inputFile, lorina::genlib_reader(gates)) != lorina::return_code::success) {
//         std::cerr << "Error reading genlib." << std::endl;
//         statusCode = 1;
//         return std::make_tuple(statusCode, gates);
//     }

//     return std::make_tuple(statusCode, gates);
// }