# Feature Spec: DiaAttribute Band-Based Initialization

**Parent:** @docs/specs/applications/dia/systems/diaattribute/diaattribute.md  
**Status:** `Done`

## Summary

Extend AttributeSchema to define named band ranges (e.g., NONE/LOW/BASELINE/MEDIUM/HIGH/APEX, or any custom vocabulary) in JSON, and AttributeSet to initialize from band names instead of direct float values. Band-to-float resolution happens exactly once (at creation or load), caching the resolved value as the attribute's base value. This enables designer-friendly data authoring, versioned band definitions, and schema evolution without touching every data file.

## Problem

Games like CoW author entity stats as human-readable bands ("this dragon has HIGH strength, MEDIUM agility") rather than raw floats. Designers tune gameplay by adjusting band ranges in one central schema file, not by editing hundreds of individual entity data files. Current DiaAttribute only supports direct float `default_value` initialization — no band vocabulary, no versioning, no designer-ergonomic authoring layer.

**Use case from CoW F-2 Core Runtime:**
- Dragon stats (power, agility, fertility, etc.) authored as bands per the GDD Phase 12 design
- Band ranges defined in a central `band_schema.json` with a `band_schema_version`
- Each dragon record carries its own `band_schema_version` — when ranges rebalance, old dragons resolve against their original schema, new dragons use the updated one
- Resolution runs once per dragon: on `restore_state()` (load) or `on_hatch()` (creation), never mid-session

Without this feature, CoW must either:
1. Embed all band-to-float resolution logic in CoW-specific code (not reusable)
2. Author floats directly (loses designer vocabulary and versioning)

## Goals

