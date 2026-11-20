# Feature Spec: DiaAttribute Parameter Files

**Parent:** @docs/specs/applications/dia/systems/diaattribute/diaattribute.md (Feature 8)
**Status:** `Done`
**Research:** @docs/research/worldsim_data_driven/summary.md (§0: decisions D1, D2, D3)

## Summary

This feature makes DiaAttribute the one runtime home for authored, tunable values, and makes DiaValueSchema sidecar files (`*.schema.json`) the one place those values are defined. It does three things:

- **The sidecar is the whole definition of a value**: kind, range, default, bands and description, plus cross-field constraints. There are no separate base parameter files.
- **A new `ParameterFile`** loads a sidecar strictly, applies override packs leniently and atomically, sets single values, reloads, writes values back out (`ToJson`) and hashes them.
- **A new `ParamBinding<T>`** copies resolved values into a plain C++ struct once per step, so simulation hot loops read plain members.

The old attribute schema JSON format is removed. S-1's `TerrainRules` is the proof migration.

## Problem

CoW's world-sim systems each specced their own parameter machinery:
- S-1: an X-macro table plus `terrain_rules.json`
- A4 and A5: separate `ParamDescriptor` tables, each with its own `ApplyOverrides`, `SetParam`, `ToJson` and hash
- A1: unspecified

That's three different validation policies. Every default is also written twice, in the sidecar and in the base file, and the CLI errors if the two differ (`data.default-mismatch`). DiaAttribute already provides named, bounded values with observers, save, condition-gated modifiers and a visual debugger, but it reads its own schema format, which is a third place for ranges. The rule being applied is **one fact, one place, in data**, where the engine, `dia validate schema` and T-2 all read it.

## Goals

1. The sidecar entry is the single source of a parameter's kind, range, default, bands and description. Cross-field rules live in the same sidecar.
2. One load path, with a policy flag: strict for the sidecar and `Set`, lenient but atomic for packs (D1).
3. Base values are authored tuning (defaults plus packs plus `Set`); modifiers are live runtime effects.
4. Parameters are fixed key sets, including maps keyed by a registry. Hot loops read a bound struct, not hash lookups.
5. C++ and the CLI agree on every finding, enforced by shared conformance fixtures.

## Non-Goals

- **Records** (species, biome registry rows, succession rules, flow-graph nodes): authored lists go through DiaData `JsonDefinitionLoader` with sidecar validation, not through this feature.
- **The biome registry** itself (world-sim spec 2). This feature only needs a key list.
- **Choosing which packs apply, and their order**: that is the caller's job (F-3). The caller passes the packs in order.
- **Debug commands** (`<prefix>.params set|get|reload|list`): these belong to the world-sim kit's `ParamCommandHelper` (spec 3), built on this API.
- **File watching**: `FileWatcher` dev reload calls `Reload`; the wiring is the consumer's.
- **Migrating A4, A5 and A1**: those are spec amendments (world-sim spec 6). Only S-1 migrates here.
- **A compiled fallback for defaults**: there isn't one (see Design, "Failure").

## Design

### Sidecar additions (`dia.value_schema` v1, additive)

| Addition | Shape | Rules |
|---|---|---|
| `bands` on an entry | `"bands": {"LOW": [0.0, 0.3], "HIGH": [0.7, 1.0]}` | Each band must lie inside `[min, max]` with `lo <= hi`. Replaces the old format's `bands`. |
| `default` on a `map` entry | an object keyed by map key, e.g. `{"Biome.Grassland": 0.6, ...}` | Values are checked against `valueType`, `min` and `max`. Every key must resolve through `keyRef` (CLI) or the caller's key list (C++). For an enum-valued map each value is an enum id string, checked against the entry's `values` (2026-11-20). |
| Top-level `constraints` | `[{"a": "precipitation.snow_threshold", "op": "<=", "b": "snow.melt_threshold"}]` | `op` is one of `< <= > >= == !=`. `a` and `b` are parameter keys of a numeric kind; a map column is addressed as `<key>.<mapKey>`. Checked on defaults, after a pack merge, and on `Set`. |

