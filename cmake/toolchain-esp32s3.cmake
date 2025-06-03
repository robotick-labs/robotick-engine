set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

set(ESP32_TOOLCHAIN /root/.espressif/tools/xtensa-esp32s3-elf/esp-12.2.0_20230208/xtensa-esp32s3-elf)

set(CMAKE_C_COMPILER   ${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-gcc)
set(CMAKE_CXX_COMPILER ${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-g++)
set(CMAKE_AR           ${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-ar)
set(CMAKE_OBJCOPY      ${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-objcopy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROBOTICK_PLATFORM_ESP32")

# Use IDF_PATH from environment
set(IDF_PATH "$ENV{IDF_PATH}" CACHE PATH "Path to ESP-IDF")
message(STATUS "Using ESP-IDF path: ${IDF_PATH}")

include_directories(
    ${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include
    ${IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/xtensa/include
    ${IDF_PATH}/components/freertos/port/xtensa/include/freertos
    ${IDF_PATH}/components/freertos/port/xtensa/include/freertos/xtensa
    ${IDF_PATH}/components/freertos/port/xtensa/include
    
    ${ESP32_TOOLCHAIN}/xtensa-esp32s3-elf/include

    ${IDF_PATH}/components/newlib/platform_include
    ${IDF_PATH}/components/esp_common/include
    ${IDF_PATH}/components/esp_system/include
    ${IDF_PATH}/components/esp_hw_support/include

    /workspaces/robotick/esp-idf-dummy
)

