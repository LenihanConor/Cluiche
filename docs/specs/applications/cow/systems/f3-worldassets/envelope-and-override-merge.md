# Feature Spec: F-3 Envelope and Override Merge

**Parent:** @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md

**Gameplay Domains:** debug

## Summary

The pure foundation of F-3: how a world-sim file declares what it is, and how a pack of overrides combines with a defaults file. It delivers:

- **`AssetEnvelope`**: read and write the `"format"` / `"version"` header and classify a file (ok, too new, older, wrong or missing format);
- **`OverrideMerge`**: `Resolve` (defaults plus pack overrides become the resolved values) and `Diff` (current values against defaults become the overrides), with the load-and-warn rules for unknown keys, type mismatches, out-of-range values and stale packs;
- the pack file builder and parser, with a stable key order so source-tree diffs stay small.

Everything here is pure logic over `Json::Value`: no files, no commands, no modules. It is built test-first and it is the part of F-3 that can start now.

## Problem

Every world-sim system names its own files today, some versioned and some not, and tuning is lost when the game closes. F-3 makes tuning a saved file: a pack holds only the values that differ from the defaults. That only works if three rules are exact: how a file proves what it is, how overrides apply on top of defaults (including when the defaults have since changed), and how current values are turned back into overrides. Getting these wrong corrupts packs silently, so they are specified and tested before any module uses them.

## Goals

1. **One header rule.** Every world-sim file starts with `format` and `version`; one function classifies it.
2. **Overrides only.** A pack holds only keys that differ from the defaults, so adding a parameter to a defaults file never breaks a pack.
3. **Round trip.** `Resolve(defaults, Diff(defaults, current))` gives back `current` (within the stated tolerance).
4. **Load and warn.** Unknown keys, wrong types and out-of-range values are dropped or clamped with one Warning each; a stale pack still loads.
5. **Stable files.** The same overrides always serialise to the same text.
6. **No CoW types.** The envelope and merge are generic so they can move to Dia later (F-3 placement note).

## Non-Goals

- Reading or writing files, catalogue upserts, aliases (Features 3 and 4).
- Loading bundles or applying values to systems (Feature 2).
- Overrides of species or biome record fields: packs cover only the parameter files `cow.weather_params` and `cow.ecology_params` in v1. Species and biome tuning keeps writing to the defaults (`tuning.save`).
- Format migrations (none in v1, per the parent spec).
- Overriding array elements: an array is one value.

## Architecture

`Cluiche/CoW/Shared/WorldAssets/` (AD-002, CoW-Shared), namespace `CoW::WorldAssets`, files `AssetEnvelope.*`, `OverrideMerge.*`, `PackFile.*`. Only the format ids passed in by callers are CoW content.

### `AssetEnvelope`

```cpp
struct EnvelopeSpec { Dia::Core::StringCRC format; int32_t currentVersion; };   // what the loader understands

enum class EnvelopeStatus { Ok, Older, TooNew, MissingFormat, WrongFormat, MissingVersion, BadVersion };
struct EnvelopeResult { EnvelopeStatus status; int32_t fileVersion; const char* message; };   // message is a static string

EnvelopeResult CheckEnvelope(const Json::Value& root, const EnvelopeSpec& spec);
void           WriteEnvelope(Json::Value& root, Dia::Core::StringCRC format, int32_t version);   // sets "format" and "version"
```

| File | Status | Meaning |
|---|---|---|
| root is not an object, or no `format` string | `MissingFormat` | Error: not loaded |
| `format` differs from `spec.format` | `WrongFormat` | Error: not loaded (for example a weather pack given to the ecology loader) |
| no `version` | `MissingVersion` | Error |
| `version` is not an integer | `BadVersion` | Error |
| `version` > `spec.currentVersion` | `TooNew` | Error: not loaded (parent version table) |
| `version` < `spec.currentVersion` | `Older` | Loads; the caller logs one Warning (no migration in v1) |
| `version` = `spec.currentVersion` | `Ok` | Loads |

`WriteEnvelope` puts `format` first and `version` second, whatever else the object holds.

### Pack file

```json
{ "format": "cow.ecology_pack", "version": 1,
  "base": { "format": "cow.ecology_params", "version": 3 },
  "displayName": "Lush",
  "overrides": { "grazing.rate": 0.8, "regrowth.forest": 1.25 } }
```

