#pragma once

#include <chrono>
#include <memory>
#include <thread>
#include <mutex>
#include <kitty/kitty.hpp>
#include "fence.hpp"
#include "chain.hpp"
#include "dag_generation.hpp"
#include "tt_utils.hpp"
#include "concurrentqueue.h"
#include "spec.hpp"
#include "floating_dag.hpp"
#include "solvers.hpp"
#include "encoders.hpp"
#include "cnf.hpp"

/*******************************************************************************
    This module defines the interface to synthesize Boolean chains from
    specifications. The inspiration for this module is taken from Mathias
    Soeken's earlier exact synthesis algorithm, which has been integrated in
    the ABC synthesis package. That implementation is itself based on earlier
    work by Éen[1] and Knuth[2].

    [1] Niklas Éen, "Practical SAT – a tutorial on applied satisfiability
    solving," 2007, slides of invited talk at FMCAD.
    [2] Donald Ervin Knuth, "The Art of Computer Programming, Volume 4,
    Fascicle 6: Satisfiability," 2015
*******************************************************************************/
namespace percy
{
	using std::chrono::high_resolution_clock;
	using std::chrono::duration;
	using std::chrono::time_point;

    /***************************************************************************
        We consider a truth table to be trivial if it is equal to (or the
        complement of) a primary input or constant zero.
    ***************************************************************************/
    template<typename TT>
    static inline bool is_trivial(const TT& tt)
    {
        TT tt_check;

        if (tt == tt_check || tt == ~tt_check) {
            return true;
        }

        for (int i = 0; i < tt.num_vars(); i++) {
            kitty::create_nth_var(tt_check, i);
            if (tt == tt_check || tt == ~tt_check) {
                return true;
            }
        }

        return false;
    }

    static inline bool is_trivial(const kitty::dynamic_truth_table& tt)
    {
        kitty::dynamic_truth_table tt_check(tt.num_vars());

        if (tt == tt_check || tt == ~tt_check) {
            return true;
        }

        for (int i = 0; i < tt.num_vars(); i++) {
            kitty::create_nth_var(tt_check, i);
            if (tt == tt_check || tt == ~tt_check) {
                return true;
            }
        }

        return false;
    }


    /***************************************************************************
        A parallel version which periodically checks if a solution has been
        found by another thread.
    template<typename T, typename TT>
    synth_result 
    pcegar_chain_exists(
            T* synth, spec& spec, chain<2>& chain,
            bool* found)
    {
        if (spec.verbosity) {
            printf("  Existence check with %d steps...\n", spec.nr_steps);
        }

        synth->restart_solver();
        synth->cegar_add_clauses(spec);
        for (int i = 0; i < spec.nr_rand_tt_assigns; i++) {
            if (!synth->create_tt_clauses(spec, rand() % spec.tt_size)) {
                return failure;
            }
        }

        while (true) {
            auto status = synth->solve(spec.conflict_limit);
            if (status == success) {
                if (spec.verbosity > 1) {
                    synth->print_solver_state(spec);
                }
                synth->chain_extract(spec, chain);
                auto sim_tts = chain.simulate(spec);
                auto xor_tt = (*sim_tts[0]) ^ (*spec.functions[0]);
                auto first_one = kitty::find_first_one_bit(xor_tt);
                if (first_one == -1) {
                    if (spec.verbosity) {
                        printf("  SUCCESS\n\n"); 
                    }
                    return success;
                }
                // Add additional constraint.
                if (spec.verbosity) {
                    printf("  CEGAR difference at tt index %ld\n", first_one);
                }
                if (!synth->create_tt_clauses(spec, first_one-1)) {
                    return failure;
                }
            } else if (status == timeout) {
                if (*found) {
                    return timeout;
                }
            } else {
                if (spec.verbosity) {
                    printf("  FAILURE\n\n"); 
                }
                return status;
            }
        }
    }
    ***************************************************************************/

