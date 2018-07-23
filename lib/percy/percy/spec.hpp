#pragma once

#include <chrono>
#include <vector>
#include "tt_utils.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-pedantic"
#include <kitty/kitty.hpp>
#include <kitty/print.hpp>
#pragma GCC diagnostic pop

namespace percy
{
    const int MAX_STEPS = 20; /// The maximum number of steps we'll synthesize
    const int MAX_FANIN =  5; /// The maximum number of fanins per step we'll synthesize

    /// The various methods types of synthesis supported by percy.
    enum SynthMethod
    {
        SYNTH_STD,
        SYNTH_STD_CEGAR,
        SYNTH_FENCE,
        SYNTH_FENCE_CEGAR,
        SYNTH_DAG,
        SYNTH_FDAG,
        SYNTH_TOTAL
    };

    const char * const SynthMethodToString[SYNTH_TOTAL] =
    {
        "SYNTH_STD",
        "SYNTH_STD_CEGAR",
        "SYNTH_FENCE",
        "SYNTH_FENCE_CEGAR",
        "SYNTH_DAG",
        "SYNTH_FDAG",
    };

    enum EncoderType
    {
        ENC_KNUTH,
        ENC_EPFL,
        ENC_BERKELEY,
        ENC_FENCE,
        ENC_DAG,
        ENC_TOTAL
    };

    const char* const EncoderTypeToString[ENC_TOTAL] = 
    {
        "ENC_KNUTH",
        "ENC_EPFL",
        "ENC_BERKELEY",
        "ENC_FENCE",
        "ENC_DAG",
    };

    enum SolverType
    {
        SLV_BSAT2,
        SLV_CMSAT,
        SLV_GLUCOSE,
        SLV_TOTAL,
    };

    const char * const SolverTypeToString[SLV_TOTAL] = 
    {
        "SLV_BSAT2",
        "SLV_CMSAT",
        "SLV_GLUCOSE",
    };

    enum Primitive
    {
        AND,
        OR,
        MAJ
    };

    /// Used to gather data on synthesis experiments.
    struct synth_stats
    {
        double overhead = 0;
        double total_synth_time = 0;
        double time_to_first_synth = 0;
        int nr_success = 0;
        int nr_timeouts = 0;
        int64_t sat_time = 0;   ///< How much time was spent on UNSAT formulae (in us)
        int64_t unsat_time = 0; ///< How much time was spent on SAT formulae (in us)
        int64_t synth_time = 0; ///< How much time was spent on SAT formulae (in us)
    }; 

    class spec
    {
        protected:
            int capacity; ///< Maximum number of output functions this specification can support
            int tt_size; ///< Size of the truth tables to synthesize (in nr. of bits)
            std::vector<kitty::dynamic_truth_table> functions; ///< Functions to synthesize
            std::vector<int> triv_functions; ///< Trivial outputs
            std::vector<int> synth_functions; ///< Nontrivial outputs
            std::vector<Primitive> primitives; ///< The primitives used in synthesis
            std::vector<kitty::dynamic_truth_table> compiled_primitives; ///< Collection of concrete truth tables induced by primitives

        public:
            int fanin = 2; ///< The fanin of the Boolean chain steps
            int nr_steps; ///< The number of Boolean operators to use
            int initial_steps = 1; ///< The number of steps from which to start synthesis
            int verbosity = 0; ///< Verbosity level for debugging purposes
            uint64_t out_inv; ///< Is 1 at index i if output i must be inverted
            uint64_t triv_flag; ///< Is 1 at index i if output i is constant zero or one or a projection.
            int nr_triv; ///< Number of trivial output functions
            int nr_nontriv; ///< Number of non-trivial output functions
            int nr_rand_tt_assigns; ///< Number of truth table bits to assign randomly in CEGAR loop

            bool add_nontriv_clauses = true; ///< Symmetry break: do not allow trivial operators
            bool add_alonce_clauses = true; ///< Symmetry break: all steps must be used at least once
            bool add_noreapply_clauses = true; ///< Symmetry break: no re-application of operators
            bool add_colex_clauses = true; ///< Symmetry break: order step fanins co-lexicographically
            bool add_lex_func_clauses = true; ///< Symmetry break: order step operators co-lexicographically
            bool add_symvar_clauses = true; ///< Symmetry break: impose order on symmetric variables
            bool add_lex_clauses = false; ///< Symmetry break: order step fanins lexicographically

            /// Limit on the number of SAT conflicts. Zero means no limit.
            int conflict_limit = 0;
            
            /// Constructs a spec with one output
            spec()
            {
                set_nr_out(1);
            }

            /// Constructs a spec with nr_out outputs.
            spec(int nr_out)
            {
                set_nr_out(nr_out);
            }

            void
            set_nr_out(int n)
            {
                capacity = n;
                functions.resize(n);
                triv_functions.resize(n);
                synth_functions.resize(n);
            }

            int get_nr_in() const { return functions[0].num_vars(); }
            int get_tt_size() const { return tt_size; }
            int get_nr_out() const { return capacity; }

