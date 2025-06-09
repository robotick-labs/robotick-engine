# Copyright Robotick Labs
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(robotick-engine)

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

# =========================
# ✅ Public Include Path
# =========================

include_directories(${CMAKE_SOURCE_DIR}/include)

# =====================================================
# ✅ Source Collection (Filtered by Workload-Preset)
# =====================================================

# Discover all .cpp files
file(GLOB_RECURSE ALL_CPP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/robotick/*.cpp
)

# Separate workload and non-workload sources
set(WORKLOAD_SOURCES "")
set(NON_WORKLOAD_SOURCES "")

foreach(SRC_FILE ${ALL_CPP_SOURCES})
    if(SRC_FILE MATCHES ".*/workloads/.*\\.cpp$")
        list(APPEND WORKLOAD_SOURCES ${SRC_FILE})
    else()
        list(APPEND NON_WORKLOAD_SOURCES ${SRC_FILE})
    endif()
endforeach()

# Begin workload filtering
set(WORKLOAD_CONFIG_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeWorkloads.json")
message(STATUS "📄 Looking for workload config at: ${WORKLOAD_CONFIG_PATH}")

if (EXISTS ${WORKLOAD_CONFIG_PATH})
    file(READ ${WORKLOAD_CONFIG_PATH} RAW_WORKLOAD_JSON)

    if (NOT DEFINED WORKLOAD_PRESET)
        message(WARNING "⚠️  WORKLOAD_PRESET not defined (e.g. via -DWORKLOAD_PRESET=ubuntu-x64-debug). No workloads will be included.")
        set(FILTERED_WORKLOAD_SOURCES "")
    else()
        message(STATUS "🔧 Using workload preset: '${WORKLOAD_PRESET}'")

        string(JSON PRESETS_JSON ERROR_VARIABLE JSON_PRESETS_ERR GET ${RAW_WORKLOAD_JSON} presets)
        string(JSON MODE ERROR_VARIABLE JSON_MODE_ERR GET ${PRESETS_JSON} ${WORKLOAD_PRESET} mode)
        string(JSON FILES ERROR_VARIABLE JSON_FILES_ERR GET ${PRESETS_JSON} ${WORKLOAD_PRESET} workloads)

        # Convert JSON array to proper CMake list
        string(JSON FILE_COUNT LENGTH ${FILES})
        set(PARSED_WORKLOAD_LIST "")
        foreach(IDX RANGE 0 ${FILE_COUNT})
            math(EXPR INDEX "${IDX} - 1")
            if (INDEX GREATER_EQUAL 0)
                string(JSON ITEM GET ${FILES} ${INDEX})
                list(APPEND PARSED_WORKLOAD_LIST ${ITEM})
            endif()
        endforeach()

        if(JSON_MODE_ERR OR JSON_FILES_ERR)
            message(WARNING "⚠️  No entry found for workload preset '${WORKLOAD_PRESET}' in CMakeWorkloads.json — no workloads will be included.")
            set(FILTERED_WORKLOAD_SOURCES "")
        else()
            set(FILTERED_WORKLOAD_SOURCES "")
            foreach(SRC_FILE ${WORKLOAD_SOURCES})
                get_filename_component(SRC_NAME ${SRC_FILE} NAME_WE)

                list(FIND PARSED_WORKLOAD_LIST ${SRC_NAME} FILE_MATCH_INDEX)

                # message(STATUS "🔍 Checking workload: ${SRC_NAME} ${FILE_MATCH_INDEX}")           

                if ((MODE STREQUAL "include" AND FILE_MATCH_INDEX GREATER -1) OR
                    (MODE STREQUAL "exclude" AND FILE_MATCH_INDEX EQUAL -1))
                    list(APPEND FILTERED_WORKLOAD_SOURCES ${SRC_FILE})
                endif()
            endforeach()

            message(STATUS "🎯 Applied workload preset '${WORKLOAD_PRESET}' from CMakeWorkloads.json")
            message(STATUS "🔍 Filter mode: ${MODE}")
            message(STATUS "📂 Including workload sources: ${FILTERED_WORKLOAD_SOURCES}")
        endif()
    endif()
else()
    message(WARNING "⚠️  CMakeWorkloads.json not found at: ${WORKLOAD_CONFIG_PATH} — no workloads will be included.")
    set(FILTERED_WORKLOAD_SOURCES "")
endif()

# Final source list = all non-workload files + filtered workload files
set(ALL_CPP_SOURCES ${NON_WORKLOAD_SOURCES} ${FILTERED_WORKLOAD_SOURCES})

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

target_include_directories(robotick-engine PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

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
        ${CMAKE_CURRENT_LIST_DIR}/src/robotick/platform/posix/WebServer.cpp
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
find_package(PahoMqttCpp REQUIRED)
target_link_libraries(robotick-engine PUBLIC PahoMqttCpp::paho-mqttpp3)
target_link_libraries(robotick-engine PRIVATE civetweb)

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
