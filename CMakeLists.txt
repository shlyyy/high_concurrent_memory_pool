cmake_minimum_required(VERSION 3.16)

project(memory_pool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable debug symbols in Debug builds
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Source files
set(SOURCES
    common.cpp
    thread_cache.cpp
    central_cache.cpp
    page_cache.cpp
    unit_test.cpp
)

# Create executable
add_executable(memory_pool ${SOURCES})

# Set output directory for executables
# set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# Add target for running tests
add_custom_target(run
    COMMAND ./bin/memory_pool
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS memory_pool
)