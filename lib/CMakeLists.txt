add_library(sparsepp INTERFACE) # v1.1
target_include_directories(sparsepp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sparsepp)

add_library(ez INTERFACE) # v1.0
target_include_directories(ez INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ez)

add_subdirectory(kitty EXCLUDE_FROM_ALL)
