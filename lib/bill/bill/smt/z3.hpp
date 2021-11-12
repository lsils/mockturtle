/*-------------------------------------------------------------------------------------------------
| This file is distributed under the MIT License.
| See accompanying file /LICENSE for details.
*------------------------------------------------------------------------------------------------*/
#pragma once

#if defined(BILL_HAS_Z3)

#include <fmt/format.h>
#include <limits>
#include <vector>
#include <type_traits>
#include <z3++.h>

namespace bill {

template<bool has_objective = false>
class z3_smt_solver {
public:
    using var_t = uint32_t;
    using lp_expr_t = std::vector<std::pair<int32_t,var_t>>;

    enum class states : uint8_t {
        satisfiable,
        unsatisfiable,
        undefined,
    };

    enum class var_types : uint8_t {
        boolean,
        integer,
        real
    };

    enum class lp_types : uint8_t {
        geq,
        leq,
        eq,
        greater,
        less,
    };

#pragma region Constructors
    z3_smt_solver()
        : solver_(ctx_)
        , variable_counter_(0u)
    {}

    ~z3_smt_solver()
    {}

    /* disallow copying */
    z3_smt_solver(z3_smt_solver const&) = delete;
    z3_smt_solver& operator=(const z3_smt_solver&) = delete;
#pragma endregion

#pragma region Modifiers
    void restart()
    {
        solver_.reset();
        vars_.clear();
        variable_counter_ = 0u;
        state_ = states::undefined;
    }

    //void set_logic(std::string const& logic)
    //{
    //    solver_.set("logic", logic.c_str());
    //}

    var_t add_variable(var_types type)
    {
        switch (type) {
        case var_types::boolean:
            vars_.push_back(ctx_.bool_const(fmt::format("bool{}", variable_counter_).c_str()));
            break;
        case var_types::integer:
            vars_.push_back(ctx_.int_const(fmt::format("int{}", variable_counter_).c_str()));
            break;
        case var_types::real:
            vars_.push_back(ctx_.real_const(fmt::format("real{}", variable_counter_).c_str()));
            break;
        default:
            assert(false && "Error: unknown variable type\n");
            return std::numeric_limits<uint32_t>::max();
        }
        
        return variable_counter_++;
    }

    void add_variables(var_types type, uint32_t num_variables = 1)
    {
        for (auto i = 0u; i < num_variables; ++i) {
            add_variable(type);
        }
    }

    /* create an integer-type variable and set it to count how many boolean-type variables in `var_set` are true */
    var_t add_integer_cardinality(std::vector<var_t> const& var_set)
    {
        var_t counter = add_variable(var_types::integer);
        solver_.add(vars_[counter] == make_integer_sum(var_set));
        return counter;
    }

    /* create an real-type variable and set it to count how many boolean-type variables in `var_set` are true */
    var_t add_real_cardinality(std::vector<var_t> const& var_set)
    {
        var_t counter = add_variable(var_types::real);
        solver_.add(vars_[counter] == make_real_sum(var_set));
        return counter;
    }

    /* create a boolean-type variable and set it to if the LP condition holds */
    var_t add_lp_condition(lp_expr_t const& lhs, int32_t rhs, lp_types type)
    {
        assert( is_real_lp_expr(lhs) );
        z3::expr expr = make_lp_expr(lhs);

        var_t cond_var = add_variable(var_types::boolean);
        z3::expr cond = vars_[cond_var];

        switch (type)
        {
        case lp_types::geq:
            solver_.add(z3::implies(cond, expr >= ctx_.real_val(rhs, 1)));
            return cond_var;
        case lp_types::leq:
            solver_.add(z3::implies(cond, expr <= ctx_.real_val(rhs, 1)));
            return cond_var;
        case lp_types::eq:
            solver_.add(z3::implies(cond, expr == ctx_.real_val(rhs, 1)));
            return cond_var;
        case lp_types::greater:
            solver_.add(z3::implies(cond, expr > ctx_.real_val(rhs, 1)));
            return cond_var;
        case lp_types::less:
            solver_.add(z3::implies(cond, expr < ctx_.real_val(rhs, 1)));
            return cond_var;
        default:
            assert(false && "unknown LP constraint type");
            return cond_var;
        }
    }

    var_t add_ilp_condition(lp_expr_t const& lhs, int32_t rhs, lp_types type)
    {
        assert( is_integer_lp_expr(lhs) );
        z3::expr expr = make_lp_expr(lhs);

        var_t cond_var = add_variable(var_types::boolean);
        z3::expr cond = vars_[cond_var];

        switch (type)
        {
        case lp_types::geq:
            solver_.add(z3::implies(cond, expr >= rhs));
            return cond_var;
        case lp_types::leq:
            solver_.add(z3::implies(cond, expr <= rhs));
            return cond_var;
        case lp_types::eq:
            solver_.add(z3::implies(cond, expr == rhs));
            return cond_var;
        case lp_types::greater:
            solver_.add(z3::implies(cond, expr > rhs));
            return cond_var;
        case lp_types::less:
            solver_.add(z3::implies(cond, expr < rhs));
            return cond_var;
        default:
            assert(false && "unknown LP constraint type");
            return cond_var;
        }
    }

