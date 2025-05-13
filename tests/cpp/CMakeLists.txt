cmake_minimum_required(VERSION 3.16)
project(robotick_tests)

# Required to enable testing features
enable_testing()

# Find installed Catch2 (you must have used `cmake --install`)
set(Catch2_DIR "C:/Users/paulw_3retm4i/libs/catch2/lib/cmake/Catch2")
find_package(Catch2 REQUIRED CONFIG)

# Find executables
file(GLOB_RECURSE ROBOTICK_TEST_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Add your test executable
add_executable(robotick_tests
  ${ROBOTICK_TEST_SOURCES}
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

add_dependencies(robotick_tests robotick_framework)

add_custom_command(TARGET robotick_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:robotick_framework>
    $<TARGET_FILE_DIR:robotick_tests>
)


