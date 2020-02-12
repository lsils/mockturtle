#include <cstdio>
#include <percy/percy.hpp>
#include <percy/printer_xmg3.hpp>
#include <../../include/mockturtle/networks/xmg.hpp>
#include <chrono>
#include <fstream>
#include <set>
#include <map>
#include <string>
#include <future>
#include <fmt/format.h>

#define MAX_TESTS 256

using namespace percy;
using kitty::dynamic_truth_table;

template <typename TT>
inline TT ternary_and(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return a & b & c; });
}

template <typename TT>
inline TT ternary_andxor(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return a ^ (b & c); });
}

template <typename TT>
inline TT ternary_dot(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return a ^ (c | (a & b)); });
}

template <typename TT>
inline TT ternary_gamble(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return ~((a & b & c) ^ (~a & ~b & ~c)); });
}

template <typename TT>
inline TT ternary_majority(const TT &first, const TT &second, const TT &third)
{
    return kitty::ternary_majority(first, second, third);
}

template <typename TT>
inline TT ternary_mux(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return (a & b) | (~a & c); });
}

template <typename TT>
inline TT ternary_onehot(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return (a & ~b & ~c) ^ (~a & b & ~c) ^ (~a & ~b & c); });
}

template <typename TT>
inline TT ternary_orand(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return a & (b | c); });
}

template <typename TT>
inline TT ternary_xor(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return a ^ b ^ c; });
}

template <typename TT>
inline TT ternary_xorand(const TT &first, const TT &second, const TT &third)
{
    return ternary_operation(first, second, third, [](auto a, auto b, auto c) { return a & (b ^ c); });
}


template <typename Fn>
void add_all_three_input_primitives(percy::spec &spec, Fn &&fn, percy::printer<std::string> &pp)
{
    
    kitty::dynamic_truth_table a{3};
    kitty::dynamic_truth_table b{3};
    kitty::dynamic_truth_table c{3};
    kitty::dynamic_truth_table cnst0{3};
    
    kitty::create_nth_var(a, 0);
    kitty::create_nth_var(b, 1);
    kitty::create_nth_var(c, 2);
    kitty::create_from_hex_string(cnst0, "00");
    
    std::array<kitty::dynamic_truth_table, 8> choices = {a, ~a, b, ~b, c, ~c, cnst0, ~cnst0};

    //std::array<kitty::dynamic_truth_table, 2> prim;
    kitty::dynamic_truth_table prim1{3};
    kitty::dynamic_truth_table prim2{3};

    
    std::set<std::string> all_xor;
    std::set<std::string> all_maj;
    
    for (auto ind0 = 0; ind0 < 8; ind0++)
    {
        for (auto ind1 = 0; ind1 < 8; ind1++)
        {
            for (auto ind2 = 0; ind2 < 8; ind2++)
            {
                prim1 = fn["xor"](choices[ind0], choices[ind1], choices[ind2]);
                prim2 = fn["majority"](choices[ind0], choices[ind1], choices[ind2]);
                std::array<int, 3> inds = {ind0, ind1, ind2};
                std::string mask1 = "[";
                std::string mask2 = "<";
                for (int j = 0; j < 3; j++)
                {
                    switch (inds[j])
                    {
                        case 0:
                            mask1 += "{0}";
                            mask2 += "{0}";
                            break;
                        case 1:
                            mask1 += "!{0}";
                            mask2 += "!{0}";
                            break;
                        case 2:
                            mask1 += "{1}";
                            mask2 += "{1}";
                            break;
                        case 3:
                            mask1 += "!{1}";
                            mask2 += "!{1}";
                            break;
                        case 4:
                            mask1 += "{2}";
                            mask2 += "{2}";
                            break;
                        case 5:
                            mask1 += "!{2}";
                            mask2 += "!{2}";
                            break;
                        case 6:
                            mask1 += "F";
                            mask2 += "F";
                            break;
                        case 7:
                            mask1 += "T";
                            mask2 += "T";
                            break;
                    }
                    if (j < 2)
                    {
                        mask1 += "";
                        mask2 += "";
                    }
                }
                mask1 += "]";
                mask2 += ">";
                
                if (!kitty::is_normal(prim1))
                {
                    prim1 = ~prim1;
                    mask1 = "!" + mask1;
                }

                if (!kitty::is_normal(prim2))
                {
                    prim2 = ~prim2;
                    mask2 = "!" + mask2;
                }
                
                auto ret_xor = all_xor.insert(kitty::to_hex(prim1));
                auto ret_maj = all_maj.insert(kitty::to_hex(prim2));
                if (ret_xor.second )
                {
                    // This is the place to add all the primitive of the basic logic function
                    spec.add_primitive(prim1);
                    pp.add_function(prim1, mask1);
                }
                if (ret_maj.second)
                {
                    spec.add_primitive(prim2);
                    pp.add_function(prim2, mask2);
                }
            }
        }
    }
}

