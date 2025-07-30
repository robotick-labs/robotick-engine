set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

# Choose toolchain root path based on platform
if(WIN32)
    set(ESP32_TOOLCHAIN "C:/Espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf")
    set(EXE_SUFFIX ".exe")
elseif(UNIX)
    set(ESP32_TOOLCHAIN "$ENV{HOME}/.espressif/tools/xtensa-esp32s3-elf/esp-14.2.0_20241119/xtensa-esp32s3-elf")
    set(EXE_SUFFIX "")
else()
    message(FATAL_ERROR "Unsupported host platform")
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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROBOTICK_PLATFORM_ESP32")

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
