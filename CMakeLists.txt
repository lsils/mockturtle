cmake_minimum_required(VERSION 3.8)
project(mockturtle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(MOCKTURTLE_TEST "Build tests" OFF)

if(UNIX)
  # some specific compiler definitions
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-fcolor-diagnostics" HAS_FCOLOR_DIAGNOSTICS)
  if (HAS_FCOLOR_DIAGNOSTICS)
    add_compile_options(-fcolor-diagnostics)
  endif()

  # show quite some warnings (but remove some intentionally)
  add_compile_options(-W -Wall -Wextra)
  foreach (WARNING unknown-pragmas gnu-anonymous-struct nested-anon-types)
    check_cxx_compiler_flag("-Wno-${WARNING}" HAS_WNO_${WARNING})
    if (HAS_WNO_${WARNING})
      add_compile_options(-Wno-${WARNING})
    endif()
  endforeach()
endif()

add_subdirectory(include)
add_subdirectory(lib)

if(MOCKTURTLE_TEST)
  add_subdirectory(test)
endif()
