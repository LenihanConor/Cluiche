# System Spec: V-1 Tile Rendering

**Parent:** @docs/specs/applications/cow/cow.md
**Testbed:** required (this is the testbed)
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/V1_TileRendering.md` (DesignDocs repo, Locked 2026-10-11, re-locked 2026-10-20 and 2026-11-17)
**System under test:** @docs/specs/applications/cow/systems/strategicmap/strategicmap.md (the Strategic Map owns the tools; this testbed hosts them), drawn by @docs/specs/applications/dia/systems/diaterrainmap3d/diaterrainmap3d.md
**Research:** docs/research/tile_renderin_art_pipeline/summary.md (prototype: `lookdev/`)

**Gameplay Domains:** rendering, debug

## Summary

V-1 is the testbed for CoW's map visuals. It loads an S-1 world and renders it through the Strategic Map. It proves that every simulation channel produces a distinct, readable change in the map, at every zoom level, at Large scale, and at frame rate.

It is also the **look-dev tool**: the in-game version of the browser prototype the user approved on 2026-11-20. Every slider, toggle, data view and stand-in driver from the prototype is a panel control backed by a DiaAPI command.

```
Simulation layers  S-1 (built) · A4 · A1 · A3 · A5     publish per-cell state
Map                Strategic Map (CoW) → DiaTerrainMap3D   renders it
Testbed            V-1                                    ← this: stage, stand-ins, panel, checks
```

**Stand-in drivers.** A1, A3, A4 and A5 aren't built when V-1 is (build order #18 puts it after A4/A1/A3, but A5 and Population Tokens may lag). V-1 therefore owns stand-in drivers that produce the same per-cell channels:
- **Ecology:** health, regional variation, time-lapse drift.
- **Weather:** storm fronts drifting with the wind, writing per-cell cloud, precipitation, snow fraction by cell temperature, and gust.
- **Water:** the "living year" sets lake level, river flow state, flood and ice from the season.
- **Wildfire:** a CA with fuel from biome × health, wind-biased spread, rain douse, and a scar that heals over about 3 game-years.
- **Herds and birds.**

Each driver has a **Live** switch that hands its channel to the real system once that system exists (`IMapChannelSource`, Strategic Map Feature 2).

## Prototype Reference

The look-dev prototype (`docs/research/tile_renderin_art_pipeline/lookdev/index.html`) is V-1's approved mockup. The user signed off the direction, scope and look on 2026-11-20, so the panel mockup gate for Feature 3 is cleared.

| Prototype code | Feature |
|---|---|
| `index.html` + `app.js` `UI` (control set, sections, data views, reference overlay, screenshot) | 3 Look-Dev Panel |
| `weather.js` `WeatherSim` (storm fronts) | 2 Stand-in Drivers (weather) |
| `alive.js` `FireSim`, `Fauna`, `YearClock.drivers`; `app.js` `healthOf` and time-lapse drift | 2 Stand-in Drivers (fire, fauna, water, ecology) |
| `tools/shot.sh` (headless Edge capture with `#key=value` state) | 4 Checks and Captures |
| `tools/convert_golden.py` (S-1 golden dumps → page worlds) | 1 Stage and World (test worlds) |

## Goals

1. **Prove legibility (GDD Tests 1–6).** At strategic and regional zoom the map makes these distinct:
   - biomes and ecology states;
   - seasons, weather, succession and water states;
   - continuous zoom from the whole world to a single cell.
2. **Prove scale (GDD Tests 7–9).** Large (104×64) at 30 fps or better with everything on: full zoom range, and channel updates kept in sync with the simulation.
3. **Look-dev tool.** The user can tune the look live and save it as the style parameter file, using the prototype's control set.
4. **Ready for live data.** Each stand-in can be swapped for the real system without any change to rendering.

## Non-Goals

- Simulation correctness: that is the job of S-2.5, S-3 and S-3.5. The stand-ins are plausible, not accurate.
- Overlays, fog of war and icons: V-2, V-3 and V-4.
- Pixel-exact regression: captures are taste checks, and correctness is asserted on data (feedback: cell data over pixels).

## Modes

