# CoW Backlog

CoW-specific backlog, derived from the CoW spec tree ([cow.md](cow.md)) and the GDD testbed roster. The main [Cluiche backlog](../../../BACKLOG.md) is for the platform and Dia; this one is only for CoW work. Cross-boundary needs (Dia, CluicheGameBaseline) live in [dia-needs.md](dia-needs.md). When an item is Done, move it to [cow-backlog-history.md](cow-backlog-history.md). The `/cow-backlog` skill shows and updates both files (`dia docs backlog move` handles only the platform backlog).

Design intent for every item lives in the GDD (`C:\GitHub\DesignDocs\COW`, `Output Docs/GDD/10_PrototypeStrategy/TestbedRoster.md`).

---

## Ready to Build (Approved)

| Item | Spec / plan | Depends On |
|------|-------------|------------|
| F-1: BootMenuModule promotion + `StageStatusModule` (7 tasks) | [bootmenu-promotion.md](systems/f1-harness/bootmenu-promotion.md) / [plan](systems/f1-harness/bootmenu-promotion.plan.md) | Nothing. Edits baseline + CluicheTest (signed off) |
| F-1: DiaCLI `cow` target + `scaffold sln-add --root` (skeleton B1, B2) | [cow-app-skeleton.md](systems/f1-harness/cow-app-skeleton.md) / [plan](systems/f1-harness/cow-app-skeleton.plan.md) | Nothing. DiaCLI + `pipeline.toml` (signed off). Can run alongside the promotion |
| F-1: DiaCLI data-driven target registry (scaffold-app task 1); makes the `cow` target toml-only | [scaffold-app.md](../dia/systems/diacli/scaffold-app.md) | Nothing. DiaCLI refactor, signed off; existing targets must resolve unchanged |
| F-1: CoW app skeleton (B3 to B7: project, manifests, `FrontEnd` stub, `Main.cpp`, verify) | same | Promotion complete |
| F-1: Boot choice (Play entry only) | [boot-choice.md](systems/f1-harness/boot-choice.md) | CoW app skeleton (verified in its B6) |
| F-1: `dia scaffold stage --app` (system plan task 6, TDD) | [f1-harness.plan.md](systems/f1-harness/f1-harness.plan.md) | CoW app skeleton built. Signed off |
| `dia scaffold app` (rest of scaffold-app: templates, command, `ScratchApp` boot proof, docs) | [scaffold-app.md](../dia/systems/diacli/scaffold-app.md) (tasks 2 to 5) | Registry refactor; boot proof needs the CoW skeleton |
| `new-cow-stage` skill | [new-cow-stage-skill.md](systems/f1-harness/new-cow-stage-skill.md) | `scaffold stage --app`; proving it needs a locked testbed design |

Order and parallelism: [f1-harness.plan.md](systems/f1-harness/f1-harness.plan.md).

---

## Features to Be Designed (GDD)

The testbeds and the pieces each one absorbs, from `TestbedRoster.md`. The GDD design session designs these; when a piece has a locked testbed design, cross it out here (`~~like this~~`) and add its implementation work to **Spec Work Needed** below. A testbed row is struck only when all its pieces are.

| # | Testbed | Absorbs (one line each, cross out when designed) | Notes |
|---|---------|--------------------------------------------------|-------|
| F-1 | Harness | Boot & Debug Menu · Event Bus & Inspector · Record & Replay | `F1_Harness.md` drafted, challenge pass done; implementation specs exist (see Ready to Build) |
| F-2 | Core Runtime | Clock & Determinism · RNG · Data Loading & Band Resolver · save/load round-trip | |
| S-1 | World Generation | World Generation | `S1_WorldGeneration.md` Locked 2026-10-11; WLD only, no A1/A3 |
| S-2 | Single Cell Ecology | Single Cell (isolated A1 unit test) | `S2_SingleCellEcology.md` Locked 2026-10-11; slider-driven, bypasses event interface |
| S-3 | Living Ecology | Living Ecology (A1+A3 full world, zero dragons) | `S3_LivingEcology.md` Locked 2026-10-11; layered visualization, integration/emergence proof |
| S-4 | Dragon | Dragon Sandbox (isolated mode) · Dragon in the World (live mode) · Pathfinding | |
| S-5 | Lineage & Hearth | Single Hearth · Hearth Economy & Lifecycle · Breeding & Drift | Exactly the dependency graph's B4/B6/C6 bucket |
| S-6 | Dynasty | Command Chain · Clans & Politics · Apotheosis (late-game preset) | |
| S-7 | Endurance | Swarm & Scale · History & Save | Both are long runs and large loads, so they share one testbed |
| V-1 | Map Visuals | Grid · overlays · fog of war · camera | |
| V-2 | Dragon & Nest Visuals | Dragon Visuals · Nest Diorama | |
| V-3 | UI & Interaction | UI kit · Input · Lineage View · Event Readability (canned events first, then live) | |
| A-1 | Audio | none listed | Dia has no audio module |
| G-1 | First Playable | Front End · every testbed's real-visuals toggle switched on | |

