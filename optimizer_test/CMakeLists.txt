cmake_minimum_required(VERSION 3.10)
project(OptimizerTest)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
        find_program(MINGW_GCC gcc.exe)
        find_program(MINGW_GXX g++.exe)
        if(MINGW_GCC AND MINGW_GXX)
            set(CMAKE_C_COMPILER "${MINGW_GCC}")
            set(CMAKE_CXX_COMPILER "${MINGW_GXX}")
            message(STATUS "Found MinGW compiler: ${MINGW_GXX}")
        endif()
    endif()
endif()

add_library(optimizer_lib optimizer.cpp)
add_executable(optimizer_test test_cases.cpp)
target_link_libraries(optimizer_test optimizer_lib)
add_executable(test_data_generator test_data_generator.cpp)

if(WIN32)
    set_target_properties(optimizer_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    set_target_properties(test_data_generator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()
