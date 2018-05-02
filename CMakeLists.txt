cmake_minimum_required(VERSION 3.1)
project(mockturtle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(MOCKTURTLE_TEST "Build tests" OFF)

# some specific compiler definitions
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fcolor-diagnostics" HAS_FCOLOR_DIAGNOSTICS)
if (HAS_FCOLOR_DIAGNOSTICS)
  add_compile_options(-fcolor-diagnostics)
endif()

# show warnings and fail to compile if there are warnings in Release mode
add_compile_options(-W -Wall)
check_cxx_compiler_flag("-Wno-unknown-pragmas" HAS_WNO_UNKNOWN_PRAGMAS)
if (HAS_WNO_UNKNOWN_PRAGMAS)
  add_compile_options(-Wno-unknown-pragmas)
endif()

add_subdirectory(include)
add_subdirectory(lib)

if(MOCKTURTLE_TEST)
  add_subdirectory(test)
endif()
