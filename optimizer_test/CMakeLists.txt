cmake_minimum_required(VERSION 3.10)
project(OptimizerTest)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(optimizer_test optimizer_test.cpp)

# Windows特定设置
if(WIN32)
    # 设置输出目录
    set_target_properties(optimizer_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

