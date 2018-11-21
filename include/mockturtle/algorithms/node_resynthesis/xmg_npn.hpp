/* mockturtle: C++ logic network library
 * Copyright (C) 2018  EPFL
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
  \file xmg_npn.hpp
  \brief Replace with size-optimum XMGs from NPN

  \author Mathias Soeken
  \author Zhufei Chu
*/

#pragma once

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <stack>

#include <kitty/dynamic_truth_table.hpp>
#include <kitty/npn.hpp>
#include <kitty/print.hpp>

#include "../../algorithms/cleanup.hpp"
#include "../../io/write_bench.hpp"
#include "../../networks/xmg.hpp"
#include "../../traits.hpp"
#include "../../views/topo_view.hpp"

namespace mockturtle
{

/*! \brief Resynthesis function based on pre-computed size-optimum MIGs.
 *
 * This resynthesis function can be passed to ``node_resynthesis``,
 * ``cut_rewriting``, and ``refactoring``.  It will produce an XMG based on
 * pre-computed size-optimum XMGs with up to at most 4 variables.
 * Consequently, the nodes' fan-in sizes in the input network must not exceed
 * 4.
 *
   \verbatim embed:rst
  
   Example
   
   .. code-block:: c++
   
      const klut_network klut = ...;
      xmg_npn_resynthesis resyn;
      const auto xmg = node_resynthesis<xmg_network>( klut, resyn );
   \endverbatim
 */
class xmg_npn_resynthesis
{
public:
  /*! \brief Default constructor.
   *
   */
  xmg_npn_resynthesis()
  {
    build_db();
  }

  template<typename LeavesIterator, typename Fn>
  void operator()( xmg_network& xmg, kitty::dynamic_truth_table const& function, LeavesIterator begin, LeavesIterator end, Fn&& fn )
  {
    assert( function.num_vars() <= 4 );
    const auto fe = kitty::extend_to( function, 4 );
    const auto config = kitty::exact_npn_canonization( fe );
    
    auto func_str = "0x" + kitty::to_hex( std::get<0>( config ) ); 
    const auto it = class2signal.find( func_str );
    assert( it != class2signal.end() );

    //const auto it = class2signal.find( static_cast<uint16_t>( std::get<0>( config )._bits[0] ) );

    std::vector<xmg_network::signal> pis( 4, xmg.get_constant( false ) );
    std::copy( begin, end, pis.begin() );

    std::vector<xmg_network::signal> pis_perm( 4 );
    auto perm = std::get<2>( config );
    for ( auto i = 0; i < 4; ++i )
    {
      pis_perm[i] = pis[perm[i]];
    }

    const auto& phase = std::get<1>( config );
    for ( auto i = 0; i < 4; ++i )
    {
      if ( ( phase >> perm[i] ) & 1 )
      {
        pis_perm[i] = !pis_perm[i];
      }
    }

    for ( auto const& po : it->second )
    {
      topo_view topo{db, po};
      auto f = cleanup_dangling( topo, xmg, pis_perm.begin(), pis_perm.end() ).front();

      if ( !fn( ( ( phase >> 4 ) & 1 ) ? !f : f ) )
      {
        return; /* quit */
      }
    }
  }

private:
  std::unordered_map<std::string, std::string> opt_xmgs;

  inline std::vector<std::string> split( const std::string& str, const std::string& sep )
  {
    std::vector<std::string> result;

    size_t last = 0;
    size_t next = 0;
    while ( ( next = str.find( sep, last ) ) != std::string::npos )
    {
      result.push_back( str.substr( last, next - last ) );
      last = next + 1;
    }
    result.push_back( str.substr( last ) );

    return result;
  }

  void load_optimal_xmgs( const unsigned& strategy )
  {
    std::vector<std::string> result;

    switch( strategy )
    {
      case 1:
        result = split( npn4_s, "\n" );
        break;

      case 2:
        result = split( npn4_sd, "\n" );
        break;
      
      case 3:
        result = split( npn4_ds, "\n" );
        break;

      default: break;
    }
    
    for( auto record : result )
    {
      auto p = split( record, " " );
      assert( p.size() == 2u );
      opt_xmgs.insert( std::make_pair( p[0], p[1] ) );
    }

  }

  std::vector<xmg_network::signal> create_xmg_from_str( const std::string& str, const std::vector<xmg_network::signal>& signals )
  {
    auto sig = signals;
    std::vector<xmg_network::signal> result;

    std::stack<int> polar;
    std::stack<xmg_network::signal> inputs;

    for( auto i = 0ul; i < str.size(); i++ )
    {
      // operators polarity
      if( str[i] == '[' || str[i] == '<' )
      {
        if( i == 0 )
        {
          polar.push( 0 );
        }
        else if( str[i - 1] == '!' )
        {
          polar.push( 1 );
        }
        else
        {
          polar.push( 0 );
        }
      }

      //input signals
      if( str[i] >= 'a' && str[i] <= 'd' ) 
      {
        inputs.push( sig[ str[i] - 'a' + 1 ] );

        polar.push( str[i - 1] == '!' ? 1 : 0 );
      }
      else if( str[i] == '0' )
      {
        inputs.push( sig[ 0 ] );

        polar.push( str[i - 1] == '!' ? 1 : 0 );
      }

      //create signals
      if( str[i] == '>' )
      {
        assert( inputs.size() >= 3u );
        auto x1 = inputs.top(); inputs.pop();
        auto x2 = inputs.top(); inputs.pop();
        auto x3 = inputs.top(); inputs.pop();

        assert( polar.size() >= 4u );
        auto p1 = polar.top(); polar.pop();
        auto p2 = polar.top(); polar.pop();
        auto p3 = polar.top(); polar.pop();
        
        auto p4 = polar.top(); polar.pop();
        
        inputs.push( db.create_maj( x1 ^ p1, x2 ^ p2, x3 ^ p3 ) ^ p4 );
        polar.push( 0 );
      }

      if( str[i] == ']' )
      {
        assert( inputs.size() >= 2u );
        auto x1 = inputs.top(); inputs.pop();
        auto x2 = inputs.top(); inputs.pop();

        assert( polar.size() >= 3u );
        auto p1 = polar.top(); polar.pop();
        auto p2 = polar.top(); polar.pop();
        
        auto p3 = polar.top(); polar.pop();
        
        inputs.push( db.create_xor( x1 ^ p1, x2 ^ p2 ) ^ p3 );
        polar.push( 0 );
      }
    }

    assert( !polar.empty() );
    auto po = polar.top(); polar.pop();
    db.create_po( inputs.top() ^ po );
    result.push_back( inputs.top() ^ po);
    return result;
  }

  void build_db()
  {
    std::vector<xmg_network::signal> signals;
    signals.push_back( db.get_constant( false ) );

    auto p = nodes.begin();
    for ( auto i = 0u; i < 4; ++i )
    {
      signals.push_back( db.create_pi() );
    }
    
    load_optimal_xmgs( 1 ); //size optimization

    for( const auto e : opt_xmgs )
    {
      class2signal.insert( std::make_pair( e.first, create_xmg_from_str( e.second, signals ) ) );
    }
  }


  xmg_network db;
  std::unordered_map<std::string, std::vector<xmg_network::signal>> class2signal;

