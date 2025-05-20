cmake_minimum_required(VERSION 3.16)
project(robotick_framework)

# =========================
# ✅ Project-wide Settings
# =========================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # Required for shared libs / Python plugins

# Enable strict warnings for quality enforcement
add_compile_options(-Wall -Wextra -Werror)

# Enable clang-tidy if desired
# set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-p=${CMAKE_BINARY_DIR}")

# =============================
# ✅ Public Include Path (SDK)
# =============================

# NOTE: This is your public API path, used by consumers of the SDK
include_directories(${CMAKE_SOURCE_DIR}/cpp/include)

# ========================================
# ✅ Source Files (Core Engine & API)
# ========================================

# Gather all .cpp and public .h files
file(GLOB_RECURSE CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/cpp/src/robotick/*.cpp
    ${CMAKE_SOURCE_DIR}/cpp/include/robotick/*.h
)

# Build shared library for external use (Python / plugin / SDK)
add_library(robotick_framework SHARED ${CORE_SOURCES})
set_target_properties(robotick_framework PROPERTIES OUTPUT_NAME robotick_framework)

# Define export macro for shared library (ROBOTICK_EXPORTS)
target_compile_definitions(robotick_framework PRIVATE ROBOTICK_EXPORTS)

# =============================
# ✅ Optional Main Executable
# =============================

# Example test app / manual entry point
add_executable(robotick_main ${CMAKE_SOURCE_DIR}/examples/cpp/robotick_main.cpp)
target_link_libraries(robotick_main PRIVATE robotick_framework)

# =========================
# ✅ Plugin Auto-Building
# =========================

# Dynamically find and build each .cpp plugin as its own shared library
file(GLOB_RECURSE PLUGIN_SOURCES src/plugins/*.cpp)

foreach(plugin_file ${PLUGIN_SOURCES})
    get_filename_component(plugin_name ${plugin_file} NAME_WE)
    add_library(${plugin_name} SHARED ${plugin_file})
    target_include_directories(${plugin_name} PRIVATE include)
endforeach()

# =========================
# ✅ Dependencies
# =========================

include(FetchContent)

# pybind11: for Python bindings (embed mode)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.11.1
)

FetchContent_MakeAvailable(pybind11)

target_link_libraries(robotick_framework PRIVATE pybind11::embed)

# Win32 platform-specific libraries
if (WIN32)
    target_link_libraries(robotick_framework PRIVATE winmm)
endif()

# =========================
# ✅ Unit Tests via Catch2
# =========================

option(ROBOTICK_BUILD_TESTS "Build Robotick tests" ON)

if (ROBOTICK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(cpp/tests)

    # Copy DLL next to robotick_tests for local testing (CMake time)
    add_custom_command(TARGET robotick_framework POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:robotick_framework>
            ${CMAKE_BINARY_DIR}/cpp/tests/$<CONFIG>
    )

    # Also copy to root-level Debug folder (for VS Code / ctest discovery)
    add_custom_command(TARGET robotick_framework POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:robotick_framework>
            ${CMAKE_BINARY_DIR}/Debug
    )
endif()

# =========================================
# ✅ Auto-add License Info to cpp Files:
# =========================================

add_custom_target(add_license
    COMMAND ${CMAKE_COMMAND} -E echo "Adding Apache 2.0 headers..."
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/add_license_header.py ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