            /// Normalizes outputs by converting them to normal functions. Also
            /// checks for trivial outputs, such as constant functions or
            /// projections. This determines which of the specifeid functions
            /// need to be synthesized.  This function expects the following
            /// invariants to hold:
            /// 1. The number of input variables has been set.
            /// 2. The number of output variables has been set.
            /// 3. The functions requested to be synthesized have been set.
            void 
            preprocess(void)
            {
                assert(!add_colex_clauses || !add_lex_clauses);

                // Verify that all functions have the same number of variables
                const auto num_vars = functions[0].num_vars();
                for (int i = 1; i < capacity; i++) {
                    if (functions[i].num_vars() != num_vars) {
                        assert(false);
                        exit(1);
                    }
                }

                tt_size = (1 << functions[0].num_vars()) - 1;

                if (verbosity) {
                    printf("\n");
                    printf("========================================"
                            "========================================\n");
                    printf("  Pre-processing for %s:\n", capacity > 1 ? 
                            "functions" : "function");
                    for (int h = 0; h < capacity; h++) {
                        printf("  ");
                        kitty::print_binary(functions[h], std::cout);
                        printf("\n");
                    }
                    printf("========================================"
                            "========================================\n");
                    printf("  SPEC:\n");
                    printf("\tnr_in=%d\n", functions[0].num_vars());
                    printf("\tnr_out=%d\n", capacity);
                    printf("\ttt_size=%d\n", tt_size);
                }

                assert((!add_colex_clauses && !add_lex_clauses) ||
                        (add_colex_clauses != add_lex_clauses));

                // Detect any trivial outputs.
                nr_triv = 0;
                nr_nontriv = 0;
                out_inv = 0;
                triv_flag = 0;
                for (int h = 0; h < capacity; h++) {
                    if (is_const0(functions[h])) {
                        triv_flag |= (1 << h);
                        triv_functions[nr_triv++] = 0;
                    } else if (is_const0(~(functions[h]))) {
                        triv_flag |= (1 << h);
                        triv_functions[nr_triv++] = 0;
                        out_inv |= (1 << h);
                    } else {
                        auto tt_var = functions[0].construct();
                        for (int i = 0; i < get_nr_in(); i++) {
                            create_nth_var(tt_var, i);
                            if (functions[h] == tt_var) {
                                triv_flag |= (1 << h);
                                triv_functions[nr_triv++] = i+1;
                                break;
                            } else if (functions[h] == ~(tt_var)) {
                                triv_flag |= (1 << h);
                                triv_functions[nr_triv++] = i+1;
                                out_inv |= (1 << h);
                                break;
                            }
                        }
                        // Even when the output is not trivial, we still need
                        // to ensure that it's normal.
                        if (!((triv_flag >> h) & 1)) {
                            if (!is_normal(functions[h])) {
                                out_inv |= (1 << h);
                            }
                            synth_functions[nr_nontriv++] = h;
                        }
                    }
                }

                if (verbosity) {
                    for (int h = 0; h < capacity; h++) {
                        if ((triv_flag >> h) & 1) {
                            printf("  Output %d is trivial\n", h+1);
                        }
                        if ((out_inv >> h) & 1) {
                            printf("  Inverting output %d\n", h+1);
                        }
                    }
                    printf("  Trivial outputs=%d\n", nr_triv);
                    printf("  Non-trivial outputs=%d\n", capacity - nr_triv);
                    printf("========================================"
                            "========================================\n");
                    printf("\n");
                }
            }

            kitty::dynamic_truth_table& 
            operator[](std::size_t idx)
            {
                if (static_cast<int>(idx) >= capacity) {
                    set_nr_out(idx + 1);
                }
                return functions[idx];
            }

            const kitty::dynamic_truth_table& 
            operator[](std::size_t idx) const
            {
                assert (static_cast<int>(idx) < capacity);
                return functions[idx];
            }

            template<class TT>
            void set_output(int i, const TT& tt)
            {
                assert(i < capacity);
                functions[i] = tt;
            }


            int
            triv_func(int i) const
            {
                assert(i < capacity);
                return triv_functions[i];
            }

            int
            synth_func(int i) const
            {
                assert(i < capacity);
                return synth_functions[i];
            }

            void
            add_primitive(Primitive p)
            {
                primitives.push_back(p);
            }

            void 
            set_primitives(std::vector<Primitive>& ps) 
            {
                primitives = ps;
            }

            int
            get_nr_primitives() const
            {
                return primitives.size();
            }

            const std::vector<kitty::dynamic_truth_table>&
            get_compiled_primitives() const
            {
                return compiled_primitives;
            }

            void
            clear_primitives()
            {
                primitives.clear();
            }

            void
            compile_primitives()
            {
                compiled_primitives.clear();
                kitty::dynamic_truth_table tt(fanin);
                std::vector<kitty::dynamic_truth_table> inputs;
                for (int i = 0; i < fanin; i++) {
                    inputs.push_back(kitty::create<kitty::dynamic_truth_table>(fanin));
                    kitty::create_nth_var(inputs[i], i);
                }
                for (auto primitive : primitives) {
                    kitty::clear(tt);
                    switch (primitive) {
                    case AND:
                        tt = inputs[0];
                        for (int i = 1; i < fanin; i++) {
                            tt ^= inputs[i];
                        }
                        compiled_primitives.push_back(tt);
                        break;
                    case OR:
                        tt = inputs[0];
                        for (int i = 1; i < fanin; i++) {
                            tt |= inputs[i];
                        }
                        compiled_primitives.push_back(tt);
                        break;
                    case MAJ:
                        kitty::create_majority(tt);
                        compiled_primitives.push_back(tt);
                        break;
                    }
                }
            }

    };

}

