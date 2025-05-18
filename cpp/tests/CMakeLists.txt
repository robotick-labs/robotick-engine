cmake_minimum_required(VERSION 3.16)
project(robotick_tests)

# ========================
# ✅ Enable CTest + Warnings
# ========================

enable_testing()

# Enforce strict compilation for test quality
add_compile_options(-Wall -Wextra -Werror)

# ========================
# ✅ Catch2 (Test Framework)
# ========================

include(FetchContent)

# Automatically download Catch2 if not already present
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)

FetchContent_MakeAvailable(Catch2)

# Avoid clang-tidy warnings inside Catch2 (external lib)
set_target_properties(Catch2 PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(Catch2WithMain PROPERTIES CXX_CLANG_TIDY "")

# ========================
# ✅ Test Sources & Target
# ========================

# Automatically discover test .cpp files
file(GLOB_RECURSE ROBOTICK_TEST_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Build test executable
add_executable(robotick_tests ${ROBOTICK_TEST_SOURCES})

# Link against core framework and Catch2
target_link_libraries(robotick_tests
  PRIVATE
    robotick_framework
    Catch2::Catch2WithMain
)

# Discover Catch2 TEST_CASEs automatically
include(Catch)
catch_discover_tests(robotick_tests)

# Ensure robotick_framework is built before running tests
add_dependencies(robotick_tests robotick_framework)

# ========================
# ✅ Runtime DLL Copy (for VS Code / Debugging)
# ========================

add_custom_command(TARGET robotick_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:robotick_framework>
    $<TARGET_FILE_DIR:robotick_tests>
)
