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
  \file optimal_buffer_insertion.hpp
  \brief Global-optimal buffer insertion for AQFP using SMT

  \author Siang-Yun (Sonia) Lee
*/

// NOTE: This file is included inside the class `mockturtle::buffer_insertion`
// It should not be included anywhere else.

void optimize_with_smt()
{
  std::ofstream os( "model.smt", std::ofstream::out );
  dump_smt_model( os );
  os.close();
  std::system( "z3 model.smt > sol.txt" );
  parse_z3_result();
  return;
}

#if 1 // ILP formulation
void dump_smt_model( std::ostream& os = std::cout )
{
  os << "(set-logic QF_LIA)\n";
  /* hard assumptions to bound the number of variables */
  uint32_t const max_depth = depth() + 3;
  uint32_t const max_relative_depth = max_depth;
  count_buffers();
  uint32_t const upper_bound = num_buffers();

  /* depth */
  std::string depth;
  if ( _ps.assume.balance_pos ) /* depth is a variable */
  {
    os << fmt::format( "(declare-const depth Int)\n" );
    os << fmt::format( "(assert (<= depth {}))\n", max_depth );
    depth = "depth";
  }
  else
  {
    depth = fmt::format( "{}", max_depth );
  }

  /* declare variables for the level of each node */
  std::vector<std::string> level_vars;
  if ( _ps.assume.branch_pis && !_ps.assume.balance_pis )
  {
    /* Only if we branch but not balance PIs, they are free variables as gates */
    _ntk.foreach_pi( [&]( auto const& n ){
      level_vars.emplace_back( fmt::format( "l{}", n ) );
      os << fmt::format( "(declare-const l{} Int)\n", n );
      os << fmt::format( "(assert (<= l{} {}))\n", n, depth );
      os << fmt::format( "(assert (>= l{} 0))\n", n );
    });
  }

  _ntk.foreach_gate( [&]( auto const& n ){
    level_vars.emplace_back( fmt::format( "l{}", n ) );
    os << fmt::format( "(declare-const l{} Int)\n", n );
    os << fmt::format( "(assert (<= l{} {}))\n", n, depth );
    os << fmt::format( "(assert (>= l{} 1))\n", n );
  });

  /* constraints for each gate's fanout */
  std::vector<std::string> bufs;
  if ( _ps.assume.branch_pis )
  {
    /* If PIs are balanced, they are always at level 0 so we don't have variables for them */
    if ( _ps.assume.balance_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        smt_constraints_balanced_pis( os, n, max_depth, max_relative_depth );
        bufs.emplace_back( fmt::format( "bufs{}", n ) );
      });
    }
    else
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        smt_constraints( os, n, max_depth, max_relative_depth );
        bufs.emplace_back( fmt::format( "bufs{}", n ) );
      });
    }
  }
  _ntk.foreach_gate( [&]( auto const& n ){
    smt_constraints( os, n, max_depth, max_relative_depth );
    bufs.emplace_back( fmt::format( "bufs{}", n ) );
  });

  os << "\n(declare-const total Int)\n";
  os << fmt::format( "(assert (= total (+ {})))\n", fmt::join( bufs, " " ) );
  os << fmt::format( "(assert (<= total {}))\n", upper_bound );
  os << "(minimize total)\n(check-sat)\n(get-value (total depth))\n";
  os << fmt::format( "(get-value ({}))\n(exit)\n", fmt::join( level_vars, " " ) );
}

