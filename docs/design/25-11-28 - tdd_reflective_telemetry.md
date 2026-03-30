# Reflective Telemetry (TDD v0.1)

**Author:** Paul Connor  
**Date:** 11th Nov 2025

---

## Summary

**Reflective Telemetry** provides a compact, binary representation of live engine state in Robotick, accompanied by a runtime-generated layout registry that describes the layout, types, and semantic meaning of each field within every workload & child data-structure. This approach allows structured, efficient, and introspectable telemetry over local or remote links, without relying on fragile JSON marshalling or hardcoded formats.

---

## Goals

- Allow clients (e.g. browser-based UIs) to interrogate, interpret and display live data from any workload without custom code.
- Support arrays, scalars, and blobs (images, audio, etc.) in a unified way.
- Enable high-performance streaming of telemetry (e.g. audio at 44.1 kHz, PNG frames at 60 Hz).
- Preserve minimal memory usage and zero-heap allocation on embedded targets.
- Be introspectable and extensible — allow both generic tools and workload-specific overlays.

---

## Key Concepts

### 1. **Workloads Buffer**

- Already core part of the Robotick Engine - this is a flat binary buffer containing all workload `config`, `inputs`, and `outputs`.
- Fixed offsets per field, no runtime packing — layout is statically known per-session.
- The buffer may also contain a `Dynamic Struct Storage Region` used for startup-sized dynamic structs such as `Blackboard` payloads or bounded compressed image buffers.
- Dynamic structs still report field offsets relative to their parent struct; reflected binary fields such as `data_buffer` may therefore point to out-of-line storage within the same used `WorkloadsBuffer`.

### 2. **Layout Registry**

- Accompanies the buffer, served once per engine session.
- Declares:
  - Top-level workload names and their offsets
  - Field types and structure layout (once per type)
  - Relative offsets per field
  - Optional semantic tags (e.g. `"content-type": "image/png"`)
- Only types actually used are emitted — types are recorded once and recursively referenced by name.
- Field offsets are always **relative to their parent struct**, never absolute.

### 3. **Session Awareness**

- Both Engine and Web UI sessions may restart independently.
- Clients must re-fetch the layout if `engine_session_id` changes. This is reported in both the binary-buffer transmission headers and layout’s own metadata.

### 4. **Semantic Tags**

- `meta` blocks annotate fields with meaning (e.g. MIME type).
- Enables automatic rendering or decoding of fields like:
  - PNG images
  - Audio buffers
  - Vector plots

---

## Example Layout Info

```json
{
  "engine_session_id": "abc123",
  "buffer_size": 8192,
  "workloads": [
    {
      "name": "face",
      "offset": 1024,
      "config_type": "FaceConfig",
      "inputs_type": "FaceInputs",
      "outputs_type": "FaceOutputs"
    }
  ],
  "types": {
    "FaceInputs": {
      "fields": [
        { "name": "look_offset", "type": "Vec2f", "offset": 0 },
        { "name": "blink_request", "type": "bool", "offset": 8 }
      ]
    },
    "FaceOutputs": {
      "fields": [
        {
          "name": "face_png_data",
          "type": "FixedVectorU8_4096",
          "offset": 0,
          "meta": { "content-type": "image/png" }
        }
      ]
    },
    "Vec2f": {
      "fields": [
        { "name": "x", "type": "float32", "offset": 0 },
        { "name": "y", "type": "float32", "offset": 4 }
      ]
    },
    "FixedVectorU8_4096": {
      "primitive": "uint8",
      "length": 4096
    },
    "float32": { "primitive": true },
    "bool": { "primitive": true },
    "uint8": { "primitive": true }
  }
}
```

---

## Initial Use Case: `FaceDisplayWorkload`

### Fields:

- `look_offset.x`, `look_offset.y` (float32[2])
- `blink_request` (bool)
- `face_png_data` (FixedVector16k, with active size known via introspection)

### UI Testbed:

- `/reflective/raw` returns full buffer (`application/octet-stream`)
- `/reflective/layout` returns JSON layout registry
- `reflective_face_debug.html`:
  - Shows scalar fields
  - Renders PNG from buffer as live thumbnail

---

## Roadmap

- [ ] Define layout registry and field type system
- [ ] Implement `/reflective/raw` and `/reflective/layout` endpoints in Hub
- [ ] Add workload registration support for `meta` tags
- [ ] Build web client that can parse buffer using layout
- [ ] Verify live image display for `FaceDisplayWorkload`
- [ ] Extend support to audio fields (`audio/pcm`, `sample-rate` tag, etc.)
- [ ] Generalise tooling to support all workloads

---

## Future Directions

- Binary layout support (e.g. for embedded JS parsers)
- Streamed updates via WebSocket
- Timeline-based recording and playback
- Tooling to diff field values over time or between robots

---

## Design Note: Large or Dynamic Data

Reflective Telemetry is intentionally designed for structured, typed, per-tick data — typically config, inputs, and outputs of real-time workloads. It is still a poor fit for very large or weakly bounded payloads such as:

- SLAM maps or occupancy grids
- Long-lived knowledge graphs or datasets
- Raw high-resolution camera frames
- Arbitrary dynamically allocated state

These types of data are not well-suited to the fixed-layout, per-tick semantics of the Workloads Buffer or the dataflow model of the Robotick Engine. Instead:

- Use **buffer/resource IDs** in Reflective Telemetry to refer to large assets.
- Exchange those IDs in fields (e.g. `slam_map_id: uint32`), and expose the actual data via a separate `/resource/` API.
- This enables **zero-copy**, **lazy access**, and maintains the strict deterministic performance guarantees of the engine.

Bounded compressed image payloads are now a valid exception to that general rule. Startup-sized dynamic structs can expose JPEG / PNG outputs directly in `WorkloadsBuffer` when:

- the payload is genuinely useful as a per-tick reflective output
- the maximum size is configured and bound at startup
- the cost is acceptable for the target platform and model

Even then, `ImageRef`-style indirection or a separate resource API remains the better fit for very large, shared, archival, or infrequently inspected image data.

This approach keeps Reflective Telemetry compact, reliable, and introspectable — while enabling extensibility to support complex or heavy data elsewhere.
