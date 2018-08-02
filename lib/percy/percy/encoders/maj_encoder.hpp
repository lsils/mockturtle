#pragma once

#include <vector>
#include <kitty/kitty.hpp>
#include "encoder.hpp"

namespace percy
{
    class maj_encoder
    {
    private:
        int level_dist[32]; // How many steps are below a certain level
        int nr_levels; // The number of levels in the Boolean fence
        int nr_sel_vars;
        int nr_res_vars;
        int nr_sim_vars;
        int total_nr_vars;
        int sel_offset;
        int res_offset;
        int sim_offset;
        bool dirty = false;
        pabc::lit pLits[2048];
        solver_wrapper* solver;

        const int NR_SIM_TTS = 32;
        std::vector<kitty::dynamic_truth_table> sim_tts { NR_SIM_TTS };

        int get_sim_var(const spec& spec, int step_idx, int t) const
        {
            return sim_offset + spec.tt_size * step_idx + t;
        }

        bool fix_output_sim_vars(const spec& spec, int t)
        {
            const auto ilast_step = spec.nr_steps - 1;
            auto outbit = kitty::get_bit(
                spec[spec.synth_func(0)], t + 1);
            if ((spec.out_inv >> spec.synth_func(0)) & 1) {
                outbit = 1 - outbit;
            }
            const auto sim_var = get_sim_var(spec, ilast_step, t);
            pabc::lit sim_lit = pabc::Abc_Var2Lit(sim_var, 1 - outbit);
            return solver->add_clause(&sim_lit, &sim_lit + 1);
        }

        int get_sel_var(
            const spec& spec, 
            const int i, 
            const int j, 
            const int k,
            const int l) const
        {
            assert(i < spec.nr_steps);

            int offset = 0;
            for (int ip = 0; ip < i; ip++) {
                const auto n = spec.nr_in + ip;
                offset += (n * (n - 1) * (n - 2)) / 6;
            }

            int svar_ctr = 0;
            for (int lp = 2; lp < spec.nr_in + i; lp++) {
                for (int kp = 1; kp < lp; kp++) {
                    for (int jp = 0; jp < kp; jp++) {
                        if (l == lp && k == kp && j == jp) {
                            return sel_offset + offset + svar_ctr;
                        }
                        svar_ctr++;
                    }
                }
            }

            assert(false);
            return -1;
        }

        int get_sel_var(const spec& spec, int idx, int var_idx) const
        {
            assert(idx < spec.nr_steps);
            const auto nr_svars_for_idx = nr_svars_for_step(spec, idx);
            assert(var_idx < nr_svars_for_idx);
            auto offset = 0;
            for (int i = 0; i < idx; i++) {
                offset += nr_svars_for_step(spec, i);
            }
            return sel_offset + offset + var_idx;
        }

        int get_res_var(const spec& spec, int step_idx, int res_var_idx) const
        {
            auto offset = 0;
            for (int i = 0; i < step_idx; i++) {
                offset += (nr_svars_for_step(spec, i) + 1) * (1 + 2);
            }

            return res_offset + offset + res_var_idx;
        }

    public:
        maj_encoder(solver_wrapper& solver)
        {
            this->solver = &solver;
        }

        ~maj_encoder()
        {
        }

        void create_variables(const spec& spec)
        {
            nr_sim_vars = spec.nr_steps * spec.tt_size;

            nr_sel_vars = 0;
            for (int i = 0; i < spec.nr_steps; i++) {
                const auto n = spec.nr_in + i;
                nr_sel_vars += (n * (n - 1) * (n - 2)) / 6;
            }

            sel_offset = 0;
            sim_offset = nr_sel_vars;
            total_nr_vars = nr_sel_vars + nr_sim_vars;

            if (spec.verbosity) {
                printf("Creating variables (MIG)\n");
                printf("nr steps = %d\n", spec.nr_steps);
                printf("nr_sel_vars=%d\n", nr_sel_vars);
                printf("nr_sim_vars = %d\n", nr_sim_vars);
                printf("creating %d total variables\n", total_nr_vars);
            }

            solver->set_nr_vars(total_nr_vars);
        }

        int first_step_on_level(int level) const
        {
            if (level == 0) { return 0; }
            return level_dist[level-1];
        }

