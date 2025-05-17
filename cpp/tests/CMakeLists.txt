cmake_minimum_required(VERSION 3.16)
project(robotick_tests)

enable_testing()

include(FetchContent)

# warnings as errors throughout - to ensure quality code at all times
add_compile_options(-Wall -Wextra -Werror)

# ✅ Download Catch2 automatically if not found
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(Catch2)

# 🔍 Find test files
file(GLOB_RECURSE ROBOTICK_TEST_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# 🛠️ Build test executable
add_executable(robotick_tests
  ${ROBOTICK_TEST_SOURCES}
)

target_link_libraries(robotick_tests
  PRIVATE
    robotick_framework
    Catch2::Catch2WithMain
)

# 🔁 Enable automatic Catch2 test discovery
include(Catch)
catch_discover_tests(robotick_tests)

# 📦 Ensure framework is built before tests
add_dependencies(robotick_tests robotick_framework)

# ✅ (Optional) Copy shared libs to test output dir
add_custom_command(TARGET robotick_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:robotick_framework>
    $<TARGET_FILE_DIR:robotick_tests>
)
