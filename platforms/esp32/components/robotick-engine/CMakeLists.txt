# /workspace/robotick-engine/platforms/esp32/components/robotick-engine/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Define the platform before anything else uses it
set(ROBOTICK_PLATFORM_ESP32 TRUE)

set(ROBOTICK_ENGINE_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${ROBOTICK_ENGINE_SOURCE_DIR}/cmake/RobotickWorkloadFilter.cmake)

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
    REQUIRES json esp_timer M5GFX M5Unified esp_wifi espressif__esp32-camera
)



target_compile_definitions(${COMPONENT_LIB} PUBLIC ROBOTICK_PLATFORM_ESP32)