---

## Spec Work Needed

| Item | What's needed | Depends On |
|------|---------------|------------|
| WorldClock | System spec **Approved** at @docs/specs/applications/cow/systems/worldclock/worldclock.md. Season counter, speed tiers (PAUSED/NORMAL/FAST/FASTEST), modal-safe pause, time bands (SEASON/YEAR/DECADE/CENTURY), 4 time domains. 4 features (not started). GDD A2_WorldClock Locked 2026-10-09 | DiaSimTime (Done), DiaMessageBus (Done) |
| F-1: Bus traffic recorder | Feature spec: versioned per-message records (tick, type id, payload), Debug-only. **Placement undecided:** new Dia capability in DiaMessageBus or a CoW-Shared subscriber that avoids a Dia change | CoW app skeleton |
| F-1: Debug command / inspector host | Feature spec: shared host for debug commands and inspector panels, proven with a stand-in command and panel | CoW app skeleton |
| E1 Save/Load (CoW implementation) | Feature or system spec: CoW's save/load integration — `serialize_state()`/`restore_state()` contract for WorldClock, RNG streams, dragon data, ecology state. GDD E1_SaveContinue Locked 2026-10-09. Depends on DiaSaveGame (Done) | WorldClock spec |
| F-2 Core Runtime testbed | Testbed stage spec: 6 tests proving WorldClock, RNG determinism (RandomStreamModule ✅), save/load round-trip, band resolver (DiaAttribute Feature 7 ✅). GDD F2_CoreRuntime Locked 2026-10-09 | WorldClock Approved, E1 spec, DiaAttribute Feature 7 (Draft) |
| S-1 World Generation | `/spec-system`: WLD data pipeline only — generation, export/import, timeout/failure, reachability validator. No A1/A3. GDD S1_WorldGeneration Locked 2026-10-11 | F-2 |
| S-2 Single Cell Ecology | `/spec-system`: A1 isolated, slider-driven unit test of tick equations (bypasses event interface). GDD S2_SingleCellEcology Locked 2026-10-11 | F-2 (time controls) |
| S-3 Living Ecology | `/spec-system`: A1+A3 full world fast-forward, zero dragons, layered visualization (population density, pressure fields, event markers, succession, time-series graphs). GDD S3_LivingEcology Locked 2026-10-11 | S-1 (world loader) |
| S-4 Dragon | `/spec-system`: isolated and live modes; pathfinding | S-3, F-2 |
| S-5 Lineage & Hearth | `/spec-system` | S-4 |
| S-6 Dynasty | `/spec-system`: heaviest testbed; command may split off | S-5 |
| S-7 Endurance | `/spec-system`: stress load, long-run history and save paging | S-1 to S-6 |
| V-1 Map Visuals | `/spec-system`: tiles, overlays, fog of war, camera on static data | F-1 |
| V-2 Dragon & Nest Visuals | `/spec-system`: genome-to-visual, lifecycle stages, nest diorama | V-1 |
| V-3 UI & Interaction | `/spec-system`: UI kit, input, lineage view, event readability (uses canned data, so needs the bus recorder) | F-1, bus recorder |
| A-1 Audio | `/spec-system`: ambient soundscape, notification sounds, time-speed-aware audio. Dia has no audio module, so a Dia spec comes first | Dia audio module (see dia-needs.md #5) |
| G-1 First Playable | `/spec-system`: real front end and every testbed's visuals toggled on; replaces the `FrontEnd` stub | All testbeds |

Each testbed is one stage (AD-005); modes and presets are panels inside the stage.

---

## Small Fixes and Tooling

| Item | Notes |
|------|-------|
| DiaMessageBus module doc: `status: stub` is stale | Confirm with a fresh `dia run googletest --filter="*MessageBus*"`, then edit the doc (approved) |
| `dia docs spec-scaffold` writes the wrong parent and fails on a relative `--output-dir` | Found while scaffolding F-1; DiaCLI fix, not yet signed off |
| Shared application-bootstrap helper | CluicheTest's `Main.cpp` has generic path-alias and base-dir code CoW copies. not yet logged in dia-needs.md; log it if wanted |

---

## Deferred / Dropped

| Item | Status |
|------|--------|
| Shipping-build gating (compile the menu out, shipping manifest) | Dropped 2026-10-09 by the user; revisit when a shipping build is planned |
| `--play` command-line flag and DiaCLI argument forwarding | Dropped; the Play entry in the Boot menu replaces it |
| Event replay from file | Deferred until V-3 needs canned event data |
| `reflect` pipeline stage for CoW (schema file) | Omitted until the editor needs CoW's schema |
| Tablet support, touch input | Later; Dia has no touch input |
| Status badges for CoW testbeds | None for now; CoW could add a `StageStatusModule` feeder later |
