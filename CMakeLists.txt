cmake_minimum_required(VERSION 3.16)
project(memory_pool LANGUAGES CXX)

# 基础配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_BUILD_TYPE Debug)

# 添加32位编译选项
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(STATUS "Building as 32-bit on 64-bit host")

  # 设置编译标志
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")

  # 添加32位头文件搜索路径
  include_directories(/usr/include/i386-linux-gnu)

  # 设置32位库搜索路径
  link_directories(/usr/lib/i386-linux-gnu)
else()
  message(STATUS "Building as native 32-bit")
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
