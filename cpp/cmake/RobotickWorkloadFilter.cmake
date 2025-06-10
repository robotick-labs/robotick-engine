# RobotickWorkloadFilter.cmake
# Output: sets ALL_CPP_SOURCES, CORE_SOURCES, PYTHON_SOURCES

# Discover all .cpp files
file(GLOB_RECURSE ALL_CPP_SOURCES CONFIGURE_DEPENDS
    ${ROBOTICK_ENGINE_SOURCE_DIR}/src/robotick/*.cpp
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

# Load and concatenate workload config
set(ENGINE_WORKLOAD_CONFIG "${ROBOTICK_ENGINE_SOURCE_DIR}/CMakeWorkloads.json")
set(PROJECT_WORKLOAD_CONFIG "${CMAKE_SOURCE_DIR}/CMakeWorkloads.json")

set(RAW_WORKLOAD_JSON "")
foreach(PATH IN LISTS PROJECT_WORKLOAD_CONFIG ENGINE_WORKLOAD_CONFIG)
    if(EXISTS ${PATH})
        file(READ ${PATH} THIS_JSON)
        string(APPEND RAW_WORKLOAD_JSON ${THIS_JSON})
    endif()
endforeach()

if(RAW_WORKLOAD_JSON STREQUAL "" OR NOT DEFINED WORKLOAD_PRESET)
    set(FILTERED_WORKLOAD_SOURCES "")
else()
    string(JSON PRESETS_JSON ERROR_VARIABLE JSON_PRESETS_ERR GET ${RAW_WORKLOAD_JSON} presets)
    string(JSON DUMMY_CHECK ERROR_VARIABLE JSON_PRESET_ERR GET ${PRESETS_JSON} ${WORKLOAD_PRESET})
    if(NOT JSON_PRESET_ERR)
        string(JSON MODE GET ${PRESETS_JSON} ${WORKLOAD_PRESET} mode)
        string(JSON FILES GET ${PRESETS_JSON} ${WORKLOAD_PRESET} workloads)

        string(JSON FILE_COUNT LENGTH ${FILES})
        set(PARSED_WORKLOAD_LIST "")
        foreach(IDX RANGE 0 ${FILE_COUNT})
            math(EXPR INDEX "${IDX} - 1")
            if(INDEX GREATER_EQUAL 0)
                string(JSON ITEM GET ${FILES} ${INDEX})
                list(APPEND PARSED_WORKLOAD_LIST ${ITEM})
            endif()
        endforeach()

        set(FILTERED_WORKLOAD_SOURCES "")
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

set(ALL_CPP_SOURCES ${NON_WORKLOAD_SOURCES} ${FILTERED_WORKLOAD_SOURCES} PARENT_SCOPE)

list(LENGTH FILTERED_WORKLOAD_SOURCES FILTERED_COUNT)
message(STATUS "📦 Final workload source count - ${FILTERED_COUNT} from: ${FILTERED_WORKLOAD_SOURCES}")

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
set(CORE_SOURCES ${CORE_SOURCES} PARENT_SCOPE)
set(PYTHON_SOURCES ${PYTHON_SOURCES} PARENT_SCOPE)


