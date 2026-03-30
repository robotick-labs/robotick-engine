// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

# Module Map & Initialization Flow

This lightweight map explains how the major subsystems (registry, engine, data, telemetry) relate to each other so new contributors can orient themselves before diving into the codebase. Follow the links below to jump into the relevant source files.

## High-level startup sequence

1. **Type registration (single-threaded)**
   - Files: `cpp/include/robotick/framework/TypeRegistry.h`, `cpp/src/robotick/framework/Engine.cpp` (`Engine::load`).
   - Workload descriptors, struct metadata, and helper types are registered on the main thread before `Engine::load()` runs. `TypeRegistry::seal()` is invoked immediately after registration so the registry becomes read-only.

2. **Engine::load – model + buffer layout**
   - Files: `cpp/src/robotick/framework/Engine.cpp`, `cpp/src/robotick/framework/data/WorkloadsBuffer.cpp`.
   - Steps:
     1. Compute the inline workload + stats footprint with alignment helpers.
     2. Run a preflight pass: allocate a scratch inline-only `WorkloadsBuffer`, placement-new workloads, apply initial config, run `pre_load()`, and plan dynamic-struct storage.
     3. Allocate the final exact-sized `WorkloadsBuffer`, reconstruct workloads, and bind the `Dynamic Struct Storage Region`.
     4. Build `WorkloadInstanceInfo` child pointers and resolve all `DataConnectionInfo` entries (see below).
     5. Call workload `setup_fn` (if present).

3. **Data connections (local)**
   - Files: `cpp/src/robotick/framework/data/DataConnection.cpp`, `cpp/include/robotick/framework/data/DataConnection.h`.
   - Each `DataConnectionSeed` (declared in the model) is resolved to a pair of pointers inside `WorkloadsBuffer`. The contiguous buffer layout and offset math guarantee deterministic field addresses.

4. **Remote subsystems**
   - Files: `cpp/src/robotick/framework/data/RemoteEngineConnections.cpp`, `cpp/src/robotick/framework/data/TelemetryServer.cpp`, `cpp/src/robotick/framework/services/*/WebServer_*.cpp`.
   - `Engine::load()` configures `RemoteEngineConnections`. `Engine::run()` starts both the telemetry HTTP server and remote connections when the first tick executes.

5. **Run loop**
   - Files: `cpp/src/robotick/framework/Engine.cpp` (`Engine::run`).
   - Order per tick:
     1. Update `TickInfo` timestamps and counters.
     2. Pump remote data connections (network exchange).
     3. Execute local `DataConnectionInfo::do_data_copy()` calls.
     4. Issue a release fence so writes are visible to workloads.
     5. Invoke the root workload’s `tick_fn` (which drives children).
     6. Record timing stats and sleep until the next tick deadline.
   - Shutdown reverses the process: stop flag set, workloads’ `stop_fn` run, then `RemoteEngineConnections` and `TelemetryServer` stop via RAII.

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
- Use `docs/ownership.md` to understand singleton rules and lifetime expectations.
