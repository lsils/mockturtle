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
  \file network_manager.hpp
  \brief Manager for network types

  \author Alessandro tempia Calvino
*/

#pragma once
#include <string>

#include <alice/alice.hpp>
#include <fmt/format.h>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/block.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/views/binding_view.hpp>
#include <mockturtle/views/cell_view.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/names_view.hpp>
#include <mockturtle/utils/standard_cell.hpp>

namespace alice
{
enum network_manager_type
{
  EMPTY,
  AIG,
  XAG,
  MIG,
  XMG,
  KLUT,
  MAPPED,
  MAPPED2
};

class network_manager
{
public:
  using aig_names = mockturtle::names_view<mockturtle::aig_network>;
  using aig_ntk = std::shared_ptr<aig_names>;
  using mig_names = mockturtle::names_view<mockturtle::mig_network>;
  using mig_ntk = std::shared_ptr<mig_names>;
  using xag_names = mockturtle::names_view<mockturtle::xag_network>;
  using xag_ntk = std::shared_ptr<xag_names>;
  using xmg_names = mockturtle::names_view<mockturtle::xmg_network>;
  using xmg_ntk = std::shared_ptr<xmg_names>;
  using klut_names = mockturtle::names_view<mockturtle::klut_network>;
  using klut_ntk = std::shared_ptr<klut_names>;
  using mapped_names = mockturtle::cell_view<mockturtle::names_view<mockturtle::block_network>>;
  using mapped_ntk = std::shared_ptr<mapped_names>;

public:
  explicit network_manager()
    : current_type( network_manager_type::EMPTY )
  {}

public:
  network_manager_type const& get_current_type() const
  {
    return current_type;
  }

  /* return previous type before deleting */
  network_manager_type delete_current()
  {
    network_manager_type previous_type = current_type;
    delete_current_local();
    return previous_type;
  }

  aig_names& add_aig()
  {
    delete_current();
    aig = std::make_shared<aig_names>( aig_names{} );
    current_type = network_manager_type::AIG;
    return *aig;
  }

  aig_names& get_aig()
  {
    return *aig;
  }

  void load_aig( aig_names& aig_to_load )
  {
    delete_current();
    aig = std::make_shared<aig_names>( aig_to_load );
    current_type = network_manager_type::AIG;
  }

  mig_names& add_mig()
  {
    delete_current();
    mig = std::make_shared<mig_names>( mig_names{} );
    current_type = network_manager_type::MIG;
    return *mig;
  }

  mig_names& get_mig()
  {
    return *mig;
  }

  void load_mig( mig_names& mig_to_load )
  {
    delete_current();
    mig = std::make_shared<mig_names>( mig_to_load );
    current_type = network_manager_type::MIG;
  }

  xag_names& add_xag()
  {
    delete_current();
    xag = std::make_shared<xag_names>( xag_names{} );
    current_type = network_manager_type::XAG;
    return *xag;
  }

  xag_names& get_xag()
  {
    return *xag;
  }

  void load_xag( xag_names& xag_to_load )
  {
    delete_current();
    xag = std::make_shared<xag_names>( xag_to_load );
    current_type = network_manager_type::XAG;
  }

  xmg_names& add_xmg()
  {
    delete_current();
    xmg = std::make_shared<xmg_names>( xmg_names{} );
    current_type = network_manager_type::XMG;
    return *xmg;
  }

  xmg_names& get_xmg()
  {
    return *xmg;
  }

  void load_xmg( xmg_names& xmg_to_load )
  {
    delete_current();
    xmg = std::make_shared<xmg_names>( xmg_to_load );
    current_type = network_manager_type::XMG;
  }

  klut_names& add_klut()
  {
    delete_current();
    klut = std::make_shared<klut_names>( klut_names{} );
    current_type = network_manager_type::KLUT;
    return *klut;
  }

  klut_names& get_klut()
  {
    return *klut;
  }

  void load_klut( klut_names& klut_to_load )
  {
    delete_current();
    klut = std::make_shared<klut_names>( klut_to_load );
    current_type = network_manager_type::KLUT;
  }

  mapped_names& add_mapped( std::vector<mockturtle::standard_cell> const& cells )
  {
    delete_current();
    mapped = std::make_shared<mapped_names>( mapped_names{ cells } );
    current_type = network_manager_type::MAPPED;
    return *mapped;
  }

  mapped_names& get_mapped()
  {
    return *mapped;
  }

  void load_mapped( mapped_names& mapped_to_load )
  {
    delete_current();
    mapped = std::make_shared<mapped_names>( mapped_to_load );
    current_type = network_manager_type::MAPPED;
  }

  const std::string describe() const
  {
    return describe_local();
  }

  const std::string stats() const
  {
    return stats_local();
  }

  const std::tuple<std::string, uint32_t, uint32_t, uint32_t> log() const
  {
    return log_local();
  }

private:
  void delete_current_local()
  {
    switch ( current_type )
    {
    case network_manager_type::AIG:
      aig.reset();
      break;
    case network_manager_type::MIG:
      mig.reset();
      break;
    case network_manager_type::XAG:
      xag.reset();
      break;
    case network_manager_type::XMG:
      xmg.reset();
      break;
    case network_manager_type::KLUT:
      klut.reset();
      break;
    case network_manager_type::MAPPED:
      mapped.reset();
      break;
    default:
      break;
    }
    current_type = network_manager_type::EMPTY;
  }

