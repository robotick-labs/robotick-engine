# ESP32-S3 toolchain configuration.
# Override these via -DESP32_TOOLCHAIN=/path/to/toolchain or -DEXE_SUFFIX=.exe when invoking CMake.
# If unspecified, the file falls back to the ESP32_TOOLCHAIN environment variable or host-specific defaults.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

set(_esp32_default_toolchain "")
set(_esp32_default_exe_suffix "")

if(WIN32)
    set(_esp32_default_toolchain "C:/Espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf")
    set(_esp32_default_exe_suffix ".exe")
elseif(UNIX)
    set(_esp32_default_toolchain "$ENV{HOME}/.espressif/tools/xtensa-esp32s3-elf/esp-14.2.0_20241119/xtensa-esp32s3-elf")
else()
    message(FATAL_ERROR "Unsupported host platform")
endif()

set(_esp32_toolchain_help "Path to the Xtensa ESP32-S3 toolchain root (override with -DESP32_TOOLCHAIN=... or ESP32_TOOLCHAIN env)")
set(_esp32_toolchain_value "")
if(DEFINED ESP32_TOOLCHAIN AND NOT "${ESP32_TOOLCHAIN}" STREQUAL "")
    set(_esp32_toolchain_value "${ESP32_TOOLCHAIN}")
elseif(DEFINED ENV{ESP32_TOOLCHAIN} AND NOT "$ENV{ESP32_TOOLCHAIN}" STREQUAL "")
    set(_esp32_toolchain_value "$ENV{ESP32_TOOLCHAIN}")
else()
    set(_esp32_toolchain_value "${_esp32_default_toolchain}")
endif()
set(ESP32_TOOLCHAIN "${_esp32_toolchain_value}" CACHE PATH "${_esp32_toolchain_help}")
if(NOT ESP32_TOOLCHAIN)
    message(FATAL_ERROR "ESP32_TOOLCHAIN is not set. Provide it via -DESP32_TOOLCHAIN=... or env ESP32_TOOLCHAIN.")
endif()

set(_esp32_exe_suffix_help "Executable suffix for Xtensa ESP32-S3 tools (e.g. .exe on Windows)")
if(NOT DEFINED EXE_SUFFIX OR "${EXE_SUFFIX}" STREQUAL "")
    if(NOT "${_esp32_default_exe_suffix}" STREQUAL "")
        set(EXE_SUFFIX "${_esp32_default_exe_suffix}" CACHE STRING "${_esp32_exe_suffix_help}")
    else()
        set(EXE_SUFFIX "" CACHE STRING "${_esp32_exe_suffix_help}")
    endif()
else()
    set(EXE_SUFFIX "${EXE_SUFFIX}" CACHE STRING "${_esp32_exe_suffix_help}")
endif()

# Compiler executables
set(CMAKE_C_COMPILER   "${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-gcc${EXE_SUFFIX}")

if(EXISTS "${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-c++${EXE_SUFFIX}")
    set(CMAKE_CXX_COMPILER "${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-c++${EXE_SUFFIX}")
else()
    set(CMAKE_CXX_COMPILER "${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-g++${EXE_SUFFIX}")
endif()

set(CMAKE_AR           "${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-ar${EXE_SUFFIX}")
set(CMAKE_OBJCOPY      "${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-objcopy${EXE_SUFFIX}")

# Force define for ESP32-specific builds
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROBOTICK_PLATFORM_ESP32S3")

# Use IDF_PATH from environment
set(IDF_PATH "$ENV{IDF_PATH}" CACHE PATH "Path to ESP-IDF")
message(STATUS "Using ESP-IDF path: ${IDF_PATH}")

# Include headers
include_directories(
    "${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include"
    "${IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/xtensa/include"
    "${IDF_PATH}/components/freertos/port/xtensa/include/freertos"
    "${IDF_PATH}/components/freertos/port/xtensa/include/freertos/xtensa"
    "${IDF_PATH}/components/freertos/port/xtensa/include"

    "${ESP32_TOOLCHAIN}/xtensa-esp32s3-elf/include"

    "${IDF_PATH}/components/newlib/platform_include"
    "${IDF_PATH}/components/esp_common/include"
    "${IDF_PATH}/components/esp_system/include"
    "${IDF_PATH}/components/esp_hw_support/include"
)