struct syn_result
{
    int n;
    std::string chain;
    long long syn_time;
};

template <typename Fn>
syn_result synthesize_func_general_primitive(kitty::dynamic_truth_table const &tt, Fn &&fn, mockturtle:: xmg_network& xmg)
{
    percy::chain chain;
    percy::spec spec;
    spec.fanin = 3;
    spec.verbosity = 0;
    
    // add all three input primitives for the function 'fn'.
    percy::printer<std::string> pp(chain, xmg);
    add_all_three_input_primitives(spec, fn, pp);

    spec[0] = tt;
    
    auto beg = std::chrono::high_resolution_clock::now();
    auto result = percy::synthesize(spec, chain);
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - beg).count();
    
    assert(result == percy::success);
    
    assert(chain.simulate()[0] == spec[0]);
    
    auto result1 = syn_result{chain.get_nr_steps(), pp(), dur};
    std::cout << "XMG has " << xmg.num_pis() << " primary inputs" << std::endl;
    std::cout << "XMG size = " << xmg.size() << std::endl;
    std::cout << "XMG num gates = " << xmg.num_gates() << std::endl;
    return result1;
}

template <typename Fn>
void synthesize_all_npn_classes(Fn &&fn, int num_inputs, std::string ofname = "logfile")
{
    std::map<std::string, int> func_count;
    mockturtle::xmg_network xmg; 
    for ( auto i = 0; i < num_inputs; ++i )
        xmg.create_pi();

    std::map<int, int> nf;
    std::map<int, int> nc;
    
    kitty::dynamic_truth_table tt{num_inputs};
    do
    {
        const auto res = kitty::to_hex(std::get<0>(kitty::exact_npn_canonization(tt)));
        func_count[res]++;
        kitty::next_inplace(tt);
    } while (!kitty::is_const0(tt));
    
    std::map<std::string, std::future<syn_result>> syn_res;
    
    std::vector<std::string> all_classes;
    for (auto it = func_count.begin(); it != func_count.end(); it++)
        all_classes.push_back(it->first);
    
    std::ofstream tt_outf("tt_logfile.txt");
    std::ofstream outf(ofname);
    outf << "class,#functions,#gates,gate-inverter decomposition,synthesis time" << std::endl;
    outf.close();
    tt_outf.close();
    
    int count = 0, block = 48;
    for (int i = 0; i < all_classes.size(); i ++)
    {
        
        std::string cls = all_classes[i];
        
        std::cout << "Synthesizing " << ++count << " out of " << func_count.size() << std::endl;
        std::cout << "\tClass " << cls << " has " << func_count[cls] << " functions" << std::endl;
        
        
        kitty::dynamic_truth_table dtt(num_inputs);
        kitty::create_from_hex_string(dtt, cls);
        auto sr = synthesize_func_general_primitive(dtt, fn, xmg);
        
        
        nc[sr.n] += 1;
        nf[sr.n] += func_count[cls];
        std::cout << "\tClass " << cls << " needs " << sr.n << " gates" << std::endl;
        std::cout << "\tClass " << cls << " chain: " << sr.chain << std::endl;
        
        outf.open(ofname, std::ios_base::app);
        outf << "0x" << cls << "," << func_count[cls] << "," << sr.n << "," << sr.chain << "," << sr.syn_time << std::endl;
        outf.close();
        
    }
    
    std::cout << "Number of classes for each gate count: " << std::endl;
    for (auto it = nc.begin(); it != nc.end(); it++)
    {
        std::cout << it->first << "\t" << it->second << std::endl;
    }
    
    std::cout << "Number of functions for each gate count: " << std::endl;
    for (auto it = nf.begin(); it != nf.end(); it++)
    {
        std::cout << it->first << "\t" << it->second << std::endl;
    }

    tt_outf.open("tt_logfile.txt", std::ios_base::app);
    xmg.foreach_node( [&]( auto const& node )
            {
            xmg.foreach_fanin( node, [&](auto const &fi, auto index)
                    {
                    uint32_t encoded_number = uint32_t( xmg.get_node( fi ) );
                    encoded_number <<= 1;
                    encoded_number = encoded_number ^ xmg.is_complemented( fi );
                    //encoded_number = encoded_number  ^ (encoded_number | xmg.is_complemented(fi));
                    // std::cout << std::hex << xmg.get_node(fi) << " " << xmg.is_complemented(fi) << std::endl;
                    if ( index == 0u )
                    {
                      encoded_number <<= 1;
                      encoded_number |= xmg.is_xor3( node );
                    }
                    tt_outf << "0x" << std::hex << encoded_number << ",";
                    std::cout << std::hex << encoded_number << ",";
                    });

            } );
            std::cout << std::endl;
            tt_outf << std::endl;
    tt_outf.close();

#if 0 
#endif
}

