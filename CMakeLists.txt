cmake_minimum_required(VERSION 3.16)
project(memory_pool LANGUAGES CXX)

# 基础配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# 调试符号配置
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# 包含目录（修正路径）
include_directories(${CMAKE_SOURCE_DIR}/include)

# 源文件（修正路径）
file(GLOB_RECURSE SOURCES "src/*.cpp" "tests/*.cpp")

# 可执行文件
add_executable(memory_pool ${SOURCES})

# 输出目录（取消注释即可启用） set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# 测试目标（改进版）
add_custom_target(
  run
  COMMAND ./memory_pool
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS memory_pool
  COMMENT "Running memory_pool executable")

# 可选：安装规则 install(TARGETS memory_pool DESTINATION bin)
