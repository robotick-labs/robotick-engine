# Robotick Remote Engine Communication Layer (REC)

**Author:** Robotick Labs (Paul Connor)
**Date:** October 2025
**License:** Apache 2.0

---

## 1. Executive Summary

The **Robotick Remote Engine Communication Layer (REC)** provides a deterministic, real-time TCP data link between Robotick engine instances — typically between an embedded controller (ESP32, Pi, or MCU) and a higher-level host (Linux or desktop).

The goal is to synchronise structured workload data (inputs, outputs, and telemetry) across heterogeneous tick-rates **without buffering, flooding, or latency drift**. REC achieves this using a minimal token-based protocol with explicit pacing and cooperative rate negotiation, designed to operate comfortably at sub-millisecond tick intervals.

Unlike ROS2, DDS, or MQTT-based systems, REC treats both endpoints as _live engines_ with their own simulation or control loops, coordinating through mutual pacing rather than pub/sub semantics. The result is a link that feels more like a synchronous bridge between two real-time loops than a message bus.

---

## 2. Layered Architecture Overview

The communication layer is composed of four cooperating components:

| Layer | Component                        | Responsibility                                                                                                          |
| ----- | -------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| L4    | **RemoteEngineDiscoverer**       | Multicast-based discovery and pairing of active engines. Handles peer announcements and automatic connection bootstrap. |
| L3    | **RemoteEngineConnections**      | Manages multiple REC instances. Provides lifecycle, reconnection, and per-peer routing.                                 |
| L2    | **RemoteEngineConnection (REC)** | Implements the 1-to-1 protocol: handshake, pacing, and data exchange.                                                   |
| L1    | **InProgressMessage**            | Framed message send/receive abstraction with non-blocking TCP I/O.                                                      |

### 2.1 Overview Diagram

```
┌───────────────────────────────┐
│  RemoteEngineDiscoverer       │   ← UDP broadcast discovery
└───────────────┬───────────────┘
                │ 1:N
┌───────────────▼────────────────┐
│  RemoteEngineConnections       │   ← Manages all active peers
└───────────────┬────────────────┘
                │ 1:1
┌───────────────▼────────────────┐
│  RemoteEngineConnection (REC)  │   ← Handshake, pacing, data transfer
└───────────────┬────────────────┘
                │ framed I/O
┌───────────────▼────────────────┐
│  InProgressMessage             │   ← Non-blocking send/receive
└────────────────────────────────┘
```

Each layer builds strictly upon the one below, maintaining clean separation: no global state, no hidden threads, and deterministic progression via the caller’s tick loop.

---

## 3. The Protocol: A Token-Based Real-Time Exchange

REC’s protocol is intentionally simple but expressive enough to support deterministic field updates. It is **not** a pub/sub or RPC layer; rather, it’s a half-duplex token-driven data pipe.

### 3.1 Message Types

| Message Type    | Direction         | Purpose                                                                                         |
| --------------- | ----------------- | ----------------------------------------------------------------------------------------------- |
| `Subscribe`     | Sender → Receiver | Handshake: announces tick-rate and field list.                                                  |
| `FieldsRequest` | Receiver → Sender | READY token signalling receiver’s readiness to accept next field(s). Includes mutual tick-rate. |
| `Fields`        | Sender → Receiver | Transmits binary field data blob, in fixed order established at handshake.                      |

### 3.2 Pacing Overview

```
Sender (500 Hz)                        Receiver (30 Hz)
───────────────────────────────────────────────────────────────
Subscribe ───────────────────────────▶ Handshake (tick rate negotiation)
◀──────────────────────────────────── READY (FieldsRequest, 30 Hz)
FIELDS [n..n+k] ─────────────────────▶
◀──────────────────────────────────── READY
FIELDS [n+k..n+2k] ─────────────────▶
```

Only the **receiver** emits READY tokens. The sender may transmit one or several FIELD frames after each READY but must pause once the mutual pacing window expires.

This creates an elegant, self-balancing dynamic: a 500 Hz sender and 30 Hz receiver converge automatically on a 30 Hz effective exchange rate without buffer overflow or wasted packets.

---

## 4. Connection Lifecycle

### 4.1 Handshake Sequence

1. **Sender connects (TCP)** → enters `ReadyForHandshake`.
2. **Sender → Receiver:** `Subscribe` message containing:

   - local tick-rate (float, 4 bytes, network-endian)
   - newline-delimited field paths.

