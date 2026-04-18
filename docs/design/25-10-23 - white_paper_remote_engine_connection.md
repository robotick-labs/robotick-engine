<!--
Copyright Robotick contributors
SPDX-License-Identifier: Apache-2.0
-->

# Robotick Remote Engine Communication Layer (REC)

Status: current implementation note  
Originally written: 2025-10-23  
Last reviewed: 2026-04-18  
Reviewed against:
- `cpp/src/robotick/framework/data/RemoteEngineConnection.cpp`
- `cpp/src/robotick/framework/data/RemoteEngineConnections.cpp`
- `cpp/src/robotick/framework/data/RemoteEngineDiscoverer.cpp`
- `cpp/src/robotick/framework/data/InProgressMessage.cpp`
- `cpp/tests/framework/data/RemoteEngineConnection.test.cpp`
- `cpp/tests/framework/data/RemoteEngineConnections.test.cpp`
- `cpp/tests/framework/data/RemoteEngineDiscoverer.test.cpp`

## 1. What REC Is

REC is the current transport stack Robotick uses to move declared workload field
data between separate engine instances.

Today it is composed of four layers:

| Layer | Component | Responsibility |
| --- | --- | --- |
| L4 | `RemoteEngineDiscoverer` | UDP multicast discovery and reply handling |
| L3 | `RemoteEngineConnections` | Engine-owned orchestration for all remote peers |
| L2 | `RemoteEngineConnection` | One TCP sender/receiver link with handshake, pacing, reconnect, and field exchange |
| L1 | `InProgressMessage` | Incremental framed message send/receive over non-blocking sockets |

This document is intended to describe how the current implementation works, not
the earlier aspirational design language.

## 2. Current Scope and Boundaries

### What `robotick-engine` owns

- remote-model declaration via `RemoteModelSeed`
- discovery and pairing
- field binding from model-declared remote edges
- framed TCP exchange
- reconnect and health-state logging
- integration into the engine tick loop

### What the model provides

- which remote model names exist
- which local fields are sent to which remote fields
- transport mode and channel metadata

### What REC does not currently try to be

- a generic pub/sub middleware
- a buffered message bus
- an RPC layer
- a schema compiler / IDL system

It is a deterministic field-link between live engines.

## 3. Current Data Model

Remote links are declared through `RemoteModelSeed` plus remote
`DataConnectionSeed` entries.

The current transport enum is:

- `IP`
- `UART`
- `Local`

Important current-truth note:

- `RemoteModelSeed::Mode` includes `UART`, but the reviewed orchestration path in
  `RemoteEngineConnections` currently implements `IP` and `Local` behaviour.
- `Local` currently resolves discovery target address to `127.0.0.1`.
- `IP` currently uses `comms_channel` as an IPv4 target, optionally prefixed by
  `ip:`.

## 4. Architecture Overview

### 4.1 `RemoteEngineDiscoverer`

`RemoteEngineDiscoverer` is a tick-driven UDP discovery helper.

Current protocol:

- multicast group: `239.10.77.42`
- port: `49777`
- sender discovery packet: `DISCOVER_PEER <target_model> <source_model> <reply_port>`
- receiver reply packet: `PEER_HERE <model_id> <rec_port> <telemetry_port> <is_gateway>`

Current behaviour:

- sender mode broadcasts at 10 Hz until a peer is discovered
- receiver mode listens for multicast discovery and replies via unicast
- receiver can ask a callback to provide the dynamic TCP port that the sender
  should connect to
- reply packets also carry telemetry routing metadata

### 4.2 `RemoteEngineConnections`

`RemoteEngineConnections` is the engine-owned orchestration layer.

Current behaviour:

- owns one discovery receiver for incoming remote senders
- owns one `RemoteEngineConnection` sender per declared `RemoteModelSeed`
- creates dynamic receiver-side `RemoteEngineConnection` objects on discovery
  request
- binds local fields for both sending and receiving using
  `DataConnectionUtils::find_field_info(...)`
