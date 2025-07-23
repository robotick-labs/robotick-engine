# Copyright Robotick Labs
# SPDX-License-Identifier: Apache-2.0

# robotick-engine/cpp/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

if(ROBOTICK_PLATFORM_LINUX)
    project(robotick-engine)
endif()

# =========================
# ✅ Project-wide Settings
# =========================

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Werror;-fno-rtti>"
    )
endif()

option(ROBOTICK_ENABLE_ASAN "Enable AddressSanitizer for runtime memory checks" OFF)
option(ROBOTICK_ASAN_DETECT_LEAKS "Enable ASAN leak detection" OFF)

if(ROBOTICK_ENABLE_ASAN)
    message(STATUS "✅ ASAN enabled")
    if(ROBOTICK_ASAN_DETECT_LEAKS)
        message(STATUS "ℹ️  Run with: ASAN_OPTIONS=detect_leaks=1 ./your_executable")
    endif()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# ===================================
# ✅ Source Collection
# ===================================

set(ROBOTICK_ENGINE_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${ROBOTICK_ENGINE_SOURCE_DIR}/cmake/RobotickWorkloadFilter.cmake)

# =========================
# ✅ Core Framework Library
# =========================

add_library(robotick-engine SHARED ${CORE_SOURCES})
set_target_properties(robotick-engine PROPERTIES OUTPUT_NAME robotick-engine)
target_compile_definitions(robotick-engine PRIVATE ROBOTICK_EXPORTS)

target_include_directories(robotick-engine PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_options(robotick-engine PRIVATE -fno-exceptions)

message("Determining platform...")

if(ROBOTICK_PLATFORM_LINUX)
    message("✅ ROBOTICK_PLATFORM_LINUX - defining ROBOTICK_PLATFORM_LINUX and ROBOTICK_PLATFORM_DESKTOP")
    target_compile_definitions(robotick-engine PUBLIC
        ROBOTICK_PLATFORM_LINUX
        ROBOTICK_PLATFORM_DESKTOP
    )
elseif(ROBOTICK_PLATFORM_ESP32)
    message("✅ ROBOTICK_PLATFORM_ESP32 - defining ROBOTICK_PLATFORM_ESP32")
    target_compile_definitions(robotick-engine PUBLIC
        ROBOTICK_PLATFORM_ESP32
    )
endif()

# =========================
# ✅ Optional Python Workloads
# =========================

if(ROBOTICK_ENABLE_PYTHON)
    message(STATUS "✅ ROBOTICK_ENABLE_PYTHON - ON")
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

    set(EXCEPTION_ENABLED_SOURCES ${PYTHON_SOURCES})
    list(APPEND EXCEPTION_ENABLED_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/robotick/framework/data/MqttFieldSync.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/robotick/workloads/telemetry/MqttClientWorkload.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/robotick/platform/MqttClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/robotick/platform/WebServer_desktop.cpp
    )

    foreach(source_file IN LISTS EXCEPTION_ENABLED_SOURCES)
        set_source_files_properties(${source_file} PROPERTIES COMPILE_FLAGS "-fexceptions -frtti")
    endforeach()
else()
    message(STATUS "❌ ROBOTICK_ENABLE_PYTHON - OFF")
endif()

# =========================
# ✅ Additional libs
# =========================

target_link_libraries(robotick-engine PUBLIC nlohmann_json::nlohmann_json)

if(ROBOTICK_PLATFORM_LINUX)
    find_package(SDL2 REQUIRED)
    find_package(PahoMqttCpp REQUIRED)
    target_link_libraries(robotick-engine PUBLIC PahoMqttCpp::paho-mqttpp3)
    target_link_libraries(robotick-engine PRIVATE civetweb)
    target_link_libraries(robotick-engine PRIVATE SDL2::SDL2)
    target_link_libraries(robotick-engine PRIVATE ${SDL2_GFX_LIBRARIES})
    target_include_directories(robotick-engine PRIVATE ${SDL2_GFX_INCLUDE_DIRS})
    target_include_directories(robotick-engine PRIVATE /usr/include/SDL2)
    target_link_libraries(robotick-engine PRIVATE SDL2_gfx)
endif()

# =========================
# ✅ ESP32 libs (if needed)
# =========================

if(ROBOTICK_PLATFORM_ESP32)
    target_include_directories(robotick-engine PRIVATE
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
    add_subdirectory(tests)

    add_custom_command(TARGET robotick-engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:robotick-engine>
            ${CMAKE_BINARY_DIR}/tests/$<CONFIG>
    )
else()
    message(STATUS "❌ ROBOTICK_BUILD_TESTS - OFF")
endif()

# =========================
# ✅ Add JSON Submodule
# =========================

add_subdirectory(lib/json)

# =========================
# ✅ Add civetweb (minimal)
# =========================

add_library(civetweb STATIC lib/civetweb/src/civetweb.c)
target_include_directories(civetweb PUBLIC lib/civetweb/include)
target_compile_definitions(civetweb PRIVATE NO_SSL)

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

if (ROBOTICK_BUILD_TESTS)
    include(CTest)
    add_test(NAME robotick_tests
             COMMAND robotick_tests
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/cpp/tests)
endif()
