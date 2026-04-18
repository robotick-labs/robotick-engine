# 🧩 Composition Workloads - Technical Design

> Historical note:
> This document was written near the start of Robotick development and should be
> read primarily as an early design note. The main body below is preserved as
> historical context. For current engine lifecycle details, see
> `robotick/robotick-engine/docs/module-map.md`. For the current concrete
> composition workload implementations, check `robotick-core-workloads`.

Composition workloads in Robotick define how a group of child workloads are ticked in coordination. Each group sets its own tick cadence and governs how and when children are invoked.

---

## 🧠 Tick Policy

- A group’s tick rate defines the **maximum frequency** at which any of its children may tick.
- **Slower children** are allowed — they will be called only when their individual tick interval has elapsed.
- **Faster children are not allowed** — their tick opportunity is limited to the group’s cadence.
- Only **root workloads** can own a tick loop. All child groups are ticked by their parent.
- Each workload instance must belong to exactly one parent group or the Engine's root. This is enforced at setup. Adding a workload to multiple groups (even across threads) is a configuration error.

---

## ✅ `SequencedGroupWorkload`

**Deterministic sequential ticking** on a single thread.

### Characteristics

- All children tick one after another in a fixed order.
- Suitable for **MCUs** or minimal-thread environments (e.g. STM32 main loop).
- Fixed tick interval defines the loop rate.
- If total execution time exceeds the interval, the group logs an **overrun**.
- Per-child tick intervals are respected within the group loop.

### Timeline (Single Thread)

```
 Main Loop →
  ┌──── Tick A ──── Tick B ──── Tick C ────┐
  │                                        │
  └─────────────── sleep ──────────────────┘
```

---

## ✅ `SyncedGroupWorkload`

**Parallel child ticking** with coordinated timing.

### Characteristics

- All children tick in parallel at each group interval.
- Designed for **Linux / PC / Raspberry Pi** (>MCU) systems with threads or async runtimes.
- Per-child tick intervals are honoured; skipped if not yet due.
- Each child is **expected** to finish before the next group tick window, but **overruns are allowed**.
- If a workload overruns, it will **not delay** the group. It will continue running in the background and will be ticked again on the **next parent-tick after it completes**.
- Overruns are **logged per-child for debugging**, unless explicitly configured as expected (e.g. 1Hz console output).
- Serves as the **root tick barrier** in systems needing safe per-frame data movement.

## 🎞️ Execution Timeline Example

Demonstrates a `SyncedGroupWorkload` running at **1kHz**, with a mixture of child workloads:

- Some ticking at slower rates
- Some with longer execution times
- A child `SequencedGroup` with fixed-order inner ticks
- All ticks aligned to a central sync frame (safe barrier for data transfer)

```
Time → → →      0ms       1ms       2ms       3ms       4ms       5ms

SyncedGroup (1kHz)
├─ A (1kHz)     [──A──]   [──A──]   [──A──]   [──A──]   [──A──]   [──A──]
├─ B (500Hz)    [───────B───────]   [───────B───────]
├─ C (1Hz)      [───────────────────────────────C──────────────────────────────...
├─ SeqGroup D (500Hz)
│               [─D1─][────D2────]  [─D1─][────D2────]  [─D1─][────D2────]
└─ E (333Hz)    [──E──]                      [──E──]                      ...

                ↑        ↑           ↑         ↑           ↑
              sync/    sync/       sync/     sync/       sync/
              tick     tick        tick      tick        tick
```

---

## ✅ `McuSyncedPair` (interrupt + main loop)

**MCU-specific tick splitting** between a fast hardware-timed ISR and a slower main loop.

### Purpose

- Enables **precise periodic ticking** via a hardware timer interrupt.
- Allows the **main loop** to run less time-critical sequencing.
- Only **one interrupt-driven workload (or group)** is supported per timer.
- The main loop can still be logically "synced" — it observes the same base cadence, but runs slower if needed.

### Differences from `SyncedGroupWorkload`

| Aspect                       | `SyncedGroupWorkload`     | `McuSyncedPair`                              |
| ---------------------------- | ------------------------- | -------------------------------------------- |
| Platform                     | Linux / PC / Raspberry Pi | MCU (e.g. STM32)                             |
| Parallel execution           | Threads / async           | ISR + main loop                              |
| Max children per timing unit | Multiple                  | One in ISR context                           |
| Scheduling base              | Software timers           | Hardware timer interrupt                     |
| Overrun handling             | Logged per-child          | Interrupt must return fast (no overrun logs) |
| Tick sync                    | True sync                 | Sync possible but looser (main may lag)      |

### Thread Model (MCU: ISR + main loop)

```
Timer ISR Thread →
                ┌ Tick SensorUpdate ┐   ┌ Tick SensorUpdate ┐

Main Loop Thread →
  ┌ Tick MainLogic (Seq Group) ┐     ┌ Tick MainLogic ┐

(Timer ISR runs fast & precise; Main loop ticks when it can)
```

---

## 🔧 Implementation Notes

- All workloads use **standard layout structs** — no virtual inheritance.
- Workload groups are configured with `set_children(...)`, which binds `WorkloadHandle`s to each group.
- Tick dispatch uses timestamps to determine whether a child is due (`now - last_tick >= interval`).
- Overrun detection to be reinstated for both `SequencedGroup` and `SyncedGroup`.

---

## 🧪 Design Rationale

- Keeps timing control **local to the group** while supporting mixed-frequency substructures.
- Avoids hidden tick escalations from children requesting higher frequencies than the parent can provide.
- Enables embedded (MCU) and high-level (threaded) systems to use a **unified scheduling abstraction**.

---

## Addendum: 2026-04-18 Corrections

The original text above is preserved intentionally. The points below correct it
where the implementation has since diverged or become clearer.

### What is current and accurate

- `SequencedGroupWorkload` is implemented in `robotick-core-workloads`.
- `SyncedGroupWorkload` is implemented in `robotick-core-workloads`.
- The core tick-policy statement still holds: a child may run at the same rate
  as its parent or slower, but not faster.
- The engine still treats the root workload as the owner of the engine-level
  tick loop; child groups are driven by their parent workload rather than by the
  engine directly.
- `set_children(...)` is still the binding hook composition workloads use to
  inspect children and claim or delegate `DataConnectionInfo` ownership.

### What should be read as historical design intent

- `McuSyncedPair` remains more of an idea than a reviewed current implementation.
- "Overrun detection to be reinstated" is outdated wording; timing/overrun stats
  are recorded in the current engine and composition workload implementations.
- The specific `SyncedGroupWorkload` execution details in the original note are
  useful as a mental model, but the authoritative behaviour now lives in the
  current `robotick-core-workloads` source and tests.

### Current implementation split

- `robotick-engine` owns:
  - model validation
  - contiguous buffer construction
  - child pointer hookup
  - data-connection resolution
  - the root tick loop
- `robotick-core-workloads` owns:
  - the concrete scheduling behaviour of sequenced and synced groups

That split is why this document still reads sensibly at a high level, while some
of the lower-level implementation statements have aged.
