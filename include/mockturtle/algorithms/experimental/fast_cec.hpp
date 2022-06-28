/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2021  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file fast_cec.hpp
  \brief Combinational equivalence checking

  \author Hanyu Wang
*/

#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

#include "../../traits.hpp"
#include "../../utils/include/percy.hpp"
#include "../../algorithms/cut_rewriting.hpp"
#include "../../utils/stopwatch.hpp"
#include "./func_reduction.hpp"
#include "../cnf.hpp"

#include <fmt/format.h>

namespace mockturtle::experimental
{

/*! \brief Parameters for fast_cec.
 *
 * The data structure `fast_cec_params` holds configurable
 * parameters with default arguments for `fast_cec`.
 */
struct fast_cec_params
{
  /*! \brief Conflict limit for SAT solver.
   *
   * The default limit is 0, which means the number of conflicts is not used
   * as a resource limit.
   */
  uint64_t sat_conflict_limit{5000u};
  uint64_t resub_conflict_limit{20u};

  uint32_t num_iterations{6u};

  /* \brief Be verbose. */
  bool verbose{true};
};

/*! \brief Statistics for fast_cec.
 *
 * The data structure `fast_cec_stats` provides data collected by
 * running `fast_cec`.
 */
struct fast_cec_stats
{
  /*! \brief Total runtime. */
  stopwatch<>::duration time_total{};

  /*! \brief Counter-example, in case miter is SAT. */
  std::vector<bool> counter_example;

  void report() const
  {
    std::cout << fmt::format( "[i] total time     = {:>5.2f} secs\n", to_seconds( time_total ) );
  }
};

namespace detail
{

template<class Ntk>
class fast_cec_impl
{
public:
  fast_cec_impl( Ntk& miter, fast_cec_params const& ps, fast_cec_stats& st )
      : miter_( miter ),
        ps_( ps ),
        st_( st )
  {
  }

  std::optional<bool> run()
  {
    stopwatch<> t( st_.time_total );
    
    uint64_t scl = ps_.sat_conflict_limit;
    uint64_t rcl = ps_.resub_conflict_limit;

    for ( uint64_t i = 0; i < ps_.num_iterations; ++i )
    {
      stopwatch<>::duration time_sat{};
      stopwatch<>::duration time_resub{};

      xag_npn_resynthesis<aig_network> resyn;
      cut_rewriting_params ps;
      ps.cut_enumeration_ps.cut_size = 4;
      for ( uint32_t j = 0; j < 3; j++ )
      {
        miter_ = cut_rewriting( miter_, resyn, ps );
        miter_ = cleanup_dangling( miter_ );
      }
      auto res = call_with_stopwatch( time_sat, [&]() { return try_sat_solver( scl <<= 1 ); } );
            
      if ( res )
      {
        return *res;
      }
      // try FRAIG
      func_reduction_params fps;
      func_reduction_stats fst;
      fps.conflict_limit = ( rcl <<= 3 );
      call_with_stopwatch( time_resub, [&]() { func_reduction( miter_, fps, &fst ); } );
      miter_ = cleanup_dangling( miter_ );
      if ( ps_.verbose )
      {
        fmt::print( "[i] iter = {}, sat: {:.2} sec, resub: {:.2} sec, #gate = {}\n", i, to_seconds( time_sat ), to_seconds( time_resub ), miter_.num_gates() );
      }

    }

    return try_sat_solver( 0 );
  }

  std::optional<bool> try_sat_solver( uint64_t conflict_limit )
  {
    percy::bsat_wrapper solver;
    int output = generate_cnf( miter_, [&]( auto const& clause ) {
      solver.add_clause( clause );
    } )[0];

    const auto res = solver.solve( &output, &output + 1, conflict_limit );

    switch ( res )
    {
    default:
      return std::nullopt;
    case percy::synth_result::success:
    {
      st_.counter_example.clear();
      for ( auto i = 1u; i <= miter_.num_pis(); ++i )
      {
        st_.counter_example.push_back( solver.var_value( i ) );
      }
      return false;
    }
    case percy::synth_result::failure:
      return true;
    }
  }

private:
  Ntk& miter_;
  fast_cec_params const& ps_;
  fast_cec_stats& st_;
};

} // namespace detail

/*! \brief Fast Combinational equivalence checking.
 *
 * This function expects as input a miter circuit that can be generated, e.g.,
 * with the function `miter`.  It returns an optional which is `nullopt`, if no
 * solution can be found (this happens when a resource limit is set using the
 * function's parameters).  Otherwise it returns `true`, if the miter is
 * equivalent or `false`, if the miter is not equivalent.  In the latter case
 * the counter example is written to the statistics pointer as a
 * `std::vector<bool>` following the same order as the primary inputs.
 *
 * \param miter Miter network
 * \param ps Parameters
 * \param st Statistics
 */
template<class Ntk>
std::optional<bool> fast_cec( Ntk& miter, fast_cec_params const& ps = {}, fast_cec_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_num_pis_v<Ntk>, "Ntk does not implement the num_pis method" );
  static_assert( has_num_pos_v<Ntk>, "Ntk does not implement the num_pos method" );

  if ( miter.num_pos() != 1u )
  {
    std::cout << "[e] miter network must have a single output\n";
    return std::nullopt;
  }

  fast_cec_stats st;
  detail::fast_cec_impl<Ntk> impl( miter, ps, st );
  const auto result = impl.run();

  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }

  return result;
}

} /* namespace mockturtle */