The CLI adds `schema.bad-band`, `schema.bad-constraint` and `data.constraint-violated`. The conformance fixtures gain cases for each. T-2's `table-and-form-editors.md` is amended to show and check constraints and bands, using the same fixtures.

For parameter entries, the CLI's `data.default-mismatch` no longer applies to base files, because base parameter files no longer exist.

### Files on disk

```
Global/<System>/<sys>_parameters.schema.json     sidecar: definition + defaults (strict)
Global/<System>/Packs/<id>.<sys>.json            sparse override pack (lenient)
   { "format": "<for>", "version": 1, "<key>": value, ... }
```

The pack's `format` equals the sidecar's `for`. Pack keys are parameter keys (`group.key`), and a map is a nested object. (2026-11-20 user decision: flat keys. A pack addresses a parameter by its flat full key, the same key language as `Set` and map columns `<key>.<mapKey>`; a nested group object such as `{"grazing": {"rate": 2.5}}` is not a pack form. `ApplyPack` drops it with a warning and the CLI reports it as `data.nested-group-pack`. Only an object under a map parameter name is accepted.)

### Kinds and storage

- DiaAttribute stays float-valued.
- `int` must be a whole number. `bool` is 0 or 1; JSON `true`/`false` are accepted. `enum` is stored as the ordinal into the sidecar's `values`; JSON carries the enum id string.
- **Modifier legality by kind**: Add and Multiply only on `float` and `int` (an `int` result is rounded half away from zero, then clamped). Override on every kind, and its value must be valid for the kind. `AddModifier` fails for an illegal operation and logs a warning naming the attribute.

### Schema construction

```cpp
namespace Dia::Attribute {
    struct MapKeys { const Dia::Core::StringCRC* ids; const char* const* names; uint32_t count; };  // PD-004
    struct SchemaError { Dia::Core::Containers::String128 key; Dia::Core::Containers::String256 reason; };

    // Built from a parsed sidecar (ValueSchemaLoader::LoadFromText). Uses kind:parameter entries only.
    // mapKeys resolves each map entry's keyRef (by keyRef string); null for none.
    static bool AttributeSchema::FromValueSchema(const ValueSchema::LoadResult& sidecar,
                                                 const IMapKeySource* mapKeys,
                                                 AttributeSchema& out, SchemaError& err);
}
```

- Each `kind: parameter` entry becomes one `AttributeDefinition` (name = `StringCRC(key)`), keeping its kind, range, default and bands. Each map entry becomes one definition per key, named `<key>.<mapKey>`.
- Definitions follow sidecar file order, and map keys follow the key source's order. **This order is the schema order** used by `ToJson`, `Hash` and binding.
- `FromValueSchema` fails, naming the key, if:
  - a parameter has no default;
  - a default is out of range or has the wrong kind;
  - a map default is missing a key, or has an unknown one;
  - a band lies outside `[min, max]`;
  - a defaults constraint is violated;
  - a constraint names an unknown or non-numeric key.
- The old `AttributeSchema::LoadFromJson` and `LoadFromJsonValue` are **deleted**. The CoW dragon stub and the DiaAttribute tests move to sidecars. `InitializeFromBands` behaves as before, reading the sidecar's bands. The old `band_schema_version` becomes the optional sidecar field `bandSchemaVersion` (default 1), not `version` (see Band schema version resolution below). The plan's Task 0 confirms nothing else relies on the old field.

### `ParameterFile`

