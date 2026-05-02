# Feature Spec: Animation Clip Loader

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaanimation2d.md | **animation-clip-loader** |

**Status:** `Approved`

---

## Problem Statement

Animation clip data must come from files — both a simple custom JSON format for hand-authored clips and Spine2D v4 JSON for AI-generated/tool-generated animation. Without a loader, game code must manually parse JSON and construct AnimClipDef structs, duplicating parsing logic across every consumer.

---

## Solution Overview

Two free functions in `Dia::Animation2D`:

```cpp
namespace Dia::Animation2D {
    // Load from custom DiaAnimation2D JSON format
    AnimClipDef LoadAnimClipFromJson(const Json::Value& root);

    // Load from Spine2D v4 JSON animation section.
    // animationName selects which animation from the Spine JSON.
    // Converts degrees->radians, Y-up->engine convention at load time (AND-018).
    AnimClipDef LoadAnimClipFromSpine(const Json::Value& spineRoot,
                                      const Dia::Core::StringCRC& animationName);
}
```

### Custom JSON Format

```json
{
    "id": "dragon_takeoff",
    "duration": 0.5,
    "tracks": [
        {
            "bone": "spine",
            "keyframes": [
                { "time": 0.0, "position": [0, 0], "rotation": 0, "scale": [1, 1] },
                { "time": 0.25, "position": [0, 0.2], "rotation": -0.1, "scale": [1, 1] },
                { "time": 0.5, "position": [0, 0.5], "rotation": -0.3, "scale": [1, 1] }
            ]
        }
    ]
}
```

Keyframe fields are optional — omitted fields use bind pose defaults (position {0,0}, rotation 0, scale {1,1}). Custom format uses radians natively.

### Spine2D v4 Format Handling

- Reads `animations.<name>.bones.<boneName>` timelines: `rotate`, `translate`, `scale`
- Spine uses degrees for rotation — convert to radians
- Spine uses Y-up — convert to engine convention
- Slots, draw order, attachment timelines, events — ignored (rendering/game concern)
- Bone names matched by string (resolved to StringCRC)
- Duration computed from maximum timeline time

### Key Behaviours

