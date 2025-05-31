# Minimal ESP32-S3 toolchain file
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

set(ESP32_TOOLCHAIN /root/.espressif/tools/xtensa-esp32s3-elf/esp-12.2.0_20230208/xtensa-esp32s3-elf)

set(CMAKE_C_COMPILER   ${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-gcc)
set(CMAKE_CXX_COMPILER ${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-g++)
set(CMAKE_AR           ${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-ar)
set(CMAKE_OBJCOPY      ${ESP32_TOOLCHAIN}/bin/xtensa-esp32s3-elf-objcopy)

# Disable features not supported
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
