cmake_minimum_required(VERSION 3.11)
project(leveldb VERSION 1.23.0 LANGUAGES C CXX)

if(NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED OFF)
    set(CMAKE_C_EXTENSIONS OFF)
endif(NOT CMAKE_C_STANDARD)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif(NOT CMAKE_CXX_STANDARD)

# 包含目录
include_directories(
#   "${PROJECT_BINARY_DIR}/include"
    "include"
    "."
)

# 添加源文件
add_library(leveldb STATIC util/status.cc)

# 添加测试可执行文件
add_executable(status_test util/status_test.cc)

add_subdirectory("third_party/googletest")

# 链接 Google Test 库
target_link_libraries(status_test gtest gtest_main leveldb)

# 启用测试
enable_testing()

# 添加测试
add_test(NAME StatusTest COMMAND status_test)