    /* assert a LP constraint */
    void add_lp_constraint(std::vector<std::pair<int32_t,var_t>> const& lhs, int32_t rhs, lp_types type)
    {
        var_t cond_var = add_lp_condition(lhs, rhs, type);
        solver_.add(vars_[cond_var]);
    }

    void add_ilp_constraint(std::vector<std::pair<int32_t,var_t>> const& lhs, int32_t rhs, lp_types type)
    {
        var_t cond_var = add_ilp_condition(lhs, rhs, type);
        solver_.add(vars_[cond_var]);
    }

    void assert_true(var_t const& v)
    {
        assert(is_boolean_type(v));
        solver_.add(vars_[v]);
    }

    void assert_false(var_t const& v)
    {
        assert(is_boolean_type(v));
        solver_.add(!vars_[v]);
    }

    template<bool enabled = has_objective, typename = std::enable_if_t<enabled>>
    void maximize(var_t const& var)
    {
        solver_.maximize(vars_[var]);
    }

    template<bool enabled = has_objective, typename = std::enable_if_t<enabled>>
    void maximize(lp_expr_t const& objective)
    {
        z3::expr expr = make_lp_expr(objective);
        solver_.maximize(expr);
    }

    template<bool enabled = has_objective, typename = std::enable_if_t<enabled>>
    void minimize(var_t const& var)
    {
        solver_.minimize(vars_[var]);
    }

    template<bool enabled = has_objective, typename = std::enable_if_t<enabled>>
    void minimize(lp_expr_t const& objective)
    {
        z3::expr expr = make_lp_expr(objective);
        solver_.minimize(expr);
    }

    states solve()
    {
        z3::expr_vector vec(ctx_);
        switch (solver_.check(vec)) {
        case z3::sat:
            state_ = states::satisfiable;
            break;
        case z3::unsat:
            state_ = states::unsatisfiable;
            break;
        case z3::unknown:
        default:
            state_ = states::undefined;
            break;
        };
        z3::reset_params();
        return state_;
    }
#pragma endregion

#pragma region Properties
    uint32_t num_variables() const
    {
        return variable_counter_;
    }
#pragma endregion

#pragma region Get Model
    bool get_boolean_variable_value(var_t var)
    {
        assert(is_boolean_type(var));
        assert(state_ == states::satisfiable);

        return solver_.get_model().eval(vars_[var]).is_true();
    }

    int64_t get_numeral_variable_value_as_integer(var_t var)
    {
        assert(is_integer_type(var) || is_real_type(var));
        assert(state_ == states::satisfiable);
        return solver_.get_model().eval(vars_[var]).get_numeral_int64();
    }
#pragma endregion

    template<bool enabled = !has_objective, typename = std::enable_if_t<enabled>>
    void print()
    {
        std::cout << solver_.to_smt2() << "\n";
    }

private:
    z3::expr make_lp_expr(lp_expr_t const& expr)
    {
        assert( expr.size() > 0 );
        z3::expr e = expr[0].first == 1 ? vars_[expr[0].second] : expr[0].first * vars_[expr[0].second];
        for ( auto i = 1u; i < expr.size(); ++i )
        {
            e = e + ( expr[i].first == 1 ? vars_[expr[i].second] : expr[i].first * vars_[expr[i].second] );
        }
        return e;
    }

    z3::expr make_integer_sum(std::vector<var_t> const& var_set)
    {
        assert(is_all_boolean(var_set));
        z3::expr_vector vec(ctx_);
        for ( auto const& v : var_set )
            vec.push_back(z3::ite(vars_[v], ctx_.int_val(1), ctx_.int_val(0)));
        return z3::sum(vec);
    }

    z3::expr make_real_sum(std::vector<var_t> const& var_set)
    {
        assert(is_all_boolean(var_set));
        z3::expr_vector vec(ctx_);
        for ( auto const& v : var_set )
            vec.push_back(z3::ite(vars_[v], ctx_.real_val(1), ctx_.real_val(0)));
        return z3::sum(vec);
    }

#pragma region Check Variable Type
    bool is_boolean_type(var_t var)
    {
        return vars_[var].is_bool();
    }

    bool is_integer_type(var_t var)
    {
        return vars_[var].is_int();
    }

    bool is_real_type(var_t var)
    {
        return vars_[var].is_real();
    }

    bool is_all_boolean(std::vector<var_t> const& var_set)
    {
        bool res = true;
        for ( auto const& var : var_set )
            res &= is_boolean_type(var);
        return res;
    }

    bool is_integer_lp_expr(lp_expr_t const& expr)
    {
        bool res = true;
        for ( auto const& term : expr )
            res &= is_integer_type(term.second);
        return res;
    }

    bool is_real_lp_expr(lp_expr_t const& expr)
    {
        bool res = true;
        for ( auto const& term : expr )
            res &= is_real_type(term.second);
        return res;
    }
#pragma endregion

private:
    /*! \brief Backend solver context object */
    z3::context ctx_;

    /*! \brief Backend solver */
    std::conditional_t<has_objective, z3::optimize, z3::solver> solver_;

    /*! \brief Current state of the solver */
    states state_ = states::undefined;

    /*! \brief Variables */
    std::vector<z3::expr> vars_;

    /*! \brief Stacked counter for number of variables */
    uint32_t variable_counter_;
};

} // namespace bill

#endif
