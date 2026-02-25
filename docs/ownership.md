<!--
Copyright Robotick contributors
SPDX-License-Identifier: Apache-2.0
-->

# Ownership & Singleton Boundaries

Robotick deliberately keeps global state to an absolute minimum so embedded targets can reason about lifetime and determinism. The sections below summarise the singletons that do exist and how they are expected to be used.

## TypeRegistry (the only global singleton)

- Ownership: process-wide singleton accessed via `TypeRegistry::get()`.
- Mutation rules: only the thread that performs workload/type registration may mutate the registry. `TypeRegistry` captures that thread’s ID on first use and asserts on subsequent mutations if a different thread attempts to modify it.
- Lifecycle: after registration is complete, call `TypeRegistry::seal()` (done inside `Engine::load()`). Once sealed, all mutation APIs assert, guaranteeing the registry is read-only for the remainder of the process lifetime.
- Rationale: we need a global catalogue of types/workloads so reflection works everywhere, but we still want deterministic ownership. Restricting mutation to a single thread during startup avoids races while keeping reads cheap.

## TelemetryServer & RemoteEngineConnections

- Ownership: members of `Engine::State`. They are constructed alongside the engine, started in `Engine::run()`, and stopped in both `Engine::~Engine()` and at the end of `Engine::run()`.
- Rationale: sockets, HTTP servers, and remote links depend on platform resources (ports, file descriptors). Keeping them as RAII members ensures they start/stay scoped to the engine instance and shut down deterministically without relying on process exit.

## Data connections, workloads, and buffers

- No global singletons. `WorkloadsBuffer`, `DataConnectionInfo`, and workload instances are all owned by `Engine::State`.
- `RemoteEngineConnection` objects are created on demand (e.g., by workloads or higher-level orchestrators) and follow the same RAII pattern: configure, start, stop/destroy.

## Contributor guidelines

1. If you think you need a singleton, pause. Can the owner be explicit instead (e.g., hang it off `Engine::State`, a workload, or a subsystem struct)?
2. If a singleton is truly required (e.g., a registry), document:
   - who owns it
   - which thread(s) may mutate it
   - how and when it is sealed or torn down
3. Prefer RAII members to raw globals for platform services. If an object opens sockets/threads/files, make sure its constructor/destructor encapsulate that lifecycle so tests and embedded targets can rely on deterministic cleanup.

Following these rules keeps ownership clear, avoids hidden dependencies, and helps MCU targets meet determinism requirements.
