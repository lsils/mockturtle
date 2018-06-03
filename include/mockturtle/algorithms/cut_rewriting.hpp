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
  \file cut_rewriting.hpp
  \brief Cut rewriting

  \author Mathias Soeken
*/

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../networks/mig.hpp"
#include "../utils/node_map.hpp"
#include "../traits.hpp"
#include "cut_enumeration.hpp"

namespace mockturtle
{

struct cut_rewriting_params
{
  cut_rewriting_params()
  {
    cut_enumeration_ps.cut_size = 6;
    cut_enumeration_ps.cut_limit = 12;
    cut_enumeration_ps.minimize_truth_table = true;
  }

  /*! \brief Cut enumeration parameters. */
  cut_enumeration_params cut_enumeration_ps{};

  /*! \brief Maximum number of replacements for each cut. */
  uint32_t max_candidates{1};

  /*! \brief Conflict limit for SAT solving in exact synthesis. */
  int32_t conflict_limit{1000};

  /*! \brief Show progress. */
  bool progress{false};

  /*! \brief Be verbose. */
  bool verbose{false};
};

namespace detail
{

inline mig_network npn4_opt_size_migs()
{
  mig_network mig;
  std::unordered_map<uint16_t, mig_network::signal> class2signal;

  std::vector<uint16_t> classes{{0x1ee1, 0x1be4, 0x1bd8, 0x18e7, 0x17e8, 0x17ac, 0x1798, 0x1796, 0x178e, 0x177e, 0x16e9, 0x16bc, 0x169e, 0x003f, 0x0359, 0x0672, 0x07e9, 0x0693, 0x0358, 0x01bf, 0x6996, 0x0356, 0x01bd, 0x001f, 0x01ac, 0x001e, 0x0676, 0x01ab, 0x01aa, 0x001b, 0x07e1, 0x07e0, 0x0189, 0x03de, 0x035a, 0x1686, 0x0186, 0x03db, 0x0357, 0x01be, 0x1683, 0x0368, 0x0183, 0x03d8, 0x07e6, 0x0182, 0x03d7, 0x0181, 0x03d6, 0x167e,0x016a, 0x007e, 0x0169, 0x006f, 0x0069, 0x0168, 0x0001, 0x019a, 0x036b, 0x1697, 0x0369, 0x0199, 0x0000, 0x169b, 0x003d, 0x036f, 0x0666, 0x019b, 0x0187, 0x03dc, 0x0667, 0x0003, 0x168e, 0x06b6, 0x01eb, 0x07e2, 0x017e, 0x07b6, 0x007f, 0x19e3, 0x06b7, 0x011a, 0x077e, 0x018b, 0x00ff, 0x0673, 0x01a8, 0x000f, 0x1696, 0x036a, 0x011b, 0x0018, 0x0117, 0x1698, 0x036c, 0x01af, 0x0016, 0x067a, 0x0118, 0x0017, 0x067b, 0x0119, 0x169a, 0x003c, 0x036e, 0x07e3, 0x017f, 0x03d4, 0x06f0, 0x011e, 0x037c, 0x012c, 0x19e6, 0x01ef, 0x16a9, 0x037d, 0x006b,0x012d, 0x012f, 0x01fe, 0x0019, 0x03fc, 0x179a, 0x013c, 0x016b, 0x06f2, 0x03c0, 0x033c, 0x1668, 0x0669, 0x019e, 0x013d, 0x0006, 0x019f, 0x013e, 0x0776, 0x013f, 0x016e, 0x03c3, 0x3cc3, 0x033f, 0x166b, 0x016f, 0x011f, 0x035e, 0x0690, 0x0180, 0x03d5, 0x06f1, 0x06b0, 0x037e, 0x03c1, 0x03c5, 0x03c6, 0x01a9, 0x166e, 0x03cf, 0x03d9, 0x07bc, 0x01bc, 0x1681, 0x03dd, 0x03c7, 0x06f9, 0x0660, 0x0196, 0x0661, 0x0197, 0x0662, 0x07f0, 0x0198, 0x0663, 0x07f1, 0x0007, 0x066b, 0x033d, 0x1669, 0x066f, 0x01ad, 0x0678, 0x01ae, 0x0679, 0x067e, 0x168b,0x035f, 0x0691, 0x0696, 0x0697, 0x06b1, 0x0778, 0x16ac, 0x06b2, 0x0779, 0x16ad, 0x01e8, 0x06b3, 0x0116, 0x077a, 0x01e9, 0x06b4, 0x19e1, 0x01ea, 0x06b5, 0x01ee, 0x06b9, 0x06bd, 0x06f6, 0x07b0, 0x07b1, 0x07b4, 0x07b5, 0x07f2, 0x07f8, 0x018f, 0x0ff0, 0x166a, 0x035b, 0x1687, 0x1689, 0x036d, 0x069f, 0x1699}};
  std::vector<uint16_t> nodes{{4, 222, 17, 24, 34, 41, 46, 56, 68, 76, 84, 96, 109, 116, 122, 127, 137, 142, 151, 157, 166, 173, 182, 188, 193, 199, 208, 214, 220, 227, 232, 239, 247, 256, 259, 264, 272, 278, 286, 293, 297, 300, 307, 312, 321, 328, 336, 344, 351, 355, 362, 372, 378, 384, 387, 389, 393, 398, 401, 408, 417, 421, 425, 433, 0, 439, 445, 451, 454, 459, 467, 472, 475, 477, 482, 486, 491, 498, 502, 506, 509, 517, 523, 526, 532, 537, 9, 545, 548, 335, 554, 560, 563, 568, 573, 576, 580, 583, 586, 594, 596, 599, 603, 605, 612, 616, 622, 627, 629, 630, 634, 638, 640, 644, 650, 657, 665, 669, 675, 677, 679, 686, 691, 696, 702, 708, 713, 718, 722, 728, 738, 747, 750, 755, 756, 761, 766, 770, 773, 778, 785, 789, 159, 797, 801, 803, 810, 812, 820, 827, 831, 836, 844, 853, 857, 862, 869, 876, 879, 887, 892, 900, 911, 915, 921, 927, 930, 938, 941, 951, 954, 960, 966, 971, 975, 977, 985, 991, 1003, 1007, 1011, 1014, 1020, 1027, 1030, 1037, 1039, 1041, 1044, 1049, 1053, 1060, 1066, 1070, 1073, 1077, 1082, 1093, 1096, 1100, 1107, 1112, 1119, 1124, 1135, 1138, 1141, 1147, 1148, 1152, 1159, 1166, 1175, 1178, 1186, 1191, 1194, 1202, 1205, 1213, 1221, 1229, 1231, 1237, 1, 2, 4, 6, 8, 11, 9, 10, 12, 7, 12, 14, 0, 2, 7, 8, 10, 19, 8, 10, 21, 18, 20, 23, 5, 6, 8, 2, 4, 6, 0, 26, 28, 1, 26, 28, 0, 31, 32, 6, 9, 28, 8, 29, 36, 7, 36, 38, 0, 8, 28, 0, 8, 43, 28, 43, 44, 0, 5, 8, 4, 7, 48, 0, 2, 8, 2, 6, 48, 50, 53, 54, 0, 4, 9, 0, 2, 59, 0, 2, 58, 7, 8, 62, 2, 5, 6, 61, 64, 66, 0, 6, 9, 1, 4, 8, 2, 71, 72, 29, 70, 74, 0, 4, 8, 2, 4, 7, 0, 3, 8, 79, 80, 82, 0, 7, 8, 2, 7, 86, 4, 6, 87, 0, 6, 8, 2, 4, 92, 88, 90, 95, 0, 2, 4, 1, 6, 98, 2, 4, 99, 8, 100, 103, 101, 102, 104, 9, 104, 106, 1, 4, 6, 0, 9, 110, 2, 8, 110, 29, 112, 114, 0, 3, 6, 4, 52, 118, 80, 118, 121, 0, 4, 6, 1, 8, 124, 0, 2, 9, 0, 4, 128, 6, 9, 130, 4, 6, 131, 128, 133, 134, 0, 2, 5, 3, 6, 78, 93, 138, 140, 0, 9, 28, 2, 4, 9, 0, 6, 147, 145, 146, 148, 4, 8, 71, 2, 4, 8, 28, 152, 155, 4, 6, 8, 0, 8, 159, 2, 5, 158, 2, 6, 83, 160, 163, 164, 1, 2, 6, 0, 3, 4, 8, 168, 170, 3, 6, 18, 1, 18, 174, 4, 9, 176, 5, 8, 176, 177, 178, 180, 2, 82, 110, 2, 83, 110, 82, 185, 186, 2, 7, 78, 99, 158, 190, 0, 5, 6, 0, 3, 194, 6, 8, 197, 4, 8, 52, 4, 7, 8, 2, 6, 200, 1, 202, 204, 0, 201, 206, 0, 6, 10, 6, 9, 10, 0, 211, 212, 6, 9, 98, 1, 80, 216, 0, 99, 218, 4, 6, 53, 3, 52, 222, 1, 52, 224, 3, 6, 170, 2, 8, 228, 8, 128, 231, 2, 6, 8, 3, 4, 8, 1, 234, 236, 1, 6, 146, 6, 146, 241, 0, 9, 242, 0, 240, 245, 2, 5, 8, 4, 6, 248, 1, 8, 250, 0, 9, 252, 251, 252, 254, 10, 131, 194, 4, 7, 248, 0, 6, 249, 79, 260, 262, 0, 4, 7, 6, 8, 266, 3, 6, 8, 18, 269, 270, 2, 7, 8, 4, 82, 275, 5, 80, 276, 2, 8, 266, 4, 8, 281, 2, 7, 282, 0, 281, 284, 5, 8, 28, 0, 4, 29, 110, 288, 290, 0, 2, 110, 8, 110, 294, 1, 6, 236, 128, 159, 298, 4, 6, 83, 2, 4, 303, 274, 302, 305, 6, 58, 128, 8, 159, 308, 129, 308, 310, 5, 6, 170, 2, 7, 170, 4, 8, 316, 1, 314, 318, 2, 6, 9, 0, 249, 322, 0, 110, 325, 8, 324, 327, 2, 9, 170, 4, 6, 171, 1, 6, 8, 330, 333, 334, 2, 5, 266, 6, 8, 338, 2, 8, 267, 0, 341, 342, 3, 4, 6, 0, 8, 110, 110, 347, 348, 4, 8, 170, 125, 168, 352, 0, 9, 346, 1, 2, 8, 4, 194, 358, 356, 358, 361, 3, 6, 266, 1, 2, 266, 0, 2, 6, 4, 8, 368, 364, 366, 371, 7, 26, 128, 3, 6, 374, 27, 374, 376, 4, 9, 66, 0, 67, 380, 5, 380, 382, 2, 145, 346, 8, 66, 139, 7, 66, 346, 1, 8, 390, 6, 8, 80, 0, 28, 395, 8, 395, 396, 1, 10, 12, 0, 3, 26, 2, 9, 402, 0, 6, 26, 402, 404, 407, 4, 6, 129, 8, 128, 410, 4, 7, 412, 5, 410, 414, 2, 8, 158, 8, 28, 419, 4, 71, 128, 5, 410, 422, 1, 6, 10, 3, 8, 10, 0, 5, 10, 426, 428, 430, 3, 4, 86, 2, 26, 434, 87, 434, 436, 2, 7, 266, 8, 267, 440, 4, 267, 442, 4, 6, 369, 8, 368, 446, 5, 446, 448, 2, 4, 93, 3, 138, 452, 2, 8, 194, 3, 10, 456, 0, 8, 154, 6, 155, 460, 0, 7, 154, 1, 462, 464, 4, 9, 196, 4, 194, 469, 8, 468, 471, 1, 12, 98, 1, 4, 334, 1, 6, 80, 2, 8, 479, 48, 80, 481, 2, 29, 70, 10, 29, 484, 0, 4, 70, 52, 346, 489, 0, 8, 93, 2, 93, 492, 4, 7, 92, 170, 494, 497, 3, 6, 160, 146, 159, 500, 1, 2, 202, 29, 70, 504, 8, 98, 334, 4, 6, 78, 4, 6, 9, 3, 78, 512, 154, 511, 514, 0, 2, 67, 8, 171, 518, 6, 67, 520, 0, 2, 27, 26, 235, 524, 6, 8, 524, 5, 26, 524, 4, 529, 530, 3, 4, 194, 1, 456, 534, 1, 4, 270, 5, 6, 538, 0, 8, 540, 271, 538, 542, 1, 2, 110, 112, 358, 547, 4, 9, 82, 2, 6, 29, 29, 550, 552, 4, 128, 202, 4, 202, 557, 128, 557, 558, 3, 10, 234, 0, 5, 346, 4, 9, 346, 347, 564, 566, 4, 6, 358, 1, 234, 570, 0, 6, 29, 43, 154, 574, 5, 86, 368, 4, 371, 578, 6, 129, 190, 4, 9, 168, 0, 29, 584, 6, 8, 98, 2, 118, 589, 4, 8, 98, 589, 590, 592, 130, 159, 270, 1, 8, 28, 0, 4, 271, 8, 465, 600, 10, 248, 346, 0, 2, 249, 6, 8, 249, 1, 2, 608, 29, 606, 610, 6, 8, 195, 4, 194, 615, 5, 8, 128, 8, 128, 158, 4, 618, 621, 0, 147, 240, 202, 240, 624, 2, 8, 346, 1, 160, 356, 8, 81, 98, 8, 70, 633, 3, 4, 26, 159, 524, 636, 26, 58, 589, 0, 28, 159, 159, 236, 642, 5, 8, 18, 2, 8, 18, 146, 646, 649, 4, 6, 139, 4, 8, 138, 5, 652, 654, 3, 8, 110, 2, 111, 658, 0, 8, 513, 658, 660, 663, 2, 6, 73, 71, 158, 666, 0, 6, 67, 3, 4, 66, 8, 671, 672, 66, 158, 369, 6, 129, 154, 0, 4, 169, 8, 169, 680, 8, 680, 683, 168, 682, 685, 4, 8, 19, 2, 99, 688, 1, 8, 110, 0, 9, 692, 111, 692, 694, 7, 8, 128, 0, 4, 129, 346, 698, 701, 9, 194, 202, 2, 5, 202, 3, 704, 706, 2, 9, 124, 2, 346, 711, 4, 8, 70, 1, 2, 714, 29, 70, 716, 6, 26, 407, 0, 407, 720, 0, 4, 159, 7, 8, 724, 6, 159, 726, 6, 8, 159, 2, 4, 730, 1, 158, 732, 158, 732, 735, 0, 734, 737, 2, 4, 335, 2, 5, 334, 3, 740, 742, 1, 92, 744, 2, 7, 72, 9, 402, 748, 0, 2, 29, 1, 158, 752, 0, 29, 146, 4, 7, 358, 8, 10, 759, 4, 8, 66, 8, 66, 763, 266, 763, 764, 5, 6, 274, 93, 170, 768, 2, 129, 158, 0, 4, 235, 2, 9, 774, 235, 248, 776, 5, 6, 78, 1, 4, 780, 7, 780, 782, 5, 6, 202, 9, 202, 786, 0, 3, 158, 6, 202, 791, 2, 159, 790, 0, 792, 795, 0, 9, 146, 2, 346, 799, 6, 8, 10, 4, 8, 92, 4, 6, 805, 0, 3, 806, 274, 805, 808, 29, 70, 154, 6, 8, 81, 1, 6, 814, 0, 7, 816, 815, 816, 818, 4, 8, 269, 2, 266, 823, 1, 268, 824, 0, 8, 81, 71, 146, 828, 0, 2, 71, 4, 6, 832, 70, 154, 835, 5, 6, 128, 4, 8, 839, 4, 8, 838, 838, 840, 843, 0, 4, 202, 2, 6, 203, 1, 4, 848, 5, 846, 850, 0, 9, 18, 59, 110, 854, 0, 4, 275, 4, 93, 274, 5, 858, 860, 1, 4, 66, 0, 7, 66, 129, 864, 866, 6, 8, 791, 0, 4, 870, 2, 4, 159, 790, 873, 874, 1, 78, 194, 2, 8, 99, 4, 7, 98, 1, 86, 98, 880, 882, 885, 8, 111, 170, 6, 111, 170, 112, 888, 891, 3, 8, 512, 0, 4, 894, 0, 7, 894, 512, 897, 898, 2, 4, 147, 6, 8, 903, 7, 146, 904, 0, 8, 903, 904, 906, 909, 2, 8, 98, 2, 268, 913, 1, 8, 18, 4, 194, 916, 1, 194, 918, 2, 4, 155, 0, 7, 922, 8, 465, 924, 2, 4, 334, 0, 95, 928, 3, 6, 72, 0, 9, 932, 4, 6, 935, 128, 932, 937, 1, 12, 740, 1, 2, 170, 5, 170, 942, 6, 8, 944, 7, 942, 946, 945, 946, 948, 2, 93, 158, 0, 95, 952, 6, 8, 93, 8, 92, 98, 0, 956, 959, 4, 8, 195, 2, 9, 194, 11, 962, 964, 4, 270, 539, 92, 538, 969, 0, 7, 146, 1, 92, 972, 1, 334, 928, 6, 8, 589, 3, 4, 978, 0, 4, 978, 588, 980, 983, 1, 4, 274, 4, 8, 159, 158, 986, 989, 2, 8, 155, 4, 6, 992, 1, 154, 994, 4, 992, 997, 0, 155, 996, 6, 998, 1001, 0, 99, 102, 6, 8, 1005, 2, 6, 266, 168, 268, 1009, 0, 6, 155, 154, 589, 1012, 1, 8, 158, 3, 4, 1016, 128, 159, 1018, 1, 6, 154, 2, 4, 1023, 8, 465, 1024, 4, 7, 18, 6, 589, 1028, 6, 124, 237, 4, 6, 82, 236, 1032, 1035, 8, 110, 368, 87, 236, 706, 3, 4, 70, 2, 29, 1042, 2, 6, 138, 28, 248, 1047, 3, 6, 48, 71, 146, 1050, 7, 8, 98, 0, 6, 99, 8, 99, 1056, 9, 1054, 1058, 4, 52, 81, 1, 4, 234, 0, 1063, 1064, 0, 3, 154, 66, 93, 1068, 99, 588, 740, 2, 8, 111, 49, 1050, 1074, 3, 358, 570, 0, 9, 570, 571, 1078, 1080, 2, 8, 124, 7, 124, 1084, 4, 8, 1086, 4, 1084, 1089, 8, 1089, 1090, 159, 248, 346, 0, 235, 1094, 0, 358, 589, 6, 589, 1098, 0, 8, 478, 2, 4, 81, 478, 1102, 1105, 4, 8, 81, 0, 3, 80, 334, 1109, 1110, 4, 71, 138, 5, 6, 1114, 235, 1114, 1116, 1, 6, 26, 2, 6, 26, 128, 1120, 1123, 3, 4, 52, 6, 52, 1127, 5, 8, 52, 2, 6, 53, 1129, 1130, 1132, 1, 4, 698, 128, 155, 1136, 87, 236, 646, 3, 6, 52, 5, 6, 52, 248, 1142, 1145, 10, 29, 70, 70, 147, 358, 7, 70, 1150, 2, 4, 86, 4, 18, 1155, 8, 87, 1156, 0, 8, 237, 6, 52, 236, 6, 236, 1161, 1160, 1163, 1164, 0, 3, 146, 6, 8, 1168, 6, 1168, 1171, 146, 1170, 1173, 0, 29, 274, 9, 334, 1176, 7, 8, 28, 0, 29, 1180, 1, 6, 1180, 9, 1182, 1184, 2, 8, 170, 1, 314, 1188, 0, 8, 87, 6, 86, 1193, 2, 6, 158, 0, 154, 1197, 1, 2, 158, 155, 1198, 1200, 19, 234, 266, 4, 8, 235, 0, 6, 234, 3, 4, 1208, 6, 1206, 1211, 1, 6, 922, 8, 154, 1215, 9, 1214, 1216, 155, 1216, 1218, 2, 9, 368, 4, 7, 1222, 4, 9, 368, 6, 1224, 1227, 3, 28, 288, 4, 86, 237, 2, 4, 1233, 86, 1233, 1234}};

  std::vector<mig_network::signal> signals;
  signals.push_back( mig.get_constant( false ) );

  auto p = nodes.begin();
  for ( auto i = 0u; i < *p; ++i )
  {
    signals.push_back( mig.create_pi() );
  }

  p++;         /* point to number of outputs */
  p += *p + 1; /* move past number of output */

  /* create nodes */
  while ( p != nodes.end() )
  {
    const auto c1 = signals[*p >> 2] ^ ( *p & 1 );
    ++p;
    const auto c2 = signals[*p >> 2] ^ ( *p & 1 );
    ++p;
    const auto c3 = signals[*p >> 2] ^ ( *p & 1 );
    ++p;

    signals.push_back( mig.create_maj( c1, c2, c3 ) );
  }

  /* create PIs */
  p = nodes.begin() + 2;
  for ( auto i = 0u; i < nodes[1]; ++i )
  {
    const auto driver = signals[*p >> 2] ^ ( *p & 1 );
    ++p;

    mig.create_po( driver );
    class2signal[classes[i]] = driver;
  }
}

template<typename Ntk, typename TermCond>
uint32_t recursive_deref( Ntk const& ntk, node<Ntk> const& n, TermCond&& terminate )
{
  /* terminate? */
  if ( terminate( n ) )
    return 0;

  /* recursively collect nodes */
  uint32_t value{1};
  ntk.foreach_fanin( n, [&]( auto const& s ) {
    if ( ntk.decr_value( ntk.get_node( s ) ) == 0 )
    {
      value += recursive_deref( ntk, ntk.get_node( s ), terminate );
    }
  } );
  return value;
}

template<typename Ntk, typename TermCond>
uint32_t recursive_ref( Ntk const& ntk, node<Ntk> const& n, TermCond&& terminate )
{
  /* terminate? */
  if ( terminate( n ) )
    return 0;

  /* recursively collect nodes */
  uint32_t value{1};
  ntk.foreach_fanin( n, [&]( auto const& s ) {
    if ( ntk.incr_value( ntk.get_node( s ) ) == 0 )
    {
      value += recursive_ref( ntk, ntk.get_node( s ), terminate );
    }
  } );
  return value;
}

template<typename Ntk, typename LeavesIterator>
uint32_t recursive_deref( Ntk const& ntk, node<Ntk> const& n, LeavesIterator begin, LeavesIterator end )
{
  return recursive_deref( ntk, n, [&]( auto const& n ) { return std::find( begin, end, n ) != end; } );
}

template<typename Ntk, typename LeavesIterator>
uint32_t recursive_ref( Ntk const& ntk, node<Ntk> const& n, LeavesIterator begin, LeavesIterator end )
{
  return recursive_ref( ntk, n, [&]( auto const& n ) { return std::find( begin, end, n ) != end; } );
}

template<typename Ntk>
uint32_t recursive_deref( Ntk const& ntk, node<Ntk> const& n )
{
  return recursive_deref( ntk, n, [&]( auto const& n ) { return ntk.is_constant( n ) || ntk.is_pi( n ); } );
}

template<typename Ntk>
uint32_t recursive_ref( Ntk const& ntk, node<Ntk> const& n )
{
  return recursive_ref( ntk, n, [&]( auto const& n ) { return ntk.is_constant( n ) || ntk.is_pi( n ); } );
}

template<typename Ntk>
uint32_t mffc_size( Ntk const& ntk, node<Ntk> const& n )
{
  auto v1 = recursive_deref( ntk, n );
  auto v2 = recursive_ref( ntk, n );
  assert( v1 == v2 );
  return v1;
}

template<class Ntk>
class cut_rewriting_impl
{
public:
  cut_rewriting_impl( Ntk& ntk, cut_rewriting_params const& ps )
      : ntk( ntk ),
        ps( ps ) {}

