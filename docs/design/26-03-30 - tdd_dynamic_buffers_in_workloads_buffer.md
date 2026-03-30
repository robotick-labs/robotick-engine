# Dynamic Buffers In `WorkloadsBuffer` (TDD)

**Author:** Paul Connor  
**Date:** 30th Mar 2026

---

## Summary

Introduce a startup-sized generic buffer / vector type whose backing storage lives in the `Dynamic Struct Storage Region` of `WorkloadsBuffer`.

The immediate driver is image output payloads, but the abstraction should be generic from the start. The aim is to preserve Robotick's deterministic startup allocation model while removing the need to choose worst-case capacities purely at compile time for payloads whose true size is only known after config and preload.

---

## Problem Today

- Some workload-facing payloads have capacities that are naturally startup-bound rather than truly compile-time-fixed.
- When these are represented inline as `FixedVector`, workload instance size depends on worst-case capacity chosen at compile time.
- In several cases, config can influence the real storage requirement, but the current container model cannot size itself from config during startup.
- `Blackboard` already proves the engine can reserve a `Dynamic Struct Storage Region` inside `WorkloadsBuffer` after preload, but `Engine.cpp` still treats that path as Blackboard-specific, despite significant inroads already made with the more generic dynamic-struct reflection model that Blackboards now use.

---

## Proposed Direction

Introduce a new generic `DynamicStructStorageVector<T>` container that:

- Is registered as a `ROBOTICK_REGISTER_DYNAMIC_STRUCT(...)` type.
- Resolves its runtime-visible fields via `DynamicStructDescriptor`.
- Stores its payload in the `Dynamic Struct Storage Region` of `WorkloadsBuffer`, not via `operator new[]`.
- Uses offset-based binding, not raw pointers, so telemetry snapshots and mirrored buffers remain valid.
- Is sized once during startup from config / preload-established state.
- Does not resize at runtime after binding.

Specialised workload-facing dynamic buffer types should then be provided as aliases / specialisations of this generic container so they clearly sit in the same family as:

- `FixedVector`
- `HeapVector`
- `DynamicStructStorageVector`

---

## Engine Generalisation

The existing `DynamicStructDescriptor` abstraction currently covers schema resolution only:

- Given an instance, return its `StructDescriptor`.

To support startup-sized backing storage, extend it with one optional storage callback that can:

- Dry-run sizing / alignment during startup.
- Bind the instance to its storage during the real bind pass.
- Advance a shared cursor within the `Dynamic Struct Storage Region`.

This lets `Engine.cpp` stop special-casing `Blackboard` by name and instead operate on dynamic structs with storage requirements.

### Important constraint

Not every dynamic struct should be assumed to require storage in the `Dynamic Struct Storage Region`.

The abstraction should distinguish between:

- Dynamic struct for reflection only.
- Dynamic struct with startup-managed tail storage.

---

## Minimal API Shape To Explore

Keep `resolve_fn`, and add one optional storage callback to `DynamicStructDescriptor`.

Potential shape:

```cpp
struct DynamicStructDescriptor
{
	const StructDescriptor* (*resolve_fn)(const void* instance);

	bool (*storage_fn)(
		void* instance,
		const WorkloadsBuffer& workloads_buffer,
		size_t& cursor_in_workloads_buffer,
		bool bind_offsets);
};
```

Intended semantics:

- `bind_offsets == false`
  Dry sizing pass only. Advance the `Dynamic Struct Storage Region` cursor, do not write offsets.
- `bind_offsets == true`
  Real bind pass. Advance the `Dynamic Struct Storage Region` cursor and write offsets / internal state.

The key point is to unify sizing and binding in one callback so the logic cannot drift.

---

## Generic Container Direction

Preferred generic name:

- `DynamicStructStorageVector<T>`

Why this name:

- It is explicit about the storage policy.
- It reads as a sibling of `FixedVector` and `HeapVector`.
- It distinguishes this container from ordinary heap-backed startup-sized vectors.
- It keeps the generic engine concept aligned with the formal `Dynamic Struct Storage Region` terminology.

This generic container should be the abstraction introduced in engine / framework code. Domain-facing names can then be added as aliases or thin wrappers on top for ergonomics.

### Reflected shape to explore

`DynamicStructStorageVector<T>` should likely expose a reflected shape similar to:

- `data_buffer`
- `count`
- `capacity`

Notes:

- `data_buffer` should be reflected in a way Studio can continue to interpret as binary bytes.
- `count` is the active element count.
- `capacity` is the startup-bound maximum size for this instance.
- Internal addressing should be offset-based relative to the container header or owning buffer region within `WorkloadsBuffer`.

Avoid raw pointers in the reflected or persisted state.

---

## Expected Engine Refactor

Refactor `Engine.cpp` so that the current Blackboard path becomes a generic dynamic-struct storage pass:

