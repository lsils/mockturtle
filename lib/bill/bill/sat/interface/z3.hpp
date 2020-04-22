/*-------------------------------------------------------------------------------------------------
| This file is distributed under the MIT License.
| See accompanying file /LICENSE for details.
*------------------------------------------------------------------------------------------------*/
#pragma once

#if defined(BILL_HAS_Z3)

#include "common.hpp"
#include "types.hpp"

#include <fmt/format.h>
#include <limits>
#include <vector>
#include <z3++.h>

namespace bill {

template<>
class solver<solvers::z3> {
public:
#pragma region Constructors
	solver()
	    : solver_(ctx_)
	{
	}

	~solver()
	{}

	/* disallow copying */
	solver(solver<solvers::z3> const&) = delete;
	solver<solvers::z3>& operator=(const solver<solvers::z3>&) = delete;
#pragma endregion

#pragma region Modifiers
	void restart()
	{
		solver_.reset();
		vars_.clear();
		var_ctr_ = 0u;
		cls_ctr_ = 0u;
		state_ = result::states::undefined;
	}

	var_type add_variable()
	{
		vars_.push_back(ctx_.bool_const(fmt::format("x{}", var_ctr_).c_str()));
		return var_ctr_++;
	}

	void add_variables(uint32_t num_variables = 1)
	{
		for (auto i = 0u; i < num_variables; ++i) {
			add_variable();
		}
	}

	auto add_clause(std::vector<lit_type>::const_iterator it,
	                std::vector<lit_type>::const_iterator ie)
	{
		z3::expr_vector vec(ctx_);
		while (it != ie) {
			vec.push_back(it->is_complemented() ? !vars_[it->variable()] :
			                                      vars_[it->variable()]);
			++it;
		}
		solver_.add(mk_or(vec));
		++cls_ctr_;
		return result::states::dirty;
	}

	auto add_clause(std::vector<lit_type> const& clause)
	{
		return add_clause(std::begin(clause), std::end(clause));
	}

	auto add_clause(lit_type lit)
	{
		solver_.add(lit.is_complemented() ? !vars_[lit.variable()] : vars_[lit.variable()]);
		return result::states::dirty;
	}

	result get_model() const
	{
		assert(state_ == result::states::satisfiable);
		result::model_type model;
		const auto m = solver_.get_model();
		for (auto const& v : vars_) {
			model.emplace_back(m.eval(v).is_true() ? lbool_type::true_ :
			                                         lbool_type::false_);
		}
		return result(model);
	}

	result get_result() const
	{
		assert(state_ != result::states::dirty);
		if (state_ == result::states::satisfiable) {
			return get_model();
		} else {
			return result();
		}
	}

	result::states solve(std::vector<lit_type> const& assumptions = {},
	                     uint32_t conflict_limit = 0u)
	{
		z3::expr_vector vec(ctx_);
		for (auto const& lit : assumptions)
			vec.push_back(lit.is_complemented() ? !vars_[lit.variable()] :
			                                      vars_[lit.variable()]);
		solver_.set("sat.max_conflicts", conflict_limit == 0u ? std::numeric_limits<uint32_t>::max() : conflict_limit);
		switch (solver_.check(vec)) {
		case z3::sat:
			state_ = result::states::satisfiable;
			break;
		case z3::unsat:
			state_ = result::states::unsatisfiable;
			break;
		case z3::unknown:
		default:
			state_ = result::states::undefined;
			break;
		};
		z3::reset_params();
		return state_;
	}
#pragma endregion

#pragma region Properties
	uint32_t num_variables() const
	{
		return var_ctr_;
	}

	uint32_t num_clauses() const
	{
		return cls_ctr_;
	}
#pragma endregion

private:
	z3::context ctx_;
	z3::solver solver_;
	result::states state_ = result::states::undefined;
	std::vector<z3::expr> vars_;
	uint32_t var_ctr_{};
	uint32_t cls_ctr_{};
};

} // namespace bill

#endif