    /*
    template<typename TT, typename Solver, typename Generator>
    void
    fence_synthesize_parallel(
            const spec& main_spec, chain<2>& chain, 
            synth_result& result, bool& stop, Generator& gen, 
            std::mutex& gen_mutex)
    {
        // We cannot directly copy the spec. We need each thread to have its
        // own specification and solver to avoid threading issues.
        spec spec;
        spec.nr_in = main_spec.nr_in;
        spec.nr_out = main_spec.nr_out;
        spec.verbosity = main_spec.verbosity;
        spec.conflict_limit = main_spec.conflict_limit;
        
        if (spec.verbosity > 2) {
            printf("Thread %lu START\n", std::this_thread::get_id());
        }

        for (int i = 0; i < MAX_OUT; i++) {
            spec.functions[i] = main_spec.functions[i];
        }

        spec_preprocess(spec);
        
        // The special case when the Boolean chain to be synthesized consists
        // entirely of trivial functions.
        if (spec.nr_triv == spec.nr_out) {
            chain.reset(spec.nr_in, spec.nr_out, 0);
            for (int h = 0; h < spec.nr_out; h++) {
                chain.set_output(h, (spec.triv_functions[h] << 1) +
                        ((spec.out_inv >> h) & 1));
            }
            std::lock_guard<std::mutex> gen_lock(gen_mutex);
            if (spec.verbosity > 2) {
                printf("Thread %lu SOLUTION(0)\n", std::this_thread::get_id());
            }
            stop = true;
            result = success;
            return;
        }

        auto synth = new_fence_synth();

        // As the topological synthesizer decomposes the synthesis problem,
        // to fairly count the total number of conflicts we should keep track
        // of all conflicts in existence checks.
        int total_conflicts = 0;
        fence f;
        int old_nnodes = 1;
        while (true) {
            {
                std::lock_guard<std::mutex> gen_lock(gen_mutex);
                if (stop) {
                    if (spec.verbosity > 2) {
                        printf("Thread %lu RETURN\n", 
                                std::this_thread::get_id());
                    }
                    return;
                }
                gen.next_fence(f);
            }
            spec.nr_steps = f.nr_nodes();
            spec.update_level_map(f);
            
            if (spec.nr_steps > old_nnodes) {
                // Reset conflict count, since this is where other synthesizers
                // reset it.
                total_conflicts = 0;
                old_nnodes = spec.nr_steps;
            }

            if (spec.verbosity > 2) {
                printf("  next fence:\n");
                print_fence(f);
                printf("\n");
                printf("nr_nodes=%d, nr_levels=%d\n", f.nr_nodes(), 
                        f.nr_levels());
                for (int i = 0; i < f.nr_levels(); i++) {
                    printf("f[%d] = %d\n", i, f[i]);
                }
            }
            // Add the clauses that encode the main constraints, but not
            // yet any DAG structure.
            auto status = chain_exists(synth.get(), spec);
            if (status == success) {
                synth->extract_chain(spec, chain);
                std::lock_guard<std::mutex> gen_lock(gen_mutex);
                stop = true;
                if (spec.verbosity > 2) {
                    printf("Thread %lu SOLUTION(%d)\n", 
                            std::this_thread::get_id(), chain.nr_steps());
                }
                result = success;
                return;
            } else if (status == failure) {
                {
                    std::lock_guard<std::mutex> gen_lock(gen_mutex);
                    if (stop) {
                        if (spec.verbosity > 2) {
                            printf("Thread %lu RETURN\n", 
                                    std::this_thread::get_id());
                        }
                        return;
                    }
                }
                total_conflicts += synth->get_nr_conflicts();
                if (spec.conflict_limit &&
                        total_conflicts > spec.conflict_limit) {
                    if (spec.verbosity > 2) {
                        printf("Thread %lu TIMEOUT\n",
                                std::this_thread::get_id());
                    }
                    return;
                }
                continue;
            } else {
                if (spec.verbosity > 2) {
                    printf("Thread %lu TIMEOUT\n", std::this_thread::get_id());
                }
                return;
            }
        }
    }

    template<typename TT, typename Solver, typename Generator>
    void 
    cegar_fence_synthesize_parallel(
            const spec& main_spec, chain<TT>& chain, 
            synth_result& result, bool& stop, Generator& gen, 
            std::mutex& gen_mutex)
    {
        // We cannot directly copy the spec. We need each thread to have its own
        // specification and solver to avoid threading issues.
        spec spec;
        spec.nr_in = main_spec.nr_in;
        spec.nr_out = main_spec.nr_out;
        spec.verbosity = main_spec.verbosity;
        spec.conflict_limit = main_spec.conflict_limit;
        
        if (spec.verbosity > 2) {
            printf("Thread %lu START\n", std::this_thread::get_id());
        }

        for (int i = 0; i < MAX_OUT; i++) {
            spec.functions[i] = main_spec.functions[i];
        }

        spec_preprocess(spec);
        spec.nr_rand_tt_assigns = 2*spec.nr_in;
        
        // The special case when the Boolean chain to be synthesized consists
        // entirely of trivial functions.
        if (spec.nr_triv == spec.nr_out) {
            chain.reset(spec.nr_in, spec.nr_out, 0);
            for (int h = 0; h < spec.nr_out; h++) {
                chain.set_output(h, (spec.triv_functions[h] << 1) +
                        ((spec.out_inv >> h) & 1));
            }
            std::lock_guard<std::mutex> gen_lock(gen_mutex);
            if (spec.verbosity > 2) {
                printf("Thread %lu SOLUTION(0)\n", std::this_thread::get_id());
            }
            stop = true;
            result = success;
            return;
        }

        auto synth = new_synth<fence_synthesizer<TT,Solver>>();

        // As the topological synthesizer decomposes the synthesis problem,
        // to fairly count the total number of conflicts we should keep track
        // of all conflicts in existence checks.
        int total_conflicts = 0;
        fence f;
        int old_nnodes = 1;
        while (true) {
            {
                std::lock_guard<std::mutex> gen_lock(gen_mutex);
                if (stop) {
                    if (spec.verbosity > 2) {
                        printf("Thread %lu RETURN\n", 
                                std::this_thread::get_id());
                    }
                    return;
                }
                gen.next_fence(f);
            }
            spec.nr_steps = f.nr_nodes();
            spec.update_level_map(f);
            
            if (spec.nr_steps > old_nnodes) {
                // Reset conflict count, since this is where other synthesizers
                // reset it.
                total_conflicts = 0;
                old_nnodes = spec.nr_steps;
            }

            if (spec.verbosity) {
                printf("  next fence:\n");
                print_fence(f);
                printf("\n");
                printf("nr_nodes=%d, nr_levels=%d\n", f.nr_nodes(), 
                        f.nr_levels());
                for (int i = 0; i < f.nr_levels(); i++) {
                    printf("f[%d] = %d\n", i, f[i]);
                }
            }
            // Add the clauses that encode the main constraints, but not
            // yet any DAG structure.
            auto status = cegar_chain_exists(synth.get(), spec, chain);
            if (status == success) {
                synth->chain_extract(spec, chain);
                std::lock_guard<std::mutex> gen_lock(gen_mutex);
                stop = true;
                if (spec.verbosity > 2) {
                    printf("Thread %lu SOLUTION(%d)\n", 
                            std::this_thread::get_id(), chain.nr_steps());
                }
                result = success;
                return;
            } else if (status == failure) {
                {
                    std::lock_guard<std::mutex> gen_lock(gen_mutex);
                    if (stop) {
                        if (spec.verbosity > 2) {
                            printf("Thread %lu RETURN\n", 
                                    std::this_thread::get_id());
                        }
                        return;
                    }
                }
                total_conflicts += synth->get_nr_conflicts();
                if (spec.conflict_limit &&
                        total_conflicts > spec.conflict_limit) {
                    if (spec.verbosity > 2) {
                        printf("Thread %lu TIMEOUT\n",
                                std::this_thread::get_id());
                    }
                    return;
                }
                continue;
            } else {
                if (spec.verbosity > 2) {
                    printf("Thread %lu TIMEOUT\n", std::this_thread::get_id());
                }
                return;
            }
        }
    }

    template<typename TT, typename Solver>
    synth_result 
    synthesize_parallel(
            const spec& spec, const int nr_threads, 
            chain<TT>& chain)
    {
        po_filter<unbounded_generator> g(unbounded_generator(), 1, 2);
        std::mutex m;
        std::vector<std::thread> threads(nr_threads);
        std::vector<percy::chain<TT>> chains(nr_threads);
        std::vector<synth_result> statuses(nr_threads);
        bool stop = false;

        for (int i = 0; i < nr_threads; i++) {
            statuses[i] = timeout;
        }

        for (int i = 0; i < nr_threads; i++) {
            auto& c = chains[i];
            auto& status = statuses[i];
            threads[i] = std::thread([&spec, &c, &status, &stop, &g, &m] {
                    fence_synthesize_parallel<TT, Solver>(spec, c, status, 
                            stop, g, m);
            });
        }

        if (spec.verbosity) {
            printf("\n\nSTARTED THREADS\n\n");
        }

        for (auto& t : threads) {
            t.join();
        }

        int best_sol = 1000000; // Arbitrary large number
        bool had_success = false;
        for (int i = 0; i < nr_threads; i++) {
            if (statuses[i] == success) {
                if (chains[i].nr_steps() < best_sol) {
                    best_sol = chains[i].nr_steps();
                    chain.copy(chains[i]);
                    had_success = true;
                }
            }
        }

        if (had_success) {
            return success;
        } else {
            return timeout;
        }
    }

    template<typename TT, typename Solver>
    synth_result 
    cegar_synthesize_parallel(
            const spec& spec, const int nr_threads, 
            chain<TT>& chain)
    {
        po_filter<unbounded_generator> g(unbounded_generator(), 1, 2);
        std::mutex m;
        std::vector<std::thread> threads(nr_threads);
        std::vector<percy::chain<TT>> chains(nr_threads);
        std::vector<synth_result> statuses(nr_threads);
        bool stop = false;

        for (int i = 0; i < nr_threads; i++) {
            statuses[i] = timeout;
        }

        for (int i = 0; i < nr_threads; i++) {
            auto& c = chains[i];
            auto& status = statuses[i];
            threads[i] = std::thread([&spec, &c, &status, &stop, &g, &m] {
                    cegar_fence_synthesize_parallel<TT, Solver>(spec, 
                            c, status, stop, g, m);
            });
        }

        if (spec.verbosity) {
            printf("\n\nSTARTED THREADS\n\n");
        }

        for (auto& t : threads) {
            t.join();
        }

        int best_sol = 1000000; // Arbitrary large number
        bool had_success = false;
        for (int i = 0; i < nr_threads; i++) {
            if (statuses[i] == success) {
                if (chains[i].nr_steps() < best_sol) {
                    best_sol = chains[i].nr_steps();
                    chain.copy(chains[i]);
                    had_success = true;
                }
            }
        }

        if (had_success) {
            return success;
        } else {
            return timeout;
        }
    }
    */