  inline static const std::vector<uint16_t> classes{{0x1ee1, 0x1be4, 0x1bd8, 0x18e7, 0x17e8, 0x17ac, 0x1798, 0x1796, 0x178e, 0x177e, 0x16e9, 0x16bc, 0x169e, 0x003f, 0x0359, 0x0672, 0x07e9, 0x0693, 0x0358, 0x01bf, 0x6996, 0x0356, 0x01bd, 0x001f, 0x01ac, 0x001e, 0x0676, 0x01ab, 0x01aa, 0x001b, 0x07e1, 0x07e0, 0x0189, 0x03de, 0x035a, 0x1686, 0x0186, 0x03db, 0x0357, 0x01be, 0x1683, 0x0368, 0x0183, 0x03d8, 0x07e6, 0x0182, 0x03d7, 0x0181, 0x03d6, 0x167e, 0x016a, 0x007e, 0x0169, 0x006f, 0x0069, 0x0168, 0x0001, 0x019a, 0x036b, 0x1697, 0x0369, 0x0199, 0x0000, 0x169b, 0x003d, 0x036f, 0x0666, 0x019b, 0x0187, 0x03dc, 0x0667, 0x0003, 0x168e, 0x06b6, 0x01eb, 0x07e2, 0x017e, 0x07b6, 0x007f, 0x19e3, 0x06b7, 0x011a, 0x077e, 0x018b, 0x00ff, 0x0673, 0x01a8, 0x000f, 0x1696, 0x036a, 0x011b, 0x0018, 0x0117, 0x1698, 0x036c, 0x01af, 0x0016, 0x067a, 0x0118, 0x0017, 0x067b, 0x0119, 0x169a, 0x003c, 0x036e, 0x07e3, 0x017f, 0x03d4, 0x06f0, 0x011e, 0x037c, 0x012c, 0x19e6, 0x01ef, 0x16a9, 0x037d, 0x006b, 0x012d, 0x012f, 0x01fe, 0x0019, 0x03fc, 0x179a, 0x013c, 0x016b, 0x06f2, 0x03c0, 0x033c, 0x1668, 0x0669, 0x019e, 0x013d, 0x0006, 0x019f, 0x013e, 0x0776, 0x013f, 0x016e, 0x03c3, 0x3cc3, 0x033f, 0x166b, 0x016f, 0x011f, 0x035e, 0x0690, 0x0180, 0x03d5, 0x06f1, 0x06b0, 0x037e, 0x03c1, 0x03c5, 0x03c6, 0x01a9, 0x166e, 0x03cf, 0x03d9, 0x07bc, 0x01bc, 0x1681, 0x03dd, 0x03c7, 0x06f9, 0x0660, 0x0196, 0x0661, 0x0197, 0x0662, 0x07f0, 0x0198, 0x0663, 0x07f1, 0x0007, 0x066b, 0x033d, 0x1669, 0x066f, 0x01ad, 0x0678, 0x01ae, 0x0679, 0x067e, 0x168b, 0x035f, 0x0691, 0x0696, 0x0697, 0x06b1, 0x0778, 0x16ac, 0x06b2, 0x0779, 0x16ad, 0x01e8, 0x06b3, 0x0116, 0x077a, 0x01e9, 0x06b4, 0x19e1, 0x01ea, 0x06b5, 0x01ee, 0x06b9, 0x06bd, 0x06f6, 0x07b0, 0x07b1, 0x07b4, 0x07b5, 0x07f2, 0x07f8, 0x018f, 0x0ff0, 0x166a, 0x035b, 0x1687, 0x1689, 0x036d, 0x069f, 0x1699}};
  inline static const std::vector<uint16_t> nodes{{4, 222, 17, 24, 34, 41, 46, 56, 68, 76, 84, 96, 109, 116, 122, 127, 137, 142, 151, 157, 166, 173, 182, 188, 193, 199, 208, 214, 220, 227, 232, 239, 247, 256, 259, 264, 272, 278, 286, 293, 297, 300, 307, 312, 321, 328, 336, 344, 351, 355, 362, 372, 378, 384, 387, 389, 393, 398, 401, 408, 417, 421, 425, 433, 0, 439, 445, 451, 454, 459, 467, 472, 475, 477, 482, 486, 491, 498, 502, 506, 509, 517, 523, 526, 532, 537, 9, 545, 548, 335, 554, 560, 563, 568, 573, 576, 580, 583, 586, 594, 596, 599, 603, 605, 612, 616, 622, 627, 629, 630, 634, 638, 640, 644, 650, 657, 665, 669, 675, 677, 679, 686, 691, 696, 702, 708, 713, 718, 722, 728, 738, 747, 750, 755, 756, 761, 766, 770, 773, 778, 785, 789, 159, 797, 801, 803, 810, 812, 820, 827, 831, 836, 844, 853, 857, 862, 869, 876, 879, 887, 892, 900, 911, 915, 921, 927, 930, 938, 941, 951, 954, 960, 966, 971, 975, 977, 985, 991, 1003, 1007, 1011, 1014, 1020, 1027, 1030, 1037, 1039, 1041, 1044, 1049, 1053, 1060, 1066, 1070, 1073, 1077, 1082, 1093, 1096, 1100, 1107, 1112, 1119, 1124, 1135, 1138, 1141, 1147, 1148, 1152, 1159, 1166, 1175, 1178, 1186, 1191, 1194, 1202, 1205, 1213, 1221, 1229, 1231, 1237, 1, 2, 4, 6, 8, 11, 9, 10, 12, 7, 12, 14, 0, 2, 7, 8, 10, 19, 8, 10, 21, 18, 20, 23, 5, 6, 8, 2, 4, 6, 0, 26, 28, 1, 26, 28, 0, 31, 32, 6, 9, 28, 8, 29, 36, 7, 36, 38, 0, 8, 28, 0, 8, 43, 28, 43, 44, 0, 5, 8, 4, 7, 48, 0, 2, 8, 2, 6, 48, 50, 53, 54, 0, 4, 9, 0, 2, 59, 0, 2, 58, 7, 8, 62, 2, 5, 6, 61, 64, 66, 0, 6, 9, 1, 4, 8, 2, 71, 72, 29, 70, 74, 0, 4, 8, 2, 4, 7, 0, 3, 8, 79, 80, 82, 0, 7, 8, 2, 7, 86, 4, 6, 87, 0, 6, 8, 2, 4, 92, 88, 90, 95, 0, 2, 4, 1, 6, 98, 2, 4, 99, 8, 100, 103, 101, 102, 104, 9, 104, 106, 1, 4, 6, 0, 9, 110, 2, 8, 110, 29, 112, 114, 0, 3, 6, 4, 52, 118, 80, 118, 121, 0, 4, 6, 1, 8, 124, 0, 2, 9, 0, 4, 128, 6, 9, 130, 4, 6, 131, 128, 133, 134, 0, 2, 5, 3, 6, 78, 93, 138, 140, 0, 9, 28, 2, 4, 9, 0, 6, 147, 145, 146, 148, 4, 8, 71, 2, 4, 8, 28, 152, 155, 4, 6, 8, 0, 8, 159, 2, 5, 158, 2, 6, 83, 160, 163, 164, 1, 2, 6, 0, 3, 4, 8, 168, 170, 3, 6, 18, 1, 18, 174, 4, 9, 176, 5, 8, 176, 177, 178, 180, 2, 82, 110, 2, 83, 110, 82, 185, 186, 2, 7, 78, 99, 158, 190, 0, 5, 6, 0, 3, 194, 6, 8, 197, 4, 8, 52, 4, 7, 8, 2, 6, 200, 1, 202, 204, 0, 201, 206, 0, 6, 10, 6, 9, 10, 0, 211, 212, 6, 9, 98, 1, 80, 216, 0, 99, 218, 4, 6, 53, 3, 52, 222, 1, 52, 224, 3, 6, 170, 2, 8, 228, 8, 128, 231, 2, 6, 8, 3, 4, 8, 1, 234, 236, 1, 6, 146, 6, 146, 241, 0, 9, 242, 0, 240, 245, 2, 5, 8, 4, 6, 248, 1, 8, 250, 0, 9, 252, 251, 252, 254, 10, 131, 194, 4, 7, 248, 0, 6, 249, 79, 260, 262, 0, 4, 7, 6, 8, 266, 3, 6, 8, 18, 269, 270, 2, 7, 8, 4, 82, 275, 5, 80, 276, 2, 8, 266, 4, 8, 281, 2, 7, 282, 0, 281, 284, 5, 8, 28, 0, 4, 29, 110, 288, 290, 0, 2, 110, 8, 110, 294, 1, 6, 236, 128, 159, 298, 4, 6, 83, 2, 4, 303, 274, 302, 305, 6, 58, 128, 8, 159, 308, 129, 308, 310, 5, 6, 170, 2, 7, 170, 4, 8, 316, 1, 314, 318, 2, 6, 9, 0, 249, 322, 0, 110, 325, 8, 324, 327, 2, 9, 170, 4, 6, 171, 1, 6, 8, 330, 333, 334, 2, 5, 266, 6, 8, 338, 2, 8, 267, 0, 341, 342, 3, 4, 6, 0, 8, 110, 110, 347, 348, 4, 8, 170, 125, 168, 352, 0, 9, 346, 1, 2, 8, 4, 194, 358, 356, 358, 361, 3, 6, 266, 1, 2, 266, 0, 2, 6, 4, 8, 368, 364, 366, 371, 7, 26, 128, 3, 6, 374, 27, 374, 376, 4, 9, 66, 0, 67, 380, 5, 380, 382, 2, 145, 346, 8, 66, 139, 7, 66, 346, 1, 8, 390, 6, 8, 80, 0, 28, 395, 8, 395, 396, 1, 10, 12, 0, 3, 26, 2, 9, 402, 0, 6, 26, 402, 404, 407, 4, 6, 129, 8, 128, 410, 4, 7, 412, 5, 410, 414, 2, 8, 158, 8, 28, 419, 4, 71, 128, 5, 410, 422, 1, 6, 10, 3, 8, 10, 0, 5, 10, 426, 428, 430, 3, 4, 86, 2, 26, 434, 87, 434, 436, 2, 7, 266, 8, 267, 440, 4, 267, 442, 4, 6, 369, 8, 368, 446, 5, 446, 448, 2, 4, 93, 3, 138, 452, 2, 8, 194, 3, 10, 456, 0, 8, 154, 6, 155, 460, 0, 7, 154, 1, 462, 464, 4, 9, 196, 4, 194, 469, 8, 468, 471, 1, 12, 98, 1, 4, 334, 1, 6, 80, 2, 8, 479, 48, 80, 481, 2, 29, 70, 10, 29, 484, 0, 4, 70, 52, 346, 489, 0, 8, 93, 2, 93, 492, 4, 7, 92, 170, 494, 497, 3, 6, 160, 146, 159, 500, 1, 2, 202, 29, 70, 504, 8, 98, 334, 4, 6, 78, 4, 6, 9, 3, 78, 512, 154, 511, 514, 0, 2, 67, 8, 171, 518, 6, 67, 520, 0, 2, 27, 26, 235, 524, 6, 8, 524, 5, 26, 524, 4, 529, 530, 3, 4, 194, 1, 456, 534, 1, 4, 270, 5, 6, 538, 0, 8, 540, 271, 538, 542, 1, 2, 110, 112, 358, 547, 4, 9, 82, 2, 6, 29, 29, 550, 552, 4, 128, 202, 4, 202, 557, 128, 557, 558, 3, 10, 234, 0, 5, 346, 4, 9, 346, 347, 564, 566, 4, 6, 358, 1, 234, 570, 0, 6, 29, 43, 154, 574, 5, 86, 368, 4, 371, 578, 6, 129, 190, 4, 9, 168, 0, 29, 584, 6, 8, 98, 2, 118, 589, 4, 8, 98, 589, 590, 592, 130, 159, 270, 1, 8, 28, 0, 4, 271, 8, 465, 600, 10, 248, 346, 0, 2, 249, 6, 8, 249, 1, 2, 608, 29, 606, 610, 6, 8, 195, 4, 194, 615, 5, 8, 128, 8, 128, 158, 4, 618, 621, 0, 147, 240, 202, 240, 624, 2, 8, 346, 1, 160, 356, 8, 81, 98, 8, 70, 633, 3, 4, 26, 159, 524, 636, 26, 58, 589, 0, 28, 159, 159, 236, 642, 5, 8, 18, 2, 8, 18, 146, 646, 649, 4, 6, 139, 4, 8, 138, 5, 652, 654, 3, 8, 110, 2, 111, 658, 0, 8, 513, 658, 660, 663, 2, 6, 73, 71, 158, 666, 0, 6, 67, 3, 4, 66, 8, 671, 672, 66, 158, 369, 6, 129, 154, 0, 4, 169, 8, 169, 680, 8, 680, 683, 168, 682, 685, 4, 8, 19, 2, 99, 688, 1, 8, 110, 0, 9, 692, 111, 692, 694, 7, 8, 128, 0, 4, 129, 346, 698, 701, 9, 194, 202, 2, 5, 202, 3, 704, 706, 2, 9, 124, 2, 346, 711, 4, 8, 70, 1, 2, 714, 29, 70, 716, 6, 26, 407, 0, 407, 720, 0, 4, 159, 7, 8, 724, 6, 159, 726, 6, 8, 159, 2, 4, 730, 1, 158, 732, 158, 732, 735, 0, 734, 737, 2, 4, 335, 2, 5, 334, 3, 740, 742, 1, 92, 744, 2, 7, 72, 9, 402, 748, 0, 2, 29, 1, 158, 752, 0, 29, 146, 4, 7, 358, 8, 10, 759, 4, 8, 66, 8, 66, 763, 266, 763, 764, 5, 6, 274, 93, 170, 768, 2, 129, 158, 0, 4, 235, 2, 9, 774, 235, 248, 776, 5, 6, 78, 1, 4, 780, 7, 780, 782, 5, 6, 202, 9, 202, 786, 0, 3, 158, 6, 202, 791, 2, 159, 790, 0, 792, 795, 0, 9, 146, 2, 346, 799, 6, 8, 10, 4, 8, 92, 4, 6, 805, 0, 3, 806, 274, 805, 808, 29, 70, 154, 6, 8, 81, 1, 6, 814, 0, 7, 816, 815, 816, 818, 4, 8, 269, 2, 266, 823, 1, 268, 824, 0, 8, 81, 71, 146, 828, 0, 2, 71, 4, 6, 832, 70, 154, 835, 5, 6, 128, 4, 8, 839, 4, 8, 838, 838, 840, 843, 0, 4, 202, 2, 6, 203, 1, 4, 848, 5, 846, 850, 0, 9, 18, 59, 110, 854, 0, 4, 275, 4, 93, 274, 5, 858, 860, 1, 4, 66, 0, 7, 66, 129, 864, 866, 6, 8, 791, 0, 4, 870, 2, 4, 159, 790, 873, 874, 1, 78, 194, 2, 8, 99, 4, 7, 98, 1, 86, 98, 880, 882, 885, 8, 111, 170, 6, 111, 170, 112, 888, 891, 3, 8, 512, 0, 4, 894, 0, 7, 894, 512, 897, 898, 2, 4, 147, 6, 8, 903, 7, 146, 904, 0, 8, 903, 904, 906, 909, 2, 8, 98, 2, 268, 913, 1, 8, 18, 4, 194, 916, 1, 194, 918, 2, 4, 155, 0, 7, 922, 8, 465, 924, 2, 4, 334, 0, 95, 928, 3, 6, 72, 0, 9, 932, 4, 6, 935, 128, 932, 937, 1, 12, 740, 1, 2, 170, 5, 170, 942, 6, 8, 944, 7, 942, 946, 945, 946, 948, 2, 93, 158, 0, 95, 952, 6, 8, 93, 8, 92, 98, 0, 956, 959, 4, 8, 195, 2, 9, 194, 11, 962, 964, 4, 270, 539, 92, 538, 969, 0, 7, 146, 1, 92, 972, 1, 334, 928, 6, 8, 589, 3, 4, 978, 0, 4, 978, 588, 980, 983, 1, 4, 274, 4, 8, 159, 158, 986, 989, 2, 8, 155, 4, 6, 992, 1, 154, 994, 4, 992, 997, 0, 155, 996, 6, 998, 1001, 0, 99, 102, 6, 8, 1005, 2, 6, 266, 168, 268, 1009, 0, 6, 155, 154, 589, 1012, 1, 8, 158, 3, 4, 1016, 128, 159, 1018, 1, 6, 154, 2, 4, 1023, 8, 465, 1024, 4, 7, 18, 6, 589, 1028, 6, 124, 237, 4, 6, 82, 236, 1032, 1035, 8, 110, 368, 87, 236, 706, 3, 4, 70, 2, 29, 1042, 2, 6, 138, 28, 248, 1047, 3, 6, 48, 71, 146, 1050, 7, 8, 98, 0, 6, 99, 8, 99, 1056, 9, 1054, 1058, 4, 52, 81, 1, 4, 234, 0, 1063, 1064, 0, 3, 154, 66, 93, 1068, 99, 588, 740, 2, 8, 111, 49, 1050, 1074, 3, 358, 570, 0, 9, 570, 571, 1078, 1080, 2, 8, 124, 7, 124, 1084, 4, 8, 1086, 4, 1084, 1089, 8, 1089, 1090, 159, 248, 346, 0, 235, 1094, 0, 358, 589, 6, 589, 1098, 0, 8, 478, 2, 4, 81, 478, 1102, 1105, 4, 8, 81, 0, 3, 80, 334, 1109, 1110, 4, 71, 138, 5, 6, 1114, 235, 1114, 1116, 1, 6, 26, 2, 6, 26, 128, 1120, 1123, 3, 4, 52, 6, 52, 1127, 5, 8, 52, 2, 6, 53, 1129, 1130, 1132, 1, 4, 698, 128, 155, 1136, 87, 236, 646, 3, 6, 52, 5, 6, 52, 248, 1142, 1145, 10, 29, 70, 70, 147, 358, 7, 70, 1150, 2, 4, 86, 4, 18, 1155, 8, 87, 1156, 0, 8, 237, 6, 52, 236, 6, 236, 1161, 1160, 1163, 1164, 0, 3, 146, 6, 8, 1168, 6, 1168, 1171, 146, 1170, 1173, 0, 29, 274, 9, 334, 1176, 7, 8, 28, 0, 29, 1180, 1, 6, 1180, 9, 1182, 1184, 2, 8, 170, 1, 314, 1188, 0, 8, 87, 6, 86, 1193, 2, 6, 158, 0, 154, 1197, 1, 2, 158, 155, 1198, 1200, 19, 234, 266, 4, 8, 235, 0, 6, 234, 3, 4, 1208, 6, 1206, 1211, 1, 6, 922, 8, 154, 1215, 9, 1214, 1216, 155, 1216, 1218, 2, 9, 368, 4, 7, 1222, 4, 9, 368, 6, 1224, 1227, 3, 28, 288, 4, 86, 237, 2, 4, 1233, 86, 1233, 1234}};
  
