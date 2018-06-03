add_executable(stg_mapping stg_mapping.cpp)
target_link_libraries(stg_mapping PUBLIC mockturtle)
#target_compile_options(stg_mapping PUBLIC -O0 -g)

add_executable(mighty mighty.cpp)
target_link_libraries(mighty PUBLIC mockturtle)

add_executable(lut_resynthesis lut_resynthesis.cpp)
target_link_libraries(lut_resynthesis PUBLIC mockturtle)