void smt_constraints_balanced_pis( std::ostream& os, node const& n, uint32_t const& max_depth, uint32_t const& max_relative_depth )
{
  os << fmt::format( "\n;constraints for node {}\n", n );
  os << fmt::format( "(declare-const bufs{} Int)\n", n );
  /* special cases */
  if ( _ntk.fanout_size( n ) == 0 ) /* dangling */
  {
    os << fmt::format( "(assert (= bufs{} 0))\n", n );
    return;
  }
  else if ( _ntk.fanout_size( n ) == 1 )
  {
    /* single fanout: only sequencing constraint */
    if ( _external_ref_count[n] > 0 )
    {
      if ( _ps.assume.balance_pos )
      {
        os << fmt::format( "(assert (= bufs{} depth))\n", n );
      }
      else
      {
        os << fmt::format( "(assert (= bufs{} 0))\n", n );
      }
    }
    else
    {
      node const& no = _fanouts[n].front().fanouts.front();
      os << fmt::format( "(assert (= bufs{} (- l{} 1)))\n", n, no );
    }
    return;
  }
  else if ( _ntk.fanout_size( n ) <= _ps.assume.splitter_capacity )
  {
    /* only one splitter is needed */

    /* sequencing */
    std::vector<node> fos;
    foreach_fanout( n, [&]( auto const& no ){ 
      os << fmt::format( "(assert (>= l{} 2))\n", no );
      fos.emplace_back( no );
    });

    if ( _external_ref_count[n] > 0 && _ps.assume.balance_pos )
    {
      os << fmt::format( "(assert (= bufs{} depth))\n", n );
    }      
    else
    {
      if ( fos.size() == 0 ) /* not balance PO; have multiple PO refs */
      {
        os << fmt::format( "(assert (= bufs{} 1))\n", n );
      }
      else if ( fos.size() == 1 ) /* not balance PO; have one gate fanout and PO ref(s) */
      {
        os << fmt::format( "(assert (= bufs{} (- l{} 1)))\n", n, fos[0] );
      }
      else if ( fos.size() >= 2 )
      {
        os << fmt::format( "(declare-const g{}maxfo Int)\n", n );/* the max level of fanouts */

        /* bound it -- just to hint the solver; should not matter if we have optimization objective */
        if ( _ps.assume.balance_pos )
          os << fmt::format( "(assert (<= g{}maxfo depth))\n", n );
        else
          os << fmt::format( "(assert (<= g{}maxfo {}))\n", n, max_depth );

        /* taking the max (lower bounded by the max) */
        for ( auto const& no : fos )
          os << fmt::format( "(assert (>= g{}maxfo l{}))\n", n, no );

        os << fmt::format( "(assert (= bufs{} (- g{}maxfo 1)))\n", n, n );
      }
    }
    return;
  }

  /* sequencing & range constraints */
  foreach_fanout( n, [&]( auto const& no ){
    os << fmt::format( "(assert (>= l{} 2))\n", no );
  });

  /* PO branching */
  if ( _external_ref_count[n] > 0 )
  {
    os << fmt::format( "(assert (>= depth {}))\n", num_splitter_levels( n ) );
  }

  uint32_t l = max_relative_depth;
  bool add_po_refs = false;
  if ( _external_ref_count[n] > 0 && _ps.assume.balance_pos )
  {
    add_po_refs = true;
    os << fmt::format( "(assert (<= depth {}))\n", l-1 );
  }

  /* initial case */
  os << fmt::format( "(declare-const g{}e{} Int)\n", n, l ); // edges at relative depth l
  os << fmt::format( "(assert (= g{}e{} (+", n, l );
  foreach_fanout( n, [&]( auto const& no ){
    os << fmt::format( " (ite (= l{} {}) 1 0)", no, l );
  });
  if ( add_po_refs ) os << fmt::format( " (ite (= depth {}) {} 0)", l-1, _external_ref_count[n] );
  os << ")))\n";

  /* general case */
  for ( --l; l > 0; --l )
  {
    os << fmt::format( "(declare-const g{}b{} Int)\n", n, l ); // buffers at relative depth l
    /* g{n}b{l} = ceil( g{n}e{l+1} / s_b ) --> s_b * ( g{n}b{l} - 1 ) < g{n}e{l+1} <= s_b * g{n}b{l} */
    os << fmt::format( "(assert (< (* {} (- g{}b{} 1)) g{}e{}))\n", _ps.assume.splitter_capacity, n, l, n, l+1 );
    os << fmt::format( "(assert (<= g{}e{} (* {} g{}b{})))\n", n, l+1, _ps.assume.splitter_capacity, n, l );
    
    os << fmt::format( "(declare-const g{}e{} Int)\n", n, l ); // edges at relative depth l
    os << fmt::format( "(assert (= g{}e{} (+", n, l ); 
    foreach_fanout( n, [&]( auto const& no ){
      os << fmt::format( " (ite (= l{} {}) 1 0)", no, l );
    });
    if ( add_po_refs ) os << fmt::format( " (ite (= depth {}) {} 0)", l-1, _external_ref_count[n] );
    os << fmt::format( " g{}b{})))\n", n, l );
  }

  /* end of loop */
  os << fmt::format( "(assert (= g{}e1 1))\n", n ); // legal
  os << fmt::format( "(assert (= bufs{} (+", n );
  for ( l = max_relative_depth - 1; l > 0; --l )
  {
    os << fmt::format( " g{}b{}", n, l );
  }
  os << ")));\n";
}

