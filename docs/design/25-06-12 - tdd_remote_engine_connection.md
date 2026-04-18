# Design: RemoteEngineConnection (Robotick Engine)
*Status: Draft – 2025-06-11*  
*Author: Paul Connor*

> Historical note:
> This document is an early design sketch and should be read as historical
> context rather than as the authoritative description of the current REC
> implementation. The main body below is preserved intentionally. For the
> current implementation note, see
> `robotick/robotick-engine/docs/design/25-10-23 - white_paper_remote_engine_connection.md`.

---

## Overview

`RemoteEngineConnection` is a bidirectional communication class used to bridge separate `robotick::Engine` instances running on different devices (e.g., Pi5 "brain" and ESP32 "spine").

Its purpose is to:
- Transmit input/output data between engines, as defined by the (locally-applicable only thus far) DataConnection model-construct.
- Allow engines to be physically or logically separated.
- Support both `proactive` (brain/host) and `passive` (spine/client) connection modes.
- Unify TCP, UART (later), and possibly other transports under a common interface that the user just sees as a DataConnection with some minimal transport-type info.

---

## Roles and Responsibilities

| Role      | Responsibilities |
|-----------|------------------|
| **Brain** (proactive) | - Connects to the spine<br> - Sends `subscribe` handshake<br> - Sends inputs each tick<br> - Receives spine outputs |
| **Spine** (passive)   | - Waits for connection<br> - Receives `subscribe` and replies with `ack`<br> - Sends outputs every tick<br> - Applies brain inputs |

After the handshake, all connections enter a `tick-synchronous` phase:
- Each engine *pushes* its current outputs.
- Each engine *receives* and applies the other's inputs.

---

## Lifecycle Summary

| State            | Brain                              | Spine                              |
|------------------|-------------------------------------|-------------------------------------|
| Disconnected     | Constructs RemoteEngineConnection   | Constructs RemoteEngineConnection   |
| Connected        | Initiates TCP connect               | Accepts TCP connection              |
| Handshake        | Sends `subscribe` JSON              | Sends `ack` JSON with field list    |
| Subscribed       | Starts pushing inputs               | Starts pushing outputs              |
| Ticking          | `tick()` each frame:<br>- Sends inputs<br>- Reads outputs | `tick()` each frame:<br>- Sends outputs<br>- Reads inputs |
| Shutdown         | Disconnects                         | Cleans up after detect disconnect   |

---

## Modes

```cpp
enum class Mode {
    Sender,  // brain-side
    Receiver     // spine-side
};
```

---

## Message Types (JSON)

### `subscribe` (brain → spine)
```json
{
  "type": "subscribe",
  "fields": ["motor1_speed", "gyro_angle"]
}
```

### `ack` (spine → brain)
```json
{
  "type": "ack",
  "supported_fields": ["motor1_speed", "motor2_speed", "gyro_angle"]
}
```

### `outputs` (spine → brain)
```json
{
  "outputs": {
    "motor1_speed": 0.42,
    "gyro_angle": 13.7
  }
}
```

### `inputs` (brain → spine)
```json
{
  "inputs": {
    "target_velocity": 0.5,
    "steering": -0.1
  }
}
```

---

## API Sketch

```cpp
class RemoteEngineConnection {
public:
    enum class Mode { Receiver, Sender };

    RemoteEngineConnection(const std::string& uri, Mode mode);

    void set_remote_name(const std::string& name);
    void set_requested_fields(const std::vector<std::string>& fields);
    void set_available_outputs(const std::vector<std::string>& fields);

    void tick();  // Sends outputs, receives inputs

    void apply_received_data();  // Called before blackboard tick
    bool is_connected() const;
    bool is_ready_for_tick() const;

private:
    void open_socket(); // Handles listener or connector
    void handle_handshake();
    void handle_tick_exchange();
    void cleanup();

    enum class State { Disconnected, Connected, Subscribed, Ticking } state;
    Mode mode;

    int socket_fd;
    std::string uri;
    std::string remote_name;
    std::vector<std::string> requested_fields;
    std::vector<std::string> available_fields;
    std::string inbound_json;
    std::string outbound_json;
};
```

---

## Future Considerations

- UART support via `uart:/dev/ttyUSB0` URIs.
- Optional authentication layer.
- Binary format for lower latency.
- Multi-connection routing layer.
- Framing protocol for reliable streaming (if not using JSON).

---

## Summary

This design supports a symmetric, extensible, and field-targeted connection mechanism that allows any engine to stream its outputs and receive inputs from remote models. It centers authority on the **brain**, keeping the **spine passive**, yet enables rich future upgrades.

---

## Addendum: 2026-04-18 Corrections

The text above is preserved as an early design note. The points below capture
the main ways the current implementation differs or has become clearer.

### What changed materially

- The current REC protocol is not JSON-based.
- The current wire protocol uses a binary `MessageHeader` with magic `RBIN`,
  versioning, payload length, and binary message types.
- The current message set is:
  - `Subscribe`
  - `FieldsRequest`
  - `Fields`
- The handshake payload is currently:
  - sender tick rate as a 4-byte float encoded through network-order `uint32_t`
  - newline-separated field paths
- Field data is currently transmitted as raw concatenated bytes in registered
  field order, not as JSON objects.

### What the current architecture actually looks like

- `RemoteEngineConnection` remains the 1-to-1 transport unit.
- `RemoteEngineConnections` owns the engine-side orchestration for multiple
  remote peers.
- `RemoteEngineDiscoverer` provides multicast discovery and unicast reply
  bootstrapping.
- `InProgressMessage` provides non-blocking framed send/receive progress.

That layered picture is much closer to the current implementation than the API
sketch in the original draft.

### Role framing that should now be read loosely

- The original "brain/spine proactive/passive" framing is still useful as an
  intuition, but the current code uses explicit `Sender` and `Receiver` roles.
- Discovery and orchestration are now more central to the real implementation
  than the original draft suggests.

### Transport scope

- `RemoteModelSeed::Mode` still exposes `IP`, `UART`, and `Local`.
- In the reviewed current orchestration path, `IP` and `Local` are the
  implemented practical modes.
- `Local` currently resolves to loopback discovery / connection bootstrap.
- `UART` remains an enum-level direction rather than a reviewed implemented path
  in `RemoteEngineConnections`.

### What is still conceptually right

- REC is still a field-targeted link between live engines rather than a generic
  message bus.
- The pacing model is still explicit and cooperative.
- Separate engine instances still exchange declared workload field data through
  a deterministic transport path.