  std::string npn4_s =  "0x3cc3 [d[!bc]]\n0x1bd8 [!<ac!d>!<a!bd>]\n0x19e3 [[!<!0a!c><!bc<!0a!c>>]<ad!<!0a!c>>]\n0x19e1 [<ac[!bc]><d[!bc]<a!b!<ac[!bc]>>>]\n0x17e8 [d<abc>]\n0x179a <[!a<!0b!c>][ad]!<abc>>\n0x178e [!d<!a!b[cd]>]\n0x16e9 [![!d<!0ab>]!<!0c<abc>>]\n0x16bc [[!c<0ad>]<a!b<!0!ac>>]\n0x16ad <!d<ad!<0!b!c>>!<ac!<0!bd>>>\n0x16ac [<acd>!<!b!d<!a[!bc]<acd>>>]\n0x16a9 <!<!0!bc>!<d!<!0!bc>![ac]><d<!b!d<!0!bc>>![ac]>>\n0x169e <[bc][a[bc]]<!c!d[a[bc]]>>\n0x169b [<c[!ab]<!b!cd>><a!cd>]\n0x169a <![!bc]<0a[!bc]>[a<cd![!bc]>]>\n0x1699 <<!a!b<0!cd>><ad!<0!cd>>!<!bd<ad!<0!cd>>>>\n0x1687 [!a!<b<!0!b<a!bd>>![c<a!bd>]>]\n0x167e <b!<bd[!ac]>[<bd[!ac]><ac![!ac]>]>\n0x166e [<cd<0ab>>!<!a!b<0ab>>]\n0x03cf <!c!d![bc]>\n0x166b <!c[a<bcd>]<!bc!d>>\n0x01be <!d!<!0b!<!bcd>><b!c![a<!0b!<!bcd>>]>>\n0x07f2 <!d![!cd]<!bd[ab]>>\n0x07e9 [<bc<!0a!b>><!ad<!0a!b>>]\n0x6996 [b[a[cd]]]\n0x01af <a!<0d<abd>>!<!0ac>>\n0x033c <!<!0bd>[c<!0bd>]![!bd]>\n0x07e3 <!c[!c<ab!d>]<!b!d[!c<ab!d>]>>\n0x1681 <<!b!cd>!<!0!a<!b!cd>>![[!ab]<!bc!d>]>\n0x01ae [!d<0<!a!bc>!<acd>>]\n0x07e2 [<!a!bd>!<!cd<0b!<!a!bd>>>]\n0x01ad [<!0c!<a!bd>><!acd>]\n0x07e1 [!<!a!bd>!<0c!<!cd!<!a!bd>>>]\n0x001f <!d<0!a!b><0!c!<0!a!b>>>\n0x01ac [<!0bc><d<!acd><!0bc>>]\n0x07e0 [d<cd<ab!d>>]\n0x07bc [<!0d!<!0a!b>><bc<!0a!b>>]\n0x03dc <b[!d<0!b!c>]!<ab!<0!b!c>>>\n0x06f6 <!d[cd][ab]>\n0x06bd [<c<a!bd>!<a!b!d>><!0d!<a!bd>>]\n0x077e <!<bcd><b![!cd]!<bcd>>[a<bcd>]>\n0x06b9 <!<ad<0!b!c>><a!b!d><0!cd>>\n0x06b7 <!d!<bc![!ac]>![b[!ac]]>\n0x06b6 <!<!0a!b><a!bc>!<0c!<0!d<a!bc>>>>\n0x077a [<bcd>!<0!a<b!c!d>>]\n0x06b5 [<a!d[ac]>!<cd<!b![ac]<a!d[ac]>>>]\n0x0ff0 [cd]\n0x0779 [<cd<ab!c>><!0!<ab!c><abc>>]\n0x06b4 [c<d![!ab]<!abc>>]\n0x0778 <!b<b!d!<!0a!b>>[c<bd<!0a!b>>]>\n0x007f <!d<0!ab>!<bc<0!ab>>>\n0x06b3 <!a<a!b!d><[cd][!bd]<a!b!d>>>\n0x007e <0!d![a<a!b!c>]>\n0x06b2 <a<!a!d[ab]>[d<cd![ab]>]>\n0x0776 <[ab]!<abd>[cd]>\n0x06b1 [!d!<c<!0!bd>![ab]>]\n0x06b0 [d<c[!ab]<ad<!0cd>>>]\n0x037c <d[d<!0bc>]!<ad<bcd>>>\n0x01ef <!c!d[d<!0ab>]>\n0x0696 <b<a!b!<!0!cd>>!<abc>>\n0x035e [!<0!a!d>!<0!c!<bd<0!a!d>>>]\n0x0678 <!<!abd>[c<abd>]!<!0a!b>>\n0x0676 <!<!0!cd>!<abc>[ab]>\n0x0669 [!d!<b<a!b<!0!cd>>!<ab!c>>]\n0x01bc [<0!b!c><!c!d<a!b!d>>]\n0x0663 [!<!acd><0b<b!c!d>>]\n0x07f0 <0[cd]<!b!<0ad>[cd]>>\n0x01bf <!b!d!<!bc![ad]>>\n0x0666 <0[ab]!<0cd>>\n0x0661 [<ab!c><b!d<!0a<a!b!c>>>]\n0x1689 [!<0d![ab]><c[ab]!<abd>>]\n0x0660 <0![!cd][ab]>\n0x0182 [!<!ab!c><!bd!<!0!c<!ab!c>>>]\n0x07b6 [c<d!<0!c[!ab]>!<ac[!ab]>>]\n0x03d7 <!d![bc]!<abc>>\n0x06f1 <c<a!c<!ab!d>><!b<!ab!d>![d<a!c<!ab!d>>]>>\n0x03dd <!<a!bd>!<!0bc>![!d<!0bc>]>\n0x0180 [!<!bc!d>!<!a!b<!bc!d>>]\n0x07b4 [<a!d![bd]>!<0c!<0d<a!d![bd]>>>]\n0x03db <!d<0!b!c>[a<!a!bc>]>\n0x07b0 <!<!0a!d>[cd]<a!b!d>>\n0x03d4 [d<bc<!0!ad>>]\n0x03c7 <[!bc]<0!a!c>!<0bd>>\n0x03c6 <!d!<!0c!d>[b<a!c!<!0c!d>>]>\n0x03c5 [!<bcd>!<!ad!<0c!d>>]\n0x03c3 <!c!<b!cd>![bc]>\n0x03c1 <!a[b<b!cd>]!<c<b!cd>![b<b!cd>]>>\n0x03c0 [d<bcd>]\n0x1798 [<!a!b<ab!c>><0!d<!c!d<ab!c>>>]\n0x036f <!c!d[b<!0ad>]>\n0x03fc [d!<0!b!c>]\n0x1698 <!<abc><0!d<abd>><!0c<abd>>>\n0x177e [!<0!a!c><bd![ac]>]\n0x066f <!c!d[ab]>\n0x035b <!<acd>!<0b!c><0a!c>>\n0x1697 [<!ab<!acd>>[!bc]]\n0x066b <a<!a!b[cd]>!<cd[b<!a!b[cd]>]>>\n0x07f8 [d<ac!<0a!b>>]\n0x035a <!d<0!bd>[c<!0ad>]>\n0x1696 <0<!0!b<b!c!d>>![a[!bc]]>\n0x003f <0!d!<bcd>>\n0x0673 <!d![!b<a!c!d>]!<bc<a!c!d>>>\n0x0359 [!<0b!c><ac![!ad]>]\n0x0672 <<0!b!d>[ab][cd]>\n0x0358 [!<b!d!<0a!d>>!<d<b!d!<0a!d>>![c<0a!d>]>]\n0x003d <0<!b!c<bc!d>><!a!d<bc!d>>>\n0x0357 <!a<!0a!d>!<!0bc>>\n0x003c <0!d[bc]>\n0x0356 [<0!a!d><0!b!c>]\n0x07e6 [<ab!d><!cd<0ab>>]\n0x033f <!b!c!d>\n0x033d [<0!d<a!cd>><!b!c<0!d!<a!cd>>>]\n0x0690 [c<cd![!ab]>]\n0x01e9 [!<bc<!0a!c>><a!<!0a!c>!<0d<bc<!0a!c>>>>]\n0x1686 [!a<!b[!bc]!<ad![!bc]>>]\n0x07f1 <!b[cd]!<a!bd>>\n0x01bd <!d[b<!a!bc>]<0!c<!a!bc>>>\n0x1683 [[!bc]<!ad<0bc>>]\n0x001e <0!d[c<!0ab>]>\n0x01ab <!d!<!0bc>[ad]>\n0x01aa <a![!ad]!<!0a<!0bc>>>\n0x01a9 <!d![a<!0bc>]<0!ad>>\n0x001b <0!<!abd>!<acd>>\n0x01a8 [<0!b!c><a!d<0!b!c>>]\n0x000f <0!c!d>\n0x0199 <!<bcd>[!ab]<0!d[!ab]>>\n0x0198 [b!<a!b[d<!0b!c>]>]\n0x0197 <!d<0!b!c>[!a<0bc>]>\n0x06f9 [d<!0c![ab]>]\n0x0196 <!b!<!0d<a!b!c>>!<!bc![ad]>>\n0x03d8 [d!<!b!c<0!d![!ab]>>]\n0x06f2 <[cd]<!d![!ab][cd]><0a![cd]>>\n0x03de <!d![!bd][!c<!ab[!bd]>]>\n0x018f <!c!d![a<!ab!d>]>\n0x0691 <<!a!bd><ab!c><!d!<ab!c><0c<ab!c>>>>\n0x01ea [!d!<!0a<bcd>>]\n0x07b1 <[cd]<0!a!c>!<!abd>>\n0x0189 <<a!b!c><0b!d>[a<a!b!c>]>\n0x036b <!c[b<!0a!c>]<a!d!<!0a!c>>>\n0x03d5 <!a[b<b!cd>]<!0a!d>>\n0x0186 [<!a!bc><c!d!<0ab>>]\n0x0119 <0[!ab]!<acd>>\n0x0368 [![c<0a!d>]!<!bc!<b!d![c<0a!d>]>>]\n0x0183 <[!bc]<0a!d><0!a!b>>\n0x07b5 <!<bcd>![ac][cd]>\n0x0181 [<b!c<!0!bd>><ab<b!c<!0!bd>>>]\n0x036e <b[b<!0ad>]!<bcd>>\n0x011f <!c!d!<!0ab>>\n0x016f <b<0!b!<!abc>>!<ad<!abc>>>\n0x016e <!d[a[bd]]<0!c[bd]>>\n0x013f <!a<0a!d>!<bcd>>\n0x019f <0<!c!d[!ab]>!<0bd>>\n0x013e [[!bd]!<!bc<a!d![!bd]>>]\n0x019e [d<a!<a!b!c><!0!c<!a!bd>>>]\n0x013d <b!<bcd><0<!0!ac>!<bcd>>>\n0x016b <!d![!a[!bc]]!<bc![!bc]>>\n0x01fe [d!<0!a!<!0bc>>]\n0x166a [<bcd><ab<d!<bcd>!<0b!c>>>]\n0x0019 [<!ab!d><0b<!ac<!ab!d>>>]\n0x006b <!d<a!b!c><0!a<0bc>>>\n0x069f <a<!ab!d>!<abc>>\n0x013c <0!<bcd><b!<0ad>!<!c!d<0ad>>>>\n0x037e [<!0ad><bc<!0!ad>>]\n0x012f <!d<a!b!c><0!a!c>>\n0x0693 [c!<!d<b!cd>![!b<!ac<b!cd>>]>]\n0x012d <0!<bcd><!0b[!ac]>>\n0x01ee <!c<ac!d>[!d<0!a!b>]>\n0x012c [!<0!b!c><cd<!0!ab>>]\n0x017f <!a!d!<bcd>>\n0x1796 <![!ad]<!b!c[!ad]>![!d<bc[!ad]>]>\n0x036d <<!ab!c><ac[bd]>!<bcd>>\n0x011e <d<0!d!<0!c<0!a!b>>>!<c!<0!a!b>!<0!d!<0!c<0!a!b>>>>>\n0x0679 <a<!a!b[cd]>!<ad<!0!bc>>>\n0x035f <!c!d!<ab[bc]>>\n0x067b <!<cd!<!acd>>![b<!acd>]!<!abd>>\n0x0118 <0[!d[a<!ab!c>]]!<b!<!ab!c>[a<!ab!c>]>>\n0x067a [<!0ac><cd<ab!<!0ac>>>]\n0x0117 <0!<abd>!<cd<ab!d>>>\n0x1ee1 [!d![!c<!0ab>]]\n0x016a <d<!d!<!0!ad><bcd>>!<a<bcd>!<!d!<!0!ad><bcd>>>>\n0x0169 <!c<!a!b<abc>><0!d<abc>>>\n0x037d <!a![d<0!b!c>]<a!b!c>>\n0x0697 <<0ac>!<abc>!<d<abc>!<abd>>>\n0x006f <!d<0!c!d>![!ab]>\n0x17ac [<abc><!ad<!0bd>>]\n0x0069 <0!d![a[bc]]>\n0x0667 <0!<0ab>!<cd!<!0ab>>>\n0x0168 <!b!<!0d!<abc>><!ab!<!bc!d>>>\n0x067e <!d[ab][!c<0!b!d>]>\n0x011b <a!<!0ab>!<acd>>\n0x036a [<!0ad><bcd>]\n0x018b <!c!<a!c[bc]><0a!d>>\n0x0001 <0<0!b!d><!ab!c>>\n0x1669 <<!a!c[bd]><ac[bd]>!<[bd]!<!a!c[bd]><bd<!a!c[bd]>>>>\n0x0018 <0!d[a<a!bc>]>\n0x168b [![d<!0bc>]<ab!<!c!d<!0bc>>>]\n0x0662 [<ab<!0!bc>><c!<!0!bc><abd>>]\n0x00ff !d\n0x168e [<!0a!<!0!bc>><c!<!0!bc><a!bd>>]\n0x0007 <0<0!c!d>!<abd>>\n0x19e6 [[ad]<b!c!<!0a!b>>]\n0x0116 <0!<abd>![!c<d[ab]!<abd>>]>\n0x036c [[bd]<0c!<!ab![bd]>>]\n0x01e8 <<!a!bc><a!c[cd]><0b!d>>\n0x06f0 <<0!ab><a!bc>![!cd]>\n0x03d6 [[c<!0!bc>]!<!0d!<!ab<!0!bc>>>]\n0x1be4 [!d!<bc[ab]>]\n0x0187 <a<!a!d[!bc]><0!c<!a!d[!bc]>>>\n0x0003 <0!d<0!b!c>>\n0x017e [[!ad]<a!b<!c[!ad]<!0bd>>>]\n0x01eb <!d[!b<!0ac>]<a!b!<!0ac>>>\n0x0006 <0!d<!cd![!ab]>>\n0x011a [a<a!<b!c!d>[cd]>]\n0x0369 <0[!c<0!d![!ab]>]!<0bd>>\n0x03d9 [!b<b!<!ab[!ad]><!bc!d>>]\n0x0000 0\n0x019b <!d!<!0bc>![ab]>\n0x1668 [<acd><ab<bcd>>]\n0x18e7 [d[!b<!abc>]]\n0x0017 <0!d!<abc>>\n0x019a <!d[a<!bcd>]<0!c<!bcd>>>\n0x0016 <0!<ab<!a!bc>>!<!cd<!a!bc>>>";
  std::string npn4_sd = "0x3cc3 [b[!cd]]\n0x1bd8 [!<b!cd><a!b!c>]\n0x19e3 [<a!b!c><c[bd]<0a!c>>]\n0x19e1 [<!b!c[bd]><a[bd]!<cd<!b!c[bd]>>>]\n0x17e8 [d<abc>]\n0x179a [<ad!<0ad>><c<0ad>[!bd]>]\n0x178e <!c[ad]![!bd]>\n0x16e9 [d<<a!b!c><abc>!<0a!b>>]\n0x16bc [<0!b!c><!d!<abc>[ad]>]\n0x16ad [!<!b!<!0a!b>[ac]><0d<!0a!b>>]\n0x16ac [!<acd><!b!d<!a[cd]<acd>>>]\n0x16a9 <0<!0a<!b!cd>>[!<0!b!c>[!ad]]>\n0x169e <!d[!a[!bc]]!<c!d[!bc]>>\n0x169b <<a!b!<0!cd>>!<ab!<0!cd>>!<!bcd>>\n0x169a [a<![!bc]<0ac><!0!bd>>]\n0x1699 <b<!a!b<0!cd>>!<b<0!cd><!abd>>>\n0x1687 [[ac]<b!<a!bd>!<0bc>>]\n0x167e <<!b!c<!abc>>[a<!abc>]!<!ad!<!abc>>>\n0x166e [<ab!<0ab>><cd<0ab>>]\n0x03cf <!c!d[bd]>\n0x166b <!c[a<bcd>]<!bc!d>>\n0x01be <c!<bcd><!<!0!bd>[ad]!<bcd>>>\n0x07f2 <[cd]<0a!b>!<0ad>>\n0x07e9 [<ac<!0!ab>><!bd<!0!ab>>]\n0x6996 [[bc][ad]]\n0x01af <[!ac]<0a!d>!<bcd>>\n0x033c <<b!cd><0c!d>!<bcd>>\n0x07e3 [c<d!<abc>!<0b!c>>]\n0x1681 <0[!<b!cd>[ab]]!<!d[ab]<0!ac>>>\n0x01ae <!<bcd>![!ad]<0b!d>>\n0x07e2 [<!a!bd>!<!cd<0b!<!a!bd>>>]\n0x01ad <!<bcd><0b!d>[!ac]>\n0x07e1 [c<d<!0!bc>!<ab!d>>]\n0x001f <!a<a!b!d><0!c!d>>\n0x01ac [d<a<!a!cd>!<0!b!c>>]\n0x07e0 [d<ac<!abd>>]\n0x07bc [<bc<!0a!b>>!<0!d<!0a!b>>]\n0x03dc <b[d<!0bc>]!<ab<!0bc>>>\n0x06f6 <!d[ab][cd]>\n0x06bd <<b!c<0!bd>><a!b!d>!<a!c<0!bd>>>\n0x077e <<!0a![!cd]><0!ab>!<bcd>>\n0x06b9 <!<!0ac><a!b!d><b<a!b!d>![!cd]>>\n0x06b7 <!d[c[ab]]<!b!c[ab]>>\n0x06b6 [!c!<<!abc>!<0c!d>[ab]>]\n0x077a [!<0a!<!bcd>>![cd]]\n0x06b5 <!<a!b!c><a!b!d>![!c<!0!ad>]>\n0x0ff0 [cd]\n0x0779 <<ac!d><b!c!<ac!d>>!<ab![cd]>>\n0x06b4 [!c<!d<a!b!c>![ab]>]\n0x0778 [[cd]<0<b!c!d><0ab>>]\n0x007f <!d<!ab!c><0!b!d>>\n0x06b3 [![!bd]!<d<0bc><a!c!d>>]\n0x007e <0!d![a<a!b!c>]>\n0x06b2 [!<0!c!<a!bd>><bd!<ab!c>>]\n0x0776 <[ab][cd]!<abc>>\n0x06b1 [d<c<!0!bd>[!ab]>]\n0x06b0 [<!a!c<ab!d>><!b<ab!d>!<0cd>>]\n0x037c [[!bd]<d<!0b!c>!<acd>>]\n0x01ef <!c!d![!c<0!a!b>]>\n0x0696 <<ab!c><0c!d>!<abc>>\n0x035e [<!0ad>!<0!c!<bd!<!0ad>>>]\n0x0678 <<!ab!d><0a!b>[c<abd>]>\n0x0676 <!<abc>[ab]<0c!d>>\n0x0669 [<0!c!d><[cd]<0!c!d>[ab]>]\n0x01bc [<bd<!acd>>!<0!b!c>]\n0x0663 <0[b<a!c!d>]!<0cd>>\n0x07f0 <[cd]!<!0!cd>!<0ab>>\n0x01bf <!d<!0a!c><0!a!b>>\n0x0666 <0[ab]!<0cd>>\n0x0661 <<a!c!d>!<ab<a!c!d>>[<a!c!d><b!c!d>]>\n0x1689 [[d<abd>]<0!c<!a!bd>>]\n0x0660 [<b!c!d>!<!acd>]\n0x0182 [<!ab!c><b!d<!0!c<!ab!c>>>]\n0x07b6 [<!0!a<!bcd>>[!c<!0bd>]]\n0x03d7 <!d[!bc]!<abc>>\n0x06f1 [<!ac<!0!bc>><d<!0!bc><0!ad>>]\n0x03dd <[bd]<0!a!d>!<0cd>>\n0x0180 [<acd>!<b!d!<acd>>]\n0x07b4 [<bd<!0!ad>><c<!0!ad><abc>>]\n0x03db <[!bc]<a!b!d>!<acd>>\n0x07b0 <<0ac>[cd]!<abc>>\n0x03d4 [!d<!b!c<0a!d>>]\n0x03c7 <!<0bd><0!a!c>[!bc]>\n0x03c6 <0!<0cd>[!b<!ac!d>]>\n0x03c5 <[bd]<0!a!c>![bc]>\n0x03c3 <!b<0b!d>[!bc]>\n0x03c1 <!<b!cd>[b<b!cd>]<!a!c<b!cd>>>\n0x03c0 [d<bcd>]\n0x1798 [<ab<!a!bc>><d<!a!bc><!0cd>>]\n0x036f <!c!d[!b<0!ac>]>\n0x03fc [d!<0!b!c>]\n0x1698 [b<<!abd>[ac][!ad]>]\n0x177e [<0!b!d>!<ac![bd]>]\n0x066f <!c!d[ab]>\n0x035b <<ac!d><0!b!c>!<0ac>>\n0x1697 <!<abc><ab!c><!bc!d>>\n0x066b [<bc<!a!bd>><a<!a!bd>!<0!cd>>]\n0x07f8 [!d!<ac!<0a!b>>]\n0x035a [!<!0c<0bd>><0!a!d>]\n0x1696 <!a<!d<0!bd><a!bc>><b!c<a!bc>>>\n0x003f <0!d!<bcd>>\n0x0673 <!b![d<ab!c>]<!a!d<ab!c>>>\n0x0359 [<!0!bc><ac[!cd]>]\n0x0672 <[cd]<0!b!d>[ab]>\n0x0358 <<!acd><0a!c>[!d<0!b!c>]>\n0x003d <!<bcd>[bc]<0!a!d>>\n0x0357 <!0!<!0bc><0!a!d>>\n0x003c <0!d![!bc]>\n0x0356 [!<0!a!d>!<0!b!c>]\n0x07e6 [<!cd!<0!a!b>><abc>]\n0x033f <!b!c!d>\n0x033d <!<bcd><bc!d><0!b<!0!ad>>>\n0x0690 [!d<!c!d[ab]>]\n0x01e9 <!d<0!b![ac]>![b<!0ac>]>\n0x1686 [[bc]<a[bc]!<!abd>>]\n0x07f1 <!b<!ab!d>[cd]>\n0x01bd <!d[c<!ab!c>]<0!b<!ab!c>>>\n0x1683 [[bc]!<!ad<0bc>>]\n0x001e <0!d![!c<ab!d>]>\n0x01ab <!d![!ad]<0!b!c>>\n0x01aa <[ad]<0!b!c><0a!d>>\n0x01a9 [a<d<0!b!c><!0a!d>>]\n0x001b <0<a!b!d>!<acd>>\n0x01a8 [a<ad!<!0bc>>]\n0x000f <0!c!d>\n0x0199 <[!ab]<a!b!d><0!a!c>>\n0x0198 [!<!abd><!b!c<0ac>>]\n0x0197 <!d[!a<0bc>]<!b!c<0bc>>>\n0x06f9 [d<!0c![ab]>]\n0x0196 <<0!d!<a!b!c>>[ad]!<bc[ad]>>\n0x03d8 [<!cd!<a!b!c>><!0b!<a!b!c>>]\n0x06f2 <a<0!a[cd]><!d[cd]![!ab]>>\n0x03de <!d![c<!ab!d>][bd]>\n0x018f <!c!d[b<!ab!c>]>\n0x0691 <0<!d<!a!bd><ab!c>><!0c<!a!bd>>>\n0x01ea [d<!0a<bcd>>]\n0x07b1 <<a!b!d><0!a!c>[cd]>\n0x0189 <[!ab]!<!0ac><0a!d>>\n0x036b <!a!<!ad![!bc]>!<!abc>>\n0x03d5 [<bcd>!<!d<bcd><0a!d>>]\n0x0186 [<!cd<0ab>>!<!a!bc>]\n0x0119 <0[!ab]!<acd>>\n0x0368 [<bcd><c<b!cd><!0ad>>]\n0x0183 <[!bc]<0a!d><0!a!b>>\n0x07b5 <!<bcd>[cd]![ac]>\n0x0181 <<!ab!c>[b<!ab!c>]<0!d!<!ab!c>>>\n0x036e <!c<bc!d>[!b<0!a!d>]>\n0x011f <!c!d!<!0ab>>\n0x016f <<!ab!c><0a!d>!<abd>>\n0x016e <!d<0!c[bd]>[a[bd]]>\n0x013f <0!<0ad>!<bcd>>\n0x019f <!d<b!c[ad]><0!a!b>>\n0x013e [[!cd]<!bc!<a!d![!cd]>>]\n0x019e <!d!<!bc[!ad]><0!b<!acd>>>\n0x013d <<bc!d>!<!0ac>!<bcd>>\n0x016b <!d<0!b!c>![!b[!ac]]>\n0x01fe [d<!0c<ab!c>>]\n0x166a <a[a<bcd>]!<ab<!bcd>>>\n0x0019 <!<!abd><0!a!b><0b!c>>\n0x006b <!d<0b<!a!bc>>!<!abc>>\n0x069f <!d<abd>!<abc>>\n0x013c <<0!ad><b!d!<!0b!c>>!<bc<0!ad>>>\n0x037e [<!0ad>!<!b!c<0a!d>>]\n0x012f <!d<a!b!c>!<!0ac>>\n0x0693 [<acd><d<!0c!d>![bd]>]\n0x012d <<0ac><!ab!c>!<bcd>>\n0x01ee [!<!0ab>!<d<0!cd><!0ab>>]\n0x012c [<0!a!b><!d[bc]<0!a!b>>]\n0x017f <!b!d!<acd>>\n0x1796 <!b!<!a!bc><!a<!0cd>!<!0!bd>>>\n0x036d <<abc>!<bcd><!a!c[bd]>>\n0x011e <d!<cd!<0!a!b>>!<d<0!a!b>!<0c!d>>>\n0x0679 <!d<!a<abd><0!b!c>>[c<abd>]>\n0x035f <!c!<0bd>!<!0ad>>\n0x067b <!d<!b!c[ad]>[![ad]<0b!c>]>\n0x0118 <<0!a!b><ac!d><!cd<0b!d>>>\n0x067a [<!0ac><cd<ab!<!0ac>>>]\n0x0117 <!b<!a!d<a!cd>><0!c!<a!cd>>>\n0x1ee1 [[cd]<0!a!b>]\n0x016a <b!<b<!0!ad>!<b!cd>>!<ab<b!cd>>>\n0x0169 <!a<a!b!c><0!d<abc>>>\n0x037d <!a<a!b!c>[!d<0!b!c>]>\n0x0697 <<!a!c<0a!b>>!<!ab!c><!ab!d>>\n0x006f <!d<0!c!d>[ab]>\n0x17ac [<abc><!0d<0!ab>>]\n0x0069 <0!d[a[!bc]]>\n0x0667 <![!ab]<0!b[!ab]>!<cd[!ab]>>\n0x0168 [<bd<!0!bc>><ad<!0bc>>]\n0x067e <!d![c<0!a!d>][ab]>\n0x011b <a!<acd>!<!0ab>>\n0x036a [!<bcd><0!a!d>]\n0x018b <!<0ad><0!b!c><0ab>>\n0x0001 <0<0!a!b><0!c!d>>\n0x1669 <<!cd<!a!bc>><b!d[!ac]>!<bd[!ac]>>\n0x0018 <0!d[a<a!bc>]>\n0x168b [[!a<!ab!d>]<!b!c<0d<!ab!d>>>]\n0x0662 <!<!0bd>[ab][d<abc>]>\n0x00ff !d\n0x168e [!<a!c!<0!b!c>><!0<0!b!c><!ab!d>>]\n0x0007 <0!<!0cd>!<0ab>>\n0x19e6 [[ad]<0b!<0ac>>]\n0x0116 [<!b!d[!ac]><0[!ac]!<!abd>>]\n0x036c [[bd]!<!0!c<!ab![bd]>>]\n0x01e8 <!d<a[bd][cd]><0!ad>>\n0x06f0 <[cd]<a!bc>!<a!bd>>\n0x03d6 [<d!<0!a!d>!<0bc>><bc!<0bc>>]\n0x1be4 [d!<!b!c[ac]>]\n0x0187 <a!<!0ac>!<ad![!bc]>>\n0x0003 <0!d<0!b!c>>\n0x017e [[!bd]<<!ab!c>[!bd]<!0bd>>]\n0x01eb <!d[b<0!a!c>]!<!ab!<0!a!c>>>\n0x0006 <0[ab]<0!c!d>>\n0x011a <0[d[ac]]!<bcd>>\n0x0369 <!b<0!d<abc>><b!c!<0a!d>>>\n0x03d9 [d<<!0!ab><bcd><a!b!d>>]\n0x0000 0\n0x019b <!d<0!b!c>![ab]>\n0x1668 [<bcd><ab<!bcd>>]\n0x18e7 [[!cd]<abc>]\n0x0017 <0!d!<abc>>\n0x019a <!d![a<b!c!d>]!<!0c<b!c!d>>>\n0x0016 <0<a!d<bc!d>>!<abc>>";
  std::string npn4_ds = "0x3cc3 [b[!cd]]\n0x1bd8 [!<b!cd><a!b!c>]\n0x19e3 [<a!b!c><c[bd]<0a!c>>]\n0x19e1 <![<a!bd>[bc]]<!d<a!bd>![bc]><!a!c[bc]>>\n0x17e8 [d<abc>]\n0x179a [<ad!<0ad>><c<0ad>[!bd]>]\n0x178e <!c[ad]![!bd]>\n0x16e9 [d<<a!b!c><abc>!<0a!b>>]\n0x16bc [<0!b!c><!d!<abc>[ad]>]\n0x16ad [!<!b!<!0a!b>[ac]><0d<!0a!b>>]\n0x16ac [<<!0!cd>[bc]<0ab>><ad!<!0!cd>>]\n0x16a9 <0<!0a<!b!cd>>[!<0!b!c>[!ad]]>\n0x169e <!d[!a[!bc]]!<c!d[!bc]>>\n0x169b <<a!b!<0!cd>>!<ab!<0!cd>>!<!bcd>>\n0x169a [a<![!bc]<0ac><!0!bd>>]\n0x1699 <b<!a!b<0!cd>>!<b<0!cd><!abd>>>\n0x1687 [[ac]<b!<a!bd>!<0bc>>]\n0x167e <<!b!c<!abc>>[a<!abc>]!<!ad!<!abc>>>\n0x166e [<ab!<0ab>><cd<0ab>>]\n0x03cf <!c!d[bd]>\n0x166b <!c[a<bcd>]<!bc!d>>\n0x01be <c!<bcd><!<!0!bd>[ad]!<bcd>>>\n0x07f2 <[cd]<0a!b>!<0ad>>\n0x07e9 [<ac<!0!ab>><!bd<!0!ab>>]\n0x6996 [[bc][ad]]\n0x01af <[!ac]<0a!d>!<bcd>>\n0x033c <<b!cd><0c!d>!<bcd>>\n0x07e3 [c<d!<abc>!<0b!c>>]\n0x1681 <0[!<b!cd>[ab]]!<!d[ab]<0!ac>>>\n0x01ae <!<bcd>![!ad]<0b!d>>\n0x07e2 [<b!cd><b<!0!bc><ab!d>>]\n0x01ad <!<bcd><0b!d>[!ac]>\n0x07e1 [c<d<!0!bc>!<ab!d>>]\n0x001f <!a<a!b!d><0!c!d>>\n0x01ac [d<a<!a!cd>!<0!b!c>>]\n0x07e0 [d<ac<!abd>>]\n0x07bc [<bc<!0a!b>>!<0!d<!0a!b>>]\n0x03dc <b[d<!0bc>]!<ab<!0bc>>>\n0x06f6 <!d[ab][cd]>\n0x06bd <<b!c<0!bd>><a!b!d>!<a!c<0!bd>>>\n0x077e <<!0a![!cd]><0!ab>!<bcd>>\n0x06b9 <!<!0ac><a!b!d><b<a!b!d>![!cd]>>\n0x06b7 <!d[c[ab]]<!b!c[ab]>>\n0x06b6 [!c!<<!abc>!<0c!d>[ab]>]\n0x077a [!<0a!<!bcd>>![cd]]\n0x06b5 <!<a!b!c><a!b!d>![!c<!0!ad>]>\n0x0ff0 [cd]\n0x0779 <<ac!d><b!c!<ac!d>>!<ab![cd]>>\n0x06b4 [!c<!d<a!b!c>![ab]>]\n0x0778 [[cd]<0<b!c!d><0ab>>]\n0x007f <!d<!ab!c><0!b!d>>\n0x06b3 [![!bd]!<d<0bc><a!c!d>>]\n0x007e <!<abd><bc!d><0a!c>>\n0x06b2 [!<0!c!<a!bd>><bd!<ab!c>>]\n0x0776 <[ab][cd]!<abc>>\n0x06b1 [d<c<!0!bd>[!ab]>]\n0x06b0 [<!a!c<ab!d>><!b<ab!d>!<0cd>>]\n0x037c [[!bd]<d<!0b!c>!<acd>>]\n0x01ef <<0b!d><a!b!c>!<0ad>>\n0x0696 <<ab!c><0c!d>!<abc>>\n0x035e <<!0d![!ac]><0b!d>!<bcd>>\n0x0678 <<!ab!d><0a!b>[c<abd>]>\n0x0676 <!<abc>[ab]<0c!d>>\n0x0669 [<0!c!d><[cd]<0!c!d>[ab]>]\n0x01bc [<bd<!acd>>!<0!b!c>]\n0x0663 <0[b<a!c!d>]!<0cd>>\n0x07f0 <[cd]!<!0!cd>!<0ab>>\n0x01bf <!d<!0a!c><0!a!b>>\n0x0666 <0[ab]!<0cd>>\n0x0661 <<a!c!d>!<ab<a!c!d>>[<a!c!d><b!c!d>]>\n0x1689 [[d<abd>]<0!c<!a!bd>>]\n0x0660 [<b!c!d>!<!acd>]\n0x0182 <0<!0a!<!ac!d>>!<ad![!bc]>>\n0x07b6 [<!0!a<!bcd>>[!c<!0bd>]]\n0x03d7 <!d[!bc]!<abc>>\n0x06f1 [<!ac<!0!bc>><d<!0!bc><0!ad>>]\n0x03dd <[bd]<0!a!d>!<0cd>>\n0x0180 [<acd>!<b!d!<acd>>]\n0x07b4 [<bd<!0!ad>><c<!0!ad><abc>>]\n0x03db <[!bc]<a!b!d>!<acd>>\n0x07b0 <<0ac>[cd]!<abc>>\n0x03d4 <[cd]<0!a!d>[bd]>\n0x03c7 <!<0bd><0!a!c>[!bc]>\n0x03c6 <0!<0cd>[!b<!ac!d>]>\n0x03c5 <[bd]<0!a!c>![bc]>\n0x03c3 <!b<0b!d>[!bc]>\n0x03c1 <!<b!cd>[b<b!cd>]<!a!c<b!cd>>>\n0x03c0 [d<bcd>]\n0x1798 [<ab<!a!bc>><d<!a!bc><!0cd>>]\n0x036f <!c!d[!b<0!ac>]>\n0x03fc [d!<0!b!c>]\n0x1698 [b<<!abd>[ac][!ad]>]\n0x177e [<0!b!d>!<ac![bd]>]\n0x066f <!c!d[ab]>\n0x035b <<ac!d><0!b!c>!<0ac>>\n0x1697 <!<abc><ab!c><!bc!d>>\n0x066b [<bc<!a!bd>><a<!a!bd>!<0!cd>>]\n0x07f8 [!d!<ac!<0a!b>>]\n0x035a [!<!0c<0bd>><0!a!d>]\n0x1696 <!a<!d<0!bd><a!bc>><b!c<a!bc>>>\n0x003f <0!d!<bcd>>\n0x0673 <!b![d<ab!c>]<!a!d<ab!c>>>\n0x0359 [<!0!bc><ac[!cd]>]\n0x0672 <[cd]<0!b!d>[ab]>\n0x0358 <<!acd><0a!c>[!d<0!b!c>]>\n0x003d <!<bcd>[bc]<0!a!d>>\n0x0357 <!0!<!0bc><0!a!d>>\n0x003c <0!d![!bc]>\n0x0356 [!<0!a!d>!<0!b!c>]\n0x07e6 [<!cd!<0!a!b>><abc>]\n0x033f <!b!c!d>\n0x033d <!<bcd><bc!d><0!b<!0!ad>>>\n0x0690 <<!a!bd><0c!d><ab!c>>\n0x01e9 <!d<0!b![ac]>![b<!0ac>]>\n0x1686 [[bc]<a[bc]!<!abd>>]\n0x07f1 <!b<!ab!d>[cd]>\n0x01bd <!d[c<!ab!c>]<0!b<!ab!c>>>\n0x1683 [[bc]!<!ad<0bc>>]\n0x001e <0!d![!c<ab!d>]>\n0x01ab <!d![!ad]<0!b!c>>\n0x01aa <[ad]<0!b!c><0a!d>>\n0x01a9 [a<d<0!b!c><!0a!d>>]\n0x001b <0<a!b!d>!<acd>>\n0x01a8 [a<ad!<!0bc>>]\n0x000f <0!c!d>\n0x0199 <[!ab]<a!b!d><0!a!c>>\n0x0198 [!<!abd><!b!c<0ac>>]\n0x0197 <!d[!a<0bc>]<!b!c<0bc>>>\n0x06f9 <<!ab!d><a!b!d>[cd]>\n0x0196 <<0!d!<a!b!c>>[ad]!<bc[ad]>>\n0x03d8 [<!cd!<a!b!c>><!0b!<a!b!c>>]\n0x06f2 <a<0!a[cd]><!d[cd]![!ab]>>\n0x03de <!d![c<!ab!d>][bd]>\n0x018f <!c!d[b<!ab!c>]>\n0x0691 <0<!d<!a!bd><ab!c>><!0c<!a!bd>>>\n0x01ea [d<!0a<bcd>>]\n0x07b1 <<a!b!d><0!a!c>[cd]>\n0x0189 <[!ab]!<!0ac><0a!d>>\n0x036b <!a!<!ad![!bc]>!<!abc>>\n0x03d5 [<bcd>!<!d<bcd><0a!d>>]\n0x0186 [<!cd<0ab>>!<!a!bc>]\n0x0119 <0[!ab]!<acd>>\n0x0368 [<bcd><c<b!cd><!0ad>>]\n0x0183 <[!bc]<0a!d><0!a!b>>\n0x07b5 <!<bcd>[cd]![ac]>\n0x0181 <<!ab!c>[b<!ab!c>]<0!d!<!ab!c>>>\n0x036e <!c<bc!d>[!b<0!a!d>]>\n0x011f <!c!d!<!0ab>>\n0x016f <<!ab!c><0a!d>!<abd>>\n0x016e <!d<0!c[bd]>[a[bd]]>\n0x013f <0!<0ad>!<bcd>>\n0x019f <!d<b!c[ad]><0!a!b>>\n0x013e [!<b<!0ac><!0!cd>>![!d<!0!cd>]]\n0x019e <!d!<!bc[!ad]><0!b<!acd>>>\n0x013d <<bc!d>!<!0ac>!<bcd>>\n0x016b <!d<0!b!c>![!b[!ac]]>\n0x01fe [d<!0c<ab!c>>]\n0x166a <a[a<bcd>]!<ab<!bcd>>>\n0x0019 <!<!abd><0!a!b><0b!c>>\n0x006b <!d<0b<!a!bc>>!<!abc>>\n0x069f <!d<abd>!<abc>>\n0x013c <<0!ad><b!d!<!0b!c>>!<bc<0!ad>>>\n0x037e [<!0ad>!<!b!c<0a!d>>]\n0x012f <!d<a!b!c>!<!0ac>>\n0x0693 [<acd><d<!0c!d>![bd]>]\n0x012d <<0ac><!ab!c>!<bcd>>\n0x01ee [!<!0ab>!<d<0!cd><!0ab>>]\n0x012c [<0!a!b><!d[bc]<0!a!b>>]\n0x017f <!b!d!<acd>>\n0x1796 <!b!<!a!bc><!a<!0cd>!<!0!bd>>>\n0x036d <<abc>!<bcd><!a!c[bd]>>\n0x011e <d!<cd!<0!a!b>>!<d<0!a!b>!<0c!d>>>\n0x0679 <!d<!a<abd><0!b!c>>[c<abd>]>\n0x035f <!c!<0bd>!<!0ad>>\n0x067b <!d<!b!c[ad]>[![ad]<0b!c>]>\n0x0118 <<0!a!b><ac!d><!cd<0b!d>>>\n0x067a <[c<abd>]<0a!b>!<acd>>\n0x0117 <!b<!a!d<a!cd>><0!c!<a!cd>>>\n0x1ee1 [[cd]<0!a!b>]\n0x016a <b!<b<!0!ad>!<b!cd>>!<ab<b!cd>>>\n0x0169 <!a<a!b!c><0!d<abc>>>\n0x037d <!a<a!b!c>[!d<0!b!c>]>\n0x0697 <<!a!c<0a!b>>!<!ab!c><!ab!d>>\n0x006f <!d<0!c!d>[ab]>\n0x17ac [<abc><!0d<0!ab>>]\n0x0069 <0!d[a[!bc]]>\n0x0667 <![!ab]<0!b[!ab]>!<cd[!ab]>>\n0x0168 [<bd<!0!bc>><ad<!0bc>>]\n0x067e <!d![c<0!a!d>][ab]>\n0x011b <a!<acd>!<!0ab>>\n0x036a [!<bcd><0!a!d>]\n0x018b <!<0ad><0!b!c><0ab>>\n0x0001 <0<0!a!b><0!c!d>>\n0x1669 <<!cd<!a!bc>><b!d[!ac]>!<bd[!ac]>>\n0x0018 <!<bcd><0ab>!<!0a!c>>\n0x168b <<0!b!c>!<a<a!bd><0!b!c>>!<!a<0!b!c>!<!b!cd>>>\n0x0662 <!<!0bd>[ab][d<abc>]>\n0x00ff !d\n0x168e [!<a!c!<0!b!c>><!0<0!b!c><!ab!d>>]\n0x0007 <0!<!0cd>!<0ab>>\n0x19e6 [[ad]<0b!<0ac>>]\n0x0116 [<!b!d[!ac]><0[!ac]!<!abd>>]\n0x036c [<!a!d<0a!c>>!<!0b<0cd>>]\n0x01e8 <!d<a[bd][cd]><0!ad>>\n0x06f0 <[cd]<a!bc>!<a!bd>>\n0x03d6 [<d!<0!a!d>!<0bc>><bc!<0bc>>]\n0x1be4 [d!<!b!c[ac]>]\n0x0187 <a!<!0ac>!<ad![!bc]>>\n0x0003 <0!d<0!b!c>>\n0x017e [[!bd]<<!ab!c>[!bd]<!0bd>>]\n0x01eb <!d[b<0!a!c>]!<!ab!<0!a!c>>>\n0x0006 <0[ab]<0!c!d>>\n0x011a <0[d[ac]]!<bcd>>\n0x0369 <!b<0!d<abc>><b!c!<0a!d>>>\n0x03d9 [d<<!0!ab><bcd><a!b!d>>]\n0x0000 0\n0x019b <!d<0!b!c>![ab]>\n0x1668 [<bcd><ab<!bcd>>]\n0x18e7 [[!cd]<abc>]\n0x0017 <0!d!<abc>>\n0x019a <!d![a<b!c!d>]!<!0c<b!c!d>>>\n0x0016 <0<a!d<bc!d>>!<abc>>";
};

} /* namespace mockturtle */
