// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

# Module Map & Initialization Flow

Status: current implementation note
Last reviewed: 2026-04-18

This note reflects the current engine load/run path in `robotick-engine`.
Concrete composition workload behaviour such as `SequencedGroupWorkload` and
`SyncedGroupWorkload` lives in `robotick-core-workloads`; this doc describes
the engine-side lifecycle and ownership boundaries those workloads plug into.

This lightweight map explains how the major subsystems (registry, engine, data, telemetry) relate to each other so new contributors can orient themselves before diving into the codebase. Follow the links below to jump into the relevant source files.

## High-level startup sequence

1. **Type registration (single-threaded)**
   - Files: `cpp/include/robotick/framework/TypeRegistry.h`, `cpp/src/robotick/framework/Engine.cpp` (`Engine::load`).
   - Workload descriptors, struct metadata, and helper types are registered on the main thread before `Engine::load()` runs. `TypeRegistry::seal()` is invoked immediately after registration so the registry becomes read-only.

2. **Engine::load – model + buffer layout**
   - Files: `cpp/src/robotick/framework/Engine.cpp`, `cpp/src/robotick/framework/data/WorkloadsBuffer.cpp`.
   - Steps:
     1. Compute the inline workload + stats footprint with alignment helpers.
     2. Run a preflight pass: allocate a scratch inline-only `WorkloadsBuffer`, placement-new workloads, call `set_engine()` where present, apply initial config, run `pre_load()`, and plan dynamic-struct storage.
     3. Allocate the final exact-sized `WorkloadsBuffer`, reconstruct workloads, and bind the `Dynamic Struct Storage Region`.
     4. Apply final config and initial inputs, then call workload `load_fn` (if present).
     5. Build `WorkloadInstanceInfo` child pointers and resolve all `DataConnectionInfo` entries (see below).
     6. Call the root workload's `set_children_fn` so composition workloads can claim or delegate connection ownership.
     7. Call workload `setup_fn` (if present).

3. **Data connections (local)**
   - Files: `cpp/src/robotick/framework/data/DataConnection.cpp`, `cpp/include/robotick/framework/data/DataConnection.h`.
   - Each `DataConnectionSeed` (declared in the model) is resolved to a pair of pointers inside `WorkloadsBuffer`. The contiguous buffer layout and offset math guarantee deterministic field addresses.

4. **Remote subsystems**
   - Files: `cpp/src/robotick/framework/data/RemoteEngineConnections.cpp`, `cpp/src/robotick/framework/data/TelemetryServer.cpp`, `cpp/src/robotick/framework/services/*/WebServer_*.cpp`.
   - `Engine::load()` configures both `RemoteEngineConnections` and `TelemetryServer`.
   - `Engine::run()` starts the telemetry HTTP server before entering the root tick loop.
   - `RemoteEngineConnections` then participates in every engine tick.

5. **Run loop**
   - Files: `cpp/src/robotick/framework/Engine.cpp` (`Engine::run`).
   - Order per tick:
     1. Update `TickInfo` timestamps and counters.
     2. Mark the workloads buffer frame as write-in-progress for telemetry readers.
     3. Pump remote data connections (network exchange).
     4. Execute local `DataConnectionInfo::do_data_copy()` calls acquired by the engine.
     5. Apply pending telemetry-originated input writes.
     6. Issue a release fence so writes are visible to workloads.
     7. Invoke the root workload’s `tick_fn` (which drives children).
     8. Record timing stats, mark the frame stable again, and sleep until the next tick deadline.
   - Shutdown reverses the process: stop flag set, workloads’ `stop_fn` run, then `RemoteEngineConnections` and `TelemetryServer` stop via RAII.

## Composition Boundary

- The engine owns:
  - the root workload tick loop
  - child pointer hookup
  - data-connection resolution
  - delegated connection propagation
- Composition workloads own:
  - how a group's children are started
  - how and when children are ticked
  - whether local connections are handled inside the group or delegated upward

In practice that means `robotick-engine` provides the generic hook surface
(`set_engine`, `set_children`, `start`, `tick`, `stop`), while
`robotick-core-workloads` provides the concrete group semantics.

## Key modules at a glance

| Module                 | Responsibility                                            | Key files                                                    |
| ---------------------- | --------------------------------------------------------- | ------------------------------------------------------------ |
| TypeRegistry           | Reflection metadata and workload descriptors              | `cpp/include/robotick/framework/TypeRegistry.h`              |
| WorkloadsBuffer        | Contiguous memory that holds workload instances and stats | `cpp/include/robotick/framework/data/WorkloadsBuffer.h`      |
| DataConnection         | Local field → field copies inside the buffer              | `cpp/src/robotick/framework/data/DataConnection.cpp`         |
| RemoteEngineConnection | TCP handshake + field streaming between engines           | `cpp/src/robotick/framework/data/RemoteEngineConnection.cpp` |
| TelemetryServer        | HTTP API for buffer layout/raw dumps                      | `cpp/src/robotick/framework/data/TelemetryServer.cpp`        |
| Engine                 | Owns all of the above and runs the tick loop              | `cpp/src/robotick/framework/Engine.cpp`                      |

## Navigation tips

- Start with `Engine::load()` and `Engine::run()` to see the entire lifecycle.
- Jump into `DataConnectionUtils::create()` to understand how dotted field paths map into the contiguous buffer.
- Check `RemoteEngineConnection::tick()` to see how remote field synchronization piggybacks on the same metadata.
- Check `robotick-core-workloads/cpp/src/robotick/workloads/composition/` for the current `SequencedGroupWorkload` and `SyncedGroupWorkload` behaviour.
- Use `docs/ownership.md` to understand singleton rules and lifetime expectations.
