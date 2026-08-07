project(nauty LANGUAGES C)

set(NAUTY_BUILD_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/src")
file(COPY "${PROJECT_SOURCE_DIR}/" DESTINATION "${NAUTY_BUILD_SOURCE_DIR}")

execute_process(COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}"
                        "./configure"
                WORKING_DIRECTORY "${NAUTY_BUILD_SOURCE_DIR}"
                RESULT_VARIABLE CONF_RESULT
                OUTPUT_VARIABLE CONF_OUTPUT)
if (NOT CONF_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to configure nauty")
endif()

file(READ "${NAUTY_BUILD_SOURCE_DIR}/makefile" NAUTY_CONFIGURED_MAKEFILE)
if (NAUTY_CONFIGURED_MAKEFILE MATCHES "(^|\n)CFLAGS=([^\n]*)")
  separate_arguments(NAUTY_CONFIGURED_CFLAGS UNIX_COMMAND "${CMAKE_MATCH_2}")
  foreach (NAUTY_CONFIGURED_CFLAG ${NAUTY_CONFIGURED_CFLAGS})
    # Mirror only -m/-D/-U flags from configure; other flags such as -W/-O
    # should not override CMake's warning or optimization settings.
    if (NAUTY_CONFIGURED_CFLAG MATCHES "^(-m|-D|-U)")
      list(APPEND NAUTY_COMPILE_OPTIONS "${NAUTY_CONFIGURED_CFLAG}")
    else()
      list(APPEND NAUTY_IGNORED_COMPILE_OPTIONS "${NAUTY_CONFIGURED_CFLAG}")
    endif()
  endforeach()
endif()
if (NAUTY_IGNORED_COMPILE_OPTIONS)
  message(WARNING "Ignoring nauty configure CFLAGS: ${NAUTY_IGNORED_COMPILE_OPTIONS}")
endif()

file(GLOB FILENAMES "${NAUTY_BUILD_SOURCE_DIR}/*.c")

list(REMOVE_ITEM FILENAMES ${NAUTY_BUILD_SOURCE_DIR}/poptest.c)
list(REMOVE_ITEM FILENAMES ${NAUTY_BUILD_SOURCE_DIR}/sorttemplates.c)
list(REMOVE_ITEM FILENAMES ${NAUTY_BUILD_SOURCE_DIR}/testg.c)
list(REMOVE_ITEM FILENAMES ${NAUTY_BUILD_SOURCE_DIR}/splay.c)
list(REMOVE_ITEM FILENAMES ${NAUTY_BUILD_SOURCE_DIR}/sumlines.c)

add_library(nauty ${FILENAMES})
target_include_directories(nauty INTERFACE "${NAUTY_BUILD_SOURCE_DIR}")

# Suppress warnings from this external library
if (UNIX)
  target_compile_options(nauty PRIVATE -w)
endif()
if (NAUTY_COMPILE_OPTIONS)
  target_compile_options(nauty PRIVATE ${NAUTY_COMPILE_OPTIONS})
endif()
