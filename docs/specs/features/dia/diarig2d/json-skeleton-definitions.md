# Feature Spec: JSON Skeleton Definitions

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarig2d.md | **json-skeleton-definitions** |

**Status:** `Approved`

---

## Problem Statement

Hand-authoring SkeletonDef structs in C++ code is tedious and error-prone. Artists and designers need a data-driven way to define skeletons. Tools that generate skeletons (e.g., future DiaRig2DEditor) need a standard interchange format. Without a file format, every skeleton is hardcoded.

---

## Solution Overview

Provide `LoadSkeletonDef()` and `SaveSkeletonDef()` functions that read/write SkeletonDef from/to JSON using DiaCore/Json (jsoncpp wrapper).

### JSON Format

```json
{
    "id": "player_skeleton",
    "bones": [
        { "name": "root",      "parent": -1,      "position": [0, 0],   "rotation": 0, "scale": [1, 1] },
        { "name": "spine",     "parent": "root",   "position": [0, 1.0], "rotation": 0 },
        { "name": "left_arm",  "parent": "spine",  "position": [-0.5, 0] },
        { "name": "right_arm", "parent": "spine",  "position": [0.5, 0] }
    ]
}
```

**Rules:**
- `parent` can be a string (bone name) or integer (bone index). Name-based references are resolved to indices on load.
- `rotation` defaults to 0 if omitted.
- `scale` defaults to [1, 1] if omitted.
- `position` is required (no sensible default).
- Bones must be in topological order in the JSON array (consistent with SkeletonDef/Skeleton requirement).
- `length` is NOT in the JSON — it's computed by Skeleton construction from the bind pose (RD-012).

### API

```cpp
namespace Dia::Rig2D {
    bool LoadSkeletonDef(const Dia::Core::FilePath& path, SkeletonDef& outDef);
    bool LoadSkeletonDefFromString(const char* jsonString, SkeletonDef& outDef);
    bool SaveSkeletonDef(const SkeletonDef& def, const Dia::Core::FilePath& path);
}
```

- `LoadSkeletonDef` returns false and logs a `Rig2D` channel error on parse failure (malformed JSON, missing required fields, unresolved parent names).
- DIA_ASSERT is NOT used for JSON load errors — these are data errors, not programming errors. The caller checks the return value.
- After successful load, the caller constructs a Skeleton from the returned SkeletonDef, which performs structural validation (DIA_ASSERT on invalid topology).

### Parent Name Resolution

On load, for each bone:
1. If `parent` is an integer, use directly.
2. If `parent` is a string, scan all preceding bones for a matching name.
3. If the string is not found, log an error and return false.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaRig2D/SkeletonJson.h` | Load/Save function declarations |
| `Dia/DiaRig2D/SkeletonJson.cpp` | JSON parsing and serialization implementation |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `LoadSkeletonDef` parses a valid JSON file and populates SkeletonDef correctly | Unit test with test fixture JSON |
| 2 | `LoadSkeletonDefFromString` parses a JSON string (for testing and embedded data) | Unit test |
| 3 | Parent references by name resolve to correct indices | Unit test: JSON with name parents, verify resolved indices |
| 4 | Parent references by integer index work correctly | Unit test: JSON with integer parents |
| 5 | Missing `rotation` defaults to 0 | Unit test |
| 6 | Missing `scale` defaults to [1, 1] | Unit test |
| 7 | Missing `position` returns false with error log | Unit test |
| 8 | Unresolved parent name returns false with error log | Unit test |
| 9 | Malformed JSON returns false with error log | Unit test |
| 10 | `SaveSkeletonDef` writes valid JSON that can be loaded back | Unit test: save, load, compare |
| 11 | `SaveSkeletonDef` writes parent references as bone names (human-readable) | Unit test: save, inspect JSON text |
| 12 | `length` is NOT serialized — computed on Skeleton construction | Unit test: load JSON, construct Skeleton, verify length is computed |
| 13 | No STL in public API | Code review |
| 14 | Load errors logged to `Rig2D` DiaLogger channel, not DIA_ASSERT | Code review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Implement `SkeletonJson.h` / `SkeletonJson.cpp` — LoadSkeletonDef, LoadSkeletonDefFromString, SaveSkeletonDef | Flat Skeleton feature | Uses DiaCore/Json/ (jsoncpp) |
| 2 | Create test fixture JSON files in `Cluiche/Tests/GoogleTests/Rig/Fixtures/` | 1 | |
| 3 | Unit tests for load (valid, defaults, errors), save, round-trip | 1, 2 | |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — SkeletonDef.id parsed from JSON "id" string into StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — FilePath and SkeletonDef use Dia types |
| PD-007 | Platform | C++20 required | Compliant |
| AD-002 | Dia App | No STL in public APIs | Compliant |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::Rig2D:: |
| RD-004 | DiaRig2D | Bone lookup by StringCRC | Compliant — bone names in JSON become StringCRC |
| RD-008 | DiaRig2D | JSON uses bone names for parent references | Compliant — names supported and preferred; integer indices also accepted |
| RD-012 | DiaRig2D | Bone length computed at construction | Compliant — length not serialized |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Error handling | Load returns bool instead of DIA_ASSERT for data errors. Should there be a richer error reporting mechanism (error codes, error messages)? | Bool + DiaLogger error message is sufficient for v1. The log message contains the specific error (missing field, unresolved parent, etc.). A structured error result type is overkill until there's a UI that needs to display specific errors. |
| 2 | SaveSkeletonDef | Should Save write parent references as names or indices? | Names — human-readable, consistent with RD-008. Tools and humans both benefit from named parents. The tiny cost of name-to-index resolution on load is negligible. |
| 3 | Encoding | Should JSON support UTF-8 bone names? | Bone names are StringCRC (CRC hash of the string). The original string is only used for JSON readability and debug logging. UTF-8 is fine for the JSON string; the CRC hash is encoding-agnostic. No special handling needed. |
| 4 | File paths | LoadSkeletonDef takes a FilePath — should it also support loading from a memory buffer? | Yes — `LoadSkeletonDefFromString(const char* json, SkeletonDef& outDef)` handles embedded data and testing. Both are provided. |

---

## Open Questions

None — all resolved above.
