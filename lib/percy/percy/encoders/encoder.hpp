#pragma once

#include <bitset>
#include "../spec.hpp"
#include "../misc.hpp"
#include "../sat_circuits.hpp"
#include <abc/vecInt.h>

namespace percy
{
    class encoder
    {
    protected:
        solver_wrapper* solver = nullptr;

    public:
        virtual void set_solver(solver_wrapper& s)
        {
            solver = &s;
        }
    };

    class enumerating_encoder : public encoder
    {
    protected:
        bool dirty = false;

    public:
        virtual bool block_solution(const spec& spec) = 0;
        virtual bool block_struct_solution(const spec& spec) = 0;
        virtual void extract_chain(const spec& spec, chain& chain) = 0;

        void reset()
        {
            dirty = false;
        }

        bool is_dirty() { return dirty; }
        void set_dirty(bool dirty) { this->dirty = dirty;  }
    };

    class std_encoder : public enumerating_encoder
    {
    public:
        virtual bool encode(const spec& spec) = 0;
        virtual bool cegar_encode(const spec& spec) = 0;
        virtual bool create_tt_clauses(const spec& spec, int idx) = 0;
        virtual void print_solver_state(const spec& spec) = 0;
    };

    class fence_encoder : public enumerating_encoder
    {
    public:
        virtual bool encode(const spec& spec, const fence& f) = 0;
        virtual bool cegar_encode(const spec& spec, const fence& f) = 0;
        virtual bool create_tt_clauses(const spec& spec, int idx) = 0;
    };

    template<int FI>
    class dag_encoder : public encoder
    {
    public:
        virtual bool encode(const spec& spec, const dag<FI>& dag) = 0;
        virtual bool cegar_encode(const spec& spec, const dag<FI>& dag) = 0;
        virtual void extract_chain(const spec& spec, const dag<2>& dag, chain& chain) = 0;
    };


    static inline void
    clear_assignment(std::vector<int>& fanin_asgn)
    {
        std::fill(fanin_asgn.begin(), fanin_asgn.end(), 0);
    }

    static inline void
    next_assignment(std::vector<int>& asgn)
    {
        for (int i = 0; i < asgn.size(); i++) {
            if (asgn[i]) {
                asgn[i] = 0;
            } else {
                asgn[i]++;
                return;
            }
        }
    }
    
    static inline void
    inc_assignment(std::vector<int>& asgn, int max_val, int i)
    {
        if (i >= asgn.size()) return;

        if (asgn[i] == max_val) {
            asgn[i] = 0;
            inc_assignment(asgn, max_val, i + 1);
        } else {
            assert(asgn[i] < max_val);
            asgn[i]++;
        }
    }
    
    static inline void
    inc_assignment(std::vector<int>& asgn, int max_val)
    {
        inc_assignment(asgn, max_val, 0);
    }


    static inline bool
    is_zero(const std::vector<int>& fanin_asgn)
    {
        auto status = true;
        for (const auto asgn : fanin_asgn) {
            if (asgn != 0) {
                status = false;
            }
        }
        return status;
    }

    void
    fanin_init(std::vector<int>& fanins, int max_fanin_id)
    {
        fanins[fanins.size()-1] = max_fanin_id--;
        for (int i = fanins.size() - 2; i >= 0; i--) {
            fanins[i] = max_fanin_id--;
        }
    }

    void
    fanin_init(std::vector<int>& fanins, int max_fanin_id, int start_idx)
    {
        fanins[start_idx] = max_fanin_id--;
        for (int i = start_idx-1; i >= 0; i--) {
            fanins[i] = max_fanin_id--;
        }
    }

    bool
    fanin_inc(std::vector<int>& fanins, const int max_fanin_id)
    {
        int inc_idx = 0;

        for (int i = 0; i < fanins.size(); i++) {
            if (i < fanins.size() - 1) {
                if (fanins[i] < fanins[i + 1] - 1) {
                    fanins[i]++;
                    if (i > 0) {
                        fanin_init(fanins, i - 1, i - 1);
                    }
                    return true;
                }
            } else {
                if (fanins[i] < max_fanin_id) {
                    fanins[i]++;
                    fanin_init(fanins, i - 1, i - 1);
                    return true;
                }
            }
        }

        return false;
    }

    void
    print_fanin(const std::vector<int>& fanins)
    {
        for (int i = 0; i < fanins.size(); i++) {
            printf("%d ", fanins[i] + 1);
        }
    }

    void
    print_fanin(const int* const fanins, int nr_fanins)
    {
        for (int i = 0; i < nr_fanins; i++) {
            printf("%d ", fanins[i] + 1);
        }
    }
}
