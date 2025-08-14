# /workspace/robotick-engine/platforms/esp32/components/robotick-engine/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Define the platform before anything else uses it
set(ROBOTICK_PLATFORM_ESP32 TRUE)

set(ROBOTICK_ENGINE_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})


# -----------------------------------------------
# Discover all .cpp files (excluding workloads/)
# -----------------------------------------------
file(GLOB_RECURSE ALL_CPP_SOURCES
    src/robotick/*.cpp
)

if(NOT ROBOTICK_WORKLOADS_USE_ALL)
    # Manually exclude workload files
    set(FILTERED_SOURCES "")
    foreach(src ${ALL_CPP_SOURCES})
        string(FIND "${src}" "/src/robotick/workloads/" match_pos)
        if(match_pos EQUAL -1)
            list(APPEND FILTERED_SOURCES "${src}")
        endif()
    endforeach()
    set(ALL_CPP_SOURCES "${FILTERED_SOURCES}")
endif()

# -----------------------------------------------
# Partition out Python-related files
# -----------------------------------------------
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

# Export results
set(CORE_SOURCES ${CORE_SOURCES} PARENT_SCOPE)
set(PYTHON_SOURCES ${PYTHON_SOURCES} PARENT_SCOPE)

list(LENGTH CORE_SOURCES CORE_COUNT)
list(LENGTH PYTHON_SOURCES PYTHON_COUNT)
message(STATUS "📦 CORE_SOURCES: ${CORE_COUNT}  |  PYTHON_SOURCES: ${PYTHON_COUNT}")

set(srcs ${CORE_SOURCES})
set(include_dirs
    ${CMAKE_CURRENT_LIST_DIR}/include
)

message(STATUS "🧐 Included sources:")
foreach(file ${srcs})
    message(STATUS "  - ${file}")
endforeach()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES json esp_timer M5GFX M5Unified esp_wifi esp_http_server
)



target_compile_definitions(${COMPONENT_LIB} PUBLIC ROBOTICK_PLATFORM_ESP32)
