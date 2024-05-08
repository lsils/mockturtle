# set(SOURCES ${PROJECT_SOURCE_DIR}/cli/mockturtle.cpp)
# include_directories(${PROJECT_BINARY_DIR}/include/)
add_executable(mockturtle_exe mockturtle_exe.cpp)
target_link_libraries(mockturtle_exe PRIVATE alice mockturtle)