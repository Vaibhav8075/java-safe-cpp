cmake_minimum_required(VERSION 3.15)
project(JavaSafeCpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# Directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)

# Add include directories
include_directories(${SRC_DIR})

# Find all source files for the compiler (excluding main.cpp)
file(GLOB_RECURSE COMPILER_SOURCES 
    ${SRC_DIR}/lexer/*.cpp
    ${SRC_DIR}/parser/*.cpp
    ${SRC_DIR}/semantic/*.cpp
    ${SRC_DIR}/bytecode/*.cpp
    ${SRC_DIR}/vm/*.cpp
    ${SRC_DIR}/common/*.cpp
)

# Compiler Library
add_library(jscpp_lib STATIC ${COMPILER_SOURCES})

# Main executable
add_executable(jscpp ${SRC_DIR}/main.cpp)
target_link_libraries(jscpp PRIVATE jscpp_lib)

# Tests: run every example and compare with tests/expected, and check that every
# program in tests/compile_errors is rejected with the expected message.
enable_testing()

file(GLOB EXAMPLE_PROGRAMS ${EXAMPLES_DIR}/*.cppsafe)
foreach(program ${EXAMPLE_PROGRAMS})
    get_filename_component(name ${program} NAME_WE)
    set(args -DEXE=$<TARGET_FILE:jscpp> -DSRC=${program} -DOUT=${TEST_DIR}/expected/${name}.out)
    if(EXISTS ${TEST_DIR}/expected/${name}.err)
        list(APPEND args -DERR=${TEST_DIR}/expected/${name}.err)
    endif()
    add_test(NAME run.${name} COMMAND ${CMAKE_COMMAND} ${args} -P ${TEST_DIR}/run_test.cmake)
    # The same program must behave identically after a save/load through a .jbc file
    add_test(NAME bytecode.${name} COMMAND ${CMAKE_COMMAND} ${args} -DROUNDTRIP=1
             -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR} -P ${TEST_DIR}/run_test.cmake)
endforeach()

file(GLOB REJECTED_PROGRAMS ${TEST_DIR}/compile_errors/*.cppsafe)
foreach(program ${REJECTED_PROGRAMS})
    get_filename_component(name ${program} NAME_WE)
    add_test(NAME reject.${name} COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:jscpp> -DSRC=${program}
             -DERR=${TEST_DIR}/compile_errors/${name}.err -P ${TEST_DIR}/run_test.cmake)
endforeach()
