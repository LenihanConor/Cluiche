# E2E Testing — Design Decisions (2026-05-20)

**Status:** Source of truth for upcoming spec work. Supersedes parts of [summary.md](summary.md) where they conflict.

This document captures the architectural decisions reached in the 2026-05-20 design pass. The original research (2026-05-05) proposed a "DiaTestHarness" with JSON scenarios. After re-thinking the boundaries, the design now centres on a **DiaOrchestrator** (Python remote) driving any Dia app through a baseline **DiaRemoteControl** capability, with Python scenarios for orchestration logic and game-side checkpoints for validation.

The naming, layering, and command split below are binding for all child specs.

---

## 1. Mental model

The orchestrator is a **remote control**. The app is the thing being driven. The remote does not know what "correct" looks like — the game does. The remote's job is to navigate, trigger, and record. The game's job is to validate.

> **Remote control**: drives the app (CLI, multi-app, multi-stage)
> **Capability**: makes the app driveable (transition guards, command dispatch, checkpoints)
> **Game**: defines what "correct" means (per-stage checkpoints, gameplay commands)

This separation is the most important decision. Every spec must preserve it.

---

## 2. Architecture (top-down)

| Layer | Component | Owns |
|---|---|---|
| Tool | **DiaOrchestrator** (Python) | CLI entry, app launcher, WebSocket client, scenario runner, plan executor, result reporter, suite mode (was "DiaE2E") |
| Data | **Plan JSON + Scenario Python** | Plans are pure config (JSON). Scenarios are scripts (Python with pytest). |
| Wire | **Protobuf** (`OrchestratorCommand` / `OrchestratorResponse`) | New message types over the existing DiaDebugServer WebSocket. Same socket, different envelope. |
| Wiring | **RemoteControlModule** (C++) | Lifecycle: registers transition guard, hooks DiaDebugServer ↔ DiaRemoteControl ↔ DiaAPI. Lives in **CluicheGameBaseline**; **duplicated into CluicheEditor** (different ApplicationFlow stack). |
| Capability | **DiaRemoteControl** (Dia system) | Checkpoint registry, pause/resume callback registry, handler functions, .proto definitions. No transport, no lifecycle. |
| Hook | **DiaApplicationFlow — transition guards** | New feature. Generic veto-on-transition mechanism. RemoteControlModule is the first consumer; loading screens, pause menus, asset gating may be later consumers. |
| Hook | **DiaApplicationFlow — baseline commands** | New feature. Registers `quit` and `report` with DiaAPI. Useful with or without the orchestrator. |
| Game | **Stage / Plugin checkpoints** | Per-stage `RegisterCheckpoint(name, fn)`. Game-defined validation, scoped to stage lifetime. |

---

## 3. Naming

| Old name (research / current spec) | New name | Reason |
|---|---|---|
| DiaTestHarness | **DiaOrchestrator** | Captures the actual capability: remote orchestration of an app. Testing is one use case. |
| TestabilityModule | **RemoteControlModule** | Honest about what the C++ side does — makes the app remotely controllable. |
| DiaE2E (separate system) | **DiaOrchestrator suite mode** | Collapsed. JUnit XML, suite directories, `--suite=<name>` are output features of the orchestrator, not a separate system. |

The four backlog items from the original research collapse to two systems (DiaOrchestrator + DiaRemoteControl) plus the dependency features below.

---

## 4. Command surface

Three layers of commands. All flow through DiaAPI; the difference is who registers each handler.

### 4.1 Baseline commands registered by DiaApplicationFlow

| Command | Why ApplicationFlow | Notes |
|---|---|---|
| `quit` | Needs ApplicationFlow's shutdown machinery. Useful for menu/UI/OS signals — not test-specific. | New |
| `report` | Reads ApplicationFlow's stage + active modules state. Useful for debug overlays — not test-specific. | New |

### 4.2 Remote-control commands registered by DiaRemoteControl

| Command | Why RemoteControl | Notes |
|---|---|---|
| `navigate_to <target>` | Releases the held transition guard that RemoteControlModule registered. Without that guard, the command has nothing to do. | Generic across game stages and editor plugins. Game registers a stage→target resolver; editor registers a plugin→target resolver. |
| `pause` | Invokes game-registered pause callbacks via RemoteControl's registry. | Game registers what "pause" means for its sim. Multi-PU aware — game decides which PU(s) to freeze. Frame-boundary guarantee is RemoteControl's responsibility. |
| `resume` | Same — symmetric. | |
| `validate <checkpoint>` | Looks up checkpoint in RemoteControl's registry, runs the registered fn, returns result. | Checkpoints are registered by stages on `DoStart` and cleared by RemoteControl on stage transition. |

