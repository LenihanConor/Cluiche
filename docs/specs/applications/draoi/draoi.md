# Application Spec: Draoi

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose
**Draoi** (Irish "druid", a mediator) is the layer between the Dia engine and the applications: reusable, content-free gameplay libraries that any game can consume but that are too opinionated to be engine. Like Dia, it is organised as an "application" for spec purposes only; it ships libraries, not an executable.

Dependencies point down only: Applications → Draoi → Dia. See PD-011 to PD-014 in the platform spec for the binding rules.

## Scope
- Scoped by **role, not genre**. A platformer character controller and a hex-world generator are both valid residents.
- Genre kits are separate opt-in packages; a game links only what it uses.
- **Admission test:** a package needs two consumers, or a credible second one. Nothing is extracted on speculation.
- **Mechanism, not content:** Draoi offers hooks, registries and generators. Species tables, parameter data, site guarantees and the like stay in the game.
- **Plain libraries:** no `SimModule`, no processing-unit affinity, no bus, save or debug-domain wiring (PD-012). Test helpers ship in `<Package>/Testing/`.
- **Sims are scheduler-agnostic:** state in, state out, safe to run off-thread, no knowledge of any step chain (PD-013). The application owns execution order and writes the stage adapter.

## Naming
Packages are prefixed `Draoi` and named by what they hang off, e.g. `DraoiHexWorld`, `DraoiHexWorldWeather`. Packages that are not tied to hexes carry no infix (`DraoiGameSpeed`).

## Planned Packages
Nothing is built yet. Extraction order follows the CoW audit of 2026-11-20.

| Package | Source | Notes |
|---------|--------|-------|
| DraoiGameSpeed | CoW `Shared/GameSpeedController.h` | Day base tick, time scale, presets |
| DraoiHexWorld | CoW `Shared/World*`, `Shared/WorldGeneration/*` | Needs `WorldGenSession` to take its guarantee set as injected configuration first; `SiteGuarantees` (Hearth/Nexus/Throne) stays in CoW |
| DraoiHexWorldWeather | A4 Weather spec | Pure sim; stage adapter lives in CoW |
| DraoiHexWorldHydrology | A5 Hydrology spec | Pure sim; stage adapter lives in CoW |
| DraoiHexWorldEcology | A1 Ecology spec | Pure sim; stage adapter lives in CoW |

The step chain (`IWorldStepStage`, `WorldStepChain`, config, listener) is not a Draoi package. It was audited as pure mechanism with no world knowledge, so it moves into Dia's `DiaDeferredStep` under generic names, pending its own feature spec (Dia placement needs sign-off under the escalation rule).

## Open Design Questions
1. ~~**World-sim kit split.**~~ **Resolved 2026-11-20** in the World-Sim Kit's "Draoi layer amendment" (@docs/specs/applications/cow/systems/worldstep/worldsim-kit.md): stage base, restore sequence, commands and names stay in CoW; `WorldCellFields`, `CellSelectorSet` and the codec data half go to DraoiHexWorld; sims expose `Step` plus a thread-neutral `Capture`/`Run`/`Commit`; packages depend one way (HexWorld <- Weather <- Hydrology <- Ecology) with feedback as optional, defaulted input arrays filled at `Capture`; the bus carries notifications only; `IParallelExecutor` moves to DiaThreading. Still owed: A1, A4 and A5 amendments, and a kit plan revision.
2. **CluicheGameBaseline overlap.** It is already referenced by CoW and may be a partial version of this layer. Not yet inspected.
3. **Engine-grade plumbing.** `RandomStreamModule`, `SaveLoadModule` and `SnapshotStoreAdapter` look like engine rather than gameplay; any move to Dia needs sign-off.

## Status
Draft. No packages exist.