void smt_constraints( std::ostream& os, node const& n, uint32_t const& max_depth, uint32_t const& max_relative_depth )
{
  os << fmt::format( "\n;constraints for node {}\n", n );
  os << fmt::format( "(declare-const bufs{} Int)\n", n );
  /* special cases */
  if ( _ntk.fanout_size( n ) == 0 ) /* dangling */
  {
    os << fmt::format( "(assert (= bufs{} 0))\n", n );
    return;
  }
  else if ( _ntk.fanout_size( n ) == 1 )
  {
    /* single fanout: only sequencing constraint */
    if ( _external_ref_count[n] > 0 )
    {
      if ( _ps.assume.balance_pos )
      {
        os << fmt::format( "(assert (= bufs{} (- depth l{})))\n", n, n );
      }
      else
      {
        os << fmt::format( "(assert (= bufs{} 0))\n", n );
      }
    }
    else
    {
      node const& no = _fanouts[n].front().fanouts.front();
      os << fmt::format( "(assert (> l{} l{}))\n", no, n );
      os << fmt::format( "(assert (= bufs{} (- l{} l{} 1)))\n", n, no, n );
    }
    return;
  }
  else if ( _ntk.fanout_size( n ) <= _ps.assume.splitter_capacity )
  {
    /* only one splitter is needed */

    /* sequencing */
    std::vector<node> fos;
    foreach_fanout( n, [&]( auto const& no ){ 
      os << fmt::format( "(assert (>= l{} (+ l{} 2)))\n", no, n );
      fos.emplace_back( no );
    });

    if ( _external_ref_count[n] > 0 && _ps.assume.balance_pos )
    {
      os << fmt::format( "(assert (> depth l{}))\n", n );
      os << fmt::format( "(assert (= bufs{} (- depth l{})))\n", n, n );
    }      
    else
    {
      if ( fos.size() == 0 ) /* not balance PO; have multiple PO refs */
      {
        os << fmt::format( "(assert (= bufs{} 1))\n", n );
      }
      else if ( fos.size() == 1 ) /* not balance PO; have one gate fanout and PO ref(s) */
      {
        os << fmt::format( "(assert (= bufs{} (- l{} l{} 1)))\n", n, fos[0], n );
      }
      else if ( fos.size() >= 2 )
      {
        os << fmt::format( "(declare-const g{}maxfo Int)\n", n );/* the max level of fanouts */

        /* bound it -- just to hint the solver; should not matter if we have optimization objective */
        if ( _ps.assume.balance_pos )
          os << fmt::format( "(assert (<= g{}maxfo depth))\n", n );
        else
          os << fmt::format( "(assert (<= g{}maxfo {}))\n", n, max_depth );

        /* taking the max (lower bounded by the max) */
        for ( auto const& no : fos )
          os << fmt::format( "(assert (>= g{}maxfo l{}))\n", n, no );

        os << fmt::format( "(assert (= bufs{} (- g{}maxfo l{} 1)))\n", n, n, n );
      }
    }
    return;
  }

  /* sequencing & range constraints */
  foreach_fanout( n, [&]( auto const& no ){
    os << fmt::format( "(assert (<= l{} (+ l{} {})))\n", no, n, max_relative_depth );
    os << fmt::format( "(assert (>= l{} (+ l{} 2)))\n", no, n );
  });

  /* PO branching */
  if ( _external_ref_count[n] > 0 )
  {
    os << fmt::format( "(assert (>= depth (+ l{} {})))\n", n, num_splitter_levels( n ) );
  }

  uint32_t l = max_relative_depth;
  bool add_po_refs = false;
  if ( _external_ref_count[n] > 0 && _ps.assume.balance_pos )
  {
    add_po_refs = true;
    os << fmt::format( "(assert (<= depth (+ l{} {})))\n", n, l-1 );
  }

  /* initial case */
  os << fmt::format( "(declare-const g{}e{} Int)\n", n, l ); // edges at relative depth l
  os << fmt::format( "(assert (= g{}e{} (+", n, l );
  foreach_fanout( n, [&]( auto const& no ){
    os << fmt::format( " (ite (= (- l{} l{}) {}) 1 0)", no, n, l );
  });
  if ( add_po_refs ) os << fmt::format( " (ite (= (+ l{} {}) depth) {} 0)", n, l-1, _external_ref_count[n] );
  os << ")))\n";

  /* general case */
  for ( --l; l > 0; --l )
  {
    os << fmt::format( "(declare-const g{}b{} Int)\n", n, l ); // buffers at relative depth l
    /* g{n}b{l} = ceil( g{n}e{l+1} / s_b ) --> s_b * ( g{n}b{l} - 1 ) < g{n}e{l+1} <= s_b * g{n}b{l} */
    os << fmt::format( "(assert (< (* {} (- g{}b{} 1)) g{}e{}))\n", _ps.assume.splitter_capacity, n, l, n, l+1 );
    os << fmt::format( "(assert (<= g{}e{} (* {} g{}b{})))\n", n, l+1, _ps.assume.splitter_capacity, n, l );
    
    os << fmt::format( "(declare-const g{}e{} Int)\n", n, l ); // edges at relative depth l
    os << fmt::format( "(assert (= g{}e{} (+", n, l ); 
    foreach_fanout( n, [&]( auto const& no ){
      os << fmt::format( " (ite (= (- l{} l{}) {}) 1 0)", no, n, l );
    });
    if ( add_po_refs ) os << fmt::format( " (ite (= (+ l{} {}) depth) {} 0)", n, l-1, _external_ref_count[n] );
    os << fmt::format( " g{}b{})))\n", n, l );
  }

  /* end of loop */
  os << fmt::format( "(assert (= g{}e1 1))\n", n ); // legal
  os << fmt::format( "(assert (= bufs{} (+", n );
  for ( l = max_relative_depth - 1; l > 0; --l )
  {
    os << fmt::format( " g{}b{}", n, l );
  }
  os << ")));\n";
}

