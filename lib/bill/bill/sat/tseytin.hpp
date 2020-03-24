/*-------------------------------------------------------------------------------------------------
| This file is distributed under the MIT License.
| See accompanying file /LICENSE for details.
*------------------------------------------------------------------------------------------------*/
#pragma once

#include "interface/types.hpp"

namespace bill {

template<typename Solver>
lit_type add_tseytin_and(Solver& solver, lit_type const& a, lit_type const& b)
{
	auto const r = solver.add_variable();
	solver.add_clause(std::vector{~a, ~b, lit_type(r, lit_type::polarities::positive)});
	solver.add_clause(std::vector{a, lit_type(r, lit_type::polarities::negative)});
	solver.add_clause(std::vector{b, lit_type(r, lit_type::polarities::negative)});
	return lit_type(r, lit_type::polarities::positive);
}

template<typename Solver>
lit_type add_tseytin_and(Solver& solver, std::vector<lit_type> const& ls)
{
	auto const r = solver.add_variable();
	std::vector<lit_type> cls;
	for (const auto& l : ls)
		cls.emplace_back(~l);
	cls.emplace_back(lit_type(r, lit_type::polarities::positive));
	solver.add_clause(cls);
	for (const auto& l : ls)
		solver.add_clause(std::vector{l, lit_type(r, lit_type::polarities::negative)});
	return lit_type(r, lit_type::polarities::positive);
}

template<typename Solver>
lit_type add_tseytin_or(Solver& solver, lit_type const& a, lit_type const& b)
{
	auto const r = solver.add_variable();
	solver.add_clause(std::vector{a, b, lit_type(r, lit_type::polarities::negative)});
	solver.add_clause(std::vector{~a, lit_type(r, lit_type::polarities::positive)});
	solver.add_clause(std::vector{~b, lit_type(r, lit_type::polarities::positive)});
	return lit_type(r, lit_type::polarities::positive);
}

template<typename Solver>
lit_type add_tseytin_or(Solver& solver, std::vector<lit_type> const& ls)
{
	auto const r = solver.add_variable();
	std::vector<lit_type> cls(ls);
	cls.emplace_back(lit_type(r, lit_type::polarities::negative));
	solver.add_clause(cls);
	for (const auto& l : ls)
		solver.add_clause(std::vector{~l, lit_type(r, lit_type::polarities::positive)});
	return lit_type(r, lit_type::polarities::positive);
}

template<typename Solver>
lit_type add_tseytin_xor(Solver& solver, lit_type const& a, lit_type const& b)
{
	auto const r = solver.add_variable();
	solver.add_clause(std::vector{~a, ~b, lit_type(r, lit_type::polarities::negative)});
	solver.add_clause(std::vector{~a, b, lit_type(r, lit_type::polarities::positive)});
	solver.add_clause(std::vector{a, ~b, lit_type(r, lit_type::polarities::positive)});
	solver.add_clause(std::vector{a, b, lit_type(r, lit_type::polarities::negative)});
	return lit_type(r, lit_type::polarities::positive);
}

template<typename Solver>
lit_type add_tseytin_equals(Solver& solver, lit_type const& a, lit_type const& b)
{
	auto const r = solver.add_variable();
	solver.add_clause(std::vector{~a, ~b, lit_type(r, lit_type::polarities::positive)});
	solver.add_clause(std::vector{~a, b, lit_type(r, lit_type::polarities::negative)});
	solver.add_clause(std::vector{a, ~b, lit_type(r, lit_type::polarities::negative)});
	solver.add_clause(std::vector{a, b, lit_type(r, lit_type::polarities::positive)});
	return lit_type(r, lit_type::polarities::positive);
}

} /* namespace bill */
