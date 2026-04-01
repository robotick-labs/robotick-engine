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

To support startup-sized backing storage, extend it with an explicit planning/binding storage contract that can:

- Describe storage size and alignment requirements during startup planning.
- Bind the instance to its assigned storage during the real bind pass.

This lets `Engine.cpp` stop special-casing `Blackboard` by name and instead operate on dynamic structs with storage requirements.

### Important constraint

Not every dynamic struct should be assumed to require storage in the `Dynamic Struct Storage Region`.

The abstraction should distinguish between:

- Dynamic struct for reflection only.
- Dynamic struct with startup-managed tail storage.

---

## Minimal API Shape To Explore

Keep `resolve_fn`, and add an optional two-phase storage contract to `DynamicStructDescriptor`.

Potential shape:

```cpp
struct DynamicStructStoragePlan
{
	size_t size_bytes = 0;
	size_t alignment = 1;
};

struct DynamicStructDescriptor
{
	const StructDescriptor* (*resolve_fn)(const void* instance);

	bool (*plan_storage_fn)(
		const void* instance,
		DynamicStructStoragePlan& out_plan);

	bool (*bind_storage_fn)(
		void* instance,
		const WorkloadsBuffer& workloads_buffer,
		size_t storage_offset_in_workloads_buffer,
		size_t storage_size_bytes);
};
```

Intended semantics:

- `plan_storage_fn`
  Describes the dynamic struct's required storage region, without writing any bound offsets or state into the instance.
- `bind_storage_fn`
  Receives the region assigned by the engine and writes the instance's bound offsets / internal state.

This keeps sizing and binding conceptually separate while still allowing each dynamic struct implementation to share internal layout helpers.

### Impact on `Blackboard`

`Blackboard` should be migrated onto this contract immediately.

That means:

- `Blackboard::compute_total_datablock_size()` / related layout planning logic becomes the basis of `plan_storage_fn`.
- `Blackboard::bind(...)` becomes the basis of `bind_storage_fn`.
- Any duplicated cursor/layout math inside `Blackboard` should be refactored behind a shared helper so the planning and binding paths remain consistent without being conflated at the descriptor API level.

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
- Like Blackboard-backed dynamic fields, telemetry/tooling should be able to follow the reflected `data_buffer` offset to its bound target region and treat that target buffer as readily as if it were inline, provided it lies within used `WorkloadsBuffer`.

Avoid raw pointers in the reflected or persisted state.

---

## Expected Engine Refactor

Refactor `Engine.cpp` so that the current Blackboard path becomes a generic dynamic-struct storage pass:

1. Compute the exact inline workload + stats footprint.
2. Allocate a scratch `WorkloadsBuffer` for just that inline region.
3. Construct workloads, apply initial config, and run `pre_load()` in that scratch buffer.
4. Scan workload config / inputs / outputs for dynamic structs with storage callbacks.
5. Run a planning pass to collect storage size / alignment requirements for each dynamic struct that needs storage.
6. Destroy the scratch workload instances.
7. Allocate the final `WorkloadsBuffer` to the exact inline-workloads + `Dynamic Struct Storage Region` size required.
8. Reconstruct workloads, re-run initial config + `pre_load()`, validate the plan is unchanged, then bind dynamic-struct storage.
9. Continue with the post-bind config pass and normal startup flow.

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

As part of that first image-focused pass, the existing fixed image aliases should be renamed to make their inline storage mode explicit.

Preferred names:

- `ImageJpegInline128k`
- `ImageJpegInline256k`
- `ImagePngInline16k`
- `ImagePngInline64k`
- `ImagePngInline128k`
- `ImagePngInline256k`
- `ImageJpegDynamic`
- `ImagePngDynamic`

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

- `dynamic_struct_storage_start_offset`
- `dynamic_struct_storage_cursor`
- `compute_dynamic_struct_storage_requirements()`
- `bind_dynamic_struct_storage_for_instances()`

Low-level comments can still describe it as the tail region of `WorkloadsBuffer`, but the formal engine concept should be `Dynamic Struct Storage Region`.

For dynamic-struct storage callbacks, storage support should be treated as an all-or-nothing contract:

- both callbacks present => storage-backed dynamic struct
- both callbacks absent => reflection-only dynamic struct
- any mixed case => descriptor validation failure

---

## Documentation Follow-Through

After implementation starts, these docs should be updated to reflect the new abstraction:

- `docs/module-map.md`
- `docs/ownership.md`
- `docs/design/25-11-28 - tdd_reflective_telemetry.md`
- core-workloads docs covering image buffer types and any workload I/O that moves from fixed-capacity inline buffers to startup-sized dynamic buffers

---