    /***************************************************************************
        Finds the smallest possible DAG that can implement the specified
        function.
    template<int FI=2>
    synth_result find_dag(spec& spec, dag<FI>& g, int nr_vars)
    {
        chain chain;
        rec_dag_generator gen;

        int nr_vertices = 1;
        while (true) {
            gen.reset(nr_vars, nr_vertices);
            g.reset(nr_vars, nr_vertices);
            const auto result = gen.find_dag(spec, g, chain, synth);
            if (result == success) {
                return success;
            }
            nr_vertices++;
        }
        return failure;
    }
    ***************************************************************************/

    /***************************************************************************
        Finds a DAG of the specified size that can implement the given
        function (if one exists).
    synth_result 
    find_dag(
            spec& spec, 
            dag<2>& g, 
            const int nr_vars, 
            const int nr_vertices)
    {
        chain chain;
        rec_dag_generator gen;
        knuth_dag_synthesizer<2> synth;

        gen.reset(nr_vars, nr_vertices);
        g.reset(nr_vars, nr_vertices);
        return gen.find_dag(spec, g, chain, synth);
    }

    synth_result 
    qpfind_dag(
            spec& spec, 
            dag<2>& g, 
            int nr_vars,
            bool verbose=false)
    {
        int nr_vertices = 1;
        while (true) {
            if (verbose) {
                fprintf(stderr, "Trying with %d vertices\n", nr_vertices);
                fflush(stderr);
            }
            const auto status = qpfind_dag(spec, g, nr_vars, nr_vertices);
            if (status == success) {
                return success;
            }
            nr_vertices++;
        }
        
        return failure;
    }
    ***************************************************************************/

