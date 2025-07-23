cmake_minimum_required(VERSION 3.16)
project(robotick-desktop-testbed)

# Path to robotick engine source
set(ROBOTICK_ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp)

# Add engine directly as a subdirectory
add_subdirectory(${ROBOTICK_ENGINE_DIR} ${CMAKE_BINARY_DIR}/robotick_engine_build)

add_executable(desktop-testbed robotick_main.cpp)

target_link_libraries(desktop-testbed PRIVATE robotick-engine)

# symlink engine runtime data for testing
set(ENGINE_DATA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../data")
set(ENGINE_DATA_DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/engine-data")

message(STATUS "🔗 Will symlink engine data from: ${ENGINE_DATA_SOURCE_DIR}")
message(STATUS "🔗            into binary path: ${ENGINE_DATA_DEST_DIR}")

if(NOT EXISTS "${ENGINE_DATA_DEST_DIR}")
    file(CREATE_LINK "${ENGINE_DATA_SOURCE_DIR}" "${ENGINE_DATA_DEST_DIR}" SYMBOLIC)
    message(STATUS "✅ Created symlink: ${ENGINE_DATA_DEST_DIR} → ${ENGINE_DATA_SOURCE_DIR}")
else()
    message(STATUS "♻️  Symlink or directory already exists: ${ENGINE_DATA_DEST_DIR}")
endif()