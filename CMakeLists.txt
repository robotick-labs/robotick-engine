cmake_minimum_required(VERSION 3.16)
project(robotick_framework)

include(FetchContent)  # ← This is the missing piece

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Public include path
include_directories(${CMAKE_SOURCE_DIR}/cpp/include)

# Core source files
file(GLOB_RECURSE CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/cpp/src/robotick/*.cpp
    ${CMAKE_SOURCE_DIR}/cpp/include/robotick/*.h
)

# Build the shared library for Python and plugin use
add_library(robotick_framework SHARED ${CORE_SOURCES})
set_target_properties(robotick_framework PROPERTIES OUTPUT_NAME robotick_framework)

# Optional: Build a test app
add_executable(robotick_main ${CMAKE_SOURCE_DIR}/examples/cpp/robotick_main.cpp)
target_link_libraries(robotick_main PRIVATE robotick_framework)

target_compile_definitions(robotick_framework PRIVATE ROBOTICK_EXPORTS)

# Plugins (if any)
file(GLOB_RECURSE PLUGIN_SOURCES src/plugins/*.cpp)
foreach(plugin_file ${PLUGIN_SOURCES})
    get_filename_component(plugin_name ${plugin_file} NAME_WE)
    add_library(${plugin_name} SHARED ${plugin_file})
    target_include_directories(${plugin_name} PRIVATE include)
endforeach()

# Dependencies:

include(FetchContent)

FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1  # or whatever version you want
)

FetchContent_MakeAvailable(pybind11)

target_link_libraries(robotick_framework PRIVATE pybind11::embed)

# Test-framework: 

option(ROBOTICK_BUILD_TESTS "Build Robotick tests" ON)

if(ROBOTICK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(cpp/tests)

  # Copy framework DLL next to robotick_tests *at CMake config time*
  add_custom_command(TARGET robotick_framework POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:robotick_framework>
      ${CMAKE_BINARY_DIR}/cpp/tests/$<CONFIG>
  )

  # Also copy it to top-level Debug dir for VS Code ctest
  add_custom_command(TARGET robotick_framework POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:robotick_framework>
      ${CMAKE_BINARY_DIR}/Debug
  )
endif()