- updates telemetry peer routes when discovery replies include telemetry port and
  gateway metadata

`Engine::load()` calls `RemoteEngineConnections::setup(...)`, and the engine run
loop calls `RemoteEngineConnections::tick(...)` every tick.

### 4.3 `RemoteEngineConnection`

`RemoteEngineConnection` is the 1-to-1 binary field transport.

One instance is always in one of two roles:

- `Sender`
- `Receiver`

Current state machine:

- `Disconnected`
- `ReadyForHandshake`
- `ReadyForFields`

The connection is fully caller-driven: there are no hidden worker threads in the
REC path itself. Progress happens only when the owner calls `tick(...)`.

### 4.4 `InProgressMessage`

`InProgressMessage` is the framing and partial-I/O helper.

Current behaviour:

- uses a binary header with magic `RBIN`
- versioned framing (`version = 1`)
- 12-byte fixed header
- supports staged payload receive and streamed payload receive
- supports non-blocking partial header/payload progress across ticks
- caps payload size at 1 MiB

This is the key reason the protocol remains deterministic without blocking the
engine loop.

## 5. Current Wire Protocol

### 5.1 Message framing

Every REC message is framed as:

1. `MessageHeader`
2. payload

Current `MessageHeader` layout:

- `magic[4]`
- `version`
- `type`
- `reserved`
- `payload_len`

The header is serialized in network byte order where relevant.

### 5.2 Current message types

| Type | Value | Direction | Purpose |
| --- | --- | --- | --- |
| `Subscribe` | `1` | Sender -> Receiver | Announces sender tick rate and newline-separated field paths |
| `FieldsRequest` | `2` | Receiver -> Sender | READY token carrying mutual tick rate |
| `Fields` | `3` | Sender -> Receiver | Raw concatenated field bytes in registered order |

This is the current protocol. It is not JSON-based.

## 6. Handshake

### 6.1 Sender side

When a sender reaches `ReadyForHandshake`, it:

1. verifies that at least one field has been registered
2. computes handshake payload capacity
3. sends a `Subscribe` message containing:
   - 4 bytes: sender tick rate as float encoded through network-order `uint32_t`
   - remaining bytes: newline-separated field paths

After the handshake message completes, the sender transitions to
`ReadyForFields` and immediately tries to emit the first `Fields` message.

### 6.2 Receiver side

When a receiver reaches `ReadyForHandshake`, it:

1. begins streamed payload receive for the `Subscribe` payload
2. reads the first 4 bytes as sender tick rate
3. parses the remaining bytes as newline-separated field paths
4. calls the binder callback once per path
5. converts successfully bound fields into its exact-sized receive field array
6. computes:
   - `mutual_tick_rate_hz = min(sender_tick_rate_hz, local_receiver_tick_rate_hz)`
7. transitions to `ReadyForFields`
8. immediately sends the first `FieldsRequest`

Current failure rules:

- invalid sender tick rate is fatal
- any failed field bind is fatal
- oversized field path is fatal

## 7. Field Exchange and Pacing

### 7.1 Receiver-driven pacing

The current pacing model is receiver-driven.

In `ReadyForFields`:

- receiver sends `FieldsRequest` messages
- sender receives `FieldsRequest`
- sender updates `mutual_tick_rate_hz` from the request payload
- sender emits `Fields` messages
- receiver consumes all pending `Fields` messages before issuing the next
  `FieldsRequest`

### 7.2 Mutual tick rate

Current rule:

- sender announces its local tick rate in the handshake
- receiver computes `min(sender_tick_rate_hz, receiver_tick_rate_hz)`
- receiver returns that mutual rate in each `FieldsRequest`
- sender uses that value to compute `ticks_until_next_send`

Current sender scheduling rule:

- `ticks_until_next_send = max(1, floor(local_tick_rate / mutual_tick_rate))`

This allows a fast sender to pace itself down to the slower receiver without
explicit buffering logic.

### 7.3 Current field payload format

The `Fields` payload is just the concatenated raw bytes of the sender's
registered fields, in registration order.

