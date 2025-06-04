# Copyright Robotick Labs
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(robotick-engine)

# =========================
# ✅ Project-wide Settings
# =========================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wall -Wextra -Werror)

# Disable rtti globally (too expensive for our purposes - we've rolled our own simplified equivalent)
add_compile_options(-fno-rtti)

# Optional AddressSanitizer support
option(ROBOTICK_ENABLE_ASAN "Enable AddressSanitizer for runtime memory checks" OFF)
option(ROBOTICK_ASAN_DETECT_LEAKS "Enable ASAN leak detection (not always useful in tests)" OFF)

if(ROBOTICK_ENABLE_ASAN)
    message(STATUS "✅ ASAN enabled")
    if(ROBOTICK_ASAN_DETECT_LEAKS)
        message(STATUS "ℹ️  Run with: ASAN_OPTIONS=detect_leaks=1 ./your_executable")
    endif()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

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
set(PYTHON_SOURCES "")

foreach(file ${ALL_CPP_SOURCES})
    if(file MATCHES ".*/PythonRuntime.cpp$" OR file MATCHES ".*/PythonWorkload.cpp$")
        if(ROBOTICK_ENABLE_PYTHON)
            list(APPEND CORE_SOURCES ${file})
            list(APPEND PYTHON_SOURCES ${file})
        endif()
    else()
        list(APPEND CORE_SOURCES ${file})
    endif()
endforeach()

# =========================
# ✅ Core Framework Library
# =========================

add_library(robotick-engine SHARED ${CORE_SOURCES})
set_target_properties(robotick-engine PROPERTIES OUTPUT_NAME robotick-engine)
target_compile_definitions(robotick-engine PRIVATE ROBOTICK_EXPORTS)

# Disable exceptions in this module unless overridden (e.g. test-suite enables them to catch errors, which it overrides to throw)
target_compile_options(robotick-engine PRIVATE -fno-exceptions)

if(ROBOTICK_PLATFORM_UBUNTU)
    add_compile_definitions(ROBOTICK_PLATFORM_UBUNTU)
    add_compile_definitions(ROBOTICK_PLATFORM_DESKTOP)
elseif(ROBOTICK_PLATFORM_ESP32)
    add_compile_definitions(ROBOTICK_PLATFORM_ESP32)
endif()

# =========================
# ✅ Optional Python Workloads
# =========================

if(ROBOTICK_ENABLE_PYTHON)
    message(STATUS "✅ ROBOTICK_ENABLE_PYTHON - ON")

    # Ensure pybind11 and Python3 are available
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)

    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

    target_compile_definitions(robotick-engine PUBLIC ROBOTICK_ENABLE_PYTHON=1)
    target_link_libraries(robotick-engine PUBLIC pybind11::embed ${Python3_LIBRARIES})

    # 👇 Enable exceptions only on Python-related sources
    foreach(source_file IN LISTS PYTHON_SOURCES)
        set_source_files_properties(${source_file} PROPERTIES COMPILE_FLAGS "-fexceptions -frtti")
    endforeach()
else()
    message(STATUS "❌ ROBOTICK_ENABLE_PYTHON - OFF")
endif()

# =========================
# ✅ Optional Executable
# =========================

add_executable(robotick_main ${CMAKE_SOURCE_DIR}/examples/cpp/desktop-testbed/robotick_main.cpp)
target_link_libraries(robotick_main PRIVATE robotick-engine)

# =========================
# ✅ ESP32 libs (if needed)
# =========================

if(ROBOTICK_PLATFORM_ESP32)
    target_include_directories(robotick_main PRIVATE
        ${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include
        ${IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/xtensa/include
        ${IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos
        ${IDF_PATH}/components/newlib/platform_include
        ${IDF_PATH}/components/esp_common/include
        ${IDF_PATH}/components/esp_system/include
        ${PROJECT_SOURCE_DIR}/esp-idf-dummy
    )
endif()

# =========================
# ✅ Unit Tests via Catch2
# =========================

if (ROBOTICK_BUILD_TESTS)
    message(STATUS "✅ ROBOTICK_BUILD_TESTS - ON")
    enable_testing()
    add_subdirectory(cpp/tests)

    add_custom_command(TARGET robotick-engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:robotick-engine>
            ${CMAKE_BINARY_DIR}/cpp/tests/$<CONFIG>
    )

    add_custom_command(TARGET robotick-engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:robotick-engine>
            ${CMAKE_BINARY_DIR}/Debug
    )
else()
    message(STATUS "❌ ROBOTICK_BUILD_TESTS - OFF")
endif()

# =========================
# ✅ Auto-add License Headers
# =========================

if(ROBOTICK_ENABLE_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

    add_custom_target(add_license
        COMMAND ${CMAKE_COMMAND} -E echo "Adding Apache 2.0 headers..."
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/add_license_header.py ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
