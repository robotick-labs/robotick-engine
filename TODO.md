## Title
Add enum type registration and expose enum mappings via Telemetry layout

## Description
- Allow enums to be registered as first-class types (TypeCategory::Enum) with string/value tables so they can be discovered like primitives/structs/workloads.
- Emit enum metadata in telemetry `/api/telemetry/workloads_buffer/layout` so clients can render dropdowns/validation using `enum_values` and flag hints.
- Keep registration model consistent with existing macros (AutoRegisterType) and preserve registry immutability/sealing guarantees.

## TDD / Tasks
- [ ] Add EnumValue/EnumDescriptor and TypeCategory::Enum + descriptor accessor in `TypeDescriptor`; wire `TypeCategoryDesc` union.
- [ ] Add ROBOTICK_REGISTER_ENUM_* macros that build the value table, assert enum type, and auto-register a TypeDescriptor with mime `text/plain`.
- [ ] Extend `TypeDescriptor::to_string/from_string` to resolve enums by name first, numeric fallback; add unit coverage for round-tripping known enum values and unknown inputs.
- [ ] Update telemetry `emit_type_info` to include `enum_values` (and `is_flags`/underlying size if present) in the type JSON; add an integration-ish test that inspects layout JSON for a sample enum field.
- [ ] Register at least one sample enum (e.g. in PrimitiveTypes or a test fixture) to exercise the path end-to-end.

## Acceptance Criteria
- Registry can register enums without breaking existing types; duplicate IDs/names still guarded.
- Telemetry layout JSON includes enum types with `enum_values` array; clients can map names to values without extra calls.
- Enum stringification/parsing works for registered values; numeric fallback handled safely; tests added and passing.
