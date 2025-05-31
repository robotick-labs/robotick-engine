cmake_minimum_required(VERSION 3.16)
project(robotick_framework)

# =========================
# ✅ Project-wide Settings
# =========================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wall -Wextra -Werror)

# Disable exceptions globally unless overridden
add_compile_options(-fno-exceptions)

# Enable/disable Python workloads
option(ROBOTICK_ENABLE_PYTHON "Enable Python workloads and pybind11" ON)

# =========================
# ✅ Public Include Path
# =========================

include_directories(${CMAKE_SOURCE_DIR}/cpp/include)

# =========================
# ✅ Source Collection
# =========================

file(GLOB_RECURSE ALL_CPP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/cpp/src/robotick/*.cpp
)

set(CORE_SOURCES "")
foreach(file ${ALL_CPP_SOURCES})
    if(NOT file MATCHES ".*/PythonRuntime.cpp$" AND NOT file MATCHES ".*/PythonWorkload.cpp$")
        list(APPEND CORE_SOURCES ${file})
    endif()
endforeach()

# =========================
# ✅ Core Framework Library
# =========================

add_library(robotick_framework SHARED ${CORE_SOURCES})
set_target_properties(robotick_framework PROPERTIES OUTPUT_NAME robotick_framework)
target_compile_definitions(robotick_framework PRIVATE ROBOTICK_EXPORTS)

if(ROBOTICK_PLATFORM_UBUNTU)
    add_compile_definitions(ROBOTICK_PLATFORM_UBUNTU)
endif()

# =========================
# ✅ Optional Python Workloads
# =========================

if(ROBOTICK_ENABLE_PYTHON)
    message(STATUS "✅ Python Workloads ENABLED")

    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)

    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

    add_library(robotick_python SHARED
        cpp/src/robotick/framework/utils/PythonRuntime.cpp
        cpp/src/robotick/workloads/scripting/PythonWorkload.cpp
    )

    target_compile_options(robotick_python PRIVATE -fexceptions)
    target_compile_definitions(robotick_python PRIVATE ROBOTICK_ENABLE_PYTHON=1)
    target_include_directories(robotick_python PRIVATE
        ${pybind11_SOURCE_DIR}/include
        ${Python3_INCLUDE_DIRS}
    )
    target_link_libraries(robotick_python PRIVATE pybind11::embed ${Python3_LIBRARIES})

    target_link_libraries(robotick_framework PRIVATE robotick_python)

else()
    message(STATUS "❌ Python Workloads DISABLED")
endif()

# =========================
# ✅ Optional Executable
# =========================

add_executable(robotick_main ${CMAKE_SOURCE_DIR}/examples/cpp/robotick_main.cpp)
target_link_libraries(robotick_main PRIVATE robotick_framework)

# =========================
# ✅ Plugin Auto-Building
# =========================

file(GLOB_RECURSE PLUGIN_SOURCES src/plugins/*.cpp)
foreach(plugin_file ${PLUGIN_SOURCES})
    get_filename_component(plugin_name ${plugin_file} NAME_WE)
    add_library(${plugin_name} SHARED ${plugin_file})
    target_include_directories(${plugin_name} PRIVATE include)
endforeach()

# =========================
# ✅ Unit Tests via Catch2
# =========================

option(ROBOTICK_BUILD_TESTS "Build Robotick tests" ON)

if (ROBOTICK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(cpp/tests)

    add_custom_command(TARGET robotick_framework POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:robotick_framework>
            ${CMAKE_BINARY_DIR}/cpp/tests/$<CONFIG>
    )

    add_custom_command(TARGET robotick_framework POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:robotick_framework>
            ${CMAKE_BINARY_DIR}/Debug
    )
endif()

# =========================
# ✅ Auto-add License Headers
# =========================

add_custom_target(add_license
    COMMAND ${CMAKE_COMMAND} -E echo "Adding Apache 2.0 headers..."
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/add_license_header.py ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