1. **Correctness (Standard 80×52):** the GDD Tests 1–6, plus every channel driven through its range.
2. **Performance (Large 104×64):** the GDD Tests 7–9, with everything on, plus bake time.
3. **Look-dev (any size):** the full panel and style save. This is the default mode.

## Features

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| 1 | Stage and World | `.diastage` from Boot (AD-005), S-1 world via the hand-off (preset/seed/size or `world_file`), mode selector, camera presets | TBD | Draft |
| 2 | Stand-in Drivers | Ecology, storm-front weather, living-year water, wildfire CA, herds/birds, year clock and time-lapse; each an `IMapChannelSource` with a Live switch | TBD | Draft |
| 3 | Look-Dev Panel | Prototype control set (world/camera, season and year, ecology, water, weather, time and life, presentation, style, data views) as `cow.map.*` / `v1.*` commands; style save/load; capture button. Mockup: the lookdev prototype, approved 2026-11-20 | TBD | Draft |
| 4 | Checks and Captures | GDD Tests 1–9 as checks: channel→parameter assertions on data (Tests 1–6), frame time and bake time (7–9); a fixed capture set (one per channel and preset) rendered headless for the user's taste review | TBD | Draft |

Build order: 1 → 2 → 3 → 4. Feature 1 needs Strategic Map Features 1, 2 and 4.

## GDD Deviations (fold-back owed)

These depart from the GDD as written. The fold-back text goes to the GDD session.

1. **No tile atlas.**
   - Tests 1–5 judge field-driven rendering, not 180 tile variants.
   - Tile counts and atlas packing (V-1 "Tile Variants Required", TileRenderingPipeline §1–4) are dropped.
2. **Camera.** The map is a tilted 3D heightfield, not "camera controls beyond zoom out of scope".
   - Tilt and orbit are in scope as the strategic camera.
3. **Motion and animation are in scope.** This reverses "static tile appearance only":
   - particles, sway and flowing rivers;
   - cross-fading seasons and succession fades;
   - fire, herds and birds.
4. **Weather is per cell.** This matches TileRenderingPipeline §7.2. Snow is a continuous pack, not `snow_cover: bool`.
5. **Mountains are relief, not peak tiles.**
6. **Naming.** The biome set, ecology state names and season names follow the code and palette doc, not V-1's lists (Strategic Map ODQ 3).

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| V1-001 | V-1 hosts the Strategic Map's tools; it owns only the stage, modes, stand-in drivers, panel layout and checks | Accepted | Yes |
| V1-002 | Stand-in drivers live in V-1 (testbed), behind `IMapChannelSource` with a Live switch; they are never used by the game build | Accepted | Yes |
| V1-003 | Correctness is asserted on data; captures are the user's taste check, one set per feature | Accepted | Yes |
| V1-004 | Specced now, built at CoW build order #18 (user decision 2026-11-20) | Accepted | No |

## Inherited Binding Decisions

| ID | Decision | How it constrains V-1 |
|---|---|---|
| PD-010 | `.diastage` metadata | The stage is a `.diastage`; modes are launch parameters, not separate stages |
| AD-001 | Engine changes need sign-off | Any Dia change raised by V-1 goes through DiaTerrainMap3D's specs |
| AD-003 | GDD owns intent | The test list comes from V1_TileRendering.md; the deviations above are fold-back |
| AD-005 | One stage per testbed | One stage; Correctness, Performance and Look-dev are panel modes |
| SM-003..005 | Strategic Map | Per-cell weather; fire and herds are stand-ins only; tooling is DiaAPI commands |

## Open Design Questions

1. **Stand-in fidelity vs throwaway.** The storm-front and fire stand-ins were good enough to judge looks. Should they stay in V-1 for good, as a visual sandbox once A4 and A1 are live? Or be deleted as each live system arrives?
2. **Reference overlay.** The prototype overlays GDD Visual Bible images (DesignDocs repo) for comparison. Should V-1 load these from a configured DesignDocs path, or ship copies? Shipping copies duplicates art across repos.

## Status

`Approved` (2026-11-20, user). The research session served as the interview, with direction, scope and tooling decided by the user. Built at #18.