#else // LP + Bool formulation
void dump_smt_model( std::ostream& os = std::cout )
{
  //os << "(set-logic QF_LRA)\n";
  /* hard assumptions to bound the number of variables */
  uint32_t const max_depth = depth() + 3;
  count_buffers();
  uint32_t const upper_bound = num_buffers();

  /* declare variables for each gate at each level */
  _ntk.foreach_gate( [&]( auto const& n ){
    std::vector<std::string> vars;
    for ( auto l = 1u; l <= max_depth; ++l )
    {
      vars.emplace_back( fmt::format( "g{}l{}", n, l ) );
      os << fmt::format( "(declare-const g{}l{} Bool)\n", n, l );
    }
    os << fmt::format( "(assert ((_ at-most 1) {}))\n", fmt::join( vars, " " ) );
    os << fmt::format( "(assert ((_ at-least 1) {}))\n", fmt::join( vars, " " ) );
  });

  /* If we branch but not balance PIs, they are free as gates */
  if ( _ps.assume.branch_pis && !_ps.assume.balance_pis )
  {
    _ntk.foreach_pi( [&]( auto const& n ){
      std::vector<std::string> vars;
      for ( auto l = 0u; l <= max_depth; ++l )
      {
        vars.emplace_back( fmt::format( "g{}l{}", n, l ) );
        os << fmt::format( "(declare-const g{}l{} Bool)\n", n, l );
      }
      os << fmt::format( "(assert ((_ at-most 1) {}))\n", fmt::join( vars, " " ) );
      os << fmt::format( "(assert ((_ at-least 1) {}))\n", fmt::join( vars, " " ) );
    });
  }

  /* variable for depth (max level) */
  if ( _ps.assume.balance_pos )
  {
    os << "(declare-const depth Real)\n";
    std::set<node> po_nodes;
    _ntk.foreach_po( [&]( auto const& f ){
      if ( !_ntk.is_pi( _ntk.get_node( f ) ) )
        po_nodes.insert( _ntk.get_node( f ) );
    });
    std::vector<std::string> front;
    front.emplace_back( "(assert (= depth " );
    for ( auto l = max_depth; l > 0; --l )
    {
      std::vector<std::string> n_at_l;
      for ( auto const& n : po_nodes )
        n_at_l.emplace_back( fmt::format( "g{}l{}", n, l ) );
      front.emplace_back( fmt::format( "(ite (or {}) {}", fmt::join( n_at_l, " " ), l ) );
    }
    std::string closings( max_depth - 1, ')' );
    os << fmt::format( "{} 0{})))\n", fmt::join( front, " " ), closings );
    // TODO: the depth variable does not take into account the PO branching buffers
    // TODO: insert inverters for POs
  }

  std::vector<std::string> bufs;
  /* constraints for each PI's fanout */
  /* PIs are totally ignored if we don't even branch them */
  if ( _ps.assume.branch_pis )
  {
    /* If PIs are balanced, they are always at level 0 so we don't have variables for them */
    if ( _ps.assume.balance_pis )
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        smt_constraints_balanced_pis( os, n, max_depth );
        bufs.emplace_back( fmt::format( "bufs{}", n ) );
      });
    }
    else
    {
      _ntk.foreach_pi( [&]( auto const& n ){
        smt_constraints( os, n, max_depth );
        bufs.emplace_back( fmt::format( "bufs{}", n ) );
      });
    }
  }

  /* constraints for each gate's fanout */
  _ntk.foreach_gate( [&]( auto const& n ){
    smt_constraints( os, n, max_depth );
    bufs.emplace_back( fmt::format( "bufs{}", n ) );
  });

  os << "\n(declare-const total Real)\n";
  os << fmt::format( "(assert (= total (+ {})))\n", fmt::join( bufs, " " ) );
  os << fmt::format( "(assert (<= total {}))\n", upper_bound );
  os << "(check-sat)\n(get-value (total))\n(exit)\n";
}

