/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2024  EPFL
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
  \file exact_library.hpp
  \brief Store for databases of exact structures

  \author Alessandro tempia Calvino
*/

#pragma once
#include <string>

#include <alice/alice.hpp>
#include <fmt/format.h>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xmg_npn.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/utils/tech_library.hpp>

namespace alice
{

class exact_library_manager
{
private:
  using resyn_aig_fn = mockturtle::xag_npn_resynthesis<mockturtle::aig_network, mockturtle::aig_network, mockturtle::xag_npn_db_kind::aig_complete>;
  using resyn_xag_fn = mockturtle::xag_npn_resynthesis<mockturtle::xag_network, mockturtle::xag_network, mockturtle::xag_npn_db_kind::xag_complete>;
  using resyn_mig_fn = mockturtle::mig_npn_resynthesis;
  using resyn_xmg_fn = mockturtle::xmg_npn_resynthesis;
  using aig_lib_type = mockturtle::exact_library<mockturtle::aig_network>;
  using mig_lib_type = mockturtle::exact_library<mockturtle::mig_network>;
  using xag_lib_type = mockturtle::exact_library<mockturtle::xag_network>;
  using xmg_lib_type = mockturtle::exact_library<mockturtle::xmg_network>;

public:
  exact_library_manager() {};

public:
  aig_lib_type& get_aig_library()
  {
    if ( aig_lib == nullptr )
    {
      /* initialize the library */
      aig_lib = std::make_shared<aig_lib_type>( aig_lib_type{ resyn_aig_fn{} } );
    }

    return *aig_lib;
  }

  mig_lib_type& get_mig_library()
  {
    if ( mig_lib == nullptr )
    {
      /* initialize the library */
      mig_lib = std::make_shared<mig_lib_type>( mig_lib_type{ resyn_mig_fn{ true } } );
    }

    return *mig_lib;
  }

  xag_lib_type& get_xag_library()
  {
    if ( xag_lib == nullptr )
    {
      /* initialize the library */
      xag_lib = std::make_shared<xag_lib_type>( xag_lib_type{ resyn_xag_fn{} } );
    }

    return *xag_lib;
  }

  xmg_lib_type& get_xmg_library()
  {
    if ( xmg_lib == nullptr )
    {
      /* initialize the library */
      xmg_lib = std::make_shared<xmg_lib_type>( xmg_lib_type{ resyn_xmg_fn{} } );
    }

    return *xmg_lib;
  }

private:
  std::shared_ptr<aig_lib_type> aig_lib{};
  std::shared_ptr<mig_lib_type> mig_lib{};
  std::shared_ptr<xag_lib_type> xag_lib{};
  std::shared_ptr<xmg_lib_type> xmg_lib{};
};

} // namespace alice