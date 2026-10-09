# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

One backlog for every project. The **Project** column says whose work it is: `Dia` (engine, including DiaCLI and editors), `CluicheTest`, `CoW`, or `Baseline` (CluicheGameBaseline). Filter with `/backlog <project>`. CoW design intent lives in the GDD (`C:\GitHub\DesignDocs\COW`); CoW specs are under [specs/applications/cow/](specs/applications/cow/cow.md). An engine capability a game needs is a `Dia` row with "CoW needs" in the notes, not a separate list.

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| Project | System | Features | Depends On |
|---------|--------|----------|------------|
| ~~CoW~~ | F-1: BootMenuModule promotion + `StageStatusModule` (7 tasks) | [spec](specs/applications/cow/systems/f1-harness/bootmenu-promotion.md) / [plan](specs/applications/cow/systems/f1-harness/bootmenu-promotion.plan.md). Edits Baseline + CluicheTest (signed off) | Nothing |
| ~~Dia~~ | F-1: DiaCLI `cow` target + `scaffold sln-add --root` (skeleton B1, B2) | [spec](specs/applications/cow/systems/f1-harness/cow-app-skeleton.md) / [plan](specs/applications/cow/systems/f1-harness/cow-app-skeleton.plan.md). DiaCLI + `pipeline.toml` (signed off) | Nothing; can run alongside the promotion |
| ~~Dia~~ | F-1: DiaCLI data-driven target registry (scaffold-app task 1); makes the `cow` target toml-only | [scaffold-app.md](specs/applications/dia/systems/diacli/scaffold-app.md). DiaCLI refactor, signed off; existing targets must resolve unchanged | Nothing |
| ~~CoW~~ | F-1: CoW app skeleton (B3 to B7: project, manifests, `FrontEnd` stub, `Main.cpp`, verify) | Same spec/plan as the `cow` target row | Promotion complete |
| ~~CoW~~ | F-1: Boot choice (Play entry only) | [boot-choice.md](specs/applications/cow/systems/f1-harness/boot-choice.md) | CoW app skeleton (verified in its B6) |
| ~~Dia~~ | F-1: `dia scaffold stage --app` (system plan task 6, TDD) | [f1-harness.plan.md](specs/applications/cow/systems/f1-harness/f1-harness.plan.md). Signed off | CoW app skeleton ✅ (built) |
| ~~Dia~~ | `dia scaffold app` (templates, command, `ScratchApp` boot proof, docs) | [scaffold-app.md](specs/applications/dia/systems/diacli/scaffold-app.md) tasks 2 to 5 | Registry refactor done; boot proof needs the CoW skeleton |
| ~~CoW~~ | `new-cow-stage` skill | [new-cow-stage-skill.md](specs/applications/cow/systems/f1-harness/new-cow-stage-skill.md) | `scaffold stage --app` (next); proving it needs a locked testbed design | 
| Dia | ~~DiaAttribute~~ | System spec + all 6 feature specs Approved ✅: core, conditional-modifiers, change-notifications, accessor-bridge, visual-debugger, save-serialization. Data-driven gameplay attribute/stat framework — same idiom as DiaEconomy (StringCRC, JSON schema, DiaCondition-gated modifiers, Observer events), no shared code. Plan: [diaattribute.plan.md](specs/applications/dia/systems/diaattribute/diaattribute.plan.md) — 6 tasks, 3 phases. Entry point: task 1 (core). Task 1 has an open design question (component attachment mechanism) to resolve before `AttributeSetComponent`; task 4 (accessor-bridge) is the highest-uncertainty task — `DiaCondition`'s `ConditionRegistry` has no unregister method, may need an upstream fix. Deferred: Archetype/Instance Layering. Parked: Dependency-Graph Derived Attributes, Non-Numeric Typed Properties. Research: [gameplay_attribut_stat_system/summary.md](research/gameplay_attribut_stat_system/summary.md). | DiaCore ✅, diaentitytemplate ✅, DiaCondition ✅ (optional), DiaSaveGame ✅ (optional) |

---

## Spec Work Needed (Draft or unset — review/approve before building)

### Other Spec Work