    /*
    synth_result 
    qpfind_dag(
            spec& spec, 
            dag<2>& g, 
            const int nr_vars, 
            const int nr_vertices)
    {
        std::vector<std::thread> threads;
       
        const auto nr_threads = std::thread::hardware_concurrency() - 1;
        
        moodycamel::ConcurrentQueue<dag<2>> q(nr_threads*3);
        

        bool finished_generating = false;
        bool* pfinished = &finished_generating;
        bool found = false;
        bool* pfound = &found;
        std::mutex found_mutex;

        // Preprocess the spec only once, and not separately in every thread
        spec.preprocess();

        g.reset(nr_vars, nr_vertices);
        for (int i = 0; i < nr_threads; i++) {
            threads.push_back(
                std::thread([&spec, pfinished, pfound, &found_mutex, &g, &q] {
                    dag<2> local_dag;
                    chain local_chain;
                    auto synth = new_dag_synth();

                    while (!(*pfound)) {
                        if (!q.try_dequeue(local_dag)) {
                            if (*pfinished) {
                                if (!q.try_dequeue(local_dag)) {
                                    break;
                                }
                            } else {
                                std::this_thread::yield();
                                continue;
                            }
                        }
                        const auto status = synth->synthesize(spec, local_dag,
                                local_chain, false);

                        if (status == success) {
                            std::lock_guard<std::mutex> vlock(found_mutex);
                            if (!(*pfound)) {
                                for (int j = 0; j <
                                        local_dag.get_nr_vertices(); j++) {
                                    const auto& v = local_dag.get_vertex(j);
                                    g.set_vertex(j, v.first, v.second);
                                }
                                *pfound = true;
                            }
                        }
                    }
                }
            ));
        }

        rec_dag_generator gen;
        gen.reset(nr_vars, nr_vertices);
        gen.qpfind_dag(q, pfound);
        finished_generating = true;

        // After the generating thread has finished, we have room to spare
        // for another thread (if no solution was found yet.)
        if (!found) {
            dag<2> local_dag;
            chain local_chain;
            auto synth = new_dag_synth();

            while (!found) {
                if (!q.try_dequeue(local_dag)) {
                    break;
                }
                const auto status = 
                    synth->synthesize(spec, local_dag, local_chain, false);

                if (status == success) {
                    std::lock_guard<std::mutex> vlock(found_mutex);
                    if (!(found)) {
                        for (int j = 0; j < nr_vertices; j++) {
                            const auto& v = local_dag.get_vertex(j);
                            g.set_vertex(j, v.first, v.second);
                        }
                        found = true;
                    }
                }
            }
        }

        for (auto& t : threads) {
            t.join();
        }

        return found ? success : failure;
    }

    synth_result 
    qpfence_synth(
            synth_stats* stats,
            const dynamic_truth_table& function,
            dag<2>& g,
            int nr_vars,
            int conflict_limit)
    {
        int nr_vertices = 1;
        while (true) {
            const auto status = qpfence_synth(
                    stats, function, g, nr_vars, nr_vertices, conflict_limit);
            if (status == success) {
                return success;
            }
            nr_vertices++;
        }
        
        return failure;
    }
    */

    /*
    synth_result 
    qpfence_synth(
            synth_stats* stats,
            const dynamic_truth_table& f, 
            dag<2>& g, 
            int nr_vars, 
            int nr_vertices,
            int conflict_limit)
    {
        std::vector<std::thread> threads;
        moodycamel::ConcurrentQueue<fence> q(1u << (nr_vertices - 1));
        
        const auto nr_threads = std::thread::hardware_concurrency() - 1;

        bool finished_generating = false;
        bool* pfinished = &finished_generating;
        bool found = false;
        bool* pfound = &found;
        std::mutex found_mutex;
        
        auto start = high_resolution_clock::now();
        time_point<high_resolution_clock> first_synth_time;

        stats->overhead = 0;
        stats->time_to_first_synth = 0;
        stats->total_synth_time = 0;

        for (int i = 0; i < nr_threads; i++) {
            threads.push_back(
                std::thread([&f, pfinished, pfound, &found_mutex, &g, &q, 
                    nr_vars, nr_vertices, &first_synth_time, conflict_limit] {
                    chain chain;
                    fence local_fence;
                    fence_synthesizer synth(new bsat_wrapper);

                    spec local_spec(nr_vars, 1);
                    local_spec.verbosity = 0;
                    local_spec.nr_steps = nr_vertices;
                    local_spec.functions[0] = f;
                    local_spec.nr_rand_tt_assigns = 2 * local_spec.get_nr_in();
                    local_spec.conflict_limit = conflict_limit;

                    while (!(*pfound)) {
                        if (!q.try_dequeue(local_fence)) {
                            if (*pfinished) {
                                if (!q.try_dequeue(local_fence)) {
                                    break;
                                }
                            } else {
                                std::this_thread::yield();
                                continue;
                            }
                        }

                        auto status = synth.cegar_synthesize(local_spec,
                                local_fence, chain);

                        if (status == success) {
                            std::lock_guard<std::mutex> vlock(found_mutex);
                            if (!(*pfound)) {
                                first_synth_time = high_resolution_clock::now();
                                chain.extract_dag(g);
                                *pfound = true;
                            }
                        }
                    }
                }
            ));
        }

        generate_fences(nr_vertices, q);
        finished_generating = true;

        // After the generating thread has finished, we have room to spare
        // for another thread (if no solution was found yet.)
        {
            chain chain;
            fence local_fence;
            fence_synthesizer synth(new bsat_wrapper);

            spec local_spec(nr_vars, 1);
            local_spec.verbosity = 0;
            local_spec.nr_steps = nr_vertices;
            local_spec.functions[0] = f;
            local_spec.nr_rand_tt_assigns = 2 * local_spec.get_nr_in();
            local_spec.conflict_limit = conflict_limit;

            while (!(found)) {
                if (!q.try_dequeue(local_fence)) {
                    if (finished_generating) {
                        if (!q.try_dequeue(local_fence)) {
                            break;
                        }
                    } else {
                        std::this_thread::yield();
                        continue;
                    }
                }

                auto status = synth.cegar_synthesize(local_spec, local_fence,
                        chain);

                if (status == success) {
                    std::lock_guard<std::mutex> vlock(found_mutex);
                    if (!(found)) {
                        first_synth_time = high_resolution_clock::now();
                        chain.extract_dag(g);
                        found = true;
                    }
                }
            }
        }

        for (auto& t : threads) {
            t.join();
        }
        if (found) {
            const auto total_synth_time = duration<double,std::milli>(
                    high_resolution_clock::now() - start).count();
    
            const auto time_to_first_synth = duration<double,std::milli>(
                    first_synth_time - start).count();

            const auto overhead = total_synth_time - time_to_first_synth;

            stats->overhead = overhead;
            stats->time_to_first_synth = time_to_first_synth;
            stats->total_synth_time = total_synth_time;
        }


        return found ? success : failure;
    }
    */