```cpp
namespace Dia::Attribute {
    enum class LoadPolicy : uint8_t { Strict, Lenient };
    enum class JsonShape  : uint8_t { Full, Diff };

    struct ParamError   { Dia::Core::Containers::String128 key, otherKey; Dia::Core::Containers::String256 reason; };
    struct ParamWarning { Dia::Core::Containers::String128 key; Dia::Core::Containers::String256 reason; };
    struct ParamWarnings { Dia::Core::Containers::DynamicArrayC<ParamWarning, 64> items; uint32_t dropped = 0; };

    class ParameterFile {
    public:
        bool Load(const char* sidecarText, const char* sidecarName,
                  const IMapKeySource* mapKeys, ParamError& err);              // strict; builds schema + AttributeSet at defaults
        bool ApplyPack(const Json::Value& pack, ParamWarnings& warn, ParamError& err);   // lenient, atomic
        bool Set(Dia::Core::StringCRC key, const Json::Value& value, ParamError& err);   // strict, atomic
        bool Reload(const char* sidecarText, const Json::Value* const* packs, uint32_t packCount,
                    ParamWarnings& warn, ParamError& err);                     // defaults + packs, atomic
        Json::Value ToJson(JsonShape shape) const;                             // Full: every parameter; Diff: a pack
        uint64_t    Hash() const;                                              // base values, schema order
        AttributeSet&       Values();                                          // modifiers, observers, save
        const AttributeSet& Values() const;
        const AttributeSchema& Schema() const;
    };
}
```

- **Strict** (`Load`, `Set`): any range, kind, constraint, envelope or unknown-key fault fails, names the key (both keys for a constraint) and changes nothing.
- **Lenient** (`ApplyPack`):
  - A wrong `format` or `version` rejects the pack.
  - An unknown key, a non-whole int or an unknown enum id is dropped, with a warning.
  - An out-of-range value is clamped, with a warning.
  - After the merge, the constraints run on the candidate values. Any failure rejects the whole pack, and the base values stay byte-identical.
- **Atomic**: every mutating call validates a candidate copy of the base values first, then commits. Observers fire only for values that actually changed, after the commit.
- **Reload**: defaults, then each pack in order. It gives exactly the state of a fresh `Load` plus the same `ApplyPack` calls. Values from `Set` are discarded (A4's "until the next reload"). Modifiers are kept.
- **`ToJson(Full)`** writes every parameter in schema order, with enum ids as strings and maps nested. **`ToJson(Diff)`** writes a pack (envelope plus the values that differ from their defaults), which is the shape `cow.<sys>.pack.save_as` writes. `ApplyPack(ToJson(Diff))` over a fresh load reproduces every base value bit for bit.
- **`Hash`**: FNV-1a 64 over the base values in schema order, as IEEE-754 bit patterns with `-0.0` normalised to `+0.0`. Modifiers are excluded. It is identical across runs and builds for identical values.
- **Failure**: there is no compiled fallback. A system whose sidecar fails to load must put its module in health Error, naming the key, and refuse to generate or step. Shipped data is gated by `dia validate schema` in the pipeline and in precommit.

### `ParamBinding<T>`

```cpp
template<class T> class ParamBinding {
public:
    void Bind(Dia::Core::StringCRC key, float T::* m);
    void Bind(Dia::Core::StringCRC key, int32_t T::* m);
    void Bind(Dia::Core::StringCRC key, bool T::* m);
    template<class E> void BindEnum(Dia::Core::StringCRC key, E T::* m);       // ordinal
    bool Resolve(const AttributeSchema& schema, BindingReport& report);         // keys -> indices, coverage
    void Snapshot(const AttributeSet& values, T& out) const;                    // resolved values incl. modifiers
};
```

- `Resolve` maps keys to indices once. It reports every sidecar parameter left unbound and every binding with no sidecar entry. Consumers assert both are empty in a test.
- `Snapshot` performs no allocation and no hash lookup. It reads by index. This requires `AttributeSet` index access to be O(1); see Open Design Question 2.

### Save

`AttributeSet`'s existing `ISaveable` saves base values and modifiers. `ParameterFile` adds nothing. A consumer that records the tuning a run used (A4's `params_hash`) stores `Hash()`.

## Acceptance Criteria

### Sidecar additions (DiaValueSchema + CLI)
1. **Bands.** Sidecar entries can carry `bands`. A band outside `[min, max]`, or with `lo > hi`, fails both `ValueSchemaLoader` and `dia validate schema`, naming the entry.
2. **Map defaults.** A map entry's `default` can be an object keyed by map key. Each value is checked against `valueType`, `min` and `max`. An unresolved key is `data.unresolved-map-key` in the CLI, and a load failure in C++.
3. **Constraints.**
   - Top-level `constraints` (`a`, `op`, `b`; `op` in `< <= > >= == !=`) are parsed and checked on defaults, after pack merges, and on `Set`.
   - A constraint naming an unknown or non-numeric key is `schema.bad-constraint`.
   - A violated constraint is `data.constraint-violated`, naming both keys.