void smt_constraints( std::ostream& os, node const& n, uint32_t const& max_depth )
{
  os << fmt::format( "\n;constraints for gate {}\n", n );
  os << fmt::format( "(declare-const bufs{} Real)\n", n );

  /* special cases */
  if ( _ntk.fanout_size( n ) == 0 ) /* dangling */
  {
    os << fmt::format( "(assert (= bufs{} 0))\n", n );
    return;
  }
  else if ( _ntk.fanout_size( n ) == 1 ) /* single fanout */
  {
    if ( _external_ref_count[n] > 0 ) /* single PO fanout */
    {
      if ( _ps.assume.balance_pos )
      {
        /* #bufs = depth - l_n */
        std::vector<std::string> ln;
        for ( auto l = _ntk.is_pi( n ) ? 0u : 1u; l <= max_depth; ++l )
          ln.emplace_back( fmt::format( "(ite g{}l{} {} 0)", n, l, l ) );
        os << fmt::format( "(assert (= bufs{} (- depth (+ {}))))\n", n, fmt::join( ln, " " ) );
      }
      else
      {
        os << fmt::format( "(assert (= bufs{} 0))\n", n );
      }
    }
    else /* single gate fanout */
    {
      node const& no = _fanouts[n].front().fanouts.front();

      /* sequencing: l_no >= l_n + 1 */
      for ( auto l = _ntk.is_pi( n ) ? 0u : 1u; l < max_depth; ++l )
      {
        std::vector<std::string> lno;
        for ( auto l2 = l + 1; l2 <= max_depth; ++l2 )
          lno.emplace_back( fmt::format( "g{}l{}", no, l2 ) );
        os << fmt::format( "(assert (=> g{}l{} (or {})))\n", n, l, fmt::join( lno, " " ) );
      }
      os << fmt::format( "(assert (not g{}l{}))\n", n, max_depth );

      /* #bufs = l_no - l_n - 1 */
      std::vector<std::string> rd;
      for ( auto l = _ntk.is_pi( n ) ? 0u : 1u; l < max_depth; ++l )
      {
        for ( auto l2 = l + 1; l2 <= max_depth; ++l2 )
          rd.emplace_back( fmt::format( "(ite (and g{}l{} g{}l{}) {} 0)", n, l, no, l2, l2 - l - 1 ) );
      }
      os << fmt::format( "(assert (= bufs{} (+ {})))\n", n, fmt::join( rd, " " ) );
    }
    return;
  }
  else if ( _ntk.fanout_size( n ) <= _ps.assume.splitter_capacity ) /* only one splitter is needed */
  {
    std::vector<node> fos;
    foreach_fanout( n, [&]( auto const& no ){ 
      fos.emplace_back( no );
    });

    /* sequencing: l_no >= l_n + 2 */
    for ( auto const& no : fos )
    { 
      for ( auto l = _ntk.is_pi( n ) ? 0u : 1u; l < max_depth - 1; ++l )
      {
        std::vector<std::string> lno;
        for ( auto l2 = l + 2; l2 <= max_depth; ++l2 )
          lno.emplace_back( fmt::format( "g{}l{}", no, l2 ) );
        os << fmt::format( "(assert (=> g{}l{} (or {})))\n", n, l, fmt::join( lno, " " ) );
      }
    }
    os << fmt::format( "(assert (not g{}l{}))\n", n, max_depth );
    if ( fos.size() > 0 ) os << fmt::format( "(assert (not g{}l{}))\n", n, max_depth - 1 );

    if ( _external_ref_count[n] > 0 && _ps.assume.balance_pos )
    {
      /* #bufs = depth - l_n */
      std::vector<std::string> ln;
      for ( auto l = _ntk.is_pi( n ) ? 0u : 1u; l < max_depth; ++l )
        ln.emplace_back( fmt::format( "(ite g{}l{} {} 0)", n, l, l ) );
      os << fmt::format( "(assert (= bufs{} (- depth (+ {}))))\n", n, fmt::join( ln, " " ) );
    }      
    else
    {
      if ( fos.size() == 0 )
      {
        /* #bufs = 1 (splitter for POs) */
        os << fmt::format( "(assert (= bufs{} 1))\n", n );
      }
      else if ( fos.size() == 1 )
      {
        /* #bufs = l_no - l_n - 1 */
        std::vector<std::string> rd;
        for ( auto l = _ntk.is_pi( n ) ? 0u : 1u; l < max_depth - 1; ++l )
        {
          for ( auto l2 = l + 2; l2 <= max_depth; ++l2 )
            rd.emplace_back( fmt::format( "(ite (and g{}l{} g{}l{}) {} 0)", n, l, fos[0], l2, l2 - l - 1 ) );
        }
        os << fmt::format( "(assert (= bufs{} (+ {})))\n", n, fmt::join( rd, " " ) );
      }
      else //( fos.size() >= 2 )
      {
        /* #bufs = max( l_no ) - l_n - 1 */
        /* use a Real to represent level of l */
        os << fmt::format( "(declare-const l{} Real)\n", n );
        std::vector<std::string> ln;
        for ( auto l = _ntk.is_pi( n ) ? 0u : 1u; l < max_depth; ++l )
          ln.emplace_back( fmt::format( "(ite g{}l{} {} 0)", n, l, l ) );
        os << fmt::format( "(assert (= l{} (+ {})))\n", n, fmt::join( ln, " " ) );

        std::vector<std::string> front;
        front.emplace_back( fmt::format( "(assert (= bufs{} ", n ) );
        for ( auto l = max_depth; l >= 2; --l )
        {
          std::vector<std::string> lno;
          for ( auto const& no : fos )
            lno.emplace_back( fmt::format( "g{}l{}", no, l ) );
          front.emplace_back( fmt::format( "(ite (or {}) (- {} l{})", fmt::join( lno, " " ), l - 1, n ) );
        }
        std::string closings( max_depth - 2, ')' );
        os << fmt::format( "{} 0{})))\n", fmt::join( front, " " ), closings );
      }
    }
    return;
  }

  /* sequencing constraints: l_no >= l_n + 2 */
  foreach_fanout( n, [&]( auto const& no ){
    for ( auto l = _ntk.is_pi( n ) ? 0u : 1u; l < max_depth - 1; ++l )
    {
      std::vector<std::string> lno;
      for ( auto l2 = l + 2; l2 <= max_depth; ++l2 )
        lno.emplace_back( fmt::format( "g{}l{}", no, l2 ) );
      os << fmt::format( "(assert (=> g{}l{} (or {})))\n", n, l, fmt::join( lno, " " ) );
    }
  });
  os << fmt::format( "(assert (not g{}l{}))\n", n, max_depth );

  /* initial case */
  uint32_t rd, lowest_level = _ntk.is_pi( n ) ? 0 : 1;
  bool add_po_refs = false;
  if ( _external_ref_count[n] > 0 )
  {
    if ( _ps.assume.balance_pos )
    {
      add_po_refs = true;
      rd = max_depth + 1 - lowest_level;
      os << fmt::format( "(declare-const g{}e{} Real)\n", n, rd ); // edges at realtive depth rd
      os << fmt::format( "(assert (= g{}e{} (ite (and g{}l{} (= depth {})) {} 0)))\n", n, rd, n, lowest_level, rd - 1, _external_ref_count[n] );
    }
    else
    {
      assert( false );
      // TODO: PO branching when not balanced
    }
  }
  else
  {
    rd = max_depth - lowest_level;
    os << fmt::format( "(declare-const g{}e{} Real)\n", n, rd ); // edges at realtive depth rd
    os << fmt::format( "(assert (= g{}e{} (+", n, rd ); 
    foreach_fanout( n, [&]( auto const& no ){
      os << fmt::format( " (ite (and g{}l{} g{}l{}) 1 0)", n, lowest_level, no, lowest_level + rd );
    });
    os << ")))\n";
  }

  /* general case */
  std::vector<std::string> buffer_counters;
  for ( --rd; rd > 0; --rd )
  {
    buffer_counters.emplace_back( fmt::format( "g{}b{}", n, rd ) );
    os << fmt::format( "(declare-const {} Real)\n", buffer_counters.back() ); // buffers at realtive depth rd

    /* g{n}b{rd} = ceil( g{n}e{rd+1} / s_b ) --> s_b * ( g{n}b{rd} - 1 ) < g{n}e{rd+1} <= s_b * g{n}b{rd} */
    os << fmt::format( "(assert (< (* {} (- g{}b{} 1)) g{}e{}))\n", _ps.assume.splitter_capacity, n, rd, n, rd+1 );
    os << fmt::format( "(assert (<= g{}e{} (* {} g{}b{})))\n", n, rd+1, _ps.assume.splitter_capacity, n, rd );

    os << fmt::format( "(declare-const g{}e{} Real)\n", n, rd ); // edges at realtive depth rd
    os << fmt::format( "(assert (= g{}e{} (+", n, rd ); 
    foreach_fanout( n, [&]( auto const& no ){
      std::vector<std::string> at_rd;
      for ( auto l = lowest_level; l <= max_depth - rd; ++l )
        at_rd.emplace_back( fmt::format( "(and g{}l{} g{}l{})", n, l, no, l + rd ) );
      os << fmt::format( " (ite (or {}) 1 0)", fmt::join( at_rd, " " ) );
    });
    if ( add_po_refs ) os << fmt::format( " (ite (= depth {}) {} 0)", rd-1, _external_ref_count[n] );
    os << fmt::format( " g{}b{})))\n", n, rd );
  }

  /* end of loop */
  os << fmt::format( "(assert (= g{}e1 1))\n", n ); // legal
  os << fmt::format( "(assert (= bufs{} (+ {})))\n", n, fmt::join( buffer_counters, " " ) );
}

