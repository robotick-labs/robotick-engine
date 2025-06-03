cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_SOURCE_DIR}/robotick_framework
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-blink)
