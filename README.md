<img src="https://robotick.org/images/logo.png" style="display: block; width: 300px; border-radius: 12px;" />

<br/>

[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)

> ⚠️ **Work In Progress**
>
> This project is under active development and not yet production-ready.
> The repo is public to support tools like [CodeRabbit](https://coderabbit.ai) and promote early collaboration and transparency.
> Expect incomplete features, ongoing changes, and occasional mess.

---

## 🤖 Overview

**Robotick** is a high-performance, modular C++ runtime for robotics and control systems - engineered for reliability, introspection, and composability across the full spectrum of platforms.

From bare-metal microcontrollers like the STM32 and ESP32 to Raspberry Pi, desktop systems, and edge-AI devices like NVIDIA Jetson (arm64), Robotick delivers real-time precision where it counts, and excellent performance everywhere else - without sacrificing ease of use or flexibility.

### 🧱 Zero-allocation policy

The engine core avoids heap-bearing `std::` containers entirely: approved STL headers are wrapped by `robotick::StdApproved` (defined in `cpp/include/robotick/framework/common/StdApproved.h`), while deterministic structures rely on native types like `FixedString` and `HeapVector` (each defined in their own headers) plus explicit placement-new logic. Platform/CLI layers can still use STL where needed, but the contiguous-engine runtime stays heap-free so ESP32/MCU targets see consistent timings.

### 🧭 Callback contract

`start_fn`, `tick_fn`, and `stop_fn` are invoked inside the core loop where exceptions are not allowed, so workloads must catch and log their own exceptions before returning. If a workload wraps a third-party API that can throw, the workload should capture the exception, emit a warning via `ROBOTICK_WARNING` (or ROBOTICK_FATAL_EXIT it can't be handled elegantly), and then cleanly return so the engine can continue ticking. We keep the engine exception-free for MCU determinism.

### 🕸️ Web Server startup

`WebServer::start` now fatally exits if it cannot bind the requested port, making port conflicts immediately visible. Passing `port = 0` lets the OS pick an available port, so automated tests/tools can avoid busy sockets and then query `get_bound_port()` once the server runs.

---

## 🚀 What is Robotick?

Robotick is a modern control engine designed to execute structured workloads with:

- Predictable, real-time performance
- Introspectable config, input, and output fields
- Zero-allocation memory buffer layout
- Modular composition and orchestration
- Full lifecycle management (load, start, tick, stop)
- Remote telemetry and future remote control

Built for early learners and industry professionals alike, Robotick is simple enough for educational bots and sophisticated enough for serious research and development.

---

## ✨ Key Features

### 🧩 Modular Workloads

Each unit of logic is a workload - a small, testable module with clearly defined inputs, outputs, and config:

```cpp
struct HelloWorkload {
    HelloConfig config;
    HelloInputs inputs;
    HelloOutputs outputs;

    void tick(const TickInfo& tick_info);
};
```

ROBOTICK_REGISTER_WORKLOAD(HelloWorkload, HelloConfig, HelloInputs, HelloOutputs)

Reflection macros make every field visible and usable for config, scripting, or telemetry.

### 🔁 Real-Time Engine

- Individual tick rates per workload
- Deterministic scheduling
- Ready for multithreaded execution
- Exceptionally consistent timing on MCUs
- Excellent latency characteristics on general-purpose platforms

### 📦 Composition System

Compose workloads into rich behaviours:

- `SequenceWorkload`: run workloads in order
- `SyncedPairWorkload`: parallel aligned ticking
- Future: event-driven, reactive, and conditional branching

### 🔬 Introspection & Reflection

Every field in config, inputs, and outputs is exposed at runtime, enabling:

- Remote configuration
- Schema-aware scripting
- Full field inspection for telemetry

No boilerplate. No fuss. Just structured access to everything that matters.

### 📡 Remote Telemetry & UI

Designed for live telemetry from the start:

- Structured data streaming
- Pluggable transport backends (UART, MQTT, etc.)
- UI- and scripting-ready introspection
- Foundation for remote tuning and remote control

### 🐍 Python Bindings

Use Python to:

- Orchestrate workload execution
- Inspect and modify live system state
- Visualise real-time data
- Integrate with scientific workflows

Zero-copy overlay support is in progress for high-efficiency interop.

### 🧪 Simulation-First Testing

Built to be tested:

- Mockable physics interfaces
- Unit testable workloads
- Consistent simulated timing for verification
- Easy integration with CI

---

## 📁 Project Structure

```bash
robotick/
├── cpp/                    # Core engine and built-in workloads
├── python/                 # Python bindings (PyBind11)
├── tests/                  # Unit test suite (Catch2)
└── tools/                  # Dev tools and profilers
```

---

## 🛠️ Platform Support

- ✅ STM32 (e.g. B-G431B-ESC1)
- ✅ Raspberry Pi (Pi 2 → Pi 5)
- ✅ Desktop (Windows / Linux)
- ✅ Jetson (Nano, Orin) via arm64

Compiled and deployed executables are tested across an expanding range of targets.

---

## 🎯 Design Principles

- **Structured**: Everything is inspectable, composable, and testable
- **Reliable**: High-precision real-time behaviour on real hardware
- **Performant**: Zero-copy, buffer-based layout for speed and clarity
- **Accessible**: Easy for learners, powerful for researchers
- **Flexible**: Modular composition and optional Python integration
- **Embeddable**: Works standalone or embedded into larger stacks

### 🧠 Ownership & Singletons

- **TypeRegistry** is the only global singleton in the engine. It is written exactly once during startup by the thread that registers workloads and sealed via `TypeRegistry::seal()`. The registry asserts that all mutations come from that same thread, so contributors must finish registration before launching `Engine::load()`. After sealing, it becomes immutable and thread-safe to read.
- **TelemetryServer** and **RemoteEngineConnections** are not globals; they are owned by `Engine::State` and rely on RAII. They start when the engine starts and stop automatically in `Engine::~Engine`/`Engine::run` teardown. This keeps sockets, ports, and threads scoped to each engine instead of leaking across processes.
- **No hidden singletons**: other subsystems (data connections, workloads, etc.) are explicit members hanging off `Engine::State`. If a component needs process-wide coordination, document it alongside its owning class rather than relying on unnamed globals.
- See `docs/ownership.md` for the full policy and contributor guidelines, and `docs/module-map.md` for a subsystem/init-order walkthrough.

---

## 🗺️ Roadmap

- [ ] Python zero-copy memory overlay
- [ ] Live telemetry viewer (web + Python)
- [ ] Graphical workload visualiser
- [ ] Composition scripting and editor
- [ ] ROS2 integration bridge
- [ ] Official templates for STM32, Pi, Jetson

---

## 📄 License

Licensed under the **Apache 2.0 License** – free to use, adapt, and build upon.

---

## 💬 Get Involved

Robotick is in active development. If you're passionate about robotics, real-time systems, or modular control architectures, your feedback and ideas are very welcome.

Visit [https://robotick.org](https://robotick.org) for updates, demos, and documentation (coming soon).