3. **Receiver:** binds field paths via user-provided `FieldBinder` callback.
4. **Receiver:** computes `mutual_tick_rate = min(sender, receiver)`.
5. **Receiver → Sender:** first `FieldsRequest` containing mutual rate.
6. **Both sides:** transition to `ReadyForFields` and begin exchange.

### 4.2 Rate-Limited Field Streaming

Each `tick()` call represents one local simulation or control iteration. The sender:

- Tracks `ticks_until_next_send = ceil(local_rate / mutual_rate)`.
- Sends field packets immediately upon READY or countdown expiry.

The receiver:

- Consumes all queued FIELD messages (burst-safe).
- Emits a new READY once all have been processed.

---

## 5. Discoverer and RECs Coordination

While a single REC forms the fundamental link, **RemoteEngineConnections (RECs)** and **RemoteEngineDiscoverer** provide the orchestration layer:

### 5.1 RemoteEngineConnections

- Maintains multiple active REC instances.
- Performs health monitoring and disconnection recovery.
- Provides per-peer tick propagation into the Robotick engine model.

### 5.2 RemoteEngineDiscoverer

- Uses UDP multicast to broadcast engine presence (`DISCOVER` and `HELLO` packets).
- Enables zero-config peer discovery in local networks.
- Passes connection parameters upward to RECs for automatic pairing.

Together, these layers provide a zero-configuration distributed runtime, where each Robotick engine can auto-discover, handshake, and synchronise with peers at runtime — ideal for separating _brain_ (host) and _body_ (MCU) executables.

---

## 6. Key Design Decisions and Rationale

| Principle                     | Explanation                                                                                                                               |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| **Deterministic pacing**      | We rely on explicit READY tokens rather than TCP flow control to ensure the link’s timing behaviour matches the engines’ real-time loops. |
| **Static schema**             | Field paths are bound once at startup — no runtime allocation or dynamic lookup. Predictable, cache-friendly, MCU-safe.                   |
| **Endianness-neutral floats** | Floats are transmitted as `uint32_t` network order (big-endian) for cross-platform consistency.                                           |
| **One connection per peer**   | Simpler lifecycle and routing; avoids multiplexing logic.                                                                                 |
| **Symmetric roles**           | Both sides share identical state machine logic, differing only by initiation role.                                                        |

---

## 7. Original Contributions

1. **Token-based mutual pacing** — The READY/FIELD exchange forms a distributed flow-control mechanism that’s both minimal and deterministic. Unlike DDS QoS or TCP congestion control, this ensures real-time sync between heterogeneous loops.
2. **Negotiated tick-rate synchronisation** — Using the `min(sender, receiver)` rule establishes a shared ground truth frequency without central arbitration.
3. **Field-path schema binding** — A human-readable yet deterministic schema exchange avoids complex reflection or IDL generation.
4. **Cooperative non-blocking I/O model** — REC advances through `tick()` calls only, avoiding threads or blocking waits. Behaviour remains identical on MCU or desktop.
5. **Hybrid discovery model** — Combining UDP multicast for discovery and TCP for payloads is unusual in embedded robotics, offering both simplicity and determinism.

---

## 8. Performance and Reliability Characteristics

- **Latency:** Sub-millisecond on LAN; deterministic jitter under 1 ms typical.
- **CPU usage:** < 1% on ESP32-S3 at 500 Hz link.
- **Fault tolerance:** Automatic reconnection with exponential retry.
- **Safety:** No heap allocations during tick; all vectors pre-allocated post-handshake.
- **Compatibility:** Any platform with BSD sockets and C++17.

---

## 9. Future Work

- **Frame IDs / sequence validation:** to enable packet loss or skip detection.
- **Timestamped data frames:** optional temporal alignment for logs.
- **Compression or delta encoding:** to reduce bandwidth for large structs.
- **WebSocket transport:** for seamless integration with Robotick Hub web interface.
- **TLS or CRC framing:** lightweight integrity or encryption layers for WAN scenarios.

---

## 10. Conclusion

The Remote Engine Communication Layer represents a clean, MCU-safe bridge between two live, real-time systems. Its design emphasises **determinism, simplicity, and expressive clarity** over generality. By replacing the complexity of middleware stacks with an explicit, rate-negotiated protocol, REC allows Robotick engines to communicate as if they were one continuous control loop — no buffering, no surprises.

We believe this approach offers a foundation not just for internal use, but as a reusable open-source communication substrate for expressive and embodied robotics systems.

---