1. Both loaders return `AnimClipDef` — construction of `AnimClip` from def handles validation (AND-030).
2. Spine2D coordinate conversion at load time, not runtime (AND-018).
3. Uses DiaCore/Json (jsoncpp) — no new parser dependency (AND-007).
4. Bone names not validated against a skeleton at load time — that happens at `AnimClip` construction (AND-017).
5. Invalid JSON structure — DIA_ASSERT (programming error to pass malformed data).

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaAnimation2D/AnimClipLoader.h` | Free function declarations |
| `Dia/DiaAnimation2D/AnimClipLoader.cpp` | Both loader implementations |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `LoadAnimClipFromJson` loads all fields correctly (id, duration, tracks with bone names, keyframes with time/position/rotation/scale) | Unit test: load a complete JSON clip, verify all AnimClipDef fields match |
| 2 | Partial keyframes in custom format default correctly: omitted position defaults to {0,0}, rotation to 0, scale to {1,1} | Unit test: load JSON with rotation-only keyframes, verify defaults for omitted fields |
| 3 | `LoadAnimClipFromSpine` converts rotation from degrees to radians | Unit test: load Spine JSON with 90-degree rotation, verify AnimClipDef contains PI/2 radians |
| 4 | `LoadAnimClipFromSpine` converts Spine2D Y-up coordinate convention to engine convention | Unit test: load Spine JSON with known translate values, verify Y-axis conversion |
| 5 | `LoadAnimClipFromSpine` ignores slots, events, and draw order timelines — only bones are loaded | Unit test: load Spine JSON containing slot/event data, verify no crash and only bone tracks present |
| 6 | Duration is computed correctly from the maximum keyframe time across all tracks | Unit test: load clip where last keyframe is on a non-last track, verify duration matches max time |
| 7 | Invalid JSON structure triggers DIA_ASSERT (missing required fields like "tracks" or "bones") | Unit test: pass malformed JSON, DIA_ASSERT fires |
| 8 | Returned AnimClipDef contains bone names as StringCRC (not raw strings) | Unit test: load clip, verify track boneId values are correct StringCRC hashes |
| 9 | Multiple animations in one Spine file are selectable by `animationName` parameter | Unit test: load Spine JSON with two animations, select each by name, verify correct data returned |
| 10 | Both loaders use DiaCore/Json (jsoncpp) — no additional parser dependencies | Code review |
| 11 | All code in `Dia::Animation2D::` namespace | Code review |
| 12 | Keyframe time values in custom format are preserved exactly as specified (no conversion applied to time) | Unit test: load custom JSON, verify keyframe times match input values |
| 13 | Empty tracks array in custom JSON produces AnimClipDef with zero tracks (valid — AnimClip construction handles validation) | Unit test: load JSON with empty tracks, verify AnimClipDef has 0 tracks |
| 14 | Spine2D animation with no bone timelines produces AnimClipDef with zero tracks and zero duration | Unit test: load Spine animation with only slot timelines, verify empty result |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Implement `LoadAnimClipFromJson` in `AnimClipLoader.h` / `AnimClipLoader.cpp` | - | Parse custom JSON format, handle optional keyframe fields with defaults |
| 2 | Implement `LoadAnimClipFromSpine` in `AnimClipLoader.h` / `AnimClipLoader.cpp` | - | Parse Spine2D v4 JSON, convert degrees to radians, convert Y-up to engine convention |
| 3 | Unit tests with sample JSON fixtures in `Cluiche/Tests/GoogleTests/Animation2D/` | 1, 2 | Cover all acceptance criteria: complete loads, partial keyframes, Spine conversion, edge cases |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — bone names in tracks resolved to StringCRC; clip id is StringCRC |
| PD-004 | Platform | No STL containers in public APIs | Compliant — AnimClipDef uses DynamicArrayC for tracks and keyframes; no std::vector or std::string in signatures |
| PD-007 | Platform | C++20 required | Compliant — code compiled under /std:c++20 |
| AD-002 | Dia App | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — all code in Dia::Animation2D:: |
| AND-007 | DiaAnimation2D | Clips loaded from JSON via DiaCore/Json (jsoncpp) | Compliant — this feature implements AND-007; both loaders take Json::Value from DiaCore/Json |
| AND-010 | DiaAnimation2D | No STL in public APIs | Compliant — reinforces PD-004/AD-002 |
| AND-017 | DiaAnimation2D | Clip bones not found in skeleton: DIA_LOG_WARNING and skip | Compliant — loader does not validate bone names against skeleton; validation deferred to AnimClip construction per AND-017 |
| AND-018 | DiaAnimation2D | Spine2D loader converts degrees to radians and Y-up to engine convention at load time | Compliant — this feature implements AND-018; all conversion happens in LoadAnimClipFromSpine, not at runtime |
| AND-030 | DiaAnimation2D | AnimClipDef validates at AnimClip construction | Compliant — loaders return AnimClipDef only; validation (zero-duration, unsorted keyframes, duplicate tracks) is AnimClip constructor's responsibility |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Spine Loader | What if a Spine2D animation has no bone timelines (only slot/event timelines)? | The loader produces an AnimClipDef with zero tracks and zero duration. This is valid — AnimClip construction may DIA_ASSERT on zero-duration if tracks exist, but zero tracks with zero duration is a degenerate-but-safe result. Callers should check track count before constructing an AnimClip. |
| 2 | Custom Loader | What if the custom JSON contains extra unknown fields (e.g. "author", "version")? | Ignore them. jsoncpp silently skips fields the loader does not request. This allows forward-compatible JSON schemas where newer versions add metadata fields that older loaders do not understand. No DIA_ASSERT on unknown fields. |
| 3 | Validation | Should the loader validate that keyframe times are sorted in ascending order? | No — the loader performs no validation beyond structural JSON parsing. Keyframe time ordering is validated at AnimClip construction (AND-030). The loader is a pure data transformer: JSON in, AnimClipDef out. This keeps the single-responsibility principle: loaders parse, AnimClip validates. |
| 4 | Compatibility | What about Spine2D v3 vs v4 JSON differences? | v4 only in this feature. Spine v3 uses a different JSON structure (e.g. "bones" timeline format differs, curve syntax changes). Supporting v3 would require a separate code path with version detection. If v3 support is needed, it should be a separate feature spec. The loader DIA_ASSERTs if the expected v4 structure is not found. |
| 5 | Batch Loading | Should the loader support loading multiple clips from one Spine file in a batch? | Not in v1. Callers iterate animation names and call `LoadAnimClipFromSpine` per animation. A batch helper (`LoadAllClipsFromSpine` returning a DynamicArrayC of AnimClipDef) is a convenience that can be added later without API changes. The per-animation API is the correct primitive. |
| 6 | Error Handling | Should the loader return an error code / optional instead of DIA_ASSERT on malformed JSON? | No — DIA_ASSERT is the correct pattern per codebase convention. Malformed JSON passed to the loader is a programming error (caller should validate file existence and basic JSON parse success before calling). Runtime file-not-found or parse-failure is the caller's responsibility (e.g. via DiaCore/Json parse result). The loader assumes it receives valid Json::Value. |

---

## Open Questions

None — all resolved above.
