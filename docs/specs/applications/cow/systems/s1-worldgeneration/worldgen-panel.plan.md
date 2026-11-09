# Plan: S-1 World Generation panel (system plan task 9)

**Spec:** @f1-debug-host-integration.md
**System plan:** @worldgenerationpipeline.plan.md (task 9)
**Mockup:** @s1-editor-mockup.html (approved 2026-11-09, task 8c)
**Status:** In Progress

Scope: the terrain part of Feature 6: `WorldGenDebugDomain` and its panel on top of the task 8b `cow.worldgen.*` commands. Seed, `WorldShapeParams` sliders, presets load/save, map size, stage timings, View section (colour mode and drawer toggles as state only; rendering is task 10), Cell Inspector with stage values. Validation, Cancel, Export/Import are stubs (tasks 13-16). Biome/region/site fields are stubs (tasks 11-12).

## Decisions (2026-11-09)

- **Cross-PU route.** The panel is read on Main, the world lives on SimPU, so panel state crosses by streams only (no cross-PU module read, as `GameSpeedDebugDomain` does). The domain itself runs on SimPU; Main registers a `ViewerStateAdapter` (Dia, generic) fed by a snapshot FrameStream.
- **Shared bridge/relay in CluicheGameBaseline, not Dia and not copied.** `DomainStateBridgeModule` (Main) and `DomainStateRelayModule` (Sim) generalise CluicheTest's `CellLayersStateBridgeModule` / `CellLayersStateRelayModule`: `Attach(channelName, domain)` / `Detach(channelName)` over a fixed pool of 4 named channels (streams `<name>State`, `<name>Demand`, `<name>Channel`). CluicheTest's two modules are replaced by them (AD-001 sign-off for that CluicheTest edit given 2026-11-09). Dia is unchanged: it already has `ViewerStateAdapter` / `ViewerStateSnapshotPublisher`.
- **Widgets are CoW-only.** New widgets go in CoW's `Cluiche/Assets/CoW/Global/Presentation/UI/debug-panel.html`. Copying them to CluicheTest's panel needs separate sign-off.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `DomainStateBridgeModule` + `DomainStateRelayModule` in CluicheGameBaseline: 4 named channels, Attach/Detach, same behaviour as the CellLayers pair (demand-armed publish, one demand event per Main tick, health registration). Module docs, vcxproj, DIA_MODULE | TDD: attach two domains on distinct channels, snapshots do not cross; unknown/over-pool channel returns nullptr; Detach stops publishing | Done | sonnet | Mirror the CellLayers modules; do not change them yet | Done 2026-11-09: 6/6 DomainStateModules tests pass; Attach(domain) takes first free of 4 slots (no names). No module .md exists in CluicheGameBaseline, so none added. |
| 2 | Migrate CluicheTest CellLayers viewer + history onto the new modules; delete `CellLayersStateBridgeModule` / `CellLayersStateRelayModule`; update stage manifests, vcxproj, docs | Existing celllayers GoogleTests and the celllayers E2E pass unchanged | Done | sonnet | Mechanical but wide. No behaviour change allowed | 2026-11-09: CellLayers gtests 51+52 pass, celllayers E2E 11/11; registeredtypes.diaschema regenerated, version 1.42 to 2.0 (type removal is breaking). History domain now health-registered. |
| 3 | `WorldGenDebugDomain` (CoW, SimPU, DIA_DEBUG): `GetJSONState` from `WorldGenSession` (controls built from the `params` ranges, status, stage timings, cell inspector, presets, size); `OnCommand` routes to the same handlers the `cow.worldgen.*` commands use | TDD: JSON state matches `params`/`status`/`cell` query results; each command changes the session as the DiaAPI command does; unknown key ignored | Pending | sonnet | SD-S1-002: no second path. Find how to call the sim-hosted handlers from the domain without duplicating logic |
| 4 | `debug-panel.html` (CoW copy): slider throttled send on `input`, plus button, select, text/number input, checkbox, segmented buttons, collapsible section, key/value grid, stage-timing bars, legend swatches. Built to the mockup | Vitest or the panel's existing test harness if present; else a mockup-vs-panel manual check recorded in notes | Pending | sonnet | CoW file only |
| 5 | Wire into the S-1 stage: add bridge/relay to the stage manifest, register the domain via the relay in `S1WorldGenerationTestbedStageModule`, unregister on stop | Stage boots; `debug.domain_state` returns the World Generation card | Pending | sonnet | Check `dia validate manifest` |
| 6 | Verify: slider drag → preview update end to end; gap audit, edge tests; one scoped gtest run (shards >= 2); `dia run cluichetest` celllayers E2E; observation scan | Feature 6 ACs for the terrain sections (AC1, AC2, AC8-label parts, AC9, AC11, AC14) | Pending | sonnet | Final verification task per cadence rule |