4. **Parity.** The shared conformance fixtures cover cases 1–3. C++ (`ValueSchemaLoader` plus `ParameterFile`) and the CLI produce the same findings for every fixture. T-2's `table-and-form-editors.md` is amended to check constraints and show bands.

### Schema
5. **Building from a sidecar.**
   - `AttributeSchema::FromValueSchema` builds one definition per `kind: parameter` entry: float, int, bool, or enum (stored as an ordinal).
   - A missing default fails the build, naming the key.
   - Entries of other kinds are ignored.
6. **Map entries.**
   - A map entry expands to one definition per key from the `IMapKeySource`, named `<key>.<mapKey>`, in key-source order.
   - A key missing from the default object, an extra key, or no key source for a map entry each fails the build, naming the key.
7. **Old format removed.** `LoadFromJson` and `LoadFromJsonValue` are deleted. The CoW dragon stub and every DiaAttribute test load sidecars. All existing DiaAttribute tests pass, including the band-initialization tests (bands now come from the sidecar).

### Parameter file
8. **Strict load.** `Load` on a valid sidecar gives an `AttributeSet` at the defaults. Each of the following fails, names the first key, and leaves the `ParameterFile` unloaded:
   - a default out of range or of the wrong kind;
   - a violated constraint;
   - a bad band;
   - a wrong `format` or `version`.
9. **Lenient, atomic packs.** `ApplyPack`:
   - rejects a wrong `format` or `version`;
   - drops an unknown key, a non-whole int, or an unknown enum id, with a warning;
   - clamps an out-of-range value, with a warning.

   If any constraint fails after the merge, the whole pack is rejected and the base values are byte-identical. A sparse pack changes only its keys.
10. **`Set`.** `Set` applies one valid value and fires `OnAttributeChanged` once. An out-of-range value, a wrong kind, an unknown key, or a value that would break a constraint returns false, changes nothing, fires nothing, and names the key (both keys for a constraint).
11. **Reload.** `Reload(sidecar, packs)` gives state identical to a fresh `Load` plus the same `ApplyPack` calls. Prior `Set` values are gone, and active modifiers are kept.
12. **`ToJson` round trip.**
    - `ToJson(Full)` lists every parameter in schema order: enum ids as strings, maps nested.
    - `ToJson(Diff)` is a valid pack (envelope plus only the non-default values) that passes `dia validate schema` as a pack.
    - `ApplyPack(ToJson(Diff))` over a fresh load reproduces every base value bit for bit.
13. **Hash.**
    - `Hash()` is the same for identical base values across two loads and two processes.
    - Changing any one value, including the last parameter in schema order, changes it.
    - `-0.0` and `+0.0` hash the same.
    - Adding or removing a modifier does not change it.

### Binding and modifiers
14. **Snapshot.** `ParamBinding<T>::Snapshot` copies resolved values (modifiers included) into `T` for float, int, bool and enum members. A test shows it allocates nothing and performs no hash lookup (by index).
15. **Coverage.** `ParamBinding<T>::Resolve` reports every unbound sidecar parameter and every binding with no sidecar entry. Both lists are empty for the S-1 migration (asserted).
16. **Modifier legality by kind.**
    - Add and Multiply succeed on float and int; an int result is rounded half away from zero, then clamped.
    - On bool and enum, Add and Multiply fail and log a warning. Override succeeds with a valid value and fails with an invalid one.

### Save
17. Save and load of a `ParameterFile`'s `AttributeSet` restore the base values, the modifiers and the same `Hash()`.