  void run()
  {
    /* enumerate cuts */
    const auto cuts = cut_enumeration<Ntk, true>( ntk, ps.cut_enumeration_ps );

    /* for cost estimation we use reference counters initialized by the fanout size */
    ntk.clear_values();
    ntk.foreach_node( [&]( auto const& n ) {
      ntk.set_value( n, ntk.fanout_size( n ) );
    } );

    /* store best replacement for each cut */
    node_map<std::vector<node<Ntk>>, Ntk> best_replacements( ntk );

    /* iterate over all original nodes in the network */
    const auto size = ntk.size();
    ntk.foreach_node( [&]( auto const& n ) {
      /* stop once all original nodes were visited */
      if ( n >= size )
        return false;

      /* do not iterate over constants or PIs */
      if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
        return true;

      /* skip cuts with small MFFC */
      if ( mffc_size( ntk, n ) == 1 )
        return true;

      /* foreach cut */
      for ( auto& cut : cuts.cuts( n ) )
      {
        /* skip trivial cuts */
        // TODO application specific filter
        if ( cut->size() < 3 )
          continue;

        const auto tt = cuts.truth_table( *cut );
        assert( cut->size() == tt.num_vars() );

        std::vector<node<Ntk>> target_leaves( cut->begin(), cut->end() );
        int32_t best_gain{0};
        signal<Ntk> best_replacement{0};

        //( *cut )->data.gain = best_gain;
        if ( best_gain )
        {
          best_replacements[n].push_back( best_replacement );
        }
      }

      return true;
    } );
  }

private:
  Ntk& ntk;
  cut_rewriting_params const& ps;
};

} /* namespace detail */

template<class Ntk>
void cut_rewriting( Ntk& ntk, cut_rewriting_params const& ps = {} )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size method" );
  static_assert( has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method" );
  static_assert( has_clear_values_v<Ntk>, "Ntk does not implement the clear_values method" );
  static_assert( has_incr_value_v<Ntk>, "Ntk does not implement the incr_value method" );
  static_assert( has_decr_value_v<Ntk>, "Ntk does not implement the decr_value method" );
  static_assert( has_set_value_v<Ntk>, "Ntk does not implement the set_value method" );

  detail::cut_rewriting_impl<Ntk> p( ntk, ps );
  p.run();
}

} /* namespace mockturtle */
