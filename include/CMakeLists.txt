add_library(mockturtle INTERFACE)
target_include_directories(mockturtle INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(mockturtle INTERFACE ez kitty lorina sparsepp percy json)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8)
target_link_libraries(mockturtle INTERFACE stdc++fs)
endif()