1. **Schema-defined bands** — AttributeSchema JSON can define named band ranges per attribute (e.g., `"NONE": [0.0, 2.0]`, `"LOW": [2.0, 5.0]`, custom names supported)
2. **Band-based initialization** — `AttributeSet::InitializeFromBands(bandMap)` accepts a map of attribute names → band names, resolves each to a float within its schema-defined range, and stores it as the base value
3. **Schema versioning** — AttributeSchema carries an optional `band_schema_version` integer; data files reference a version; different versions can coexist (old data resolves against old ranges)
4. **One-time resolution** — Band resolution happens once per AttributeSet (at creation or load), not on every `GetValue()` call; the resolved float is the base value, modifiers work normally from there
5. **Arbitrary band names** — Band names are user-defined StringCRCs, not hardcoded enums (NONE/LOW/BASELINE/MEDIUM/HIGH/APEX is CoW's vocabulary, not engine-enforced)
6. **Range validation** — Schema validation ensures every band's `[min, max]` is coherent and within the attribute's overall `[minimum_value, maximum_value]`
7. **DIA_LOG_WARNING on missing bands** — If a data file references a band name not in the schema, log a warning and fall back to the attribute's `default_value`

## Non-Goals

1. **Band-based modifiers** — Modifiers remain float-valued (`AttributeModifier::value` is still a float); bands are an authoring convenience for base values only, not a modifier type
2. **Runtime re-resolution** — Once a band resolves to a float, it never re-resolves unless `InitializeFromBands()` is explicitly called again (no live schema swapping, no automatic re-resolution on schema file changes)
3. **UI for band authoring** — Tooling (e.g., a DragonDesign editor) can read/write band-authored data, but that's Phase 12 scope, not DiaAttribute Core
4. **Automatic migration** — Old data files that carry obsolete band names must explicitly handle the version mismatch (e.g., CoW's load path might detect version skew and prompt the player or reauthor); DiaAttribute just logs and falls back, it doesn't auto-migrate
5. **Band-to-band arithmetic** — No concept of "add MEDIUM + LOW = HIGH"; resolution is always band → float, then arithmetic happens in float space via modifiers

## Public API Changes

### AttributeSchema JSON Extension

```json
{
  "schema_name": "dragon",
  "band_schema_version": 1,
  "attributes": [
    {
      "attribute_name": "strength",
      "minimum_value": 0.0,
      "maximum_value": 200.0,
      "default_value": 50.0,
      "bands": {
        "NONE":     [0.0, 10.0],
        "LOW":      [10.0, 40.0],
        "BASELINE": [40.0, 70.0],
        "MEDIUM":   [70.0, 110.0],
        "HIGH":     [110.0, 160.0],
        "APEX":     [160.0, 200.0]
      }
    },
    {
      "attribute_name": "agility",
      "minimum_value": 0.0,
      "maximum_value": 100.0,
      "default_value": 30.0,
      "bands": {
        "LOW":    [0.0, 30.0],
        "MEDIUM": [30.0, 60.0],
        "HIGH":   [60.0, 100.0]
      }
    }
  ]
}
```

**Notes:**
- `band_schema_version` is optional (defaults to 0 if omitted, meaning unversioned)
- `bands` is optional per attribute (attributes without bands use `default_value` directly)
- Band names are arbitrary strings (converted to StringCRC internally)
- Band range `[min, max]` must satisfy `attribute.minimum_value <= min < max <= attribute.maximum_value`

### AttributeSet C++ API

```cpp
namespace Dia::Attribute {

    struct BandValue {
        StringCRC   attribute_name;
        StringCRC   band_name;
    };

    class AttributeSet {
    public:
        // Existing:
        static AttributeSet CreateFromSchema(const AttributeSchema& schema);
        
        // NEW: Initialize from band map
        // Resolves each band_name to a float using the schema's band ranges, stores as base value.
        // For each BandValue:
        // - If the attribute has no bands defined, logs a warning and uses default_value
        // - If the band_name is not in the attribute's bands, logs a warning and uses default_value
        // - Otherwise, resolves to midpoint of [min, max] (exact resolution policy is implementation detail)
        // Attributes not listed in band_values initialize to default_value.
        void InitializeFromBands(const Dia::Core::Containers::DynamicArrayC<BandValue>& band_values);
        
        // Existing API unchanged:
        float       GetValue(StringCRC attribute_name) const;
        float       GetBaseValue(StringCRC attribute_name) const;
        void        SetBaseValue(StringCRC attribute_name, float value);
        ModifierHandle AddModifier(const AttributeModifier& modifier);
        void           RemoveModifier(ModifierHandle handle);
    };

    class AttributeSchema {
    public:
        // Existing API unchanged, plus:
        unsigned int GetBandSchemaVersion() const;  // Returns 0 if unversioned
        bool         HasBands(StringCRC attribute_name) const;
        // Internal: ResolveBand(StringCRC attribute_name, StringCRC band_name) -> float
    };
}
```

## Resolution Policy (Implementation Detail)

**Midpoint strategy** (simplest):
- Band `[min, max]` resolves to `(min + max) / 2.0`
- Deterministic, version-stable, designer-tunable (designers adjust ranges to shift resolved values)

**Alternatives considered and deferred:**
- Random within range (requires RNG seeding, complicates determinism)
- Percentile within range (requires extra `"percentile": 0.75` field per band in data)

**Decision:** Start with midpoint. If a game needs finer control, add an optional `"resolve_percentile"` field to BandValue in a future iteration (not Feature 7 scope).

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | Use StringCRC for entity/component IDs | Band names are StringCRCs; `band_schema_version` is an integer |
| PD-004, AD-002 | No STL in public APIs | `DynamicArrayC<BandValue>` for band map input; no `std::map`/`std::vector` |
| PD-007 | C++20 required | `[[nodiscard]]` on `InitializeFromBands()` return (if it returns a result type); `constexpr` for band name constants |
| AD-001 | Module YAML frontmatter | Feature increments the existing `dia.attribute.architecture.module.md`; no new module |
| AD-003 | `Dia::<Module>::` namespace | All new types in `Dia::Attribute::` |

## Open Design Questions

1. **Resolution percentile flexibility** — Midpoint is simple and sufficient for CoW's needs. Should we add an optional `resolve_percentile` field to `BandValue` from the start (e.g., `0.5` = midpoint, `0.75` = high end), or wait until a game actually needs it?
   - **Recommendation:** Defer. YAGNI until a real use case appears.

2. **Schema file vs. per-AttributeSet versioning** — CoW's design has each dragon carry its own `band_schema_version`, allowing old and new dragons to coexist under different schemas. Should DiaAttribute support loading multiple AttributeSchema files (one per version) simultaneously, or is it the caller's job to cache schemas by version?
   - **Recommendation:** Caller's job. DiaAttribute provides `GetBandSchemaVersion()` on the schema; the caller (e.g., CoW's dragon loader) maintains a `std::unordered_map<int, AttributeSchema>` version cache if needed. DiaAttribute remains stateless.

3. **Validation strictness on band overlap** — Should schema validation reject overlapping band ranges (e.g., `"LOW": [0, 50]`, `"MEDIUM": [40, 80]`), or allow it and rely on caller to choose one band name?
   - **Recommendation:** Allow overlap. Validation only checks that each band's `[min, max]` is within the attribute's `[minimum_value, maximum_value]` and that `min < max`. Designer can author overlapping bands if that's their intent (e.g., "transition zones").

## Acceptance Criteria

1. **AC-1:** Schema with bands field loads correctly; band names and ranges accessible via API
2. **AC-2:** `InitializeFromBands()` resolves a single band to the midpoint of its range, stores as base value
3. **AC-3:** `InitializeFromBands()` resolves multiple attributes from bands in one call
4. **AC-4:** Missing band name (not in schema) logs `DIA_LOG_WARNING`, falls back to `default_value`
5. **AC-5:** Attribute with no bands field logs `DIA_LOG_WARNING` on band init, falls back to `default_value`
6. **AC-6:** Schema validation rejects band range where `min > max`
7. **AC-7:** Schema validation rejects band range outside attribute's `[minimum_value, maximum_value]`
8. **AC-8:** `band_schema_version` field in schema is accessible via `GetBandSchemaVersion()`
9. **AC-9:** Modifiers work normally on band-initialized attributes (base value + modifier stack)
10. **AC-10:** Empty `InitializeFromBands()` call leaves all attributes at `default_value`
11. **AC-11:** Band-initialized attribute round-trips through `DiaSaveGame` serialization (integration test in CoW F-2)
12. **AC-12:** Band resolution happens exactly once per AttributeSet lifecycle (integration test in CoW F-2)

## Test Utilities

Add the following helpers to `DiaAttribute/Testing/AttributeTestHelpers.h` (shipped with the library, consumer opt-in via include):

```cpp
namespace Dia::Attribute::Testing {

    // Create an AttributeSchema with band definitions from JSON string
    AttributeSchema MakeSchemaWithBands(const char* json_string);

    // Create a simple schema with one attribute and predefined bands
    // Example: MakeTestSchemaWithBands("power", 0.0, 100.0, {{"LOW", 0.0, 30.0}, {"MEDIUM", 30.0, 70.0}, {"HIGH", 70.0, 100.0}})
    AttributeSchema MakeTestSchemaWithBands(
        const char* attribute_name,
        float min_value,
        float max_value,
        const std::initializer_list<std::tuple<const char*, float, float>>& bands
    );

    // Assert that a band resolves to the expected value (midpoint)
    void AssertBandResolvesToMidpoint(
        const AttributeSet& set,
        StringCRC attribute_name,
        float expected_midpoint,
        float tolerance = 0.01f
    );

    // Assert that InitializeFromBands logs a warning for a missing band
    void AssertMissingBandWarning(
        const AttributeSet& set,
        const DynamicArrayC<BandValue>& band_values
    );
}
```

These helpers reduce boilerplate in unit tests, allowing concise band-initialization test cases while matching the existing pattern in DiaAttribute test utilities.

## Test Strategy

### Unit Tests (GoogleTests/DiaAttribute/TestDiaAttributeBandInitialization.cpp)

New test file covering Feature 7 acceptance criteria:

1. **AC-1: Band resolution midpoint** — Schema defines `"MEDIUM": [10.0, 20.0]`, `InitializeFromBands({"strength", "MEDIUM"})` → `GetBaseValue("strength") == 15.0`
2. **AC-2: Band not in schema** — Schema defines bands for "strength" but not "agility"; `InitializeFromBands({"agility", "MEDIUM"})` → logs warning, uses `default_value`
3. **AC-3: Attribute has no bands** — Schema omits `bands` field for "health"; `InitializeFromBands({"health", "LOW"})` → logs warning, uses `default_value`
4. **AC-4: Schema validation (inverted range)** — Band range `[50, 40]` (min > max) fails validation with `DIA_LOG_WARNING`
5. **AC-5: Schema validation (out of bounds)** — Band range `[150, 250]` outside attribute's `[0, 200]` fails validation
6. **AC-6: Versioning** — Two schemas loaded (v1 and v2), each resolves "HIGH" differently; AttributeSet initialized from v1 produces different value than v2
7. **AC-7: Modifiers after band init** — `InitializeFromBands({"strength", "MEDIUM"})` → `AddModifier(Add, 10.0)` → `GetValue() == 25.0` (15 base + 10 modifier)
8. **AC-8: Multiple attributes** — Initialize three attributes from bands in one call, verify all resolve correctly
9. **AC-9: Attribute not in band map** — Call `InitializeFromBands()` with only "strength"; "agility" (also in schema) uses `default_value`
10. **AC-10: Empty band map** — Call `InitializeFromBands()` with empty array; all attributes use `default_value`

### Integration Tests (Deferred to CoW F-2 — see loose ends)

~~11. **AC-11: Dragon load round-trip** — Save a dragon with band-initialized stats, reload, confirm values unchanged~~
~~12. **AC-12: Hatch-time resolution** — Hatch a new dragon mid-session, confirm band resolution runs once and produces stable floats~~

## Dependencies

- **DiaAttribute Core** (Feature 1) — band init extends AttributeSet, requires schema loading infrastructure
- **DiaCore** — StringCRC, DynamicArrayC, HashTable, DIA_LOG_WARNING
- **No dependency on DiaSaveGame** — band resolution is orthogonal to save/load; Feature 6 (save-serialization.md) handles persistence

## Estimated Effort

- **Schema JSON parsing** (bands field, version field): ~2 hours
- **AttributeSchema::ResolveBand()** internal API: ~1 hour
- **AttributeSet::InitializeFromBands()** public API: ~2 hours
- **Validation (band range coherence)**: ~1 hour
- **Test utilities** (AttributeTestHelpers.h band helpers): ~1.5 hours
- **Unit tests** (TestDiaAttributeBandInitialization.cpp, 10 ACs): ~3 hours
- **Module doc update**: ~0.5 hours

**Total:** ~11 hours (1.4 days)

## Status

**Status:** `Approved`  
**Plan:** [band-based-initialization.plan.md](band-based-initialization.plan.md)

## Notes

**Integration tests deferred:** AC-11 (save/load round-trip) and AC-12 (one-time resolution proof) moved to loose ends — these require CoW F-2 testbed infrastructure (dragon entities, hatch/load lifecycle). The 10 unit test ACs provide sufficient coverage for DiaAttribute Core approval.