### Proof migration (S-1)
18. **Sidecar replaces the table.** `TERRAIN_RULES_TABLE`'s values, ranges and kinds move into `Global/WorldGen/Rules/terrain_rules.schema.json`, and `terrain_rules.json` is deleted. `TerrainRules` becomes a struct filled by `ParamBinding<TerrainRules>::Snapshot`. `WorldGenerationModule` loads the sidecar through its catalogue alias (this replaces the earlier "wire `terrain_rules.json` now" loose end).
19. **No behaviour change.** S-1 layer goldens are bit-identical and every S-1 test passes. A broken sidecar edit puts `WorldGenerationModule` in health Error, naming the key, and generation refuses to run.

### Plan-gap additions (added 2026-11-18, plan-gap resolution)
20. **BindMap snapshot.** (added 2026-11-18, plan-gap resolution) `ParamBinding<T>::BindMap` snapshots a two-column biome map into per-key arrays, in key-source order, with no allocation and no hash lookup. Int, bool and enum maps use the typed overloads.
21. **Map coverage.** (added 2026-11-18, plan-gap resolution) `Resolve` treats a bound map as covering every `<key>.<mapKey>` attribute; an unbound map column reports every one of its keys, and a `count` that differs from the key source's count is reported.
22. **In-place writer round trip.** (added 2026-11-18, plan-gap resolution) `WriteDefaultsInPlace` over a sidecar with the unchanged `default` values returns text byte-identical to the input (key order, whitespace, labels).
23. **Only `default` tokens change.** (added 2026-11-18, plan-gap resolution) After writing new values, a byte diff against the input touches only `default` value tokens of the named entries.
24. **Map defaults per key.** (added 2026-11-18, plan-gap resolution) A map entry's default object is rewritten per key, with every other byte preserved.
25. **Clean failure.** (added 2026-11-18, plan-gap resolution) An entry with no `default`, or an unknown entry key, makes the writer fail with `Error` naming the key; `out` is left empty and the input is unchanged.
26. **SchemaHash ignores presentation.** (added 2026-11-18, plan-gap resolution) Changing a label, description, hint, unit, ui or colour does not change `SchemaHash()`.
27. **SchemaHash tracks meaning.** (added 2026-11-18, plan-gap resolution) Changing a key, kind, `min`/`max`, `default`, an enum value (id or order), a band, a constraint, or a map `keyRef`/`keyEnum` changes `SchemaHash()`.
28. **Missing `basedOn` is stale.** (added 2026-11-18, plan-gap resolution) A pack with no `basedOn` gives exactly one stale Warning (key `basedOn`) and still applies leniently.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaValueSchema/ValueInfo.h`, `ValueSchemaLoader.cpp` | Bands, object map defaults, `constraints` |
| `Dia/DiaValueSchema/Testing/Conformance/` | New fixtures |
| `Dia/DiaValueSchema/ValueSchemaWriter.h/.cpp` (or the existing loader's files) | **New** `WriteDefaultsInPlace` (plan-gap resolution) |
| `Dia/DiaAttribute/ParamBinding.h` | `BindMap` overloads (listed below as new) |
| `Dia/DiaCLI/dia_cli/commands/validate/{schema_rules,data_rules}.py` + tests | Bands, constraints, map defaults; `default-mismatch` scoped off parameters |
| `Dia/DiaAttribute/AttributeSchema.h/.cpp` | `FromValueSchema`; old loader deleted; kinds |
| `Dia/DiaAttribute/AttributeSet.h/.cpp` | Kind-aware modifiers; O(1) index access |
| `Dia/DiaAttribute/ParameterFile.h/.cpp` | **New** |
| `Dia/DiaAttribute/ParamBinding.h` | **New** |
| `Dia/DiaAttribute/IMapKeySource.h` | **New** |
| `Dia/DiaAttribute/Docs/dia.attribute.architecture.module.md` | DiaValueSchema dependency; new responsibilities |
| `Dia/DiaAttribute/Testing/AttributeTestHelpers.h` | Sidecar fixtures |
| `Cluiche/Tests/GoogleTests/DiaAttribute/*` | Migrated, plus new `TestDiaAttributeParameterFile.cpp`, `TestDiaAttributeParamBinding.cpp` |
| `Cluiche/CoW/Modules/F2DragonStub.cpp` + its schema asset | Move to a sidecar |
| `Cluiche/CoW/Shared/WorldGeneration/TerrainRules.h/.cpp` | Bound struct; X-macro removed |
| `WorldGenerationModule.cpp`, `WorldGenSession.cpp` | Load the sidecar |
| `Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json` | **New** |
| `Cluiche/Assets/CoW/Global/WorldGen/terrain_rules.json` | Deleted |
| `assets.catalogue.json` | Alias update |
| `docs/specs/applications/cow/systems/t2-worlddataeditor/table-and-form-editors.md` | Amendment |
| `DiaAttribute.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-001 | StringCRC for identifiers | Attribute names, map keys and binding keys are `StringCRC`. Key strings are kept only for diagnostics and `ToJson`. |
| PD-004 / AD-002 | No STL containers in public APIs | `MapKeys` is a pointer and count; warnings are a `DynamicArrayC` with a dropped count; errors are `String128` and `String256`. `Json::Value` is already used in DiaAttribute's public headers. |
| AD-001 (Dia) | Module YAML frontmatter | DiaAttribute's module doc gains the DiaValueSchema dependency (domain → foundation, allowed). `dia check deps` passes. |
| AD-001 (CoW) | Ask before Dia edits | Sign-off given in principle by the user on 2026-11-18 (research §0, D2). This spec's approval confirms it. The Plan-gap resolutions below (`BindMap`, `WriteDefaultsInPlace` in DiaValueSchema, `SchemaHash` inputs) were each signed off by the user on 2026-11-18. |
| D1 / D3 (research) | Strict base, lenient packs; sidecars own ranges | Designed in directly: policy flag; sidecar-only definitions and defaults. |

## Test Plan

TDD per `.claude/skills/tdd.md`, in this order:
1. Sidecar additions: the C++ loader and CLI rules against the new fixtures; parity test.
2. `FromValueSchema` (kinds, maps, failures).
3. Migration of the existing DiaAttribute tests to sidecars (green before any new behaviour).
4. `ParameterFile` strict load.
5. Packs.
6. `Set`.
7. Reload.
8. `ToJson` round trip.
9. Hash.
10. Modifier kind rules.
11. `ParamBinding` (coverage, no-allocation snapshot).
12. Save round trip.
13. S-1 migration with goldens.

Scoped runs use `--filter="*DiaAttribute*:*ValueSchema*:*TerrainRules*:*WorldGen*"` with `--shards 2`, plus CLI `pytest` for `validate`. There is one full suite run at close-out. The gap audit and edge pass is the last plan task. No mutation checks, no sanitizer.

## Open Design Questions

1. **Writing defaults back into a hand-authored sidecar.** "Save as defaults" (A1 `tuning.save`, T-2) now edits the sidecar, which also holds labels, descriptions and layout. A naive rewrite reorders keys and loses formatting. **Recommended:** a targeted writer that replaces only `default` values in place (one shared helper in DiaValueSchema, used by the C++ tools and T-2), with a round-trip test that unchanged fields are byte-identical. Decide before the first consumer adds "save as defaults".
2. **O(1) index access in `AttributeSet`.** `mSlots` is a `HashTable` keyed by name, and `GetAttributeNameByIndex` is O(index). AC 14 needs index reads with no hashing. **Recommended:** store slots in a schema-ordered `DynamicArrayC` and keep the `HashTable` as a name → index map. This touches Core internals, but not its public behaviour. Confirm in plan Task 0 against `AttributeSet.cpp`.
3. **Who supplies map keys before the biome registry exists?** `IMapKeySource` lets A5 pass biome ids. Until the registry (spec 2) is built, a consumer could pass the enum's id list, which the CLI can't see (its `keyRef` resolves against data files). **Recommended:** no map-using consumer migrates before the registry lands. This feature tests maps with a fixture key source and a fixture `keyRef` data file, so C++ and the CLI agree.

## Resolved Design Questions (2026-11-18, user)

The three Open Design Questions above are decided as recommended:
1. An in-place writer that replaces only `default` values (a shared DiaValueSchema helper, with a byte-identical round-trip test).
2. `AttributeSet` slots in a schema-ordered `DynamicArrayC`, with the `HashTable` as a name → index map (plan Task 0 confirms this against `AttributeSet.cpp`).
3. No consumer that uses maps migrates before the biome registry; maps are tested with a fixture key source and a fixture `keyRef` data file.

The interim S-1 "wire `terrain_rules.json` now" fix is dropped: AC 18 replaces it.


## Spec-6 resolutions (2026-11-18, user)

These settle open points raised while amending. Each item supersedes contradicting text above, including the world-sim review amendment.

1. **What a pack is (R3).**
   - A pack is a sparse document **of the parameter format itself**: `format` = the sidecar's `for`, e.g. `cow.weather_params`. It is identified as a pack by living under `Packs/`, exactly as `dia validate schema` already does.
   - There are no separate pack format ids (`cow.weather_pack` etc.) and no `base` + `overrides` shape.
2. **`basedOn`.** A pack carries `"basedOn": <hex>`, the schema hash of the sidecar's parameter entries (keys, kinds, ranges and defaults) when the pack was saved.
   - `ParameterFile` exposes `SchemaHash()`.
   - A pack whose `basedOn` differs from the current `SchemaHash()` is **stale**. It still applies leniently, with one Warning, following F-3's load-and-warn rule.
   - `ToJson(Diff)` writes `basedOn`.
   - This adds to ACs 9 and 12.

## Plan-gap resolutions (2026-11-18, user)

Each item supersedes contradicting text above (including Open Design Question 1's "decide before the first consumer" and the Spec-6 `SchemaHash` wording).

1. **`ParamBinding<T>::BindMap(key, float T::* firstElement | float* array, uint32_t count)`.**
   - Binds a map entry (one value per map key, in key-source order) to a per-key array, so maps snapshot like scalars.
   - Int, bool and enum maps use typed overloads.
   - `Resolve` treats a bound map as covering every `<key>.<mapKey>` attribute.
   - ACs 20, 21.
2. **In-place default writer.** `Dia::ValueSchema::WriteDefaultsInPlace(const char* sidecarText, const DefaultValue* values, uint32_t count, String& out, Error&)` (or an equivalent shared helper).
   - Replaces only the `default` values of the named entries; every other byte is preserved (key order, whitespace, labels).
   - Used by `cow.<sys>.params {action: save_defaults}` and by T-2.
   - Map defaults are rewritten per key; an entry with no `default` fails cleanly.
   - ACs 22-25. The helper is built in this feature (not deferred to the first consumer).
3. **`SchemaHash()` inputs.**
   - Hashed: keys, kinds (`type`), `min`/`max`, `default`, enum values (ids and order), bands, `constraints`, map `keyRef`/`keyEnum`.
   - Excluded (presentation): label, description, hint, unit, ui, colour.
   - A pack with no `basedOn` is **stale**: one Warning, then it applies leniently (supersedes "accept silently").
   - ACs 26-28.

## Status

**Status:** `Done` (approved 2026-11-18, user; built and closed 2026-11-20). Plan: @docs/specs/applications/dia/systems/diaattribute/parameter-files.plan.md. Interview done with the user on 2026-11-18. Design choices: the sidecar is the only format and owns defaults; packs go into base values and runtime effects are modifiers; parameters versus records; constraints live in the sidecar; maps are one per column; a load failure fails loudly with no fallback.

## Band schema version resolution (2026-11-20, user)

The sidecar `version` remains the format version (the loader still accepts only `version: 1`). A separate optional file-level field `bandSchemaVersion` (a whole number from 1 to 2147483647, default 1 when absent; anything else fails the load naming `bandSchemaVersion`) feeds `AttributeSchema::GetBandSchemaVersion()`. `LoadResult::mBandSchemaVersion` carries it, and `dia validate schema` mirrors the rule as `schema.bad-band-schema-version`. This supersedes any text above saying `GetBandSchemaVersion()` returns the sidecar `version`.
