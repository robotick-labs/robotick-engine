idf_component_register(
    SRCS
        src/robotick/framework/Engine.cpp
        src/robotick/framework/Model.cpp
        src/robotick/framework/WorkloadInstanceInfo.cpp
        src/robotick/framework/data/Blackboard.cpp
        src/robotick/framework/data/DataConnection.cpp
        src/robotick/framework/registry/FieldRegistry.cpp
        src/robotick/framework/registry/WorkloadRegistry.cpp
        src/robotick/framework/utils/ConsoleTable.cpp
        src/robotick/framework/utils/ConsoleTelemetryTable.cpp
        src/robotick/framework/utils/WorkloadFieldsIterator.cpp
        src/robotick/workloads/composition/SequencedGroupWorkload.cpp
        src/robotick/workloads/composition/SyncedGroupWorkload.cpp
        src/robotick/workloads/diagnostics/ConsoleTelemetryWorkload.cpp
        src/robotick/workloads/diagnostics/HelloWorkload.cpp
        src/robotick/workloads/diagnostics/TimingDiagnosticsWorkload.cpp
        # Add other minimal ESP32-safe files only

    INCLUDE_DIRS include
)


target_link_options(${COMPONENT_LIB} PRIVATE "-Wl,--gc-sections")
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/robotick_linker_patch.ld")
target_link_options(${COMPONENT_LIB} PRIVATE
  "-T${CMAKE_CURRENT_SOURCE_DIR}/robotick_linker_patch.ld")
  
