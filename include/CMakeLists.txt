add_library(mockturtle INTERFACE)
target_include_directories(mockturtle SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(mockturtle INTERFACE kitty lorina parallel_hashmap percy json bill libabcesop abcresub)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
target_link_libraries(mockturtle INTERFACE stdc++fs)
endif()

# Compile ABC as a library with the following command and change the path to the generated libabc.a
# make ABC_USE_NAMESPACE=abc ABC_USE_NO_READLINE=1 ABC_USE_NO_CUDD=1 libabc.a
target_link_libraries(mockturtle INTERFACE ${PROJECT_SOURCE_DIR}/lib/abc_static/libabc_linux.a)
target_link_libraries(mockturtle INTERFACE dl)
