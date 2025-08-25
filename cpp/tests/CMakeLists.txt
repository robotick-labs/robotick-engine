cmake_minimum_required(VERSION 3.16)
project(robotick_engine_tests)

# ========================
# ✅ Enable CTest + Warnings
# ========================

enable_testing()

# Enforce strict compilation for test quality
add_compile_options(-Wall -Wextra -Werror)

# ========================
# ✅ Catch2 (Test Framework)
# ========================

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
find_package(Catch2 3 CONFIG)

if (Catch2_FOUND)
    message(STATUS "✅ Found Catch2: ${Catch2_DIR}")
else()
    message(FATAL_ERROR "❌ Catch2 v3 not found. Please install it:\n"
                        "    sudo apt install catch2")
endif()


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
add_executable(robotick_engine_tests ${ROBOTICK_TEST_SOURCES})

# AddressSanitizer (optional runtime debug support)
# Uncomment to enable memory error detection during testing (impacts performance)
# if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#   message(STATUS "🔎 Enabling AddressSanitizer for robotick_engine_tests")
#   target_compile_options(robotick_engine_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
#   target_link_options(robotick_engine_tests PRIVATE -fsanitize=address)
# endif()

# Link against core framework and Catch2
target_link_libraries(robotick_engine_tests
  PRIVATE
    robotick-engine
    Catch2::Catch2WithMain
)

# Link against curl lib (for WebServer testing)
target_link_libraries(robotick_engine_tests PRIVATE curl)

# ✅ Re-enable exceptions just for test target
target_compile_options(robotick_engine_tests PRIVATE -fexceptions)

target_compile_definitions(robotick_engine_tests PRIVATE ROBOTICK_TEST_MODE=1)
target_compile_definitions(robotick_engine_tests PRIVATE CATCH_CONFIG_ENABLE_EXCEPTIONS)

# Discover Catch2 TEST_CASEs automatically
include(Catch)
catch_discover_tests(robotick_engine_tests)

# Ensure robotick-engine is built before running tests
add_dependencies(robotick_engine_tests robotick-engine)

# ========================
# ✅ Runtime DLL Copy (for VS Code / Debugging)
# ========================

add_custom_command(TARGET robotick_engine_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:robotick-engine>
    $<TARGET_FILE_DIR:robotick_engine_tests>
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

