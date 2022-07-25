project(nauty LANGUAGES C)

include_directories(${PROJECT_SOURCE_DIR})

execute_process(COMMAND "./configure"
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
		RESULT_VARIABLE CONF_RESULT
		OUTPUT_VARIABLE CONF_OUTPUT)

file(GLOB FILENAMES *.c)

list(REMOVE_ITEM FILENAMES ${PROJECT_SOURCE_DIR}/poptest.c)
list(REMOVE_ITEM FILENAMES ${PROJECT_SOURCE_DIR}/sorttemplates.c)
list(REMOVE_ITEM FILENAMES ${PROJECT_SOURCE_DIR}/testg.c)
list(REMOVE_ITEM FILENAMES ${PROJECT_SOURCE_DIR}/splay.c)
list(REMOVE_ITEM FILENAMES ${PROJECT_SOURCE_DIR}/sumlines.c)

# Surpress warnings from this external library
if (UNIX)
    set(CMAKE_C_FLAGS "-w")
endif()

add_library(nauty ${FILENAMES})
target_include_directories(nauty INTERFACE ${PROJECT_SOURCE_DIR})
