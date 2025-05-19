> ⚠️ Draft TDD. For review and iterative development.

# 🧩 Data Pipeline Helper - Technical Design

This Robotick module manages **data propagation** between workloads in a safe, deterministic, and efficient manner.  
It is not a workload, but a shared subsystem invoked by the engine and composition-workload implementations during tick orchestration.

---

## 🎯 Purpose

To provide:
- ✅ Safe **tick-synchronous data propagation**
- ✅ Explicit **data-connection declarations**
- ✅ Efficient **thread-aware input fetching**
- ✅ Support for special bulk consumers like logging/telemetry

---

## 🧠 Key Design Principles

- All data flows are **declared in the model** as **data-connections**.
- A data-connection defines **what** is connected (field, workload, or all), and where the data flows from/to.
- No workload may modify another's data directly — only via the data-pipeline copy process.
- Workloads never include other workload headers, to reinforce separation.
- Tick-phase safety is guaranteed via:
  - **Read-before-write**: All required data is fetched before any workload modifies its own outputs.
  - **Commit-after-tick**: Output data is committed immediately after a workload ticks, and read during the next input gather phase.
- Workload instances may only be assigned to a **single parent group**.
- Optimisations (e.g. direct binding or change-detection) are not implemented initially — correctness comes first.

---

## 🧱 Core Concepts

### ✅ DataConnectionSeed and DataConnectionInfo

- `DataConnectionSeed` is a model-level declaration using string-based paths (e.g. `"A.output.temp"` to `"B.input.temp"`).
- At `model.finalise()`, seeds resolve into `DataConnectionInfo`, which:
  - Points to source and destination memory
  - Indicates thread-local vs thread-external status
  - Points to the exact field (not whole structs or buffers)
  - Stores resolved metadata (size, offset, type checks)

Each connection describes a **1:1 memory copy**. Even bulk declarations like `connect_all` are expanded into individual field-wise copies.

---

## 🔗 Registry + Field Access

The registry interface:

```cpp
class WorkloadRegistryEntry {
    std::vector<WorkloadFieldEntry> config_fields;
    std::vector<WorkloadFieldEntry> input_fields;
    std::vector<WorkloadFieldEntry> output_fields;
};

struct WorkloadFieldEntry {
    std::string name;
    size_t offset;
    size_t size;
    enum class Kind { Config, Input, Output };
};
```

Registered via:

```cpp
const WorkloadRegistryEntry* WorkloadRegistry::find(const std::string& name) const;
```

Connections may route:
- `output → config`
- `output → input`

Config values can be dynamically edited at runtime, and are treated like regular data fields.

---

## 📦 Memory Semantics

### Thread-local connection
- Source and destination pointers are both within the same thread's group.
- Direct `memcpy` is performed using precomputed pointers.

### Thread-external connection
- Source and destination are in **different thread groups**.
- A per-thread **input/output field clone** is used (not the entire buffer).
- Copy-in and copy-out are performed only for referenced fields.

---

## 🔄 Tick Lifecycle: Synced + SequencedGroup

### `SequencedGroup`

Each tick follows:
1. `DataPipeline::fetch_inputs_local()` at start of group tick
2. For each workload:
   - Tick workload (shared memory)
   - Write any dependent thread-local fields to thread-local output buffer
3. At end of tick: no further action required

### `SyncedGroup`

Each tick frame:
1. `DataPipeline::commit_outputs()` for all thread-local fields produced by previous ticks
2. `DataPipeline::fetch_inputs()` into thread-local buffers before launching threads
3. Dispatch all children to tick (they operate on shared memory)
4. Output buffers are retained until next tick

Note:
- Each direct child of `SyncedGroup` runs in its own thread.
- All workloads within that child group share a thread-local input/output cache.

---

## 🔍 Zero-Copy Considerations

While zero-copy pointer aliasing between fields might seem attractive, it is **not used** in Robotick because:

- Workloads declare fields using **value-based structs**, not pointer fields.
- Patchable pointer aliasing would require awkward manual dereferencing, and checking if each pointer is set - causing costly branching.
- We aim for **simple, clean, and predictable code**, especially for MCUs.

Therefore, **all data-connections are implemented as explicit memcpy operations**, precomputed and minimal.  
This enables performance without sacrificing clarity or safety.

---

## 🧧 Data Setup API

```cpp
model.data_connect_field("A.output.temperature", "B.input.temp");
model.data_connect_workload("SensorFusion", "Logger.input.all_data");
model.data_connect_all("ConsoleLogger.input.all_data");
```

Notes:
- Even bulk connections expand to multiple 1:1 field-level connections
- Targets must be field-capable (e.g. an array or struct of expected fields)
- `DataConnectionHandle` is not needed — connections are one-way setup-only

---

## 🚫 Constraints

- A workload instance may only be connected to **one parent group**
- No circular data dependencies are permitted
- Connections are immutable after `model.finalise()`

---

## ❌ Not Yet Implemented

- Change-detection
- DataConnectionHandle
- Runtime-configurable filters

---

## 🧪 Future Extensions

- Time-stamped telemetry and observability
- Field-level change filtering and diffing
- Visual dataflow graph generation
- Support for distributed/remote (cross-process or network) data routes