```cpp
struct PackSpec { EnvelopeSpec pack; Dia::Core::StringCRC baseFormat; };
struct PackDoc  { int32_t baseVersion; std::string displayName; OverrideSet overrides; };   // std::string allowed: CoW code, not a Dia public API

PackParseResult ParsePack(const Json::Value& root, const PackSpec&, PackDoc& out);   // envelope check + base.format must equal baseFormat
Json::Value     BuildPack(const PackSpec&, int32_t baseVersion, const char* displayName, const OverrideSet&);
```

- `ParsePack` fails (Error, not loaded) for any envelope error, for `base.format` differing from `baseFormat`, and for an `overrides` that is not an object or has non-leaf values.
- `BuildPack` writes keys in the order `format`, `version`, `base`, `displayName`, `overrides`, with overrides sorted by dotted path, two-space indentation and a trailing newline, so one input always gives one file.

### `OverrideMerge`

An `OverrideSet` is an ordered list of `(dotted path, leaf value)`. A **leaf** is a number, bool, string or array; a path names a leaf by walking nested objects of the defaults, splitting on `.`.

**Defaults rule.** Parameter names in the defaults file must not contain `.`. `ValidateDefaults(defaults)` checks this and returns the offending paths, an Error at load for any format that accepts packs, so a path always means one thing.

```cpp
struct MergeReport {
    struct Dropped  { std::string path; DropReason reason; };      // UnknownKey, TypeMismatch, NotALeaf
    struct Clamped  { std::string path; double from; double to; };
    std::vector<Dropped> dropped;  std::vector<Clamped> clamped;
    uint32_t applied = 0;
    bool stale = false;  bool newerBase = false;  int32_t baseVersionDelta = 0;
};

struct IValueRanges { virtual bool Range(const char* path, double& lo, double& hi) const = 0; };   // optional

Json::Value Resolve(const Json::Value& defaults, const OverrideSet&, const IValueRanges*, MergeReport&);
OverrideSet Diff   (const Json::Value& defaults, const Json::Value& current, MergeReport&);
void        EvaluateStale(int32_t baseVersion, int32_t defaultsVersion, MergeReport&);
```

**`Resolve`** starts from a copy of the defaults and applies each override in order:
- a path that does not exist in the defaults is **dropped** (`UnknownKey`), one Warning each;
- an override that names an object node is dropped (`NotALeaf`);
- an override whose JSON kind differs from the default leaf's is dropped (`TypeMismatch`). Integer and real numbers count as the same kind; bool, string and array are each their own kind;
- an array override replaces the whole array;
- a number outside the range given by `IValueRanges` is **clamped** and reported (VS-004; a null `IValueRanges` skips this step);
- a key absent from the pack keeps its default.

**`Diff`** walks the defaults and emits an override for every leaf whose value in `current` differs:
- numbers differ when `|a − b| > 1e-6 × max(|a|, |b|)` and `|a − b| > 1e-9` (so slider noise such as 0.8000000119 against 0.8 is not an override); bool and string by equality; arrays element by element with the same number rule;
- a leaf missing from `current` is treated as unchanged; a key in `current` that is not in the defaults is ignored and reported as `UnknownKey`;
- emitted numbers are rounded to 9 significant digits; paths are sorted ascending.

**`EvaluateStale`** (the parent version table):
- `baseVersion` < `defaultsVersion`: `stale = true`, with `baseVersionDelta` set. The pack still loads.
- `baseVersion` > `defaultsVersion`: `newerBase = true`. The pack still loads, best effort, with one Warning; unknown keys are dropped as usual.
- equal: neither flag.

**Saving a stale pack** (Feature 3) writes the current `baseVersion`, which clears the stale flag; nothing in this feature keeps old versions.

### Logging

Pure functions do not log. They return the report; the module (Feature 2) logs one Warning per entry under channel `WAS`. A small helper, `LogMergeReport(report, ctx)`, formats entries for that module and is covered by a test with a fake logger.

## Acceptance Criteria

