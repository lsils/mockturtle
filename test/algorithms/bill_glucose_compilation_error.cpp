//
// Created by marcel on 14.02.22.
//

#include <catch.hpp>

#include <bill/sat/solver.hpp>
#include <bill/sat/tseytin.hpp>

TEST_CASE( "Glucose compilation problem", "[glucose]" )
{
  bill::solver<bill::solvers::glucose_41> solver{};

  // example from bill's README
  auto const a = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );
  auto const b = bill::lit_type( solver.add_variable(), bill::lit_type::polarities::positive );

  auto const t0 = bill::add_tseytin_and( solver, a, b );
  auto const t1 = ~bill::add_tseytin_or( solver, ~a, ~b );
  auto const t2 = add_tseytin_xor( solver, t0, t1 );
  solver.add_clause( t2 );

  CHECK( solver.solve() == bill::result::states::unsatisfiable );
}
