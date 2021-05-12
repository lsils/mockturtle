include_directories(catch2) # v2.2.1

file(GLOB_RECURSE FILENAMES *.cpp)

# add_executable(run_tests ${FILENAMES})
# target_link_libraries(run_tests PUBLIC mockturtle)
# if (ENABLE_COVERAGE)
#   target_link_libraries(run_tests PUBLIC gcov)
# endif()
# target_compile_definitions(run_tests PUBLIC BENCHMARKS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../experiments/benchmarks")
# target_compile_definitions(run_tests PUBLIC CATCH_CONFIG_CONSOLE_WIDTH=300)

add_executable(error1 test.cpp algorithms/aqfp_resynthesis/dag_gen_error.cpp)
target_link_libraries(error1 PUBLIC mockturtle)

add_executable(error2 test.cpp algorithms/node_resynthesis/cached.cpp)
target_link_libraries(error2 PUBLIC mockturtle)