Current implications:

- there is no field name or per-field header in a `Fields` packet
- sender and receiver must agree on path order and field sizes at handshake
- receiver writes incoming bytes directly into bound destination pointers

## 8. Connection Lifecycle

### 8.1 Sender lifecycle

Current sender flow:

1. configured with:
   - `my_model_name`
   - `target_model_name`
   - `remote_ip`
   - `remote_port`
2. in `Disconnected`, creates a non-blocking TCP socket and attempts connect
3. transitions to `ReadyForHandshake`
4. sends `Subscribe`
5. enters `ReadyForFields`
6. alternates between receiving `FieldsRequest` and sending `Fields`
7. on failure, disconnects and schedules reconnect backoff

### 8.2 Receiver lifecycle

Current receiver flow:

1. configured with `my_model_name`
2. in `Disconnected`, binds a non-blocking TCP listening socket on port `0`
3. records the assigned ephemeral listen port
4. accepts one incoming TCP connection
5. transitions to `ReadyForHandshake`
6. receives `Subscribe`
7. enters `ReadyForFields`
8. alternates between receiving `Fields` and sending `FieldsRequest`
9. on failure, disconnects, clears bound fields, and returns to listening

## 9. Reconnect and Health Behaviour

The current implementation includes explicit reconnect and health-state
behaviour.

### 9.1 Backoff

Current constants:

- short retry cadence while still disconnected: `0.05 s`
- backoff min: `0.10 s`
- backoff max: `2.00 s`
- multiplier: `2.0`

### 9.2 Health logging

Current behaviour:

- logs when connection attempt starts
- logs when a connection becomes healthy
- logs when a previously healthy connection becomes unhealthy
- suppresses repeated spam for the same unhealthy period using a bounded shared
  log-state table

### 9.3 Disconnect semantics

`disconnect()` currently:

- tries to drain partially in-progress send/receive messages for up to 500 ms
- vacates both half-duplex `InProgressMessage` instances
- closes the socket
- schedules reconnect timing
- clears receiver-side bound fields so the next handshake starts cleanly

That drain-before-close behaviour is there to avoid leaving peers desynchronized
 by half-written framed payloads.

## 10. Current Integration with Engine and Telemetry

`RemoteEngineConnections::setup(...)` currently:

- initializes a discovery receiver for the local model
- configures incoming receiver connections on demand
- for outgoing remote models:
  - creates one discoverer sender
  - creates one sender `RemoteEngineConnection`
  - binds local source fields for each declared remote edge

When a peer is discovered:

- telemetry peer routing is updated through `TelemetryServer::update_peer_route(...)`
- the sender-side `RemoteEngineConnection` is configured with peer IP and REC
  port

This is why the REC and telemetry systems are related but still separate:

- REC moves bound field data
- telemetry uses discovered peer metadata to expose gateway and peer routes

## 11. Current Guarantees and Non-Guarantees

### What the current implementation guarantees

- non-blocking progress through caller-owned `tick(...)`
- binary framed protocol with version and payload-length validation
- deterministic field ordering once handshake succeeds
- receiver-driven pacing
- reconnect after disconnect
- engine ownership via `RemoteEngineConnections`

### What is not currently implemented as a reviewed truth

- UART transport in the reviewed orchestration path
- authentication or encryption
- per-frame sequence IDs
- compression or delta encoding
- generic multiplexing over one socket
- arbitrary dynamic schema negotiation beyond path-list handshake

## 12. Practical Reading Order

For the current implementation, read in this order:

1. `cpp/src/robotick/framework/data/RemoteEngineConnections.cpp`
2. `cpp/src/robotick/framework/data/RemoteEngineDiscoverer.cpp`
3. `cpp/src/robotick/framework/data/RemoteEngineConnection.cpp`
4. `cpp/src/robotick/framework/data/InProgressMessage.cpp`
5. `cpp/tests/framework/data/RemoteEngineConnection.test.cpp`

That path goes from orchestration to transport to framing to executable proof.
