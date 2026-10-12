# Implementation Plan: DiaAttribute Band-Based Initialization

**Spec:** @docs/specs/applications/dia/systems/diaattribute/band-based-initialization.md  
**Status:** Not Started

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Extend AttributeDefinition and AttributeSchema for bands | Unit test band schema loading (AC-1) | Not Started | sonnet | Add `bands` map (StringCRC → [min, max]) to AttributeDefinition; add `band_schema_version` field to AttributeSchema; extend JSON parsing in `AttributeSchema::LoadFromJson()` |
| 2 | Implement band range validation | Unit test validation failures (AC-6, AC-7) | Not Started | sonnet | Schema validation: reject `min > max`, reject range outside `[minimum_value, maximum_value]`; emit `DIA_LOG_WARNING` on failure |
| 3 | Implement AttributeSchema::ResolveBand() | Unit test midpoint resolution (AC-1) | Not Started | sonnet | Internal API: `float ResolveBand(StringCRC attribute_name, StringCRC band_name) const` — returns midpoint of `[min, max]`, logs warning and returns `default_value` if band not found |
| 4 | Implement AttributeSet::InitializeFromBands() | Unit tests AC-2, AC-3, AC-4, AC-5, AC-9, AC-10 | Not Started | sonnet | Public API accepting `DynamicArrayC<BandValue>` — resolve each band via schema, store as base value; handle missing bands, attributes without bands, empty input |
| 5 | Add GetBandSchemaVersion() API | Unit test versioning (AC-8) | Not Started | haiku | Add `unsigned int AttributeSchema::GetBandSchemaVersion() const` — return version field (default 0 if unset) |
| 6 | Implement test utilities | Verify helpers compile and link | Not Started | sonnet | Add `MakeSchemaWithBands()`, `MakeTestSchemaWithBands()`, `AssertBandResolvesToMidpoint()`, `AssertMissingBandWarning()` to `DiaAttribute/Testing/AttributeTestHelpers.h` |
| 7 | Write unit tests | All 10 unit test ACs pass | Not Started | sonnet | New file `GoogleTests/DiaAttribute/TestDiaAttributeBandInitialization.cpp` — 10 test cases covering AC-1 through AC-10 |
| 8 | Verify modifiers work with band-initialized attributes | Unit test AC-9 (base from band + modifier stack) | Not Started | haiku | Integration check: band init → add modifier → verify `GetValue()` resolves correctly |
| 9 | Update module documentation | Docs reflect Feature 7 | Not Started | haiku | Add Feature 7 summary to `dia.attribute.architecture.module.md` — band initialization API, versioning support |
| 10 | Run full DiaAttribute test suite | All existing + new tests pass | Not Started | haiku | `dia run googletest --filter="*Attribute*"` — confirm no regressions |

## Notes

- **Integration tests (AC-11, AC-12) deferred** — moved to loose ends; require CoW F-2 testbed infrastructure
- **Model selection:** Sonnet for implementation tasks, Haiku for mechanical tasks (API stubs, doc updates, test runs)
- **Commit strategy:** One commit per task cluster (1-2, 3-4, 5-6, 7-8, 9-10) to keep incremental progress reviewable