    uint64_t 
    parallel_dag_count(int nr_vars, int nr_vertices, int nr_threads)
    {
        printf("Initializing %d-thread parallel DAG count\n", nr_threads);
        std::vector<std::thread> threads;

        int nr_branches = 0;
        std::vector<std::pair<int,int>> starting_points;
        for (int k = 1; k < nr_vars; k++) {
            for (int j = 0; j < k; j++) {
                ++nr_branches;
                starting_points.push_back(std::make_pair(j, k));
            }
        }
        printf("Nr. root branches: %d\n", nr_branches);
        if (nr_branches > nr_threads) {
            printf("Error: unable to distribute %d branches over %d "
                    "threads\n", nr_branches, nr_threads);
            return 0;
        }

        // Each thread will write the number of solutions it has found to this
        // array.
        auto solv = new uint64_t[starting_points.size()];
        
        for (int i = 0; i < starting_points.size(); i++) {
            const auto& sp = starting_points[i];
            threads.push_back(
                std::thread([i, solv, &sp, nr_vars, nr_vertices] {
                    printf("Starting thread %d\n", i+1);

                    rec_dag_generator gen;
                    gen.reset(nr_vars, nr_vertices);
                    gen.add_selection(sp.first, sp.second);
                    const auto nsols = gen.count_dags();
                    solv[i] = nsols;
                }
            ));
        }

        for (auto& t : threads) {
            t.join();
        }

        uint64_t total_nsols =0 ;
        for (int i = 0; i < starting_points.size(); i++) {
            printf("solv[%d] = %lu\n", i, solv[i]);
            total_nsols += solv[i];
        }
        printf("total_nsols=%lu\n", total_nsols);

        delete[] solv;

        return total_nsols;
    }

    std::vector<dag<2>> 
    parallel_dag_gen(int nr_vars, int nr_vertices, int nr_threads)
    {
        printf("Initializing %d-thread parallel DAG gen\n", nr_threads);
        printf("nr_vars=%d, nr_vertices=%d\n", nr_vars, nr_vertices);
        std::vector<std::thread> threads;

        // Each thread will write the DAGs it has found to this vector.
        std::vector<dag<2>> dags;

        // First estimate the number of solutions down each branch by looking
        // at DAGs with small numbers of vertices.
        int nr_branches = 0;
        std::vector<std::pair<int,int>> starting_points;
        for (int k = 1; k < nr_vars; k++) {
            for (int j = 0; j < k; j++) {
                ++nr_branches;
                starting_points.push_back(std::make_pair(j, k));
            }
        }
        printf("Nr. root branches: %d\n", nr_branches);
        if (nr_branches > nr_threads) {
            printf("Error: unable to distribute %d branches over %d "
                    "threads\n", nr_branches, nr_threads);
            return dags;
        }

        std::mutex vmutex;
        
        for (int i = 0; i < starting_points.size(); i++) {
            const auto& sp = starting_points[i];
            threads.push_back(
                std::thread([i, &dags, &vmutex, &sp, nr_vars, nr_vertices] {
                    printf("Starting thread %d\n", i+1);

                    rec_dag_generator gen;
                    gen.reset(nr_vars, nr_vertices);
                    gen.add_selection(sp.first, sp.second);
                    auto tdags = gen.gen_dags();
                    {
                        std::lock_guard<std::mutex> vlock(vmutex);
                        for (auto& dag : tdags) {
                            dags.push_back(dag);
                        }
                    }
                }
            ));
        }

        for (auto& t : threads) {
            t.join();
        }

        printf("Generated %lu dags\n", dags.size());

        return dags;
    }

