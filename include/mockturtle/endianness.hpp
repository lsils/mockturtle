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

// As I adapted this from RAPIDJSON, here is its MIT liscense header:

// Tencent is pleased to support the open source community by making RAPIDJSON available.
//
// Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#pragma once

#define MOCKTURTLE_LITTLEENDIAN 0
#define MOCKTURTLE_BIGENDIAN 1

#ifndef MOCKTURTLE_ENDIAN
// Detect with GCC 4.6's macro
#ifdef __BYTE_ORDER__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define MOCKTURTLE_ENDIAN MOCKTURTLE_LITTLEENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MOCKTURTLE_ENDIAN MOCKTURTLE_BIGENDIAN
#else
#error Unknown machine endianness detected. User needs to define MOCKTURTLE_ENDIAN.
#endif // __BYTE_ORDER__
// Detect with GLIBC's endian.h
#elif defined( __GLIBC__ )
#include <endian.h>
#if ( __BYTE_ORDER == __LITTLE_ENDIAN )
#define MOCKTURTLE_ENDIAN MOCKTURTLE_LITTLEENDIAN
#elif ( __BYTE_ORDER == __BIG_ENDIAN )
#define MOCKTURTLE_ENDIAN MOCKTURTLE_BIGENDIAN
#else
#error Unknown machine endianness detected. User needs to define MOCKTURTLE_ENDIAN.
#endif // __GLIBC__
// Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro
#elif defined( _LITTLE_ENDIAN ) && !defined( _BIG_ENDIAN )
#define MOCKTURTLE_ENDIAN MOCKTURTLE_LITTLEENDIAN
#elif defined( _BIG_ENDIAN ) && !defined( _LITTLE_ENDIAN )
#define MOCKTURTLE_ENDIAN MOCKTURTLE_BIGENDIAN
// Detect with architecture macros
#elif defined( __sparc ) || defined( __sparc__ ) || defined( _POWER ) || defined( __powerpc__ ) || defined( __ppc__ ) || defined( __hpux ) || defined( __hppa ) || defined( _MIPSEB ) || defined( _POWER ) || defined( __s390__ )
#define MOCKTURTLE_ENDIAN MOCKTURTLE_BIGENDIAN
#elif defined( __i386__ ) || defined( __alpha__ ) || defined( __ia64 ) || defined( __ia64__ ) || defined( _M_IX86 ) || defined( _M_IA64 ) || defined( _M_ALPHA ) || defined( __amd64 ) || defined( __amd64__ ) || defined( _M_AMD64 ) || defined( __x86_64 ) || defined( __x86_64__ ) || defined( _M_X64 ) || defined( __bfin__ )
#define MOCKTURTLE_ENDIAN MOCKTURTLE_LITTLEENDIAN
#elif defined( _MSC_VER ) && ( defined( _M_ARM ) || defined( _M_ARM64 ) )
#define MOCKTURTLE_ENDIAN MOCKTURTLE_LITTLEENDIAN
#elif defined( MOCKTURTLE_DOXYGEN_RUNNING )
#define MOCKTURTLE_ENDIAN
#else
#error Unknown machine endianness detected. User needs to define MOCKTURTLE_ENDIAN.
#endif
#endif // MOCKTURTLE_ENDIAN
