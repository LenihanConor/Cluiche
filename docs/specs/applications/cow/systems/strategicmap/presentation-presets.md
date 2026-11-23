# Feature Spec: Strategic Map Presentation Presets

## Parent System
@docs/specs/applications/cow/systems/strategicmap/strategicmap.md (Feature 5)

**Gameplay Domains:** rendering
**Depends on:** Strategic Map Features 1 and 4; DraoiHexWorldRender3D Hex Terrain Material and Map Assembly; DiaBgfx3D Post-Process Stack
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** presentation controls in `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (state defaults and the "Presentation" and "Style" panel sections); screenshots `lookdev/shots/` (overview, close, storms, collapsed, presentation)

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

Named, reviewable bundles of style values: tilt-shift, haze, painted sky, depth silhouettes, form lines, contours, line boil, camera curves and so on, tuned for different uses:

- **`strategic`**: the default play look. Readable at world zoom, moderate tilt-shift, haze, painted sky, silhouettes, form lines, boil on.
- **`cinematic`**: stronger tilt-shift and haze, lower camera, more sky. For screenshots and trailers.
- **`debug-flat`**: everything cosmetic off (no post, no boil, no props), flat shading, grid on. For verifying data.
- **`sketch`**: the cheap path: no post pass, no particles, fewer noise octaves, for weaker GPUs.

Presets are small override files applied over the defaults. They are the art direction in data.

## Problem

- Tuning is only useful if good settings survive and can be reapplied. A preset is the tuned look made repeatable.
- Different jobs want different looks (play, capture, verify data). Without presets each tool re-sets a dozen values by hand.
- Cost control: low-end machines need a one-click way to turn off the expensive parts.

## Goals

1. **`map_presets/<name>.json`** (`cow.map_preset`): a sparse set of style overrides, with a description and a `basedOn` preset.
2. **Four shipped presets** with the values below, reviewed by the user from the prototype.
3. **Apply, list, save-as** through the commands in Feature 4; presets compose (`basedOn`), with cycle detection.
4. **Cost tiers**: `sketch` documents which features it disables and by how much, so the cost story is explicit.
5. **A per-preset capture** set: one capture per preset at a fixed world and camera for the user's taste review.

## Non-Goals

- A UI for picking presets (V-1 panel; later settings screen).
- Per-player quality settings and auto-detection of hardware.
- Presets for gameplay overlays.

## Design

### File

```json
{ "format": "cow.map_preset", "version": 1, "name": "cinematic",
  "description": "Stronger depth cues for screenshots", "basedOn": "strategic",
  "style": { "post.tiltShift.strength": 1.4, "post.haze.strength": 1.3, "camera.pitchNear": 48,
             "post.sky.cloudAmount": 0.6, "particles.on": true } }