### 4.3 Game-specific commands registered by stages / modules

Game stages register handlers directly with DiaAPI. RemoteControl is not involved.

```cpp
api.RegisterCommand("cluichetest.spawn_shape", [this](const ParamsJson& p) { ... });
```

**Naming convention:** prefix with `<app>.` or `<system>.` to keep the global namespace clean (matches the metric-name convention `dia.jobs.queue_depth`).

### 4.4 Why the split

The rule is: **the handler lives with the data it needs to operate on.**

- `quit` / `report` → data lives in DiaApplicationFlow → handler there
- `navigate_to` / `pause` / `resume` / `validate` → data lives in DiaRemoteControl (guard state, pause registry, checkpoint registry) → handler there
- Game commands → data lives in the game stage → handler there

DiaAPI is the dispatcher. It does not own command logic. It only routes names to handlers.

---

## 5. Scenario format — Python, not JSON

**Decision:** Scenarios are Python files (pytest functions). Plans remain JSON.

**Rationale:**
- Scenarios need loops, parameterisation, and helper composition (e.g. "for each shape pair, validate intersection"). JSON requires writing 12 near-identical entries.
- Pytest gives parametrize, fixtures, marks (xfail/skipif) and reporting infrastructure for free.
- Refactoring a checkpoint name finds every caller; JSON is invisible to refactoring tools.
- AI generates valid Python at least as reliably as valid JSON.

**Risk and mitigation:** Python's expressive power tempts authors to put validation logic in scripts. The convention is **scenarios orchestrate, checkpoints validate**. Code review enforces it.

**Plans stay JSON.** Plans are pure config (which scenarios run, gate ordering, port). No logic. JSON is correct.

---

## 6. Validation model

Two paths, both supported:

| Path | Where | When to use |
|---|---|---|
| **Game-side checkpoint** | C++ `RegisterCheckpoint(name, fn)` returning `CheckpointResult` | Complex game-logic validation that needs internal state (physics intersection, entity invariants, layout consistency) |
| **Script-side assertion** | Python scenario asserts on data the game reports | Simple factual assertions on values the game already exposes (counts, metric thresholds, phase names) |

Game-registered checkpoints:
- Auto-clear on stage transition (RemoteControl handles it; stage `DoStop` does not need manual cleanup)
- Return `{ passed, message, duration_ms }` — failure messages bubble back to the scenario

Implicit per-scenario assertions:
- `assert_no_log_errors` runs at end of every scenario unless explicitly disabled. Catches silent failures that pass all explicit checks.
- DiaObservation log subscription provides this for free.

Per-checkpoint reporting granularity:
- Result file captures **each named validation independently**, not just the overall scenario PASS/FAIL.
- "circle_vs_box: PASS, circle_vs_triangle: FAIL, no_asserts: PASS" — visible in the report.

---

## 7. Bootstrap and stage navigation

**Transition guards** are the mechanism. New feature on **DiaApplicationFlow**:

- Any module can register a guard on stage transitions. Guards return `Hold` or `Allow`. ApplicationFlow fires the transition only when all guards allow it.
- No guards registered → current auto-advance behaviour. Existing apps unchanged.
- `RemoteControlModule` registers a guard on start that holds every transition until `navigate_to` arrives. The harness becomes the pacing authority.
- Useful beyond testing (loading screens, asset gating, pause menus). Earns its place in DiaApplicationFlow as a first-class feature.

This means an app with RemoteControlModule active **never auto-advances past bootstrap** — the orchestrator must drive every transition. An app without it advances normally.

---

## 8. Editor testing

The same architecture applies with one mapping table:

| Game concept | Editor equivalent |
|---|---|
| Stage | Active plugin |
| Stage transition | Plugin load / unload |
| Checkpoint | Plugin self-validation |
| `navigate_to "RigidBody2DStage"` | `navigate_to "DiaApplicationFlowEditor"` (plugin id) |
| Bootstrap hold | Editor holds on splash until told to load a project |

**Required additions to the editor:**
1. Duplicate RemoteControlModule wiring into CluicheEditor's ApplicationFlow stack (it can't share with CluicheGameBaseline — different module hierarchies).
2. Add `IEditorPlugin::RegisterCheckpoints(DiaRemoteControl&)` virtual (default no-op) so plugins can expose validations.
3. Plugin-target resolver in RemoteControlModule (editor variant) that maps `navigate_to` arguments to `EditorPluginRegistry` entries.

Plugins register checkpoints on activation, RemoteControl auto-clears them on plugin unload (same lifecycle pattern as stages).

