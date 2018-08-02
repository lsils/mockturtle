#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-pedantic"
#include <kitty/kitty.hpp>
#pragma GCC diagnostic pop

namespace percy
{
    
    template<typename TT>
    static inline bool is_normal(const TT& tt)
    {
        return !kitty::get_bit(tt, 0);
    }

    /***************************************************************************
        We consider a truth table to be trivial if it is equal to (or the
        complement of) a primary input or constant zero.
    ***************************************************************************/
    template<typename TT>
    static inline bool is_trivial(const TT& tt)
    {
        TT tt_check = tt;

        if (kitty::is_const0(tt) || kitty::is_const0(~tt)) {
            return true;
        }

        for (auto i = 0; i < tt.num_vars(); i++) {
            kitty::create_nth_var(tt_check, i);
            if (tt == tt_check || tt == ~tt_check) {
                return true;
            }
        }

        return false;
    }

}