1. Construct workloads as today.
2. Apply initial config as today.
3. Run preload as today.
4. Scan workload config / inputs / outputs for dynamic structs with storage callbacks.
5. Run a dry sizing pass to total required `Dynamic Struct Storage Region` usage.
6. Validate against reserved capacity.
7. Run a bind pass to assign offsets / storage.
8. Continue with the post-bind config pass and normal startup flow.

This should replace Blackboard-specific naming and logic with dynamic-struct storage terminology where appropriate.

---

## Survey Findings

Survey scope:

- `robotick-core-workloads/cpp/src/robotick/workloads`
- `knitware-shared/cpp/src/workloads`
- `robots/*/cpp/src/workloads`

The survey looked across workload `Config`, `Inputs`, and `Outputs`, plus the immediate system types they embed, to find storage that is both:

- materially sized by capacity rather than by a fixed signal-processing frame shape
- plausibly influenced by config or workload role at startup

### Strong first-wave adopters

Image output buffers remain the strongest first adopter:

- `ImageRefToJpegWorkload::outputs.jpeg_data`
- `MuJoCoCameraWorkload::outputs.png_data`
- `CanvasWorkload::outputs.face_png_data`
- `HeartbeatDisplayWorkload::outputs.display_png`
- `CochlearVisualizerWorkload::outputs.visualization_png`

These are the clearest match because output size is workload/config dependent, but the current field storage is worst-case inline.

### Likely later adopters

- `Transcript`
  Used by `SpeechToTextWorkload::outputs` and embedded in `ProsodyFusionWorkload::inputs`. It currently contains `TranscribedWords = FixedVector<TranscribedWord, 64>` plus `FixedString512 text`. It becomes interesting if transcript size / word count should become startup-bound rather than hard-fixed.

- `ProsodicSegmentBuffer`
  Used by `ProsodyFusionWorkload::outputs`. It is currently `FixedVector<ProsodicSegment, 32>`, with each segment carrying multiple fixed 128-sample curves plus transcript words. This is especially interesting because `ProsodyFusionConfig::simplified_sample_count` already expresses a runtime density target.

- `MemoryDebugRecentList`
  Used inside `MemoryCognitionOutputs::debug_info`. It is a weaker candidate than image/transcript/prosody, but notable because `MemoryCognitionConfig::debug_recent_memory_limit` already expresses a desired runtime limit.

### Things that should probably stay fixed

- `AudioFrame`
- `CochlearFrame`, harmonic buffers, FFT working shapes, and similar signal-processing vectors
- Small `FixedString` config and status fields
- `Blackboard` itself, which is already dynamic and should be generalised rather than replaced

These represent either true fixed-shape DSP geometry or small scalar/string fields, not startup-sized payload buffers.

---

## Image Use In First Phase

For image-related types, the intended family should feel coherent:

- Fixed inline image buffer type
- `DynamicStructStorageVector<uint8_t>`-backed image buffer type
- `ImageRef`

This gives image handling three related but distinct modes:

- Fixed inline image bytes
- Dynamic-struct-storage-backed image bytes
- `ImageRef` for externally stored image data

The image use-case should likely be the first implementation target, even though the underlying abstraction is generic.

---

## Telemetry / Studio Considerations

- Studio already understands `{ data_buffer, count }`-style binary objects in several places.
- Telemetry naming currently hardcodes `Blackboard_...` for dynamic struct type emission; this should be generalised so multiple dynamic struct families are represented correctly.
- Any new dynamic image buffer should preserve a stable and obvious reflected shape so existing image viewers need little or no change.

---

## Non-Goals For First Pass

- General runtime reallocation
- Fragmenting allocations across the lifetime of the engine
- Replacing every `FixedVector`
- Changing the engine to depend on ordinary heap allocation for workload output payloads
- Forcing fixed-frame DSP types to become dynamic where that would weaken determinism or algorithm clarity

---

## Naming To Standardise

Use `Dynamic Struct Storage Region` as the formal term for the post-instance area of `WorkloadsBuffer` reserved for startup-bound dynamic struct backing storage.

Likely code / API names:

- `DEFAULT_MAX_DYNAMIC_STRUCT_STORAGE_BYTES`
- `dynamic_struct_storage_start_offset`
- `dynamic_struct_storage_cursor`
- `compute_dynamic_struct_storage_requirements()`
- `bind_dynamic_struct_storage_for_instances()`

Low-level comments can still describe it as the tail region of `WorkloadsBuffer`, but the formal engine concept should be `Dynamic Struct Storage Region`.

---

## Documentation Follow-Through

After implementation starts, these docs should be updated to reflect the new abstraction:

- `docs/module-map.md`
- `docs/ownership.md`
- `docs/design/25-11-28 - tdd_reflective_telemetry.md`
- core-workloads docs covering image buffer types and any workload I/O that moves from fixed-capacity inline buffers to startup-sized dynamic buffers