  const std::string describe_local() const
  {
    std::string name, type;
    uint32_t input, output, gates;
    switch ( current_type )
    {
    case network_manager_type::AIG:
      name = aig->get_network_name();
      type = "AIG";
      input = aig->num_pis();
      output = aig->num_pos();
      gates = aig->num_gates();
      break;
    case network_manager_type::MIG:
      name = mig->get_network_name();
      type = "MIG";
      input = mig->num_pis();
      output = mig->num_pos();
      gates = mig->num_gates();
      break;
    case network_manager_type::XAG:
      name = xag->get_network_name();
      type = "XAG";
      input = xag->num_pis();
      output = xag->num_pos();
      gates = xag->num_gates();
      break;
    case network_manager_type::XMG:
      name = xmg->get_network_name();
      type = "XMG";
      input = xmg->num_pis();
      output = xmg->num_pos();
      gates = xmg->num_gates();
      break;
    case network_manager_type::KLUT:
      name = klut->get_network_name();
      type = "kLUT";
      input = klut->num_pis();
      output = klut->num_pos();
      gates = klut->num_gates();
      break;
    case network_manager_type::MAPPED:
      name = mapped->get_network_name();
      type = "Ntk";
      input = mapped->num_pis();
      output = mapped->num_pos();
      gates = mapped->num_gates();
      break;
    default:
      return "Empty network";
      break;
    }

    return fmt::format( "{} : {}  i/0 = {:5d}/{:5d}  gates = {:6d}",
                        name,
                        type,
                        input,
                        output,
                        gates );
  }

  const std::string stats_local() const
  {
    std::string name, type;
    uint32_t input, output, gates, depth;
    switch ( current_type )
    {      
    case network_manager_type::AIG:
      name = aig->get_network_name();
      type = "AIG";
      input = aig->num_pis();
      output = aig->num_pos();
      gates = aig->num_gates();
      depth = mockturtle::depth_view{ *aig }.depth();
      break;
    case network_manager_type::MIG:
      name = mig->get_network_name();
      type = "MIG";
      input = mig->num_pis();
      output = mig->num_pos();
      gates = mig->num_gates();
      depth = mockturtle::depth_view{ *mig }.depth();
      break;
    case network_manager_type::XAG:
      name = xag->get_network_name();
      type = "XAG";
      input = xag->num_pis();
      output = xag->num_pos();
      gates = xag->num_gates();
      depth = mockturtle::depth_view{ *xag }.depth();
      break;
    case network_manager_type::XMG:
      name = xmg->get_network_name();
      type = "XMG";
      input = xmg->num_pis();
      output = xmg->num_pos();
      gates = xmg->num_gates();
      depth = mockturtle::depth_view{ *xmg }.depth();
      break;
    case network_manager_type::KLUT:
      name = klut->get_network_name();
      type = "kLUT";
      input = klut->num_pis();
      output = klut->num_pos();
      gates = klut->num_gates();
      depth = mockturtle::depth_view{ *klut }.depth();
      break;
    case network_manager_type::MAPPED:
      return fmt::format( "{} : NTK  i/0 = {:5d}/{:5d}  gates = {:6d}  area = {:>8.2f}  delay = {:>8.2f}",
                          mapped->get_network_name(),
                          mapped->num_pis(),
                          mapped->num_pos(),
                          mapped->num_gates(),
                          mapped->compute_area(),
                          mapped->compute_worst_delay() );
    default:
      return "Empty network";
      break;
    }

    return fmt::format( "{} : {}  i/0 = {:5d}/{:5d}  gates = {:6d}  lev = {}",
                        name,
                        type,
                        input,
                        output,
                        gates,
                        depth );
  }

  const std::tuple<std::string, uint32_t, uint32_t, uint32_t> log_local() const
  {
    std::string name;
    uint32_t input = 0, output = 0, gates = 0;
    switch ( current_type )
    {
    case network_manager_type::AIG:
      name = aig->get_network_name();
      input = aig->num_pis();
      output = aig->num_pos();
      gates = aig->num_gates();
      break;
    case network_manager_type::MIG:
      name = mig->get_network_name();
      input = mig->num_pis();
      output = mig->num_pos();
      gates = mig->num_gates();
      break;
    case network_manager_type::XAG:
      name = xag->get_network_name();
      input = xag->num_pis();
      output = xag->num_pos();
      gates = xag->num_gates();
      break;
    case network_manager_type::XMG:
      name = xmg->get_network_name();
      input = xmg->num_pis();
      output = xmg->num_pos();
      gates = xmg->num_gates();
      break;
    case network_manager_type::KLUT:
      name = klut->get_network_name();
      input = klut->num_pis();
      output = klut->num_pos();
      gates = klut->num_gates();
      break;
    case network_manager_type::MAPPED:
      name = mapped->get_network_name();
      input = mapped->num_pis();
      output = mapped->num_pos();
      gates = mapped->num_gates();
    default:
      name = "Empty network";
      break;
    }

    return std::make_tuple( name, input, output, gates );
  }

private:
  network_manager_type current_type;
  aig_ntk aig{};
  mig_ntk mig{};
  xag_ntk xag{};
  xmg_ntk xmg{};
  klut_ntk klut{};
  mapped_ntk mapped{};
};

ALICE_ADD_STORE( network_manager, "ntk", "n", "Network", "Networks" )

ALICE_DESCRIBE_STORE( network_manager, man )
{
  return man.describe();
}

ALICE_PRINT_STORE_STATISTICS( network_manager, os, man )
{
  os << man.stats() << std::endl;
}

ALICE_LOG_STORE_STATISTICS( network_manager, man )
{
  auto [name, inputs, outputs, gates ] = man.log();
  return { { "name", name },
           { "inputs", inputs },
           { "outputs", outputs },
           { "gates", gates } };
}

} // namespace alice