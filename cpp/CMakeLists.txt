# Copyright Robotick Labs
# SPDX-License-Identifier: Apache-2.0

# robotick-engine/cpp/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# ============================
# ✅ Detect Platform Macros
# ============================

set(PLATFORM_DETECTED FALSE)

if (DEFINED ROBOTICK_PLATFORM_LINUX)
    message(STATUS "✅ Platform: ROBOTICK_PLATFORM_LINUX")
    set(PLATFORM_DETECTED TRUE)
endif()

if (DEFINED ROBOTICK_PLATFORM_ESP32)
    message(STATUS "✅ Platform: ROBOTICK_PLATFORM_ESP32")
    set(PLATFORM_DETECTED TRUE)
endif()

if (DEFINED ROBOTICK_PLATFORM_DESKTOP)
    message(STATUS "✅ Platform: ROBOTICK_PLATFORM_DESKTOP")
    set(PLATFORM_DETECTED TRUE)
endif()

if (NOT PLATFORM_DETECTED)
    message(FATAL_ERROR "❌ No platform macro defined! Please define at least one of: ROBOTICK_PLATFORM_LINUX, ROBOTICK_PLATFORM_DESKTOP, ROBOTICK_PLATFORM_ESP32.")
endif()

# =========================
# ✅ Project-wide Settings
# =========================

# set project() if not esp32 (idf.py pipeline has its own approach to this):
if (NOT DEFINED ROBOTICK_PLATFORM_ESP32)
    project(robotick-engine)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Werror;-fno-rtti>"
    )
endif()

# -----------------------------------------------
# Discover all .cpp files
# -----------------------------------------------
file(GLOB_RECURSE ALL_CPP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/robotick/*.cpp
)

list(LENGTH ALL_CPP_SOURCES ALL_CPP_SOURCES_COUNT)
message(STATUS "📦 ALL_CPP_SOURCES: ${ALL_CPP_SOURCES_COUNT}")

# =========================
# ✅ Core Framework Library
# =========================

add_library(robotick-engine SHARED ${ALL_CPP_SOURCES})
set_target_properties(robotick-engine PROPERTIES OUTPUT_NAME robotick-engine)
target_compile_definitions(robotick-engine PRIVATE ROBOTICK_EXPORTS)

target_include_directories(robotick-engine PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_options(robotick-engine PRIVATE -fno-exceptions)

# =========================
# ✅ Additional libs
# =========================

target_link_libraries(robotick-engine PUBLIC nlohmann_json::nlohmann_json)

if (DEFINED ROBOTICK_PLATFORM_LINUX)
    find_package(SDL2 REQUIRED)
    target_link_libraries(robotick-engine PRIVATE civetweb)
    target_link_libraries(robotick-engine PRIVATE SDL2::SDL2)
    target_link_libraries(robotick-engine PRIVATE SDL2_gfx)
    target_link_libraries(robotick-engine PRIVATE SDL2_ttf)
    target_include_directories(robotick-engine PRIVATE /usr/include/SDL2)
    target_include_directories(robotick-engine PRIVATE ${SDL2_GFX_INCLUDE_DIRS})
    target_include_directories(robotick-engine PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()

# =========================
# ✅ ESP32 libs (if needed)
# =========================

if (DEFINED ROBOTICK_PLATFORM_ESP32)
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

if (DEFINED ROBOTICK_ENABLE_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    add_custom_target(add_license
        COMMAND ${CMAKE_COMMAND} -E echo "Adding Apache 2.0 headers..."
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/add_license_header.py ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if (ROBOTICK_BUILD_TESTS)
    include(CTest)
    add_test(NAME robotick_engine_tests
             COMMAND robotick_engine_tests
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/cpp/tests)
endif()
