# RobotickWorkloadFilter.cmake
# Output: sets ALL_CPP_SOURCES, CORE_SOURCES, PYTHON_SOURCES

# Discover all .cpp files
file(GLOB_RECURSE ALL_CPP_SOURCES
    ${ROBOTICK_ENGINE_SOURCE_DIR}/src/robotick/*.cpp
)

# -----------------------------------------------
# Resolve WORKLOAD_PRESET from CMake var or ENV
# -----------------------------------------------
if(NOT DEFINED WORKLOAD_PRESET OR "${WORKLOAD_PRESET}" STREQUAL "")
    if(DEFINED ENV{WORKLOAD_PRESET} AND NOT "$ENV{WORKLOAD_PRESET}" STREQUAL "")
        set(WORKLOAD_PRESET "$ENV{WORKLOAD_PRESET}")
        message(STATUS "🌍 WORKLOAD_PRESET loaded from environment: ${WORKLOAD_PRESET}")
    else()
        message(FATAL_ERROR "❌ WORKLOAD_PRESET is not set. Define it in CMake or as an environment variable.")
    endif()
else()
    message(STATUS "📦 WORKLOAD_PRESET set via CMake: ${WORKLOAD_PRESET}")
endif()

# -----------------------------------------------
# Resolve ROBOTICK_PROJECT_ROOT from CMake var or ENV
# -----------------------------------------------
if(NOT DEFINED ROBOTICK_PROJECT_ROOT OR "${ROBOTICK_PROJECT_ROOT}" STREQUAL "")
    if(DEFINED ENV{ROBOTICK_PROJECT_ROOT} AND NOT "$ENV{ROBOTICK_PROJECT_ROOT}" STREQUAL "")
        set(ROBOTICK_PROJECT_ROOT "$ENV{ROBOTICK_PROJECT_ROOT}")
        message(STATUS "🌍 ROBOTICK_PROJECT_ROOT loaded from environment: ${ROBOTICK_PROJECT_ROOT}")
    else()
        message(FATAL_ERROR "❌ ROBOTICK_PROJECT_ROOT is not set. Define it in CMake or as an environment variable.")
    endif()
else()
    message(STATUS "📦 ROBOTICK_PROJECT_ROOT set via CMake: ${ROBOTICK_PROJECT_ROOT}")
endif()

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

# Load config from engine/project

message(STATUS "🔎 CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "🔎 CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "🔎 CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")

set(ENGINE_WORKLOAD_CONFIG "${ROBOTICK_ENGINE_SOURCE_DIR}/CMakeWorkloads.json")

if(DEFINED ROBOTICK_PROJECT_ROOT)
    message(STATUS "📁 ROBOTICK_PROJECT_ROOT = ${ROBOTICK_PROJECT_ROOT}")
    set(PROJECT_WORKLOAD_CONFIG "${ROBOTICK_PROJECT_ROOT}/CMakeWorkloads.json")
else()
    message(WARNING "⚠️ ROBOTICK_PROJECT_ROOT not defined — falling back to CMAKE_SOURCE_DIR")
    set(PROJECT_WORKLOAD_CONFIG "${CMAKE_SOURCE_DIR}/CMakeWorkloads.json")
endif()

message(STATUS "📦 ENGINE_WORKLOAD_CONFIG: ${ENGINE_WORKLOAD_CONFIG}")
message(STATUS "📦 PROJECT_WORKLOAD_CONFIG: ${PROJECT_WORKLOAD_CONFIG}")

set(RAW_WORKLOAD_JSON "")
foreach(PATH IN LISTS PROJECT_WORKLOAD_CONFIG ENGINE_WORKLOAD_CONFIG)
    if(EXISTS ${PATH})
        message(STATUS "📂 Found workload config: ${PATH}")
        file(READ ${PATH} THIS_JSON)
        string(APPEND RAW_WORKLOAD_JSON ${THIS_JSON})
    endif()
endforeach()

set(FILTERED_WORKLOAD_SOURCES "")
set(PARSED_WORKLOAD_LIST "")

if(RAW_WORKLOAD_JSON STREQUAL "")
    message(WARNING "⚠️ No workload config JSON found. All workloads disabled.")
elseif(NOT DEFINED WORKLOAD_PRESET)
    message(WARNING "⚠️ WORKLOAD_PRESET not set. All workloads disabled.")
else()
    string(JSON PRESETS_JSON ERROR_VARIABLE JSON_PRESETS_ERR GET ${RAW_WORKLOAD_JSON} presets)
    string(JSON DUMMY_CHECK ERROR_VARIABLE JSON_PRESET_ERR GET ${PRESETS_JSON} ${WORKLOAD_PRESET})

    if(JSON_PRESET_ERR)
        message(WARNING "⚠️ WORKLOAD_PRESET '${WORKLOAD_PRESET}' not found in presets. No workloads will be included.")
    else()
        string(JSON MODE GET ${PRESETS_JSON} ${WORKLOAD_PRESET} mode)
        string(JSON FILES GET ${PRESETS_JSON} ${WORKLOAD_PRESET} workloads)
        message(STATUS "🎛 Using workload preset '${WORKLOAD_PRESET}' (mode: ${MODE})")

        string(JSON FILE_COUNT LENGTH ${FILES})
        foreach(IDX RANGE 0 ${FILE_COUNT})
            math(EXPR INDEX "${IDX} - 1")
            if(INDEX GREATER_EQUAL 0)
                string(JSON ITEM GET ${FILES} ${INDEX})
                list(APPEND PARSED_WORKLOAD_LIST ${ITEM})
            endif()
        endforeach()

        foreach(SRC_FILE ${WORKLOAD_SOURCES})
            get_filename_component(SRC_NAME ${SRC_FILE} NAME_WE)
            list(FIND PARSED_WORKLOAD_LIST ${SRC_NAME} FILE_MATCH_INDEX)
            if ((MODE STREQUAL "include" AND FILE_MATCH_INDEX GREATER -1) OR
                (MODE STREQUAL "exclude" AND FILE_MATCH_INDEX EQUAL -1))
                list(APPEND FILTERED_WORKLOAD_SOURCES ${SRC_FILE})
                message(STATUS "✅ Including workload: ${SRC_NAME}")
            endif()
        endforeach()
    endif()
endif()

# Compose final sources
set(ALL_CPP_SOURCES ${NON_WORKLOAD_SOURCES} ${FILTERED_WORKLOAD_SOURCES} PARENT_SCOPE)
list(LENGTH FILTERED_WORKLOAD_SOURCES FILTERED_COUNT)
message(STATUS "📦 Final workload source count: ${FILTERED_COUNT}")

# Partition out Python
set(CORE_SOURCES "")
set(PYTHON_SOURCES "")
foreach(file ${NON_WORKLOAD_SOURCES} ${FILTERED_WORKLOAD_SOURCES})
    if(file MATCHES ".*/PythonRuntime.cpp$" OR file MATCHES ".*/PythonWorkload.cpp$")
        if(ROBOTICK_ENABLE_PYTHON)
            list(APPEND CORE_SOURCES ${file})
            list(APPEND PYTHON_SOURCES ${file})
        endif()
    else()
        list(APPEND CORE_SOURCES ${file})
    endif()
endforeach()
set(CORE_SOURCES ${CORE_SOURCES} PARENT_SCOPE)
set(PYTHON_SOURCES ${PYTHON_SOURCES} PARENT_SCOPE)
