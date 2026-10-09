# System Spec: F-1 Harness

**Parent:** @docs/specs/applications/cow/cow.md
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F1_Harness.md` (DesignDocs repo; owns what must be proven, scenarios, metrics, failure conditions — see AD-003)
**Status:** `Approved`

## Summary

The shared boot and debug infrastructure every other CoW testbed launches from: a boot choice between the debug menu and the real front end, a promoted `BootMenuModule` in CluicheGameBaseline, verification of the Primary/Reaction event bus on a live stage, a shared debug-command/inspector host, and a Debug-only recorder that writes bus traffic to a file. Nothing simulation- or visual-specific lives here.

## Problem

CoW needs 12 standalone testbeds launchable from one menu. Dia already has the pattern (CluicheTest `Boot` stage + `BootMenuModule`), but the menu is CluicheTest-private and coupled to `CluicheTest::TestResultsRegistry`. Bus traffic can only be inspected in memory (Debug ring buffer), not after the process exits.

## Goals

1. Dev builds boot to the debug menu; `--play` jumps to a stub front-end stage; shipping builds contain no menu code.
2. `BootMenuModule` lives in CluicheGameBaseline with no CluicheTest types; CluicheTest's ~25 stages behave unchanged through it.
3. Menu lists a game's `Boot` transitions, launches any, and returns cleanly with no state leaking between launches.
4. Confirm the DiaMessageBus Primary/Reaction semantics on a live stage; correct the stale `status: stub` in its module doc.
5. Debug-only file recorder for bus traffic.
6. One shared host for debug commands and inspector panels.

## Non-Goals

1. Event replay from file (deferred until V-3 needs canned data).
2. The real front end's screens (G-1); F-1 only provides a stub destination for `--play`.
3. Touch input on the menu.
4. Any simulation or visual content.

## Features

| Feature | Spec | Status |
|---------|------|--------|
| BootMenuModule promotion + StageStatusModule | @docs/specs/applications/cow/systems/f1-harness/bootmenu-promotion.md | Approved |
| Boot choice (`--play`, dev/shipping gating) | TBD | Draft |
| CoW app skeleton (`CoW.diaapp`, Boot stage, stub front end) + `dia scaffold stage --app` | TBD | Draft |
| Bus live verification (Test 4) | TBD | Draft |
| Bus traffic file recorder (Debug-only) | TBD | Draft |
| Debug command / inspector host | TBD | Draft |

## Architecture

Findings from the code that shape the design:

- `BootMenuModule` (RenderPU) already depends only on baseline types (`DebugUIModule`, `DebugServerHostModule`, `RenderToSimNavRequest`) plus `CluicheTest::TestResultsRegistry`, used at three sites for per-stage status.
- DiaMessageBus is implemented and tested (`GoogleTests/DiaMessageBus`). `Bus::Update()` runs a Primary pass, then a Reaction pass. A post from outside `Update()` is delivered in Primary; a post from a Primary handler reaches Reaction subscribers the same tick; a Post during the Reaction pass is blocked (DIA_ASSERT in Debug, silent false in Release).
- `LedgerHistory` is in-memory, Debug-only (3600 ticks), no file output.
- `Cluiche/CoW/` is a skeleton (`Shared/` only); no `.diaapp` yet. `dia scaffold stage` only targets CluicheTest.

Design:

1. **Status abstraction.** A concrete baseline `StageStatusModule` holding generic per-stage status (label, severity, visited). `BootMenuModule` finds it through an optional `ModuleRef` (an interface cannot be found by `ModuleRef`); with none, badges are omitted. A CluicheTest bridge module feeds it from `TestResultsRegistry`; CoW adds none. See the feature spec.
2. **Promotion.** Move `BootMenuModule` to `Cluiche/CluicheGameBaseline/Modules/`, retitle the window from the app name rather than a hard-coded "CluicheTest", update CluicheTest includes and vcxproj entries.
3. **Dev vs shipping.** One target; a compile-time define (shipping = Release/`DIA_SHIPPING`) excludes `BootMenuModule` registration and the Boot transitions. Test 1's "absent, not hidden" is checked by manual symbol inspection of a shipping build (no automated guard).
4. **Boot choice.** `--play` is read from the command line at startup and selects the first transition target (stub front-end stage) instead of the `Boot` stage. CoW's `main()` reads the flag (no baseline change).
5. **CoW app.** `Cluiche/CoW/` gets `CoW.diaapp` with a `Boot` stage whose transitions list the testbed stages, plus a stub `FrontEnd` stage. `dia scaffold stage` gains an `--app` option so CoW stages are generated under `Cluiche/CoW/`.
6. **Recorder.** New optional Debug-only recorder in DiaMessageBus writes per-message records (tick, type id, payload bytes) to a versioned file as messages dispatch (new Dia capability).
7. **Debug host.** Reuse `DebugPanelPageModule`/`IDebugDomain` as the shared host; F-1 proves a stand-in command and panel register without stage-specific wiring.

## Binding Decisions

| Source | Constraint on F-1 |
|--------|-------------------|
| AD-001 | Baseline, Dia, CluicheTest, and DiaCLI edits (promotion, recorder, scaffold `--app`, doc status fix) need explicit human sign-off per change. |
| AD-002 | The recorder is logged in `dia-needs.md` (Dia tier). |
| AD-003 | Test scenarios, metrics and failure conditions are owned by the GDD file; this spec does not redefine them. |
| AD-004 | Boot model and promotion as defined there. |

Per the engine/application boundary, recorder and `scaffold --app` are generic capabilities and belong in Dia/DiaCLI; this spec only proposes them.

## Open Design Questions

1. ~~How does the baseline read `--play`?~~ **Resolved:** CoW's `main()` scans `argv` (as CluicheTest's does for `--automation`) and selects the initial stage, `FrontEnd` vs `Boot`, before Application start. No new baseline API.
2. ~~Does the recorder format need to be replay-ready?~~ **Resolved:** yes. The recorder writes versioned binary per-message records (tick, type id, raw payload bytes), not `LedgerSnapshot`s (which hold counts only, no payloads). This needs a Debug-only hook in `Bus` dispatch and per-type payload size from `RegisterType`. The replay reader stays deferred to V-3.
3. ~~What proves the menu is absent from shipping?~~ **Resolved:** manual inspection only. Inspect the shipping binary's symbols for `BootMenuModule` and confirm no Boot-stage transitions, then run it. No build guard or automated check. Accepted risk: nothing prevents a later regression, so re-inspect whenever shipping configuration changes.
