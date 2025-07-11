cmake_minimum_required(VERSION 3.16)
project(robotick-desktop-testbed)

# Path to robotick engine source
set(ROBOTICK_ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp)

# Add engine directly as a subdirectory
add_subdirectory(${ROBOTICK_ENGINE_DIR} ${CMAKE_BINARY_DIR}/robotick_engine_build)

add_executable(desktop-testbed robotick_main.cpp)

target_link_libraries(desktop-testbed PRIVATE robotick-engine)