void smt_constraints_balanced_pis( std::ostream& os, node const& n, uint32_t const& max_depth )
{
  assert( _ntk.is_pi( n ) );
  assert( _ps.assume.branch_pis && _ps.assume.balance_pis );

  os << fmt::format( "\n;constraints for PI node {}\n", n );
  os << fmt::format( "(declare-const bufs{} Real)\n", n );
  /* special cases */
  if ( _ntk.fanout_size( n ) == 0 ) /* dangling */
  {
    os << fmt::format( "(assert (= bufs{} 0))\n", n );
    return;
  }
  else if ( _ntk.fanout_size( n ) == 1 ) /* single fanout */
  {
    if ( _external_ref_count[n] > 0 ) /* single PO fanout: #bufs = depth */
    {
      if ( _ps.assume.balance_pos )
      {
        os << fmt::format( "(assert (= bufs{} depth))\n", n );
      }
      else
      {
        os << fmt::format( "(assert (= bufs{} 0))\n", n );
      }
    }
    else /* single gate fanout: #bufs = l_no - 1 */
    {
      node const& no = _fanouts[n].front().fanouts.front();
      std::vector<std::string> lno;
      for ( auto l = 1u; l <= max_depth; ++l )
        lno.emplace_back( fmt::format( "(ite g{}l{} {} 0)", no, l, l - 1 ) );
      os << fmt::format( "(assert (= bufs{} (+ {})))\n", n, fmt::join( lno, " " ) );
    }
    return;
  }
  else if ( _ntk.fanout_size( n ) <= _ps.assume.splitter_capacity ) /* only one splitter is needed */
  {
    /* sequencing: l_no != 1 (+ collect fanout gates in a vector) */
    std::vector<node> fos;
    foreach_fanout( n, [&]( auto const& no ){ 
      os << fmt::format( "(assert (not g{}l1))\n", no );
      fos.emplace_back( no );
    });

    if ( _external_ref_count[n] > 0 && _ps.assume.balance_pos )
    {
      os << fmt::format( "(assert (= bufs{} depth))\n", n );
    }      
    else
    {
      if ( fos.size() == 0 )
      {
        /* #bufs = 1 (splitter for POs) */
        os << fmt::format( "(assert (= bufs{} 1))\n", n );
      }
      else if ( fos.size() == 1 )
      {
        /* #bufs = l_no - 1 */
        std::vector<std::string> lno;
        for ( auto l = 2u; l <= max_depth; ++l )
          lno.emplace_back( fmt::format( "(ite g{}l{} {} 0)", fos[0], l, l - 1 ) );
        os << fmt::format( "(assert (= bufs{} (+ {})))\n", n, fmt::join( lno, " " ) );
      }
      else //( fos.size() >= 2 )
      {
        /* #bufs = max( l_no - 1 ) */
        std::vector<std::string> front;
        front.emplace_back( fmt::format( "(assert (= bufs{} ", n ) );
        for ( auto l = max_depth; l >= 2; --l )
        {
          std::vector<std::string> lno;
          for ( auto const& no : fos )
            lno.emplace_back( fmt::format( "g{}l{}", no, l ) );
          front.emplace_back( fmt::format( "(ite (or {}) {}", fmt::join( lno, " " ), l - 1 ) );
        }
        std::string closings( max_depth - 2, ')' );
        os << fmt::format( "{} 0{})))\n", fmt::join( front, " " ), closings );
      }
    }
    return;
  }

  /* sequencing constraints always hold because n is PI at level 0 */

  /* initial case */
  uint32_t l = max_depth;
  bool add_po_refs = false;
  if ( _external_ref_count[n] > 0 )
  {
    if ( _ps.assume.balance_pos )
    {
      add_po_refs = true;
      l = max_depth + 1;
      os << fmt::format( "(declare-const g{}e{} Real)\n", n, l ); // edges at realtive depth l (i.e. at level l because n is PI at level 0)
      os << fmt::format( "(assert (= g{}e{} (ite (= depth {}) {} 0)))\n", n, l, l-1, _external_ref_count[n] );
    }
    else
    {
      assert( false );
      // TODO: PO branching when not balanced
    }
  }
  else
  {
    os << fmt::format( "(declare-const g{}e{} Real)\n", n, l ); // edges at realtive depth l (i.e. at level l because n is PI at level 0)
    os << fmt::format( "(assert (= g{}e{} (+", n, l ); 
    foreach_fanout( n, [&]( auto const& no ){
      os << fmt::format( " (ite g{}l{} 1 0)", no, l );
    });
    os << ")))\n";
  }

  /* general case */
  std::vector<std::string> buffer_counters;
  for ( --l; l > 0; --l )
  {
    buffer_counters.emplace_back( "g{}b{}", n, l );
    os << fmt::format( "(declare-const {} Real)\n", buffer_counters.back() ); // buffers at realtive depth l

    /* g{n}b{l} = ceil( g{n}e{l+1} / s_b ) --> s_b * ( g{n}b{l} - 1 ) < g{n}e{l+1} <= s_b * g{n}b{l} */
    os << fmt::format( "(assert (< (* {} (- g{}b{} 1)) g{}e{}))\n", _ps.assume.splitter_capacity, n, l, n, l+1 );
    os << fmt::format( "(assert (<= g{}e{} (* {} g{}b{})))\n", n, l+1, _ps.assume.splitter_capacity, n, l );
    os << fmt::format( "(declare-const g{}e{} Real)\n", n, l ); // edges at realtive depth l
    os << fmt::format( "(assert (= g{}e{} (+", n, l ); 
    foreach_fanout( n, [&]( auto const& no ){
      os << fmt::format( " (ite g{}l{} 1 0)", no, l );
    });
    if ( add_po_refs ) os << fmt::format( " (ite (= depth {}) {} 0)", l-1, _external_ref_count[n] );
    os << fmt::format( " g{}b{})))\n", n, l );
  }

  /* end of loop */
  os << fmt::format( "(assert (= g{}e1 1))\n", n ); // legal
  os << fmt::format( "(assert (= bufs{} (+ {})))\n", n, fmt::join( buffer_counters, " " ) );
}
#endif