```

Keys are the dotted style paths from Feature 4. Unknown keys are warnings (kept); out-of-range values are clamped with a warning. A preset never changes data (palette, biome rows) — only style.

### Shipped values

| Setting | strategic | cinematic | debug-flat | sketch |
|---|---|---|---|---|
| Post pass | on | on | **off** | **off** |
| Tilt-shift strength / band | 1.0 / 0.42 | 1.4 / 0.40 | 0 | 0 |
| Haze | 1.0 | 1.3 | 0 | 0 |
| Sky | on | on | off | off |
| Depth silhouettes | on | on | off | off |
| Form lines / contours | on / off | on / off | off / off | off / off |
| Line boil | on | on | **off** | off |
| Props | on | on | **off** | on, density 0.6 |
| Particles / clouds | on | on | off | **off** |
| Hex grid overlay | off | off | **on** | off |
| Data view | none | none | class | none |
| Camera pitch near/far | 55° / 70° | 48° / 66° | 90° (top-down) | 55° / 70° |
| Height factor far | 0.6 | 0.7 | 0 (flat) | 0.6 |

### Interface

```cpp
namespace CoW::Map {
    struct MapPreset { Name name; Name basedOn; String description; /* sparse key/value list */ };
    class PresetLibrary {
    public:
        bool LoadFolder(const char* dir, std::string* err);
        const MapPreset* Find(StringCRC name) const;
        bool Resolve(StringCRC name, StyleValues& out, std::string* err) const;   // applies basedOn chain over defaults
        uint32_t Count() const;  const MapPreset* At(uint32_t) const;
    };
}
```

## Acceptance Criteria

1. **Shipped presets load.** All four load with no warnings, and `list` returns them in a stable order with descriptions.
2. **Resolve.** `Resolve("cinematic")` equals defaults + strategic overrides + cinematic overrides; a key in both takes the later value.
3. **Cycles and missing bases.** A preset with `basedOn` pointing at itself or in a cycle fails with the chain printed; a missing base fails naming it. Neither crashes or hangs.
4. **Idempotent apply.** `apply` resets the style to defaults, then layers the resolved preset, so the result does not depend on what was applied before. A test applies the four presets in several orders and gets the same final style for each preset every time.
5. **Debug-flat is flat.** With `debug-flat` applied: no post pass runs, no sprites or particles draw, boil is off, the capture shows class colours and the hex grid, and two captures one second apart are byte-identical.
6. **Sketch is cheap.** With `sketch` applied, no render-target or post work is submitted (counters), and frame time in the Large capture is lower than `strategic` (recorded, not gated).
7. **Capture set.** `cow.map.capture` for each preset on the fixed world and camera produces four PNGs; the strategic one matches the approved prototype look (user sign-off, shared with Feature 1 AC 7).
8. **Save-as.** `style.save {name}` writes a new preset file with only the diff against the currently applied preset and `basedOn` set to it; loading it reproduces the saved style (hash equal).
9. **Reload.** Editing a preset file in a dev launch updates the next apply; applying a preset does not write any file.
10. **Observability.** Log channel `MAP`: info on apply with the resolved chain, warnings on unknown or clamped keys; metric `map.preset_applied` (count) and the current preset name as a status field.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| SM-005 | Tooling ships; tuned values persist as parameter files | Presets are the persisted tuning |
| TMAP-006 | No prop boil or frequency-varying sway | No preset enables prop boil; the setting does not exist |
| GDD (AestheticStyleCore) | Look rules: ink, hatch, grain, no bloom or lens flare | Presets only vary presentation within those rules; none enables bloom or flare (not implemented) |
| Feedback: quick and dirty perf | Ballpark only; no benchmark rigs | AC 6 records, does not gate |
| AD-001 | Engine changes need sign-off | None: CoW-Shared data and a small resolver |

## Open Design Questions

1. **Are four presets right?** `sketch` is speculative (no one has asked for a low-end mode). It costs almost nothing, but could be dropped until a real low-end target exists. Keep or drop?
2. **Does `apply` reset to defaults first?** AC 4 says yes (idempotent). The alternative is layering on the current state, which is more flexible for tweaking a preset live but makes results depend on history. Proposed: reset first, matching preset semantics in most tools.
3. **Time of day.** The GDD says seasonal lighting only (no time of day). Presets don't include one; if a day/night cycle is ever added it is a new feature, not a preset value.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Preset schema, sidecar, `PresetLibrary` load/resolve, cycles | AC 1, 2, 3 | Not Started | sonnet | |
| 2 | Shipped four preset files | AC 1, 4 | Not Started | haiku | values from the table; user reviews |
| 3 | Apply semantics and command hook | AC 4 | Not Started | sonnet | with Feature 4 |
| 4 | Flat and sketch behaviour tests | AC 5, 6 | Not Started | sonnet | |
| 5 | Save-as | AC 8, 9 | Not Started | sonnet | |
| 6 | Capture set and sign-off | AC 7 | Not Started | sonnet | user signs off |
| 7 | Observability | AC 10 | Not Started | haiku | |
