cmake_minimum_required(VERSION 3.14)
project(robotick_core)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Public include path
include_directories(${CMAKE_SOURCE_DIR}/robotick/cpp/include)

# Core source files
file(GLOB_RECURSE CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/robotick/cpp/src/core/*.cpp
    ${CMAKE_SOURCE_DIR}/robotick/cpp/include/robotick/core/*.hpp
)

# Build the shared library for Python and plugin use
add_library(robotick_core SHARED ${CORE_SOURCES})
set_target_properties(robotick_core PROPERTIES OUTPUT_NAME robotick_core)

# Optional: Build a test app
add_executable(robotick_main ${CMAKE_SOURCE_DIR}/examples/cpp/robotick_main.cpp)
target_link_libraries(robotick_main PRIVATE robotick_core)

target_compile_definitions(robotick_core PRIVATE ROBOTICK_EXPORTS)

# Plugins (if any)
file(GLOB_RECURSE PLUGIN_SOURCES src/plugins/*.cpp)
foreach(plugin_file ${PLUGIN_SOURCES})
    get_filename_component(plugin_name ${plugin_file} NAME_WE)
    add_library(${plugin_name} SHARED ${plugin_file})
    target_include_directories(${plugin_name} PRIVATE include)
endforeach()
