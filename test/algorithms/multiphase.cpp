#include <catch.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <lorina/genlib.hpp>
#include <lorina/super.hpp>
#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xmg_npn.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/io/super_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/binding_view.hpp>
#include <mockturtle/views/mph_view.hpp>

#include <parallel_hashmap/phmap.h>

#include <mockturtle/algorithms/multiphase.hpp>

using namespace mockturtle;

// const std::string test_library =    "GATE zero 0.00 z=CONST0;\n"
//                                     "GATE one 0.00 z=CONST1;\n"
//                                     "GATE AND2_SA 3 O=(a&b);           PIN * UNKNOWN 1.0 999.0 1 0.025 1 0.025\n"
//                                     "GATE OR2_AA 3 O=a|b;         PIN * UNKNOWN 1.0 999.0 1 0.025 1 0.025\n"
//                                     "GATE XOR2_AS 11 O=(a&!b)|(!a&b);  PIN * UNKNOWN 1.0 999.0 1 0.025 1 0.025\n"
//                                     "GATE NOT_AS 9 O=!a;               PIN a UNKNOWN 1.0 999.0 1 0.025 1 0.025\n"
//                                     "GATE buf 7 O=a;                PIN a UNKNOWN 1.0 999.0 1 0.025 1 0.025\n";
	
std::string const test_library = "GATE   inv1    1 O=!a;            PIN * INV     1 999 0.9 0.3 0.9 0.3\n"
                                 "GATE   inv2    2 O=!a;            PIN * INV     2 999 1.0 0.1 1.0 0.1\n"
                                 "GATE   nand2   2 O=!(a*b);        PIN * INV     1 999 1.0 0.2 1.0 0.2\n"
                                 "GATE   xor2    5 O=a^b;           PIN * UNKNOWN 2 999 1.9 0.5 1.9 0.5\n"
                                 "GATE   maj3    3 O=a*b+a*c+b*c;   PIN * INV     1 999 2.0 0.2 2.0 0.2\n"
                                 "GATE   buf     2 O=a;             PIN * NONINV  1 999 1.0 0.0 1.0 0.0\n"
                                 "GATE   zero    0 O=CONST0;\n"
                                 "GATE   one     0 O=CONST1;";
  
// const phmap::flat_hash_map<std::string, uint8_t> gate_types = { {"AND2_SA", SA_GATE},  {"OR2_AA", AA_GATE},  {"XOR2_AS", AS_GATE},  {"NOT_AS", AS_GATE},  {"DFF_AS", AS_GATE},  {"buf", AS_GATE} };

const phmap::flat_hash_map<std::string, uint8_t> gate_types = { {"AND2_SA", SA_GATE},  {"OR2_AA", AA_GATE},  {"XOR2_AS", AS_GATE},  {"NOT_AS", AS_GATE},  {"DFF_AS", AS_GATE},  {"buf", AS_GATE},
{"inv1", AS_GATE},  {"inv2", AS_GATE},  {"nand2", AS_GATE},  {"xor2", AS_GATE},  {"maj3", SA_GATE} 
 };

constexpr uint8_t NUM_VARS = 4u;
constexpr uint8_t NUM_PHASES = 4u;

typedef mockturtle::klut_network klut;
typedef mockturtle::mph_view<klut, NUM_PHASES> mph_klut;
typedef uint64_t node_t;

template <typename Ntk, typename tech_lib_t>
std::tuple<mph_klut, mockturtle::map_stats> map_wo_pb 
( 
  const Ntk & input_ntk, 
  const tech_lib_t & tech_lib, 
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

  mockturtle::mph_view<klut, NUM_PHASES> mph_ntk { res, gate_types };
  
  return std::make_tuple( mph_ntk, st );
}

TEST_CASE( "testmapping", "[mph_view]" )
{
  std::vector<gate> gates;

  std::istringstream in( test_library );
  auto techlib_result = lorina::read_genlib( in, genlib_reader( gates ) );
  CHECK( techlib_result == lorina::return_code::success );

  tech_library<NUM_VARS, mockturtle::classification_type::p_configurations> tech_lib( gates );

  mockturtle::mig_network ntk_original;
  
  auto a = ntk_original.create_pi();
  auto b = ntk_original.create_pi();
  auto c = ntk_original.create_pi();
  auto d = ntk_original.create_pi();
  auto e = ntk_original.create_pi();

  // Create basic gates
  auto and_ab  = ntk_original.create_and(a, b); // AND gate with inputs a and b
  auto nand_cd = ntk_original.create_nand(c, d); // NAND gate with inputs c and d
  auto xor_ab  = ntk_original.create_xor(a, b); // XOR gate with inputs a and b

  auto maj_abc = ntk_original.create_maj(a, b, c); // Majority gate with inputs a, b, and c

  auto temp_xor_cd = ntk_original.create_xor(c, d);
  auto xo3_abcd = ntk_original.create_xor(temp_xor_cd, e); // XOR for c XOR d with e

  auto ite_abc = ntk_original.create_ite(a, b, c); // ITE gate: if a then b else c

  auto complex_gate = ntk_original.create_and(xor_ab, ntk_original.create_or(maj_abc, ite_abc));

  ntk_original.create_po(and_ab);
  ntk_original.create_po(nand_cd);
  ntk_original.create_po(xo3_abcd);
  ntk_original.create_po(complex_gate);

  auto _result = map_wo_pb(ntk_original, tech_lib, false, gate_types, false); 
  auto mapped_ntk = std::get<0>(_result);
  auto mapper_stats = std::get<1>(_result);

  auto st = mockturtle::multiphase_balancing<mph_klut, NUM_PHASES> ( mapped_ntk );
  
  mapped_ntk.foreach_node([&] (const auto node) 
  {
    if (mapped_ntk.is_pi(node))
    {
      CHECK( mapped_ntk.get_type(node) == PI_GATE );
      return;
    }
    mapped_ntk.foreach_fanin(node, [&] (const auto fanin)
    {
      auto node_stage = mapped_ntk.get_stage(node);
      auto node_type = mapped_ntk.get_type(node);
      if (mapped_ntk.get_type(fanin) == AS_GATE && node_type == AS_GATE)
      {
        CHECK(node_stage > mapped_ntk.get_stage(fanin));
      }
      else
      {
        CHECK(node_stage >= mapped_ntk.get_stage(fanin));
      }
    });

  });  

  st.report();

}