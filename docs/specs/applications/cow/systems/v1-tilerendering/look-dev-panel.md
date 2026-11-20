# Feature Spec: V-1 Look-Dev Panel

## Parent System
@docs/specs/applications/cow/systems/v1-tilerendering/v1-tilerendering.md (Feature 3)

**Gameplay Domains:** rendering, debug, tools
**Depends on:** V-1 Stage and World, Stand-in Drivers; Strategic Map Feature 4 (`cow.map.*`)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Mockup:** the approved look-dev prototype, `docs/research/tile_renderin_art_pipeline/lookdev/index.html` (user sign-off 2026-11-20). The mockup gate is cleared.
**Prototype:** `lookdev/js/app.js` (`UI` spec, `buildUI`, reference overlay), `lookdev/index.html` (layout, thumbnails, screenshot)

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

The in-game control panel for the look-dev tooling: every slider, toggle, dropdown and button from the browser prototype, in the stage's panel, each posting a DiaAPI command (V1-001, SM-005). It is also the way the user tunes the look and saves it (presets and the style file).

The panel contains no logic. It builds itself from the command and parameter schemas and posts commands (feedback: UI separate from business logic; editor actions via DiaAPI).

## Problem

- The prototype's panel was hand-coded HTML in a throwaway page. The same control set must exist in the real stage, driving the real renderer, with the same behaviour.
- Controls must not diverge from commands. A panel that does things commands cannot is untestable and unreachable from E2E and CLI.
- Tuned values must be savable from the panel (style file, named presets), not only through commands.

## Goals

