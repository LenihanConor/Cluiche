# System Spec: F-1 Harness

**Parent:** @docs/specs/applications/cow/cow.md
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F1_Harness.md` (DesignDocs repo; owns what must be proven, scenarios, metrics, failure conditions — see AD-003)
**Status:** `Approved`
**Plan:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.plan.md

## Summary

The shared boot and debug infrastructure every other CoW testbed launches from: a boot choice between the debug menu and the real front end, a promoted `BootMenuModule` in CluicheGameBaseline, verification of the Primary/Reaction event bus on a live stage, a shared debug-command/inspector host, and a Debug-only recorder that writes bus traffic to a file. Nothing simulation- or visual-specific lives here.

## Problem

CoW needs 12 standalone testbeds launchable from one menu. Dia already has the pattern (CluicheTest `Boot` stage + `BootMenuModule`), but the menu is CluicheTest-private and coupled to `CluicheTest::TestResultsRegistry`. Bus traffic can only be inspected in memory (Debug ring buffer), not after the process exits.

## Goals

1. CoW boots to the debug menu, which has a Play entry for a stub front-end stage. Shipping-build gating is out of scope (user decision).
2. `BootMenuModule` lives in CluicheGameBaseline with no CluicheTest types; CluicheTest's ~25 stages behave unchanged through it.
3. Menu lists a game's `Boot` transitions, launches any, and returns cleanly with no state leaking between launches.
4. ~~Confirm the DiaMessageBus Primary/Reaction semantics on a live stage; correct the stale `status: stub` in its module doc.~~ Already satisfied: CluicheTest MessageBusStressTestStage E2E test proves Primary/Reaction semantics (10,000+ messages); module doc status is `active`.
5. Debug-only file recorder for bus traffic.
6. In-game debug infrastructure: text console for executing DiaAPI commands, data panel registry for live tables/logs, and a searchable palette for discovering all available debug tools (commands, panels, visual debug domains).

## Non-Goals

1. Event replay from file (deferred until V-3 needs canned data).
2. The real front end's screens (G-1); F-1 only provides a stub destination for the Play entry.
3. Touch input on the menu.
4. Any simulation or visual content.
5. Crash containment: a crashed testbed stage kills the process and the dev relaunches (GDD decision). No try/catch-style recovery around stage execution or transitions.

## Features

| Feature | Spec | Status |
|---------|------|--------|
| BootMenuModule promotion + StageStatusModule | @docs/specs/applications/cow/systems/f1-harness/bootmenu-promotion.md | Approved |
| Boot choice (Play menu entry; shipping gating dropped) | @docs/specs/applications/cow/systems/f1-harness/boot-choice.md | Approved |
| CoW app skeleton (project, manifests, Boot stage, stub front end) + `dia scaffold stage --app` | @docs/specs/applications/cow/systems/f1-harness/cow-app-skeleton.md | Approved |
| Bus live verification (Test 4) | N/A — already proven by CluicheTest MessageBusStressTestStage (10,000 message E2E test) | Done |
| Bus traffic file recorder (Debug-only) | @docs/specs/applications/dia/systems/diamessagebus/bus-traffic-recorder.md | Approved |
| Debug Console (in-game text console for DiaAPI commands) | @docs/specs/applications/cow/systems/f1-harness/debug-console.md | Draft |
| Debug Data Panel Registry (in-game data table/log panels) | @docs/specs/applications/cow/systems/f1-harness/debug-data-panel.md | Draft |
| Debug Palette (command/panel/domain discovery UI) | @docs/specs/applications/cow/systems/f1-harness/debug-palette.md | Draft |

## Architecture

Findings from the code that shape the design:

- `BootMenuModule` (RenderPU) already depends only on baseline types (`DebugUIModule`, `DebugServerHostModule`, `RenderToSimNavRequest`) plus `CluicheTest::TestResultsRegistry`, used at three sites for per-stage status.
- DiaMessageBus is implemented and tested (`GoogleTests/DiaMessageBus`). `Bus::Update()` runs a Primary pass, then a Reaction pass. A post from outside `Update()` is delivered in Primary; a post from a Primary handler reaches Reaction subscribers the same tick; a Post during the Reaction pass is blocked (DIA_ASSERT in Debug, silent false in Release).
- `LedgerHistory` is in-memory, Debug-only (3600 ticks), no file output.
- `Cluiche/CoW/` is a skeleton (`Shared/` only); no `.diaapp` yet. `dia scaffold stage` only targets CluicheTest.

Design:

1. **Status abstraction.** A concrete baseline `StageStatusModule` holding generic per-stage status (label, severity, visited). `BootMenuModule` finds it through an optional `ModuleRef` (an interface cannot be found by `ModuleRef`); with none, badges are omitted. A CluicheTest bridge module feeds it from `TestResultsRegistry`; CoW adds none. See the feature spec.
2. **Promotion.** Move `BootMenuModule` to `Cluiche/CluicheGameBaseline/Modules/`, retitle the window from the app name rather than a hard-coded "CluicheTest", update CluicheTest includes and vcxproj entries.
3. **Shipping.** Not in scope. No compile-out, no second manifest.
4. **Boot choice.** CoW's single manifest starts at `Boot`, whose menu lists `FrontEnd` as the Play entry. No command-line flag, no shipping variant. See the boot-choice feature spec.
5. **CoW app.** `Cluiche/CoW/` gets `CoW.diaapp` with a `Boot` stage whose transitions list the testbed stages, plus a stub `FrontEnd` stage. `dia scaffold stage` gains an `--app` option so CoW stages are generated under `Cluiche/CoW/`.
6. **Recorder.** New optional Debug-only recorder in DiaMessageBus writes per-message records (tick, type id, payload bytes) to a versioned file as messages dispatch (new Dia capability).
7. **Debug infrastructure.** F-1 exposes three gaps in in-game debug tooling: (a) no text console for executing DiaAPI commands in-game, (b) no registry for data panels (tables/logs) as distinct from visual debug draw (shapes), (c) no discovery UI for available commands/panels/domains. These gaps drive three new Dia systems with mockups at `@docs/specs/applications/cow/systems/f1-harness/mockups/`. F-1 proves each with bus_traffic stand-ins: console executes `bus_traffic.*` commands, data panel shows live message table, palette lists and launches both.

## External Dependencies

- **GDD Test 3 (Promotion Parity)** needs CluicheTest's regression coverage, which CoW work cannot run itself under AD-001. It is tracked as a dependency on the Cluiche/engine side (the promotion plan's verify task, task 6), not as a CoW deliverable, and F-1's lock does not wait on it.
- The Play destination is a trivial stub `FrontEnd` stage built as part of F-1 (it logs "front end not yet built" and idles), so Test 1 is runnable now. G-1 replaces it.

## Acceptance Metrics (from the GDD, CH-01)

- Process launch to interactive debug menu: under 2 seconds.
- Stage launch, return and re-launch cycle repeated 50 times: zero state leaks or crashes.
- Event bus run of 10,000 messages: delivered exactly 1:1 with posted, zero drops, zero duplicates.

## Change Sign-off (AD-001)

User decisions on the cross-boundary changes this system needs:

| Change | Decision |
|--------|----------|
| Move `BootMenuModule` to the baseline; new baseline `StageStatusModule` | Approved |
| CluicheTest follow-through (bridge module, manifest, project edits) | Approved |
| `cow` DiaCLI target and `[targets.cow]` in `pipeline.toml` (separate target, shared DiaCLI pipeline code) | Approved |
| `CoW` entry in `Cluiche.sln`; `scaffold sln-add` path option | Approved |
| `dia scaffold stage --app` | Approved |
| Shipping gating in the baseline | Dropped |
| Bus traffic recorder | Waiting (deferred feature) |
| DiaMessageBus module doc status fix | N/A — module doc already shows `status: active` |
| `dia scaffold app` (spec Approved) incl. data-driven DiaCLI target registry | Approved |
| `new-cow-stage` skill (spec Approved) | Approved |

## Binding Decisions

| Source | Constraint on F-1 |
|--------|-------------------|
| AD-001 | Baseline, Dia, CluicheTest, and DiaCLI edits (promotion, recorder, scaffold `--app`, doc status fix) need explicit human sign-off per change. |
| AD-002 | The recorder is logged in `dia-needs.md` (Dia tier). |
| AD-003 | Test scenarios, metrics and failure conditions are owned by the GDD file; this spec does not redefine them. |
| AD-004 | Boot model and promotion as defined there. |

Per the engine/application boundary, recorder and `scaffold --app` are generic capabilities and belong in Dia/DiaCLI; this spec only proposes them.

## Open Design Questions

1. ~~How does the baseline read `--play`?~~ **Superseded:** no flag. The front-end jump is a Boot menu entry (a stage transition), which needs no argument parsing or DiaCLI forwarding.
2. ~~Does the recorder format need to be replay-ready?~~ **Resolved:** yes. The recorder writes versioned binary per-message records (tick, type id, raw payload bytes), not `LedgerSnapshot`s (which hold counts only, no payloads). This needs a Debug-only hook in `Bus` dispatch and per-type payload size from `RegisterType`. The replay reader stays deferred to V-3.
3. ~~What proves the menu is absent from shipping?~~ **Superseded:** shipping gating is out of scope (user decision), so no proof is needed.