    synth_result 
    std_synthesize(
        spec& spec, 
        chain& chain, 
        solver_wrapper& solver, 
        std_encoder& encoder,
        synth_stats* stats = NULL)
    {
        assert(spec.get_nr_in() >= spec.fanin);
        spec.preprocess();
        encoder.set_dirty(true);

        if (stats) {
            stats->synth_time = 0;
            stats->sat_time = 0;
            stats->unsat_time = 0;
        }

        // The special case when the Boolean chain to be synthesized
        // consists entirely of trivial functions.
        if (spec.nr_triv == spec.get_nr_out()) {
            chain.reset(spec.get_nr_in(), spec.get_nr_out(), 0, spec.fanin);
            for (int h = 0; h < spec.get_nr_out(); h++) {
                chain.set_output(h, (spec.triv_func(h) << 1) +
                    ((spec.out_inv >> h) & 1));
            }
            return success;
        }

        spec.nr_steps = spec.initial_steps;
        while (true) {
            solver.restart();
            if (!encoder.encode(spec)) {
                spec.nr_steps++;
                continue;
            }

            auto begin = std::chrono::steady_clock::now();
            const auto status = solver.solve(spec.conflict_limit);
            auto end = std::chrono::steady_clock::now();
            auto elapsed_time =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    end - begin
                    ).count();

            if (stats) {
                stats->synth_time += elapsed_time;
            }

            if (status == success) {
                encoder.extract_chain(spec, chain);
                if (spec.verbosity > 2) {
                    //    encoder.print_solver_state(spec);
                }
                if (stats) {
                    stats->sat_time = elapsed_time;
                }
                return success;
            } else if (status == failure) {
                if (stats) {
                    stats->unsat_time += elapsed_time;
                }
                spec.nr_steps++;
            } else {
                return timeout;
            }
        }
    }

    synth_result
    std_cegar_synthesize(
        spec& spec, 
        chain& chain, 
        solver_wrapper& solver, 
        std_encoder& encoder,
        synth_stats* stats = NULL)
    {
        assert(spec.get_nr_in() >= spec.fanin);
        spec.preprocess();
        encoder.set_dirty(true);

        if (stats) {
            stats->synth_time = 0;
            stats->sat_time = 0;
            stats->unsat_time = 0;
        }

        // The special case when the Boolean chain to be synthesized
        // consists entirely of trivial functions.
        if (spec.nr_triv == spec.get_nr_out()) {
            chain.reset(spec.get_nr_in(), spec.get_nr_out(), 0, spec.fanin);
            for (int h = 0; h < spec.get_nr_out(); h++) {
                chain.set_output(h, (spec.triv_func(h) << 1) +
                    ((spec.out_inv >> h) & 1));
            }
            return success;
        }

        spec.nr_rand_tt_assigns = 2 * spec.get_nr_in();
        spec.nr_steps = spec.initial_steps;
        while (true) {
            solver.restart();
            if (!encoder.cegar_encode(spec)) {
                spec.nr_steps++;
                continue;
            }
            while (true) {
                auto begin = std::chrono::steady_clock::now();
                auto stat = solver.solve(spec.conflict_limit);
                auto end = std::chrono::steady_clock::now();
                auto elapsed_time =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        end - begin
                        ).count();

                if (stats) {
                    stats->synth_time += elapsed_time;
                }
                if (stat == success) {
                    encoder.extract_chain(spec, chain);
                    auto sim_tts = chain.simulate(spec);
                    auto xor_tt = (sim_tts[0]) ^ (spec[0]);
                    auto first_one = kitty::find_first_one_bit(xor_tt);
                    if (first_one == -1) {
                        if (stats) {
                            stats->sat_time += elapsed_time;
                        }
                        return success;
                    }
                    // Add additional constraint.
                    if (spec.verbosity) {
                        printf("  CEGAR difference at tt index %ld\n",
                            first_one);
                    }
                    if (!encoder.create_tt_clauses(spec, first_one - 1)) {
                        if (stats) {
                            stats->sat_time += elapsed_time;
                        }
                        spec.nr_steps++;
                        break;
                    }
                } else if (stat == failure) {
                    if (stats) {
                        stats->unsat_time += elapsed_time;
                    }
                    spec.nr_steps++;
                    break;
                } else {
                    return timeout;
                }
            }
        }
    }

    std::unique_ptr<solver_wrapper>
    get_solver(SolverType type = SLV_BSAT2)
    {
        solver_wrapper * solver = nullptr;
        std::unique_ptr<solver_wrapper> res;

        switch (type) {
        case SLV_BSAT2:
            solver = new bsat_wrapper;
            break;
#ifdef USE_CMS
        case SLV_CMSAT:
            solver = new cmsat_wrapper;
            break;
#endif
#if !defined(_WIN32) && !defined(_WIN64)
//        case SLV_GLUCOSE:
//            solver = new glucose_wrapper;
//            break;
#endif
        default:
            fprintf(stderr, "Error: solver type %d not found", type);
            exit(1);
        }

        res.reset(solver);
        return res;
    }

    std::unique_ptr<encoder>
    get_encoder(solver_wrapper& solver, EncoderType enc_type = ENC_KNUTH)
    {
        encoder * enc = nullptr;
        std::unique_ptr<encoder> res;

        switch (enc_type) {
        case ENC_KNUTH:
            enc = new knuth_encoder(solver);
            break;
        case ENC_EPFL:
            enc = new epfl_encoder(solver);
            break;
        case ENC_BERKELEY:
            enc = new berkeley_encoder(solver);
            break;
        case ENC_FENCE:
            enc = new knuth_fence_encoder(solver);
            break;
        case ENC_DAG:
            enc = new knuth_dag_encoder<2>();
            break;
        default:
            fprintf(stderr, "Error: encoder type %d not found\n", enc_type);
            exit(1);
        }

        res.reset(enc);
        return res;
    }

    std::unique_ptr<enumerating_encoder>
    get_enum_encoder(solver_wrapper& solver, EncoderType enc_type = ENC_KNUTH)
    {
        enumerating_encoder * enc = nullptr;
        std::unique_ptr<enumerating_encoder> res;

        switch (enc_type) {
        case ENC_KNUTH:
            enc = new knuth_encoder(solver);
            break;
        case ENC_EPFL:
            enc = new epfl_encoder(solver);
            break;
        case ENC_FENCE:
            enc = new knuth_fence_encoder(solver);
            break;
        default:
            fprintf(stderr, "Error: enumerating encoder of ctype %d not found\n", enc_type);
            exit(1);
        }

        res.reset(enc);
        return res;
    }


    synth_result 
    fence_synthesize(spec& spec, chain& chain, solver_wrapper& solver, fence_encoder& encoder)
    {
        assert(spec.get_nr_in() >= spec.fanin);
        spec.preprocess();
        encoder.set_dirty(true);

        // The special case when the Boolean chain to be synthesized
        // consists entirely of trivial functions.
        if (spec.nr_triv == spec.get_nr_out()) {
            chain.reset(spec.get_nr_in(), spec.get_nr_out(), 0, spec.fanin);
            for (int h = 0; h < spec.get_nr_out(); h++) {
                chain.set_output(h, (spec.triv_func(h) << 1) +
                    ((spec.out_inv >> h) & 1));
            }
            return success;
        }

        // As the topological synthesizer decomposes the synthesis
        // problem, to fairly count the total number of conflicts we
        // should keep track of all conflicts in existence checks.
        int total_conflicts = 0;
        fence f;
        po_filter<unbounded_generator> g(
            unbounded_generator(spec.initial_steps),
            spec.get_nr_out(), spec.fanin);
        int old_nnodes = 1;
        while (true) {
            g.next_fence(f);
            spec.nr_steps = f.nr_nodes();

            if (spec.nr_steps > old_nnodes) {
                // Reset conflict count, since this is where other
                // synthesizers reset it.
                total_conflicts = 0;
                old_nnodes = spec.nr_steps;
            }

            solver.restart();
            if (!encoder.encode(spec, f)) {
                continue;
            }

            if (spec.verbosity) {
                printf("  next fence:\n");
                print_fence(f);
                printf("\n");
                printf("nr_nodes=%d, nr_levels=%d\n", f.nr_nodes(),
                    f.nr_levels());
                for (int i = 0; i < f.nr_levels(); i++) {
                    printf("f[%d] = %d\n", i, f[i]);
                }
            }
            auto status = solver.solve(spec.conflict_limit);
            if (status == success) {
                encoder.extract_chain(spec, chain);
                return success;
            } else if (status == failure) {
                total_conflicts += solver.nr_conflicts();
                if (spec.conflict_limit &&
                    total_conflicts > spec.conflict_limit) {
                    return timeout;
                }
                continue;
            } else {
                return timeout;
            }
        }
    }

    synth_result
    fence_synthesize(
        spec& spec, 
        chain& chain, 
        solver_wrapper& solver, 
        fence_encoder& encoder, 
        fence& fence)
    {
        solver.restart();
        if (!encoder.encode(spec, fence)) {
            return failure;
        }
        auto status = solver.solve(spec.conflict_limit);
        if (status == success) {
            encoder.extract_chain(spec, chain);
        }
        return status;
    }

    synth_result
    fence_cegar_synthesize(
        spec& spec, 
        chain& chain, 
        solver_wrapper& solver, 
        fence_encoder& encoder, 
        fence& fence)
    {
        spec.nr_rand_tt_assigns = 2 * spec.get_nr_in();
        solver.restart();
        if (!encoder.cegar_encode(spec, fence)) {
            return failure;
        }
        
        while (true) {
            auto status = solver.solve(spec.conflict_limit);
            if (status == success) {
                encoder.extract_chain(spec, chain);
                auto sim_tts = chain.simulate(spec);
                auto xor_tt = (sim_tts[0]) ^ (spec[0]);
                auto first_one = kitty::find_first_one_bit(xor_tt);
                if (first_one == -1) {
                    return success;
                }
                // Add additional constraint.
                if (spec.verbosity) {
                    printf("  CEGAR difference at tt index %ld\n",
                        first_one);
                }
                if (!encoder.create_tt_clauses(spec, first_one - 1)) {
                    return failure;
                }
            } else {
                return status;
            }
        }
    }
    
    synth_result 
    fence_cegar_synthesize(spec& spec, chain& chain, solver_wrapper& solver, fence_encoder& encoder)
    {
        assert(spec.get_nr_in() >= spec.fanin);

        spec.preprocess();
        encoder.set_dirty(true);

        // The special case when the Boolean chain to be synthesized
        // consists entirely of trivial functions.
        if (spec.nr_triv == spec.get_nr_out()) {
            chain.reset(spec.get_nr_in(), spec.get_nr_out(), 0, spec.fanin);
            for (int h = 0; h < spec.get_nr_out(); h++) {
                chain.set_output(h, (spec.triv_func(h) << 1) +
                    ((spec.out_inv >> h) & 1));
            }
            return success;
        }

        spec.nr_rand_tt_assigns = 2 * spec.get_nr_in();

        fence f;
        po_filter<unbounded_generator> g(
            unbounded_generator(spec.initial_steps),
            spec.get_nr_out(), spec.fanin);
        int total_conflicts = 0;
        int old_nnodes = 1;
        while (true) {
            g.next_fence(f);
            spec.nr_steps = f.nr_nodes();

            if (spec.nr_steps > old_nnodes) {
                // Reset conflict count, since this is where other
                // synthesizers reset it.
                total_conflicts = 0;
                old_nnodes = spec.nr_steps;
            }

            if (spec.verbosity) {
                printf("  next fence:\n");
                print_fence(f);
                printf("\n");
                printf("nr_nodes=%d, nr_levels=%d\n", f.nr_nodes(),
                    f.nr_levels());
                for (int i = 0; i < f.nr_levels(); i++) {
                    printf("f[%d] = %d\n", i, f[i]);
                }
            }

            solver.restart();
            if (!encoder.cegar_encode(spec, f)) {
                continue;
            }
            while (true) {
                auto status = solver.solve(spec.conflict_limit);
                if (status == success) {
                    encoder.extract_chain(spec, chain);
                    auto sim_tts = chain.simulate(spec);
                    auto xor_tt = (sim_tts[0]) ^ (spec[0]);
                    auto first_one = kitty::find_first_one_bit(xor_tt);
                    if (first_one == -1) {
                        if (spec.verbosity) {
                            printf("  SUCCESS\n\n");
                        }
                        return success;
                    }
                    // Add additional constraint.
                    if (spec.verbosity) {
                        printf("  CEGAR difference at tt index %ld\n",
                            first_one);
                    }
                    if (!encoder.create_tt_clauses(spec, first_one - 1)) {
                        break;
                    }
                } else if (status == failure) {
                    break;
                } else {
                    return timeout;
                }
            }
        }
    }

    ///< TODO: implement
    synth_result
    dag_synthesize(spec& spec, chain& chain, solver_wrapper& solver, dag_encoder<2>& encoder)
    {
        return failure;
    }

    synth_result 
    synthesize(
        spec& spec, 
        chain& chain, 
        solver_wrapper& solver, 
        encoder& encoder, 
        SynthMethod synth_method = SYNTH_STD,
        synth_stats * stats = NULL)
    {
        switch (synth_method) {
        case SYNTH_STD:
            return std_synthesize(spec, chain, solver, static_cast<std_encoder&>(encoder), stats);
        case SYNTH_STD_CEGAR:
            return std_cegar_synthesize(spec, chain, solver, static_cast<std_encoder&>(encoder), stats);
        case SYNTH_FENCE:
            return fence_synthesize(spec, chain, solver, static_cast<fence_encoder&>(encoder));
        case SYNTH_FENCE_CEGAR:
            return fence_cegar_synthesize(spec, chain, solver, static_cast<fence_encoder&>(encoder));
        case SYNTH_DAG:
            return dag_synthesize(spec, chain, solver, static_cast<dag_encoder<2>&>(encoder));
        default:
            fprintf(stderr, "Error: synthesis method %d not supported\n", synth_method);
            exit(1);
        }
    }
            
    synth_result
    pf_fence_synthesize(
        spec& spec, 
        chain& c, 
        int num_threads = std::thread::hardware_concurrency())
    {
        std::vector<std::thread> threads(num_threads);

        moodycamel::ConcurrentQueue<fence> q(num_threads * 3);

        bool finished_generating = false;
        bool* pfinished = &finished_generating;
        bool found = false;
        bool* pfound = &found;
        std::mutex found_mutex;

        spec.nr_steps = spec.initial_steps;
        auto status = failure;
        while (true) {
            for (int i = 0; i < num_threads; i++) {
                threads[i] = std::thread([&spec, pfinished, pfound, &found_mutex, &c, &q] {
                    bsat_wrapper solver;
                    knuth_fence_encoder encoder(solver);
                    fence local_fence;
                    chain local_chain;

                    while (!(*pfound)) {
                        if (!q.try_dequeue(local_fence)) {
                            if (*pfinished) {
                                if (!q.try_dequeue(local_fence)) {
                                    break;
                                }
                            } else {
                                std::this_thread::yield();
                                continue;
                            }
                        }
                        const auto status = fence_synthesize(spec, local_chain, solver, encoder, local_fence);
                        if (status == success) {
                            std::lock_guard<std::mutex> vlock(found_mutex);
                            if (!(*pfound)) {
                                c.copy(local_chain);
                                *pfound = true;
                            }
                        }
                    }
                });
            }
            generate_fences(spec, q);
            finished_generating = true;

            for (auto& thread : threads) {
                thread.join();
            }
            if (found) {
                break;
            }
            spec.nr_steps++;
        }

        return success;
    }
    
    /// Performs fence-based parallel synthesis.
    /// One thread generates fences and places them on a concurrent
    /// queue. The remaining threads dequeue fences and try to
    /// synthesize chains with them.
    synth_result
    pf_synthesize(
        spec& spec, 
        chain&  chain, 
        SynthMethod synth_method = SYNTH_FENCE)
    {
        switch (synth_method) {
        case SYNTH_FENCE:
            return pf_fence_synthesize(spec, chain);
//        case SYNTH_FENCE_CEGAR:
//            return pf_fence_cegar_synthesize(spec, chain, solver, encoder);
        default:
            fprintf(stderr, "Error: synthesis method %d not supported\n", synth_method);
            exit(1);
        }
    }

    synth_result
    synthesize(
        spec& spec, 
        chain& chain, 
        SolverType slv_type = SLV_BSAT2, 
        EncoderType enc_type = ENC_KNUTH, 
        SynthMethod method = SYNTH_STD)
    {
        auto solver = get_solver(slv_type);
        auto encoder = get_encoder(*solver, enc_type);
        return synthesize(spec, chain, *solver, *encoder, method);
    }

    synth_result
    next_solution(
        spec& spec, 
        chain& chain, 
        solver_wrapper& solver, 
        enumerating_encoder& encoder, 
        SynthMethod synth_method = SYNTH_STD)
    {
        if (!encoder.is_dirty()) {
            switch (synth_method) {
            case SYNTH_STD:
            case SYNTH_STD_CEGAR:
                return std_synthesize(spec, chain, solver, static_cast<std_encoder&>(encoder));
            case SYNTH_FENCE:
                return fence_synthesize(spec, chain, solver, static_cast<fence_encoder&>(encoder));
            default:
                fprintf(stderr, "Error: solution enumeration not supported for synth method %d\n", synth_method);
                exit(1);
            }
        }

        // The special case when the Boolean chain to be synthesized
        // consists entirely of trivial functions.
        // In this case, only one solution exists.
        if (spec.nr_triv == spec.get_nr_out()) {
            return failure;
        }

        if (encoder.block_solution(spec)) {
            const auto status = solver.solve(spec.conflict_limit);

            if (status == success) {
                encoder.extract_chain(spec, chain);
                return success;
            } else {
                return status;
            }
        }

        return failure;
    }

    synth_result
    next_struct_solution(
        spec& spec, 
        chain& chain, 
        solver_wrapper& solver, 
        enumerating_encoder& encoder,
        SynthMethod synth_method = SYNTH_STD)
    {
        if (!encoder.is_dirty()) {
            switch (synth_method) {
            case SYNTH_STD:
            case SYNTH_STD_CEGAR:
                return std_synthesize(spec, chain, solver, static_cast<std_encoder&>(encoder));
            case SYNTH_FENCE:
                return fence_synthesize(spec, chain, solver, static_cast<fence_encoder&>(encoder));
            default:
                fprintf(stderr, "Error: solution enumeration not supported for synth method %d\n", synth_method);
                exit(1);
            }
        }

        // The special case when the Boolean chain to be synthesized
        // consists entirely of trivial functions.
        // In this case, only one solution exists.
        if (spec.nr_triv == spec.get_nr_out()) {
            return failure;
        }

        if (encoder.block_solution(spec)) {
            const auto status = solver.solve(spec.conflict_limit);

            if (status == success) {
                encoder.extract_chain(spec, chain);
                return success;
            } else {
                return status;
            }
        }

        return failure;
    }

}