| Project | Item | Spec | What's needed |
|---------|------|------|---------------|
| CoW | F-1: Bus live verification | — | Feature spec: live-stage proof of Primary and Reaction delivery (Test 4, 10,000 messages at 1:1). Depends on CoW app skeleton |
| CoW / Dia | F-1: Bus traffic recorder | — | Feature spec: versioned per-message records (tick, type id, payload), Debug-only. **Placement undecided:** new Dia capability in DiaMessageBus, or a CoW-Shared subscriber that avoids a Dia change. Depends on CoW app skeleton, bus live verification |
| CoW | F-1: Debug command / inspector host | — | Feature spec: shared host for debug commands and inspector panels, proven with a stand-in command and panel. Depends on CoW app skeleton |
| CoW | F-2 Core Runtime | — | `/spec-system`: world clock, RNG determinism, save/load round-trip, data loading and band resolver. Depends on F-1. GDD `F2_CoreRuntime.md` drafted, not locked |
| CoW | S-1 World | — | `/spec-system`: seeded generation; isolated-cell and full-world modes. Depends on F-2 |
| CoW | S-2 Dragon | — | `/spec-system`: isolated and live modes; pathfinding. Depends on S-1, F-2 |
| CoW | S-3 Lineage & Hearth | — | `/spec-system`. Depends on S-2 |
| CoW | S-4 Dynasty | — | `/spec-system`: heaviest testbed; command may split off. Depends on S-3 |
| CoW | S-5 Endurance | — | `/spec-system`: stress load, long-run history and save paging. Depends on S-1 to S-4 |
| CoW | V-1 Map Visuals | — | `/spec-system`: tiles, overlays, fog of war, camera on static data. Depends on F-1 |
| CoW | V-2 Dragon & Nest Visuals | — | `/spec-system`: genome-to-visual, lifecycle stages, nest diorama. Depends on V-1 |
| CoW | V-3 UI & Interaction | — | `/spec-system`: UI kit, input, lineage view, event readability (canned data, so needs the bus recorder). Depends on F-1, bus recorder |
| CoW | A-1 Audio | — | `/spec-system`: ambient soundscape, notification sounds, time-speed-aware audio. Depends on the Dia audio module row below |
| Dia | Audio module | — | Needs a Dia spec first; Dia has no audio module. CoW needs it for A-1 |
| Dia | Multi-stream RNG in `DiaMaths/Core/Random.h` | — | Today one global Mersenne Twister per thread. CoW F-2 needs 8 independent, individually serializable streams (WLD, A1, B4, B6, C1, C2, B5, C4). New engine capability; needs a spec. F-2 draft not locked, so treat the stream list as likely-stable |
| CoW | G-1 First Playable | — | `/spec-system`: real front end and every testbed's visuals toggled on; replaces the `FrontEnd` stub. Depends on all testbeds |
| Dia | RenderTestPlugin (CluicheEditor) | — | Needs `/spec-system` — visual debugger panel: wipe slider, region grid, expectation authoring, AI triage panel, render targets. DiaRenderTest CLI Pipeline ✅ unblocked. Mockup: [render_test_debugger_mockup.html](research/render_offline_test/render_test_debugger_mockup.html). Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |

---

## E2E Orchestration Stack