1. `CheckEnvelope` returns `Ok`, `Older`, `TooNew`, `MissingFormat`, `WrongFormat`, `MissingVersion` and `BadVersion` for the cases in the table, and loads only on `Ok` and `Older`.
2. `WriteEnvelope` places `format` first and `version` second in any object.
3. `ParsePack` accepts a valid pack and rejects wrong pack format, wrong `base.format`, missing `base.version`, a non-object `overrides`, and non-leaf override values.
4. `Resolve` applies a scalar override to the right leaf, replaces an array as a whole, and leaves keys not in the pack at their defaults.
5. An override path not present in the defaults is dropped with `UnknownKey`; one naming an object is dropped with `NotALeaf`; one of a different JSON kind is dropped with `TypeMismatch`; integer and real numbers are interchangeable.
6. With an `IValueRanges`, an out-of-range number is clamped and reported with the original and clamped values; without one, no clamping happens.
7. `Diff` emits exactly the leaves that differ beyond the tolerance (relative 1e-6 and absolute 1e-9), ignores keys missing from `current`, reports keys not in the defaults as `UnknownKey`, rounds numbers to 9 significant digits and sorts paths ascending.
8. Round trip: `Resolve(defaults, Diff(defaults, current))` equals `current` within the tolerance, for fixtures with nested objects, bools, strings and arrays.
9. Adding a new parameter to the defaults leaves an existing pack's resolved values for all old keys unchanged and gives the new key its default.
10. `EvaluateStale` sets `stale` and `baseVersionDelta` when the pack's base is older, `newerBase` when it is newer, and neither when equal; both still load.
11. `ValidateDefaults` reports every key containing `.` with its path and passes a defaults file without any.
12. `BuildPack` output is byte-identical for equal inputs regardless of the order the overrides were added in, and parses back to the same `PackDoc`.
13. `LogMergeReport` produces one Warning line per dropped, clamped or stale entry.
14. The files contain no CoW-specific types; format ids and field names reach them only as arguments.

## Test Strategy

- **GoogleTests (TDD, test-first per the project rule):** one group per acceptance criterion, run at Small fixture sizes: envelope cases from literal JSON strings; merge and diff over small nested fixtures; a randomised round-trip test (fixed seed) over generated defaults trees and perturbed copies, asserting the round-trip property and sorted, stable output.
- **Fakes:** a fake `IValueRanges` backed by a table; a fake logger for `LogMergeReport`.
- **Test utilities** live with the module in `CoW/Shared/WorldAssets/Testing/`: JSON fixture helpers and a tolerance-equal comparer for `Json::Value`.
- **Golden file:** one checked-in pack file produced by `BuildPack`, compared byte for byte, so formatting changes are deliberate.
- No Vitest, E2E or perf tests: this feature has no UI and its cost is negligible.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| AD-002 | Four-tier placement | CoW-Shared, generic parts free of CoW types |
| PD-001 | StringCRC ids | Format ids are `StringCRC` |
| VS-004 (DiaValueSchema) | Out-of-range pack overrides clamp with a Warning | `IValueRanges` clamp path; the adapter over the schema is Feature 2 |
| F-3 version table | Load and warn rules | Implemented as written |
| CLAUDE.md TDD rule | New public API is built test-first | Pure logic, tests before code |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaCore Json (`jsoncpp`), `StringCRC` | Values, format ids | Done |
| DiaValueSchema Feature 1 | Supplies ranges through the Feature 2 adapter, not used here | Approved |
| Features 2 to 6 | Consumers | Draft |

## Resolved Decisions

1. **Packs cover the parameter files only** (2026-10-20). Species and biome record fields are not overridable in v1. The parent Asset Table already names `cow.ecology_params` and `cow.weather_params` as pack bases; A1's species and biome tuning keeps using `cow.ecology.tuning.save`. Record addressing and a multi-file `base` can be added as a format version later.
2. **Numeric tolerance and precision** (2026-10-20). Two numbers differ when they differ by more than 1e-6 relative and 1e-9 absolute; written numbers keep 9 significant digits. This stops slider noise becoming overrides and keeps float32 values exact when read back.
3. **Dots are reserved in parameter names** (2026-10-20). A dotted path is only unambiguous if names have no dots, so `ValidateDefaults` rejects a defaults file that has any. The cost is one rename if an existing file used a dotted key.
4. **A newer base is a Warning, not an Error** (2026-10-20). A pack saved against newer defaults than the game has is loaded best effort, since unknown keys are already dropped one by one. Only the file's own `version` being too new is fatal.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Can start now: pure logic, no dependencies.