        int nr_svars_for_step(const spec& spec, int i) const
        {
            // Determine the level of this step.
            const auto level = get_level(spec, i + spec.nr_in);
            auto nr_svars_for_i = 0;
            assert(level > 0);
            for (auto l = first_step_on_level(level - 1);
                l < first_step_on_level(level); l++) {
                // We select l as fanin 3, so have (l choose 2) options 
                // (j,k in {0,...,(l-1)}) left for fanin 1 and 2.
                nr_svars_for_i += (l * (l - 1)) / 2;
            }

            return nr_svars_for_i;
        }

        void fence_create_variables(const spec& spec)
        {
            nr_sim_vars = spec.nr_steps * spec.tt_size;

            nr_sel_vars = 0;
            for (int i = 0; i < spec.nr_steps; i++) {
                nr_sel_vars += nr_svars_for_step(spec, i);
            }

            sel_offset = 0;
            sim_offset = nr_sel_vars;
            total_nr_vars = nr_sel_vars + nr_sim_vars;

            if (spec.verbosity) {
                printf("Creating variables (MIG)\n");
                printf("nr steps = %d\n", spec.nr_steps);
                printf("nr_sel_vars=%d\n", nr_sel_vars);
                printf("nr_sim_vars = %d\n", nr_sim_vars);
                printf("creating %d total variables\n", total_nr_vars);
            }

            solver->set_nr_vars(total_nr_vars);
        }

        void cegar_fence_create_variables(const spec& spec)
        {
            nr_sim_vars = spec.nr_steps * spec.tt_size;

            nr_sel_vars = 0;
            nr_res_vars = 0;
            for (int i = 0; i < spec.nr_steps; i++) {
                const auto nr_svars_for_i = nr_svars_for_step(spec, i);
                nr_sel_vars += nr_svars_for_i;
                nr_res_vars += (nr_svars_for_i + 1) * (1 + 2);
            }

            sel_offset = 0;
            res_offset = nr_sel_vars;
            sim_offset = nr_sel_vars + nr_res_vars;
            total_nr_vars = nr_sel_vars + nr_res_vars + nr_sim_vars;

            if (spec.verbosity) {
                printf("Creating variables (MIG)\n");
                printf("nr steps = %d\n", spec.nr_steps);
                printf("nr_sel_vars=%d\n", nr_sel_vars);
                printf("nr_sel_vars=%d\n", nr_res_vars);
                printf("nr_sim_vars = %d\n", nr_sim_vars);
                printf("creating %d total variables\n", total_nr_vars);
            }

            solver->set_nr_vars(total_nr_vars);
        }

        /// Ensures that each gate has the proper number of fanins.
        bool create_fanin_clauses(const spec& spec)
        {
            auto status = true;

            if (spec.verbosity > 2) {
                printf("Creating fanin clauses (MIG)\n");
                printf("Nr. clauses = %d (PRE)\n", solver->nr_clauses());
            }

            for (int i = 0; i < spec.nr_steps; i++) {
                auto ctr = 0;
                for (int l = 2; l < spec.nr_in + i; l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto svar = get_sel_var(spec, i, j, k, l);
                            pLits[ctr++] = pabc::Abc_Var2Lit(svar, 0);
                        }
                    }
                }
                status &= solver->add_clause(pLits, pLits + ctr);
            }

            if (spec.verbosity > 2) {
                printf("Nr. clauses = %d (POST)\n", solver->nr_clauses());
            }