---

## 9. What lives where (module home decisions)

| Component | Home | Rationale |
|---|---|---|
| DiaRemoteControl (system) | `Dia/DiaRemoteControl/` | New Dia system. Pure capability. Depends on DiaCore, DiaApplicationFlow, DiaAPI. |
| RemoteControlModule (game variant) | `Cluiche/CluicheGameBaseline/Modules/RemoteControlModule.{h,cpp}` | Wiring lives at the application baseline. Future games inherit from baseline and get it for free. |
| RemoteControlModule (editor variant) | `Cluiche/CluicheEditor/ApplicationFlow/Modules/RemoteControlModule.{h,cpp}` | **Duplicated**, not shared. Different ApplicationFlow stack, different navigation semantics. Acceptable cost for a thin wiring module. |
| Protobuf `.proto` | `Dia/DiaRemoteControl/Protocol/` | Lives with the system that owns the message contract. |
| DiaOrchestrator (Python) | `Tools/orchestrator/` (rename from `Tools/e2e/`) | External tool. Mirrors DiaCLI's Python conventions. |
| Scenarios | `Tools/orchestrator/scenarios/<app>/<stage>/*.py` | Per-app, per-stage directories. Scenarios are pytest test functions. |
| Plans | `Tools/orchestrator/plans/<app>/*.json` | Pure config. JSON. |

---

## 10. Out of scope (consciously declined)

- **Visual regression testing** — screenshots are an artifact-only output, not a pass/fail mechanism. Driver/resolution sensitive, expensive to maintain reference images, headless CI doesn't render.
- **Input replay / record** — research already discarded. Brittle to changes.
- **Networked multi-instance testing** — out of scope until there is a networked game.
- **Coverage tracking** — different problem, different tool.
- **Parallel scenario execution** — fresh app per scenario stays sequential. Revisit if suite duration becomes a problem.

---

## 11. Forward goals (add to scope but don't gold-plate)

- **Metric threshold assertions** — `assert_metric("dia.frame.duration_ms", "<", 33)` as a scenario primitive. Catches regressions, not just crashes. Small addition, large payoff.
- **Implicit `assert_no_log_errors`** — runs at end of every scenario unless disabled. Catches silent failures.
- **Flaky / known-fail markers** — pytest has `xfail` natively; use it. Means a known-broken checkpoint can stay in the suite without failing the run.

---

## 12. Spec-work implications

The original research produced two specs that are now obsolete and need to be **superseded** rather than implemented as written:

| Existing spec | Disposition |
|---|---|
| `docs/specs/systems/dia/diatestharness.md` | **Supersede** — replace with `dia/diaorchestrator.md`. JSON scenarios and DiaTestHarness naming are out of date. |
| `docs/specs/features/dia/diatestharness/harness-core.md` | **Supersede** — folded into the new DiaOrchestrator system spec; Python scenarios change the surface significantly. |
| `docs/specs/features/cluichetest/cluichetestscenarios/smoke-test-scenario.md` | **Rewrite** — same intent, new format (Python), new commands (`navigate_to` instead of `wait_for_phase`). |
| `docs/specs/systems/cluichetest/cluichetestscenarios.md` | Keep, but update references to use new names + Python format. |

New specs needed (in dependency order):

1. `/spec-feature` — DiaApplicationFlow / **transition-guards** (S)
2. `/spec-feature` — DiaApplicationFlow / **baseline-commands** (`quit`, `report`) (XS)
3. `/spec-system` — **DiaRemoteControl** + RemoteControlModule (M)
4. `/spec-system` — **DiaOrchestrator** (Python tool, supersedes DiaTestHarness) (M)
5. `/spec-feature` — CluicheTest **smoke scenario** (rewrite under new format) (XS)
6. `/spec-system` — **CluicheTest TestStages** with checkpoint pattern (M-L)
7. `/spec-feature` — CluicheEditor **RemoteControlModule wiring** + `IEditorPlugin::RegisterCheckpoints` (S)
8. `/spec-feature` — DiaOrchestrator / **metric threshold assertions** (S)

Items 1+2 can run in parallel. 3 depends on 1+2. 4 depends on 3. 5 onwards depend on 4.

---

## 13. References

- [summary.md](summary.md) — Original research (2026-05-05), partially superseded by this document
- [explore.md](explore.md), [ideate.md](ideate.md), [evaluate.md](evaluate.md), [choose.md](choose.md) — Original research artifacts
- [docs/research/observ_telemetry/summary.md](../observ_telemetry/summary.md) — DiaObservation hooks the orchestrator subscribes to