Architecture redesigned 2026-05-20. Source of truth: **[docs/research/e2e_testing/design-decisions.md](research/e2e_testing/design-decisions.md)**.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| DiaRig3D system | feature spec exists (`skeleton-and-pose.md`) | Needs `/spec-system` — Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`, `Rig3DAsset`. Mirrors DiaRig2D. |
| DiaAnimation3D system | feature spec exists (`clip-and-player.md`) | Needs `/spec-system` — AnimationClip3D, ClipPlayer3D, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. glTF animation import is build-time only. |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |

---

---

## Loose Ends (non-spec items)

| Project | Item | Notes |
|---------|------|-------|
| Dia | RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Dia | Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| Dia | ~~DiaDebugDraw.vcxproj duplicate-compile hack~~ | `DiaDebugDraw.vcxproj` still duplicate-compiles `DebugLayerManager.cpp` et al. out of `DiaVisualDebugger` via relative path (unrelated to layer violations — `dia check arch` is clean). Tried removing the duplicate entries once (see [arch_layer_violations/plan.md](refactors/arch_layer_violations/plan.md) task 9): broke the real link for `GoogleTests.exe` with 59 unresolved externals, even though `DiaDebugDraw.lib` isn't referenced anywhere in `GoogleTests.vcxproj` (not in `ProjectReference`, not in `AdditionalDependencies`) while `DiaVisualDebugger.lib` — which should contain these exact symbols — is explicitly linked. Reverted immediately, confirmed clean. Working theory, unconfirmed: a shared intermediate-object-path collision between the two projects compiling the same source files under MSBuild's default parallel build. Needs real investigation (check `IntDir` settings, try a forced serial rebuild) before attempting again. |
| CluicheEditor | Uncommitted pre-existing editor UI `dist/` bundles + untracked `External/SDL3` | Found sitting in the working tree during the 2026-10-07/08 DiaConsole run-tabs session, predating that work: modified `dist/` build output under CluicheEditor, DiaChatPlugin, DiaApplicationFlowEditor/Inspector, DiaBlackboardInspector, DiaEntityInspector, DiaPipelineEditor, DiaPythonConsoleEditor, DiaSchemaBrowser; plus an untracked `External/SDL3` directory. Left alone — unclear whether these are in-progress work, stale rebuild output, or should be committed/discarded. Needs owner triage. |
| Dia | Flaky `test_build_end_to_end_produces_a_real_exe` | `Dia/DiaCLI/tests/test_dia_console_console_cli.py` — failed once during a full-suite (incl. integration) run, passed cleanly standalone. Repro attempted 2026-10-08: (1) this test + `test_compile_code_builds_googletest` (real MSBuild compile) forced onto concurrent `pytest-xdist` workers via `-n 2`, both passed in 46s; (2) full 1024-test suite incl. all `integration`-marked tests under `-n auto` (64 workers) — note only one test in the whole suite invokes real PyInstaller, so "concurrent PyInstaller contention" was never literally possible, the real risk is PyInstaller's build racing other heavy integration tests (the MSBuild compile, and `test_test_cli_exits_0`'s nested `dia test cli` subprocess) for CPU/disk on `-n auto`'s worker count — all 1024 passed in 251s, no ordering or resource issue surfaced. Still not reproduced on this (64-core) machine; original failure's host/load conditions unknown. No code change made (anti-shotgun-debugging: no repro, no fix). Next step if it recurs: capture the actual pytest output (return code, stderr tail) from the failing run instead of retrying blind. |
| CoW | DiaMessageBus module doc: `status: stub` is stale | Confirm with a fresh `dia run googletest --filter="*MessageBus*"`, then edit `dia.messagebus.architecture.module.md` (approved) |
| Dia | `dia docs spec-scaffold` writes the wrong parent and fails on a relative `--output-dir` | Found while scaffolding CoW F-1; DiaCLI fix, not yet signed off |
| CoW | `new-cow-stage` skill trial run (spec task 2, AC4/AC5) | Run `/new-cow-stage` on the first real testbed once its design is locked in the GDD; fix the skill from what it shows. Skill and command are written |
| CoW | Shared application-bootstrap helper | CluicheTest's `Main.cpp` has generic path-alias and base-dir code CoW copies. Not yet judged for Dia; log it if wanted |
| CoW | Deferred: event replay from file | Until V-3 needs canned event data |
| CoW | Deferred: `reflect` pipeline stage (schema file) | Until the editor needs CoW's schema |
| CoW | Deferred: tablet support, touch input | Later; Dia has no touch input |
| CoW | Deferred: status badges for testbeds | CoW could add a `StageStatusModule` feeder later |

---

## CoW: Features to Be Designed (GDD)

The testbeds and the pieces each absorbs, from `TestbedRoster.md`. Cross a piece out (`~~like this~~`) once its testbed design is locked in the GDD, and make sure its `/spec-system` row exists under Spec Work Needed. Strike a testbed only when all its pieces are.

| # | Testbed | Absorbs | Notes |
|---|---------|---------|-------|
| F-1 | Harness | Boot & Debug Menu · Event Bus & Inspector · Record & Replay | `F1_Harness.md` drafted, challenge pass done; specs exist (see Ready to Build) |
| F-2 | Core Runtime | Clock & Determinism · RNG · Data Loading & Band Resolver · save/load round-trip | `F2_CoreRuntime.md` drafted, challenge pass pending |
| S-1 | World | World Generation · Single Cell (mode) · Living Ecology (mode) | |
| S-2 | Dragon | Dragon Sandbox (isolated mode) · Dragon in the World (live mode) · Pathfinding | |
| S-3 | Lineage & Hearth | Single Hearth · Hearth Economy & Lifecycle · Breeding & Drift | |
| S-4 | Dynasty | Command Chain · Clans & Politics · Apotheosis (late-game preset) | |
| S-5 | Endurance | Swarm & Scale · History & Save | |
| V-1 | Map Visuals | Grid · overlays · fog of war · camera | |
| V-2 | Dragon & Nest Visuals | Dragon Visuals · Nest Diorama | |
| V-3 | UI & Interaction | UI kit · Input · Lineage View · Event Readability | |
| A-1 | Audio | none listed | Dia has no audio module |
| G-1 | First Playable | Front End · every testbed's real-visuals toggle | |