            return status;
        }

        ///< We synthesize MIGs with 4 possible operators since all
        ///< the others are symmetries of these. The operators are:
        ///< (1) <abc>
        ///< (2) <!abc>
        ///< (3) <a!bc>
        ///< (4) <ab!c>
        /*
        bool create_op_clauses(const spec& spec)
        {
            auto status = true;

            kitty::static_truth_table<3> in1, in2, in3, arb_tt,
                maj1, maj2, maj3, maj4;
            kitty::create_nth_var(in1, 0);
            kitty::create_nth_var(in2, 1);
            kitty::create_nth_var(in3, 2);

            maj1 = (in1 & in2) | (in1 & in3) | (in2 & in3);
            maj2 = (~in1 & in2) | (~in1 & in3) | (in2 & in3);
            maj3 = (in1 & ~in2) | (in1 & in3) | (~in2 & in3);
            maj4 = (in1 & in2) | (in1 & ~in3) | (in2 & ~in3);

            for (int i = 0; i < spec.nr_steps; i++) {
                kitty::clear(arb_tt);
                int nr_block_clauses = 0;
                do {
                    if (arb_tt == maj1 || arb_tt == maj2 || 
                        arb_tt == maj3 || arb_tt == maj4) {
                        kitty::next_inplace(arb_tt);
                        continue;
                    }
                    if (!is_normal(arb_tt)) {
                        kitty::next_inplace(arb_tt);
                        continue;
                    }

                    pLits[0] = pabc::Abc_Var2Lit(get_op_var(spec, i, 0), 
                        kitty::get_bit(arb_tt, 1));
                    pLits[1] = pabc::Abc_Var2Lit(get_op_var(spec, i, 1), 
                        kitty::get_bit(arb_tt, 2));
                    pLits[2] = pabc::Abc_Var2Lit(get_op_var(spec, i, 2), 
                        kitty::get_bit(arb_tt, 3));
                    pLits[3] = pabc::Abc_Var2Lit(get_op_var(spec, i, 3), 
                        kitty::get_bit(arb_tt, 4));
                    pLits[4] = pabc::Abc_Var2Lit(get_op_var(spec, i, 4), 
                        kitty::get_bit(arb_tt, 5));
                    pLits[5] = pabc::Abc_Var2Lit(get_op_var(spec, i, 5), 
                        kitty::get_bit(arb_tt, 6));
                    pLits[6] = pabc::Abc_Var2Lit(get_op_var(spec, i, 6), 
                        kitty::get_bit(arb_tt, 7));
                    status &= solver->add_clause(pLits, pLits + 7);
                    assert(status);

                    if (nr_block_clauses++ > 256) {
                        break;
                    }
                    kitty::next_inplace(arb_tt);
                } while (!kitty::is_const0(arb_tt));
            }

            return status;
        }
        */

        int maj3(int a, int b, int c) const
        {
            return (a & b) | (a & c) | (b & c);
        }

        bool add_simulation_clause(
            const spec& spec,
            const int t,
            const int i,
            const int j,
            const int k,
            const int l,
            const int c,
            const int b,
            const int a,
            const int sel_var
            )
        {
            int ctr = 0;
            //assert(j > 0);

            if (j < spec.nr_in) {
                if ((((t + 1) & (1 << j)) ? 1 : 0) != a) {
                    return true;
                }
            } else {
                pLits[ctr++] = pabc::Abc_Var2Lit(
                    get_sim_var(spec, j - spec.nr_in, t), a);
            }

            if (k < spec.nr_in) {
                if ((((t + 1) & (1 << k)) ? 1 : 0) != b) {
                    return true;
                }
            } else {
                pLits[ctr++] = pabc::Abc_Var2Lit(
                    get_sim_var(spec, k - spec.nr_in, t), b);
            }

            if (l < spec.nr_in) {
                if ((((t + 1) & (1 << l)) ? 1 : 0) != c) {
                    return true;
                }
            } else {
                pLits[ctr++] = pabc::Abc_Var2Lit(
                    get_sim_var(spec, l - spec.nr_in, t), c);
            }

            pLits[ctr++] = pabc::Abc_Var2Lit(sel_var, 1);

            if (maj3(a, b, c)) {
                pLits[ctr++] = pabc::Abc_Var2Lit(get_sim_var(spec, i, t), 0);
            } else {
                pLits[ctr++] = pabc::Abc_Var2Lit(get_sim_var(spec, i, t), 1);
            }

            const auto ret = solver->add_clause(pLits, pLits + ctr);
            assert(ret);
            return ret;
        }

        bool create_tt_clauses(const spec& spec, const int t)
        {
            bool ret = true;
            for (int i = 0; i < spec.nr_steps; i++) {
                for (int l = 2; l < spec.nr_in + i; l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, j, k, l);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 0, 0, 0, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 0, 0, 1, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 0, 1, 0, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 0, 1, 1, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 1, 0, 0, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 1, 0, 1, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 1, 1, 0, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 1, 1, 1, sel_var);
                        }
                    }
                }
                assert(ret);
            }

            ret &= fix_output_sim_vars(spec, t);

            return ret;
        }

        bool fence_create_tt_clauses(const spec& spec, const int t)
        {
            bool ret = true;
            for (int i = 0; i < spec.nr_steps; i++) {
                const auto level = get_level(spec, i + spec.nr_in);
                int ctr = 0;
                for (int l = first_step_on_level(level - 1);
                    l < first_step_on_level(level); l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, ctr++);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 0, 0, 0, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 0, 0, 1, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 0, 1, 0, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 0, 1, 1, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 1, 0, 0, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 1, 0, 1, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 1, 1, 0, sel_var);
                            ret &= add_simulation_clause(spec, t, i, j, k, l, 1, 1, 1, sel_var);
                        }
                    }
                }
                assert(ret);
            }

            ret &= fix_output_sim_vars(spec, t);

            return ret;
        }

        void create_main_clauses(const spec& spec)
        {
            for (int t = 0; t < spec.tt_size; t++) {
                (void)create_tt_clauses(spec, t);
            }
        }

        bool fence_create_main_clauses(const spec& spec)
        {
            bool ret = true;
            for (int t = 0; t < spec.tt_size; t++) {
                ret &= fence_create_tt_clauses(spec, t);
            }
            return ret;
        }

        void create_alonce_clauses(const spec& spec)
        {
            for (int i = 0; i < spec.nr_steps - 1; i++) {
                int ctr = 0;
                const auto idx = spec.nr_in + i;
                for (int ip = i + 1; ip < spec.nr_steps; ip++) {
                    for (int l = spec.nr_in + i; l < spec.nr_in + ip; l++) {
                        for (int k = 1; k < l; k++) {
                            for (int j = 0; j < k; j++) {
                                if (j == idx || k == idx || l == idx) {
                                    const auto sel_var = get_sel_var(spec, ip, j, k, l);
                                    pLits[ctr++] = pabc::Abc_Var2Lit(sel_var, 0);
                                }
                            }
                        }
                    }
                }
                const auto res = solver->add_clause(pLits, pLits + ctr);
                assert(res);
            }
        }

        void fence_create_alonce_clauses(const spec& spec)
        {
            for (int i = 0; i < spec.nr_steps - 1; i++) {
                auto ctr = 0;
                const auto idx = spec.nr_in + i;
                const auto level = get_level(spec, idx);
                for (int ip = i + 1; ip < spec.nr_steps; ip++) {
                    auto levelp = get_level(spec, ip + spec.nr_in);
                    assert(levelp >= level);
                    if (levelp == level) {
                        continue;
                    }
                    auto svctr = 0;
                    for (int l = first_step_on_level(levelp - 1);
                        l < first_step_on_level(levelp); l++) {
                        for (int k = 1; k < l; k++) {
                            for (int j = 0; j < k; j++) {
                                if (j == idx || k == idx || l == idx) {
                                    const auto sel_var = get_sel_var(spec, ip, svctr);
                                    pLits[ctr++] = pabc::Abc_Var2Lit(sel_var, 0);
                                }
                                svctr++;
                            }
                        }
                    }
                    assert(svctr == nr_svars_for_step(spec, ip));
                }
                solver->add_clause(pLits, pLits + ctr);
            }
        }

        void create_noreapply_clauses(const spec& spec)
        {
            for (int i = 0; i < spec.nr_steps - 1; i++) {
                for (int l = 2; l < spec.nr_in + i; l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, j, k, l);
                            pLits[0] = pabc::Abc_Var2Lit(sel_var, 1);
                            for (int ip = i + 1; ip < spec.nr_steps; ip++) {
                                for (int kp = 1; kp < spec.nr_steps + i; kp++) {
                                    for (int jp = 0; jp < kp; jp++) {
                                        if ((kp == l && jp == k) || 
                                            (kp == k && jp == j) ||
                                            (kp == l && jp == k)) {
                                            const auto sel_varp = get_sel_var(spec, ip, jp, kp, spec.nr_in + i);
                                            pLits[1] = pabc::Abc_Var2Lit(sel_varp, 1);
                                            auto status = solver->add_clause(pLits, pLits + 2);
                                            assert(status);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        void fence_create_noreapply_clauses(const spec& spec)
        {
            for (int i = 0; i < spec.nr_steps - 1; i++) {
                const auto level = get_level(spec, spec.nr_in + i);
                int svar_ctr = 0;
                for (int l = first_step_on_level(level - 1); 
                    l < first_step_on_level(level); l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, svar_ctr);
                            pLits[0] = pabc::Abc_Var2Lit(sel_var, 1);
                            for (int ip = i + 1; ip < spec.nr_steps; ip++) {
                                const auto levelp = get_level(spec, spec.nr_in + ip);
                                if (level == levelp) {
                                    continue;
                                }
                                int svar_ctrp = 0;
                                for (int lp = first_step_on_level(levelp - 1);
                                    lp < first_step_on_level(levelp); lp++) {
                                    for (int kp = 1; kp < l; kp++) {
                                        for (int jp = 0; jp < kp; jp++) {
                                            if ((lp == spec.nr_in + i) && 
                                                ((kp == l && jp == k) ||
                                                (kp == k && jp == j) ||
                                                (kp == l && jp == k))) {
                                                const auto sel_varp = get_sel_var(spec, ip, svar_ctrp);
                                                pLits[1] = pabc::Abc_Var2Lit(sel_varp, 1);
                                                auto status = solver->add_clause(pLits, pLits + 2);
                                                assert(status);
                                            }
                                            svar_ctrp++;
                                        }
                                    }
                                }
                            }
                            svar_ctr++;
                        }
                    }
                }
            }
        }

        /// Ensure that all steps are in strict co-lexicographic
        /// order. Unlike the general Boolean chain case, here we 
        /// don't want two steps pointing to the same inputs since
        /// they cannot implement different functions.
        void create_colex_clauses(const spec& spec)
        {
            for (int i = 0; i < spec.nr_steps - 1; i++) {
                for (int l = 2; l < spec.nr_in + i; l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            pLits[0] = pabc::Abc_Var2Lit(get_sel_var(spec, i, j, k, l), 1);

                            // Cannot have lp <= l
                            for (int lp = 2; lp <= l; lp++) {
                                for (int kp = 1; kp < lp; kp++) {
                                    for (int jp = 0; jp < kp; jp++) {
                                        pLits[1] = pabc::Abc_Var2Lit(get_sel_var(spec, i + 1, jp, kp, lp), 1);
                                        const auto res = solver->add_clause(pLits, pLits + 2);
                                        assert(res);
                                    }
                                }
                            }

                            // May have lp == l and kp > k
                            for (int kp = 1; kp <= k; kp++) {
                                for (int jp = 0; jp < kp; jp++) {
                                    pLits[1] = pabc::Abc_Var2Lit(get_sel_var(spec, i + 1, jp, kp, l), 1);
                                    const auto res = solver->add_clause(pLits, pLits + 2);
                                    assert(res);
                                }
                            }
                            // OR lp == l and kp == k
                            for (int jp = 0; jp <= j; jp++) {
                                pLits[1] = pabc::Abc_Var2Lit(get_sel_var(spec, i + 1, jp, k, l), 1);
                                const auto res = solver->add_clause(pLits, pLits + 2);
                                assert(res);
                            }
                        }
                    }
                }
            }
        }

        void fence_create_colex_clauses(const spec& spec)
        {
            for (int i = 0; i < spec.nr_steps - 1; i++) {
                const auto level = get_level(spec, i + spec.nr_in);
                const auto levelp = get_level(spec, i + 1 + spec.nr_in);
                int svar_ctr = 0;
                for (int l = first_step_on_level(level-1); 
                        l < first_step_on_level(level); l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            if (l < 3) {
                                svar_ctr++;
                                continue;
                            }
                            const auto sel_var = get_sel_var(spec, i, svar_ctr);
                            pLits[0] = pabc::Abc_Var2Lit(sel_var, 1);
                            int svar_ctrp = 0;
                            for (int lp = first_step_on_level(levelp - 1);
                                lp < first_step_on_level(levelp); lp++) {
                                for (int kp = 1; kp < lp; kp++) {
                                    for (int jp = 0; jp < kp; jp++) {
                                        if ((lp == l && kp == k && jp <= j) || 
                                            (lp == l && kp <= k) || (lp <= l)) {
                                            const auto sel_varp = get_sel_var(spec, i + 1, svar_ctrp);
                                            pLits[1] = pabc::Abc_Var2Lit(sel_varp, 1);
                                            (void)solver->add_clause(pLits, pLits + 2);
                                        }
                                        svar_ctrp++;
                                    }
                                }
                            }
                            svar_ctr++;
                        }
                    }
                }
            }
        }

        bool create_symvar_clauses(const spec& spec)
        {
            for (int q = 1; q < spec.nr_in; q++) {
                for (int p = 0; p < q; p++) {
                    auto symm = true;
                    for (int i = 0; i < spec.nr_nontriv; i++) {
                        auto f = spec[spec.synth_func(i)];
                        if (!(swap(f, p, q) == f)) {
                            symm = false;
                            break;
                        }
                    }
                    if (!symm) {
                        continue;
                    }

                    for (int i = 1; i < spec.nr_steps; i++) {
                        for (int l = 2; l < spec.nr_in + i; l++) {
                            for (int k = 1; k < l; k++) {
                                for (int j = 0; j < k; j++) {
                                    if (!(j == q || k == q || l == q) || (j == p || k == p)) {
                                        continue;
                                    }
                                    pLits[0] = pabc::Abc_Var2Lit(get_sel_var(spec, i, j, k, l), 1);
                                    auto ctr = 1;
                                    for (int ip = 0; ip < i; ip++) {
                                        for (int lp = 2; lp < spec.nr_in + ip; lp++) {
                                            for (int kp = 1; kp < lp; kp++) {
                                                for (int jp = 0; jp < kp; jp++) {
                                                    if (jp == p || kp == p || lp == p) {
                                                        pLits[ctr++] = pabc::Abc_Var2Lit(get_sel_var(spec, ip, jp, kp, lp), 0);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (!solver->add_clause(pLits, pLits + ctr)) {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return true;
        }

        void fence_create_symvar_clauses(const spec& spec)
        {
            for (int q = 1; q < spec.nr_in; q++) {
                for (int p = 0; p < q; p++) {
                    auto symm = true;
                    for (int i = 0; i < spec.nr_nontriv; i++) {
                        auto& f = spec[spec.synth_func(i)];
                        if (!(swap(f, p, q) == f)) {
                            symm = false;
                            break;
                        }
                    }
                    if (!symm) {
                        continue;
                    }
                    for (int i = 1; i < spec.nr_steps; i++) {
                        const auto level = get_level(spec, i + spec.nr_in);
                        int svar_ctr = 0;
                        for (int l = first_step_on_level(level - 1);
                            l < first_step_on_level(level); l++) {
                            for (int k = 1; k < l; k++) {
                                for (int j = 0; j < k; j++) {
                                    if (!(j == q || k == q || l == q) || (j == p || k == p)) {
                                        svar_ctr++;
                                        continue;
                                    }
                                    const auto sel_var = get_sel_var(spec, i, svar_ctr);
                                    pLits[0] = pabc::Abc_Var2Lit(sel_var, 1);
                                    auto ctr = 1;
                                    for (int ip = 0; ip < i; ip++) {
                                        const auto levelp = get_level(spec, spec.nr_in + ip);
                                        auto svar_ctrp = 0;
                                        for (int lp = first_step_on_level(levelp - 1);
                                            lp < first_step_on_level(levelp); lp++) {
                                            for (int kp = 1; kp < lp; kp++) {
                                                for (int jp = 0; jp < kp; jp++) {
                                                    if (jp == p || kp == p || lp == p) {
                                                        const auto sel_varp = get_sel_var(spec, ip, svar_ctrp);
                                                        pLits[ctr++] = pabc::Abc_Var2Lit(sel_varp, 0);
                                                    }
                                                    svar_ctrp++;
                                                }
                                            }
                                        }
                                    }
                                    (void)solver->add_clause(pLits, pLits + ctr);
                                    svar_ctr++;
                                }
                            }
                        }
                    }
                }
            }
        }

        void create_cardinality_constraints(const spec& spec)
        {
            std::vector<int> svars;
            std::vector<int> rvars;

            for (int i = 0; i < spec.nr_steps; i++) {
                svars.clear();
                rvars.clear();
                const auto level = get_level(spec, spec.nr_in + i);
                auto svar_ctr = 0;
                for (int l = first_step_on_level(level - 1);
                    l < first_step_on_level(level); l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, svar_ctr++);
                            svars.push_back(sel_var);
                        }
                    }
                }
                assert(svars.size() == nr_svars_for_step(spec, i));
                const auto nr_res_vars = (1 + 2) * (svars.size() + 1);
                for (int j = 0; j < nr_res_vars; j++) {
                    rvars.push_back(get_res_var(spec, i, j));
                }
                create_cardinality_circuit(solver, svars, rvars, 1);

                // Ensure that the fanin cardinality for each step i 
                // is exactly FI.
                const auto fi_var =
                    get_res_var(spec, i, svars.size() * (1 + 2) + 1);
                auto fi_lit = pabc::Abc_Var2Lit(fi_var, 0);
                (void)solver->add_clause(&fi_lit, &fi_lit + 1);
            }
        }

        void reset_sim_tts(int nr_in)
        {
            for (int i = 0; i < NR_SIM_TTS; i++) {
                sim_tts[i] = kitty::dynamic_truth_table(nr_in);
                if (i < nr_in) {
                    kitty::create_nth_var(sim_tts[i], i);
                }
            }
        }

        bool encode(const spec& spec)
        {
            assert(spec.nr_in >= 3);

            create_variables(spec);
            create_main_clauses(spec);

            if (!create_fanin_clauses(spec)) {
                return false;
            }

            if (spec.add_alonce_clauses) {
                create_alonce_clauses(spec);
            }

            if (spec.add_colex_clauses) {
                create_colex_clauses(spec);
            }
            
            if (spec.add_noreapply_clauses) {
                create_noreapply_clauses(spec);
            }
            
            if (spec.add_symvar_clauses && !create_symvar_clauses(spec)) {
                return false;
            }

            return true;
        }

        void update_level_map(const spec& spec, const fence& f)
        {
            nr_levels = f.nr_levels();
            level_dist[0] = spec.nr_in;
            for (int i = 1; i <= nr_levels; i++) {
                level_dist[i] = level_dist[i-1] + f.at(i-1);
            }
        }

        int get_level(const spec& spec, int step_idx) const
        {
            // PIs are considered to be on level zero.
            if (step_idx < spec.nr_in) {
                return 0;
            } else if (step_idx == spec.nr_in) { 
                // First step is always on level one
                return 1;
            }
            for (int i = 0; i <= nr_levels; i++) {
                if (level_dist[i] > step_idx) {
                    return i;
                }
            }
            return -1;
        }

        void fence_create_fanin_clauses(const spec& spec)
        {
            for (int i = 0; i < spec.nr_steps; i++) {
                const auto nr_svars_for_i = nr_svars_for_step(spec, i);
                for (int j = 0; j < nr_svars_for_i; j++) {
                    const auto sel_var = get_sel_var(spec, i, j);
                    pLits[j] = pabc::Abc_Var2Lit(sel_var, 0);
                }

                const auto res = solver->add_clause(pLits, pLits + nr_svars_for_i);
                assert(res);
            }
        }

        bool
        encode(const spec& spec, const fence& f)
        {
            assert(spec.nr_in >= 3);
            assert(spec.nr_steps == f.nr_nodes());

            update_level_map(spec, f);
            fence_create_variables(spec);
            if (!fence_create_main_clauses(spec)) {
                return false;
            }

            fence_create_fanin_clauses(spec);
            
            if (spec.add_alonce_clauses) {
                fence_create_alonce_clauses(spec);
            }
            
            if (spec.add_colex_clauses) {
                fence_create_colex_clauses(spec);
            }

            if (spec.add_noreapply_clauses) {
                fence_create_noreapply_clauses(spec);
            }

            if (spec.add_symvar_clauses) {
                fence_create_symvar_clauses(spec);
            }

            return true;
        }

        bool
        cegar_encode(const spec& spec, const fence& f)
        {
            update_level_map(spec, f);
            cegar_fence_create_variables(spec);
            for (int i = 0; i < spec.nr_rand_tt_assigns; i++) {
                const auto t = rand() % spec.get_tt_size();
                (void)fence_create_tt_clauses(spec, t);
            }

            fence_create_fanin_clauses(spec);
            create_cardinality_constraints(spec);
            
            if (spec.add_alonce_clauses) {
                fence_create_alonce_clauses(spec);
            }
            
            if (spec.add_colex_clauses) {
                fence_create_colex_clauses(spec);
            }

            if (spec.add_noreapply_clauses) {
                fence_create_noreapply_clauses(spec);
            }

            if (spec.add_symvar_clauses) {
                fence_create_symvar_clauses(spec);
            }

            return true;

            assert(false);
            return false;
        }

        void extract_mig(const spec& spec, mig& chain)
        {
            int op_inputs[3] = { 0, 0, 0 };

            chain.reset(spec.nr_in, 1, spec.nr_steps);

            for (int i = 0; i < spec.nr_steps; i++) {
                const int op = 0;

                for (int l = 2; l < spec.nr_in + i; l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, j, k, l);
                            if (solver->var_value(sel_var)) {
                                op_inputs[0] = j;
                                op_inputs[1] = k;
                                op_inputs[2] = l;
                                break;
                            }
                        }
                    }
                }
                chain.set_step(i, op_inputs[0], op_inputs[1], op_inputs[2], op);
            }

            // TODO: support multiple outputs
            chain.set_output(0,
                ((spec.nr_steps + spec.nr_in) << 1) +
                ((spec.out_inv) & 1));
        }

        void fence_extract_mig(const spec& spec, mig& chain)
        {
            int op_inputs[3] = { 0, 0, 0 };

            chain.reset(spec.nr_in, 1, spec.nr_steps);

            for (int i = 0; i < spec.nr_steps; i++) {
                const int op = 0;

                int ctr = 0;
                const auto level = get_level(spec, spec.nr_in + i);
                for (int l = first_step_on_level(level - 1); 
                    l < first_step_on_level(level); l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, ctr++);
                            if (solver->var_value(sel_var)) {
                                op_inputs[0] = j;
                                op_inputs[1] = k;
                                op_inputs[2] = l;
                                break;
                            }
                        }
                    }
                }
                chain.set_step(i, op_inputs[0], op_inputs[1], op_inputs[2], op);
            }

            // TODO: support multiple outputs
            chain.set_output(0,
                ((spec.nr_steps + spec.nr_in) << 1) +
                ((spec.out_inv) & 1));
        }

        void print_solver_state(spec& spec)
        {
            for (auto i = 0; i < spec.nr_steps; i++) {
                for (int l = 2; l < spec.nr_in + i; l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, j, k, l);
                            if (solver->var_value(sel_var)) {
                                printf("s[%d][%d][%d][%d]=1\n", i, j, k, l);
                            } else {
                                printf("s[%d][%d][%d][%d]=0\n", i, j, k, l);
                            }
                        }
                    }
                }
            }

            for (auto i = 0; i < spec.nr_steps; i++) {
                printf("tt_%d_0=0\n", i);
                for (int t = 0; t < spec.tt_size; t++) {
                    const auto sim_var = get_sim_var(spec, i, t);
                    if (solver->var_value(sim_var)) {
                        printf("tt_%d_%d=1\n", i, t + 1);
                    } else {
                        printf("tt_%d_%d=0\n", i, t + 1);
                    }
                }
            }
        }

        void fence_print_solver_state(spec& spec)
        {
            for (auto i = 0; i < spec.nr_steps; i++) {
                const auto level = get_level(spec, i);
                for (int l = first_step_on_level(level - 1);
                    l < first_step_on_level(level); l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, j, k, l);
                            if (solver->var_value(sel_var)) {
                                printf("s[%d][%d][%d][%d]=1\n", i, j, k, l);
                            } else {
                                printf("s[%d][%d][%d][%d]=0\n", i, j, k, l);
                            }
                        }
                    }
                }
            }

            for (auto i = 0; i < spec.nr_steps; i++) {
                printf("tt_%d_0=0\n", i);
                for (int t = 0; t < spec.tt_size; t++) {
                    const auto sim_var = get_sim_var(spec, i, t);
                    if (solver->var_value(sim_var)) {
                        printf("tt_%d_%d=1\n", i, t + 1);
                    } else {
                        printf("tt_%d_%d=0\n", i, t + 1);
                    }
                }
            }
        }
        
        bool cegar_encode(const spec& spec)
        {
            // TODO: implement
            assert(false);
            return false;
        }
        
        bool block_solution(const spec& spec)
        {
            int ctr = 0;
            for (int i = 0; i < spec.nr_steps; i++) {
                for (int l = 2; l < spec.nr_in + i; l++) {
                    for (int k = 1; k < l; k++) {
                        for (int j = 0; j < k; j++) {
                            const auto sel_var = get_sel_var(spec, i, j, k, l);
                            if (solver->var_value(sel_var)) {
                                pLits[ctr++] = pabc::Abc_Var2Lit(sel_var, 1);
                                break;
                            }
                        }
                    }
                }
            }
            assert(ctr == spec.nr_steps);

            return solver->add_clause(pLits, pLits + ctr);
        }
        
        bool block_struct_solution(const spec& spec)
        {
            return block_solution(spec);
        }

        bool is_dirty() 
        {
            return dirty;
        }

        void set_dirty(bool _dirty)
        {
            dirty = _dirty;
        }
        
    };
}