template<class Fn>
void foreach_fanout( node const& n, Fn&& fn )
{
  for ( auto it1 = _fanouts[n].begin(); it1 != _fanouts[n].end(); ++it1 )
  {
    for ( auto it2 = it1->fanouts.begin(); it2 != it1->fanouts.end(); ++it2 )
    {
      fn( *it2 );
    }
  }
}

void parse_z3_result()
{
  std::ifstream fin( "sol.txt", std::ifstream::in );
  assert( fin.is_open() );

  /* parsing */
  std::string line;
  std::getline( fin, line ); /* first line: "sat" */
  assert( line == "sat" );
  std::getline( fin, line ); /* second line: "((total <>)" */
  uint32_t total = std::stoi( line.substr( 8, line.find_first_of( ')' ) - 8 ) );
  std::getline( fin, line ); /* third line: " (depth <>))" */
  uint32_t depth = std::stoi( line.substr( 8, line.find_first_of( ')' ) - 8 ) );
  std::cout << "[i] total = " << total << ", depth = " << depth << "\n";

  while ( std::getline( fin, line ) ) /* remaining lines: "((l<> <>)" or " (l<> <>)" or " (l<> <>))" */
  {
    line = line.substr( line.find( 'l' ) + 1 );
    uint32_t n = std::stoi( line.substr( 0, line.find( ' ' ) ) );
    line = line.substr( line.find( ' ' ) + 1 );
    _levels[n] = std::stoi( line.substr( 0, line.find_first_of( ')' ) ) );
  }
  _outdated = true;
}
