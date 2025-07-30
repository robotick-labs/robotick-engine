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

# Disable -Werror inside Catch2 (even if it tries to re-enable it)
set(CATCH_ENABLE_EXCEPTIONS ON CACHE BOOL "" FORCE)
set(CATCH_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(CATCH_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Catch2)

# Manually remove -Werror from Catch2 targets if needed
get_target_property(_catch2_opts Catch2 COMPILE_OPTIONS)
if(_catch2_opts)
  list(REMOVE_ITEM _catch2_opts "-Werror")
  set_target_properties(Catch2 PROPERTIES COMPILE_OPTIONS "${_catch2_opts}")
endif()

# Avoid clang-tidy warnings inside Catch2 (external lib)
set_target_properties(Catch2 PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(Catch2WithMain PROPERTIES CXX_CLANG_TIDY "")

# ========================
# ✅ Test Sources & Target
# ========================

# Automatically discover test .cpp files
file(GLOB_RECURSE ROBOTICK_TEST_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.test.cpp
)

if(false) # enable to debug test-sources
    foreach(source ${ROBOTICK_TEST_SOURCES})
        message(STATUS "Test source: ${source} from ${CMAKE_CURRENT_SOURCE_DIR}")
    endforeach()
endif()

# Build test executable
add_executable(robotick_tests ${ROBOTICK_TEST_SOURCES})

# AddressSanitizer (optional runtime debug support)
# Uncomment to enable memory error detection during testing (impacts performance)
# if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#   message(STATUS "🔎 Enabling AddressSanitizer for robotick_tests")
#   target_compile_options(robotick_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
#   target_link_options(robotick_tests PRIVATE -fsanitize=address)
# endif()

# Link against core framework and Catch2
target_link_libraries(robotick_tests
  PRIVATE
    robotick-engine
    Catch2::Catch2WithMain
)

# Link against curl lib (for WebServer testing)
target_link_libraries(robotick_tests PRIVATE curl)

# ✅ Re-enable exceptions just for test target
target_compile_options(robotick_tests PRIVATE -fexceptions)

target_compile_definitions(robotick_tests PRIVATE ROBOTICK_ENABLE_PYTHON=1)
target_compile_definitions(robotick_tests PRIVATE ROBOTICK_TEST_MODE=1)
target_compile_definitions(robotick_tests PRIVATE CATCH_CONFIG_ENABLE_EXCEPTIONS)

# Discover Catch2 TEST_CASEs automatically
include(Catch)
catch_discover_tests(robotick_tests)

# Ensure robotick-engine is built before running tests
add_dependencies(robotick_tests robotick-engine)

# ========================
# ✅ Runtime DLL Copy (for VS Code / Debugging)
# ========================

add_custom_command(TARGET robotick_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:robotick-engine>
    $<TARGET_FILE_DIR:robotick_tests>
)

# =======================================================
# ✅ Test data for WebServer and other tests
# =======================================================

set(TEST_DATA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
set(TEST_DATA_DEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/data)

message(STATUS "📦 Copying test data from: ${TEST_DATA_SOURCE_DIR}")
message(STATUS "📦 Copying test data to:   ${TEST_DATA_DEST_DIR}")

file(COPY ${TEST_DATA_SOURCE_DIR}
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

