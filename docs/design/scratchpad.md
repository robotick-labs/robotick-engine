> ⚠️ Draft notes. Not yet reviewed/formalised.

# 🧩 Composition Workloads (Robotick)

Defines how child workloads are ticked in a coordinated group. Each group owns its tick schedule and manages its children accordingly.

## ✅ SequencedGroupWorkload

- Ticks **all children sequentially** on a **single thread**
- Driven by a fixed **tick rate** on the parent group
- Suitable for **MCUs** or simple systems (e.g. STM32 main loop)
- Logs overruns if the full group tick takes too long

```
Tick Model:
[Tick A] → [Tick B] → [Tick C] → sleep → repeat
```

## ✅ SyncedGroupWorkload

- Ticks **all children in parallel**
- **Each tick occurs at a fixed interval** (e.g. every 10 ms)
- Each child must complete before the next tick window
- **Overruns are logged** per-child, but the group continues at a **steady drumbeat**
- Designed for **PC / Pi / Linux** environments with real threading
- Children are treated equally — no special priority or rate

```
Tick Model:
[Tick A] [Tick B] [Tick C]  ← all start together
   ↓        ↓       ↓
Check for overrun next tick; do not wait.
```

## ⚠️ (ResyncedGroupWorkload) – Possibly redundant

- Considered as an adaptive model where the **fastest child defines the tick**
- Others join in if they're ready
- Might be replaced by **SyncedGroupWorkload with soft overrun tolerance**
- Not currently prioritised for implementation

---

## Notes

- All workloads are **standard-layout**, no virtual methods or inheritance
- `set_children(...)` provides a list of child `WorkloadHandle`s at setup time
- Future MCU implementations may reuse these structures but substitute ISR-driven backends
