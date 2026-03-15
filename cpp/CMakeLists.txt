# Copyright Robotick contributors
# SPDX-License-Identifier: Apache-2.0

# robotick-engine/cpp/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Detect python interpreter for code policy checks
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# ============================
# Detect Platform Macros
# ============================

set(PLATFORM_DETECTED FALSE)

if (DEFINED ROBOTICK_PLATFORM_LINUX)
    message(STATUS "Platform: ROBOTICK_PLATFORM_LINUX")
    set(PLATFORM_DETECTED TRUE)
    add_compile_definitions(ROBOTICK_PLATFORM_LINUX)
endif()

if (DEFINED ROBOTICK_PLATFORM_ESP32S3)
    message(STATUS "Platform: ROBOTICK_PLATFORM_ESP32S3")
    set(PLATFORM_DETECTED TRUE)
endif()

if (DEFINED ROBOTICK_PLATFORM_DESKTOP)
    message(STATUS "Platform: ROBOTICK_PLATFORM_DESKTOP")
    set(PLATFORM_DETECTED TRUE)
    add_compile_definitions(ROBOTICK_PLATFORM_DESKTOP)
endif()

if (NOT PLATFORM_DETECTED)
    message(FATAL_ERROR "❌ No platform macro defined! Please define at least one of: ROBOTICK_PLATFORM_LINUX, ROBOTICK_PLATFORM_DESKTOP, ROBOTICK_PLATFORM_ESP32S3.")
endif()

# ============================
# Project-wide Settings
# ============================

if (NOT DEFINED ROBOTICK_PLATFORM_ESP32S3)
    project(robotick-engine)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Werror;-fno-rtti>")
endif()

# ============================
# Source Discovery
# ============================

file(GLOB_RECURSE ALL_CPP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/robotick/*.cpp
)

list(LENGTH ALL_CPP_SOURCES ALL_CPP_SOURCES_COUNT)
message(STATUS "ALL_CPP_SOURCES: ${ALL_CPP_SOURCES_COUNT}")

# Minimal civetweb (added before linking)
add_library(civetweb STATIC lib/civetweb/src/civetweb.c)
target_include_directories(civetweb PUBLIC lib/civetweb/include)
target_compile_definitions(civetweb PRIVATE NO_SSL)

# ============================
# Shared Library: robotick-engine
# ============================

add_library(robotick-engine SHARED ${ALL_CPP_SOURCES})
set_target_properties(robotick-engine PROPERTIES OUTPUT_NAME robotick-engine)

target_compile_definitions(robotick-engine PRIVATE ROBOTICK_EXPORTS)
target_compile_options(robotick-engine PRIVATE -fno-exceptions)

target_include_directories(robotick-engine PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/lib/rapidjson/include
)

if (DEFINED ROBOTICK_PLATFORM_LINUX OR DEFINED ROBOTICK_PLATFORM_DESKTOP)
    find_package(CURL REQUIRED)
    find_package(SDL2 REQUIRED)
    target_include_directories(robotick-engine PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/civetweb/include)
    target_link_libraries(robotick-engine PRIVATE civetweb SDL2::SDL2 CURL::libcurl)
endif()

if (DEFINED ROBOTICK_PLATFORM_ESP32S3)
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

# ============================
# Static Library: robotick-engine-static
# ============================

add_library(robotick-engine-static STATIC ${ALL_CPP_SOURCES})
set_target_properties(robotick-engine-static PROPERTIES OUTPUT_NAME robotick-engine)

target_compile_options(robotick-engine-static PRIVATE -fno-exceptions)

target_include_directories(robotick-engine-static PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/lib/rapidjson/include
)

if (DEFINED ROBOTICK_PLATFORM_LINUX OR DEFINED ROBOTICK_PLATFORM_DESKTOP)
    find_package(CURL REQUIRED)
    target_include_directories(robotick-engine-static PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/civetweb/include)
    target_link_libraries(robotick-engine-static PRIVATE civetweb SDL2::SDL2 CURL::libcurl)
endif()

add_custom_target(code_policy_check ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/code_policy_check.py ${CMAKE_CURRENT_SOURCE_DIR}/..
    COMMENT "Checking code policy (STL usage + Apache headers)"
)
add_dependencies(robotick-engine code_policy_check)
add_dependencies(robotick-engine-static code_policy_check)

# ============================
# Unit Tests via Catch2
# ============================

if (ROBOTICK_BUILD_TESTS)
    message(STATUS "ROBOTICK_BUILD_TESTS - ON")

    enable_testing()
    add_subdirectory(tests)

    add_custom_command(TARGET robotick-engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:robotick-engine>
            ${CMAKE_BINARY_DIR}/tests/$<CONFIG>
    )

    add_test(NAME robotick_engine_tests
             COMMAND robotick_engine_tests
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
else()
    message(STATUS "ROBOTICK_BUILD_TESTS - OFF")
endif()
