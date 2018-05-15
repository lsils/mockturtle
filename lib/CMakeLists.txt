add_library(sparsepp INTERFACE) # v1.1
target_include_directories(sparsepp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sparsepp)

add_library(ez INTERFACE) # v1.0
target_include_directories(ez INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ez)

add_library(mockturtle_fmt INTERFACE)
target_include_directories(mockturtle_fmt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fmt)
target_compile_definitions(mockturtle_fmt INTERFACE FMT_HEADER_ONLY)

add_subdirectory(kitty EXCLUDE_FROM_ALL)

add_library(rang INTERFACE)
target_include_directories(rang INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/rang)

add_library(lorina INTERFACE) # v0.1
target_include_directories(lorina INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lorina)
target_link_libraries(lorina INTERFACE rang mockturtle_fmt)
