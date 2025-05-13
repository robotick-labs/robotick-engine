cmake_minimum_required(VERSION 3.16)
project(robotick_tests)

# Required to enable testing features
enable_testing()

# Find installed Catch2 (you must have used `cmake --install`)
set(Catch2_DIR "C:/Users/paulw_3retm4i/libs/catch2/lib/cmake/Catch2")
find_package(Catch2 REQUIRED CONFIG)

# Add your test executable
add_executable(robotick_tests
  engine_tests.cpp
)

target_link_libraries(robotick_tests
  PRIVATE
    robotick_framework
    Catch2::Catch2
    Catch2::Catch2WithMain
)

# Enable Catch2 auto test registration
include(Catch)
catch_discover_tests(robotick_tests)
