
set(ROBOTICK_ENGINE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/../../../../cpp)
include(${ROBOTICK_ENGINE_SOURCE_DIR}/cmake/RobotickWorkloadFilter.cmake)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/../../../../cpp/include"
)