1. **One control per command argument**, grouped like the prototype: World and camera; Season and year; Ecology; Water; Weather; Time and life; Presentation; Style; Data views; Presets and files.
2. **Built from schemas**: ranges, defaults, descriptions and tooltips come from the style sidecar and command help (DiaValueSchema), so the panel and CLI agree.
3. **Status area**: world, size, cell count; instance, particle and tile counts; frame and bake times; current preset and style hash.
4. **File actions**: Save style, Save as preset, Reload, Reset to defaults, Capture screenshot (writes to `out/CoW/v1/`).
5. **Stand-in controls**: each driver's parameters and its Live switch, showing whether the real source is available.
6. **Reference overlay** (optional): thumbnails of GDD Visual Bible images to overlay for comparison (ODQ 2).
7. **Mode awareness**: sections shown depend on the mode (Feature 1).
8. **The same panel mechanism as S-1 and S-2.5** (a card on the stage's debug panel, state published as `debug.domain_state`, actions as `debug.panel_command`), not a new UI stack.

## Non-Goals

- A new UI framework or editor plugin. This is a stage panel like S-1's.
- Game-facing UI (settings screens).
- Editing visuals data (palettes, prop rules): files only, hot reloaded.
- Undo/redo for slider changes (Reset and Save cover it; see ODQ 3).

## Design

### Control table

Every row is one command; the panel has no other behaviour. Sections follow the prototype:

| Section | Controls | Command |
|---|---|---|
| World and camera | world, view (split dropped: the stage is 3D; a 2D comparison is not shipped), tilt, yaw, height scale, camera presets, store/recall | `v1.world.set`, `cow.map.camera.set`, `cow.map.style.set camera.*`, `v1.camera.store/recall` |
| Season and year | jump to season, day of year, year clock running, days per second, living year, season grade strength | `v1.time.scrub`, `v1.time.run`, `v1.time.speed`, `v1.driver.set water.living`, `cow.map.style.set season.*` |
| Ecology | health, regional variation, prop density, vegetation on | `v1.driver.set ecology.*`, `cow.map.layer.toggle props`, `cow.map.style.set props.density` |
| Water | river flow state, lake level, flood, ice | `v1.driver.set water.*` |
| Weather | storm count, drift with wind, intensity, precipitation type, background cloud, cloud shadows, storm clouds, wind direction/strength, snow pack | `v1.driver.set weather.*`, `cow.map.layer.toggle` |
| Time and life | time-lapse, auto wildfires, ignite at view centre, herds and birds | `v1.driver.set`, `v1.driver.fire.ignite {cell|view}` |
| Presentation | post pass, tilt-shift (on, strength, focus), haze (on, strength), sky, silhouettes, form lines, contours, boil | `cow.map.style.set post.*`, `cow.map.layer.toggle` |
| Style | ink (on, weight), hatching, ground marks, toon, hillshade, zone ambient, paper grain, hex grid | `cow.map.style.set ink.*, hatch.*, shade.*` |
| Data views | off, class, elevation, condition, zone, temperature, margin, tiles | `cow.map.view.data` |
| Presets and files | preset dropdown, apply, save, save as, reload, reset, screenshot | `cow.map.preset.apply`, `cow.map.style.save/load/reload`, `cow.map.capture` |
| Stand-ins | Live switches, availability | `v1.driver.live` |

### Panel state

Published as one `debug.domain_state` payload: current values (from `get` commands), schema-derived metadata (labels, ranges, tooltips, enums), mode, status counters. Updated on change and at 4 Hz for counters. Size stays within the 32 KB slot (S-1's rule); the schema metadata is sent once.

### Mockup

The approved HTML prototype is the visual mockup. The panel mirrors its section order and labels; the 2D-versus-3D split view is dropped because 3D won.

## Acceptance Criteria

1. **One command per control.** A table test enumerates every control and asserts a registered command and argument exists for it; no control without a command and no unregistered command referenced.
2. **Schema driven.** Ranges, defaults and descriptions in the panel state equal the style sidecar and command help byte for byte; changing a sidecar range changes the panel after a reload with no panel code change.
3. **Set and reflect.** Moving a control posts the command; the next state shows the new value; an out-of-range value from a script is clamped and the panel shows the clamped value.
4. **No logic.** The panel module has no dependency on the renderer or drivers beyond command invocation (dependency check), and the same sequence of commands issued from E2E produces the same map hash as the panel.
5. **Files.** Save style writes the diff; Save as preset creates the preset (Strategic Map Feature 5 AC 8); Reload picks up an external edit; Reset restores defaults.
6. **Screenshot.** The screenshot button writes a PNG to `out/CoW/v1/` and the status line shows the path; the file matches `cow.map.capture` with the same arguments.
7. **Mode awareness.** Correctness mode hides the file-editing controls; Look-dev shows all sections; Performance shows the status area and the capture set only.
8. **Status.** Frame ms, bake ms, instances, particles and tiles resident are within the panel state and refresh at 4 Hz without dropping the frame rate (the panel update is under 0.5 ms per frame on the dev machine, ballpark).
9. **Stand-in availability.** A driver whose real source is not registered shows its Live switch disabled with the reason; registering the source enables it without restarting.
10. **Payload size.** The panel state stays under the 32 KB slot for Large worlds.
11. **User sign-off.** The user reviews the panel in the running stage against the prototype and signs off section by section (mockup parity).
12. **Observability.** Log channel `V1`; each file action logs its path; command errors surface as a status line, never silently.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| V1-001 / SM-005 | Tooling is DiaAPI commands; the panel calls the same commands | AC 1, 4 |
| Feedback: UI separate from business logic | Typed command model first | The panel is generated from schemas |
| Feedback: editor actions via DiaAPI | Every action and query is a command | AC 1 |
| Feedback: HTML mockups for editor features | Mockup gate before building | The approved prototype is the mockup (AC 11) |
| Feedback: tool tiers | Editor offline, Inspector live game, VisualDebugger in-game overlay | This is an **in-game stage panel** (like S-1/S-2.5), not an Editor plugin or Inspector view |
| PD-001 | StringCRC | Command and domain ids |

## Open Design Questions

1. **Which panel stack?** S-1 and S-2.5 use the same domain-state + panel-command mechanism with a web UI card. Confirm V-1 reuses it, including the build step (`dia pipeline` UI build and copy) required for editor-hosted web UIs. If V-1's panel can be a plain DiaDebugPanel (ImGui) instead, it is cheaper but looks different from the other testbeds.
2. **Reference overlay.** The prototype loaded the GDD Visual Bible images from the DesignDocs repo by absolute path. The panel can't assume that path. Options: a configured reference folder (dev only), or ship a handful of references with the stage (the user's own approved images; check licensing, they are AI-generated project art). Proposed: configured folder, optional.
3. **Undo.** Reset-to-defaults and Save cover most needs. If tuning sessions want per-slider undo, the command log can be replayed (a future feature).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Control table → commands; table test | AC 1 | Not Started | sonnet | |
| 2 | Panel state publisher from schemas | AC 2, 10 | Not Started | sonnet | |
| 3 | Panel UI (sections, controls, tooltips) following the prototype | AC 3, 7, 11 | Not Started | sonnet | build via `dia pipeline` |
| 4 | File actions and screenshot | AC 5, 6 | Not Started | sonnet | |
| 5 | Stand-in section and availability | AC 9 | Not Started | sonnet | |
| 6 | Status area and perf check | AC 8 | Not Started | sonnet | |
| 7 | Command-vs-panel equivalence test, observability | AC 4, 12 | Not Started | sonnet | |
