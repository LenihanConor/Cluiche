# Feature Spec: DiaAutomation Checkpoint Registry

## Parent System
@docs/specs/applications/dia/systems/diaautomation/diaautomation.md

## Depends On
@docs/specs/applications/dia/systems/diaautomation/module-and-build.md

## Purpose

Modules register named validation functions (checkpoints) with the AutomationService. The orchestrator calls `dia.automation.validate` to run a checkpoint and get a pass/fail/message result. Checkpoints are automatically cleared when the owning module stops.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `RegisterCheckpoint(owner, name, fn)` stores the entry; `HasCheckpoint(name)` returns true. | Unit test |
| AC2 | `RegisterCheckpoint` rejects duplicate names (same name regardless of owner); logs error; returns without registering. | Unit test |
| AC3 | `UnregisterCheckpoints(owner)` removes all entries owned by that module. | Unit test |
| AC4 | `RunCheckpoint(name)` invokes the registered fn and returns its `CheckpointResult`. | Unit test |
| AC5 | `RunCheckpoint` for an unknown name returns `{passed: false, message: "checkpoint not found", durationMs: 0}`. | Unit test |
| AC6 | `dia.automation.validate` command registered; params `{"checkpoint": "<name>"}`. Returns `{"passed": bool, "message": "...", "duration_ms": float}`. | Unit test via ExecuteCommandJson |
| AC7 | `dia.automation.validate` for unknown checkpoint returns `{"success": false, "error": "checkpoint not found: '<name>'"}`. | Unit test |
| AC8 | After `UnregisterCheckpoints(owner)`, `HasCheckpoint(name)` returns false for that owner's checkpoints. | Unit test |

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004 |
| Application | @docs/specs/applications/dia/dia.md | AD-002, AD-003 |
| System | @docs/specs/applications/dia/systems/diaautomation/diaautomation.md | SD-AUT-001, SD-AUT-002, SD-AUT-005, SD-AUT-007 |

## Binding Decisions Compliance

| Binding Decision | Source | How This Feature Honors It |
|-----------------|--------|---------------------------|
| PD-001 (StringCRC for IDs) | Platform | Checkpoint names are `StringCRC`; command name `dia.automation.validate` is StringCRC |
| PD-004 (No STL containers) | Platform | Registry uses `DynamicArrayC<CheckpointEntry, 64>`; `CheckpointFn` is `std::function` (callable) |
| SD-AUT-002 (Checkpoint lifetime tied to owner) | DiaAutomation | `UnregisterCheckpoints(Module*)` clears all entries by that owner |
| SD-AUT-005 (Consistent envelope) | DiaAutomation | `dia.automation.validate` uses `{success, data/error}` envelope |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should `RegisterCheckpoint` return bool (success/fail) or void? | Returns void; duplicate registration is a programming error (debug assert + log). Consistent with TransitionGuardFn pattern. |
| 2 | Does `dia.automation.validate` need to reset the heartbeat? | Yes — SD-AUT-007: any incoming automation command resets heartbeat. Implemented in `RegisterCommands()` wrapper. |

## Status

`Approved` (2026-05-21). Amendment 2026-11-19 (Sim-owned checkpoints) below: Draft.

---

## Amendment 2026-11-19: Sim-owned checkpoints (Sim checkpoint runner)

**Status of this amendment:** Draft, awaiting user answers to the Open Design Questions. AD-001 sign-off for the new `CheckpointSnapshot` type, the Sim-side runner module and the Main-side serving was given by the user on 2026-11-19. **Plan:** @docs/specs/applications/dia/systems/diaautomation/checkpoint-registry.plan.md. ACs 1 to 8 stand except where AC10 below narrows AC4.

### Problem

`AutomationService::RunCheckpoint` calls the registered `CheckpointFn` on whichever thread calls it, which for `dia.automation.validate` is the Main thread (DiaAPI command). About 140 checkpoints are registered by SimPU modules (every `TestStageModuleBase` subclass in CluicheTest, a `SimModule`) and their lambdas read plain Sim-owned fields (`mFieldsInitialized`, grids, entity pools). Main reads them while SimPU writes them: a data race, and the same cross-PU hazard that Sim-Hosted Queries and the CellLayers Streams Migration removed. Platform rule: every PU boundary is crossed by streams; no mutex-guarded cross-PU reads.

Second hazard of the same kind: `RegisterCheckpoint` / `UnregisterCheckpoints` are called from SimPU (`TestStageModuleBase` stages call them in `OnStart(service)` via `GetAutomationService()`), yet mutate `mCheckpoints`, a `DynamicArrayC` owned by the Main-side service and read by Main commands (`HasCheckpoint`, `list_checkpoints`). This amendment removes that too (see Design).

Third: `CheckpointResult::message` is a `const char*`. Across a thread boundary it must be copied, not passed.

### Design

Model on the CellLayers state-relay shape (@docs/specs/applications/dia/systems/diacelllayers/streams-migration.md, Task 4: Sim publisher writes a fixed slot to a `FrameStream`, Main adapter reads the latest).

- **Sim side:** `SimCheckpointRunnerModule` (new `SimModule` in `Dia/DiaAutomation`; generic, no game types). It owns a Sim-local table of Sim-owned checkpoints. Each Sim frame, after the stage modules have run (manifest order: runner last on SimPU), it evaluates every entry on the SimPU thread, times it, copies the result into a `CheckpointSnapshot` and writes it to a `FrameStream` (`$automation.checkpoints`).
- **Main side:** `AutomationService` keeps its Main-owned table for Main-owned checkpoints (unchanged path; `fn()` runs on the caller). For Sim-owned names, `RunCheckpoint` reads the latest snapshot and serves the entry; it never invokes a Sim-owned `fn`. Result lag is up to one Sim frame plus one Main frame.
- **Registration handoff:** Sim-owned registrations never touch the Main table. The service forwards them to an `ISimCheckpointHost` (small interface in DiaAutomation, implemented by the runner, bound once when the runner connects to the service). The host is only called on SimPU (the registering module runs there), so the runner table needs no lock. Main learns the Sim-owned name set from the snapshot, so `HasCheckpoint` and `dia.automation.list_checkpoints` include Sim-owned names with up to one frame of lag after registration.
- **Streams only:** no mutex, no pointer from Main into Sim state.

### `CheckpointSnapshot`

Fixed-size POD, one slot per Sim frame (`FrameStream`, latest wins).

| Field | Type | Meaning |
|-------|------|---------|
| `simFrame` | `uint64_t` | Frame counter of this publish; Main uses it to detect a frozen publisher |
| `generation` | `uint32_t` | Increments each time the Sim-owned name set changes (register/unregister) |
| `count` | `uint32_t` | Valid entries, at most `kMaxEntries` (64, same cap as `mCheckpoints`) |
| `overflow` | `bool` | More Sim-owned checkpoints registered than fit; extras are unserved, one Warning logged |
| `entries[kMaxEntries]` | `Entry` | See below |

`Entry`: `uint32_t nameCrc` (the `StringCRC` value), `bool passed`, `float durationMs` (measured by the runner around `fn()`, replacing the lambda's value if that is 0), `char message[96]` (truncated, NUL-terminated; the runner copies the lambda's `const char*` immediately on SimPU, so literals and short-lived buffers are both safe), `uint8_t flags` (reserved, see ODQ 3). Slot about 8 KB. Streams budget: 35 of 64 used today (CellLayers state relay); this adds 1.

### Missing and stale semantics

All non-fresh cases return `passed = false` with a distinct message so a scenario can tell them apart. "checkpoint not found" keeps its exact text and means "no registration anywhere".

| Case | Condition | Result message | Notes |
|------|-----------|----------------|-------|
| Not found | name in neither the Main table nor the latest snapshot's names | `checkpoint not found` | Unchanged (AC5) |
| Pending (missing) | name known (forwarded registration) but absent from the latest snapshot's entries, or no snapshot published yet | `checkpoint pending: no snapshot yet` | Normal for the first frame after `OnStart`; orchestrator polls. Debug log only |
| Stale | a snapshot exists but `simFrame` has not advanced for more than `kStaleMs` of Main wall time | `checkpoint stale: snapshot <N> ms old (sim frame <F>)` | A cached pass is never served as a pass: stale is always `passed = false` |
| Fresh | otherwise | the entry's own result | |

Thresholds (constants in `CheckpointSnapshot.h`, adjustable via `AutomationService::SetStaleThresholdMs`): **`kStaleMs` = 1000 ms** (about 30 Sim frames at 30 Hz: far above the expected one-frame lag, below E2E polling timeouts). Main measures age as wall time since it last observed a changed `simFrame`, never by subtracting clocks across PUs. Sim legitimately stops advancing while automation is paused; the proposed behaviour is a runner heartbeat re-publish (ODQ 2).

### Registration tagging (no call-site changes)

All ~140 sites call `service->RegisterCheckpoint(this, name, fn)` with `this` a `Module*`. The tag is derived inside `RegisterCheckpoint`, with no new parameter: `owner->GetProcessingUnit()->GetAffinity() == PUAffinity::kSim` means Sim-owned. `GetAffinity()` is set from the PU instance id (`SimPU` / `MainPU` / `RenderPU`). `owner == nullptr`, `kMain` or `kAny` means Main-owned and keeps today's path (this is what every existing unit test with a `nullptr` owner exercises). No registration-time default is needed and no site is touched. A `kRender` owner is Main-owned by this rule (same hazard as today, none exist); log a Warning at registration.

Cannot be done without touching sites: a Main-PU module registering a checkpoint that reads Sim data (its PU says Main). None exist today. If one appears, an additive explicit overload `RegisterCheckpoint(owner, name, fn, CheckpointThread::kSim)` is the escape hatch. Premise to verify in plan Task 1: every one of the 30 registering files is a `SimModule` on SimPU, and `GetProcessingUnit()` is set by the time `OnStart` registers.

### Acceptance Criteria (amendment)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC9 | A checkpoint registered by a module whose PU has `kSim` affinity is Sim-owned; `owner == nullptr` or a Main-affinity module is Main-owned. No `RegisterCheckpoint` call site changes. | Unit test; call-site diff empty |
| AC10 | **Thread identity:** a Sim-owned `fn` executes on the SimPU thread and never on the thread that calls `RunCheckpoint` / `dia.automation.validate` (Main). AC4 is narrowed: for Sim-owned names `RunCheckpoint` returns the latest snapshot entry, not a fresh invocation. | RED-first test: lambda records `std::this_thread::get_id()`; asserts equal to the Sim thread id, not equal to the caller's, and that the invocation count changes only when the Sim frame ticks, never from `RunCheckpoint` |
| AC11 | Main-owned path unchanged: `fn` runs synchronously on the caller; AC1 to AC8 still pass for `nullptr`-owner registrations. | Existing `TestAutomationService` suite, unmodified |
| AC12 | After a Sim frame in which a Sim-owned checkpoint passes, `RunCheckpoint` returns `passed = true`, the lambda's message (copied, truncated to 95 chars plus NUL) and a `durationMs` measured on Sim, within one Sim frame plus one Main frame. | Unit test with a stepped runner |
| AC13 | Pending, stale and not-found are distinguishable per the table; stale is `passed = false` even if the last cached entry passed. | One unit test per row |
| AC14 | `UnregisterCheckpoints(owner)` for a Sim-owned owner removes its runner entries; the next snapshot omits them and bumps `generation`; `HasCheckpoint` is false one frame later. | Unit test |
| AC15 | Duplicate-name rejection (AC2) holds across a Main-owned and a Sim-owned registration of the same name. | Unit test |
| AC16 | `dia.automation.validate` and `list_checkpoints` include Sim-owned names and results; validate on a pending name returns `passed:false` with the pending message, not an error envelope. | Unit test via ExecuteCommandJson |
| AC17 | More than 64 Sim-owned checkpoints: extras are unserved, `overflow` is set, one Warning is logged. | Unit test |
| AC18 | `Dia/DiaAutomation` production code has no `std::mutex`, no Main-thread call of a Sim-owned `fn`, and no pointer from Main into Sim state. | Grep and review |
| AC19 | The existing CluicheTest checkpoints pass under E2E with the runner active (poll timeouts unchanged, or adjusted where a scenario read a checkpoint on the very frame it became true). | Full E2E run (last plan task) |

### Observability

- `DIA_LOG_WARNING("Automation", ...)` on stale (once per transition into stale and once on recovery, not per call) and on overflow; `DIA_LOG_DEBUG` on pending; `DIA_LOG_INFO` on Sim-owned register/unregister, matching the existing `checkpoint.registered` lines.
- `DIA_PROFILE_SCOPE` around the runner's per-frame evaluate-and-publish.
- Metrics: gauge `dia.automation.checkpoints.sim_evaluated` (evaluated last frame), gauge `dia.automation.checkpoints.sim_eval_ms` (runner cost per frame), counter `dia.automation.checkpoints.stale_served`.
- Health: runner `IHealthReporter` unhealthy on overflow or on a frame whose evaluation exceeds 5 ms (ODQ 1).

### Binding decisions

| Source | ID | How honoured |
|--------|----|--------------|
| Dia application | AD-001 | New public type `CheckpointSnapshot`, runner module and Main-side serving: user sign-off 2026-11-19 |
| DiaAutomation | SD-AUT-001 | `AutomationService` stays a non-Module; the runner is a separate `SimModule` that connects to it |
| DiaAutomation | SD-AUT-002 | Lifetime tied to owner kept: `UnregisterCheckpoints(owner)` clears whichever table holds the entries |
| Project rule | PU boundaries are streams | Snapshot over a `FrameStream`; registration forwarded to a Sim-only host; no mutex |
| Project rule | No singletons | Host binding and stream wired through the existing stream/service connection, no statics |

### Open Design Questions

1. **Evaluate every Sim frame, or on demand?** Every checkpoint runs every Sim frame (5 to 11 per active stage; some, like `FindLocalMaxima` or `FindCellsAboveThreshold`, do real work). The viewer state relay chose demand-driven arming because its snapshot cost 3 to 4 ms. Every frame is simplest and gives the lowest lag; demand-driven adds 1 to 2 frames on the first read and a request channel back to Sim. Proposal: every frame, with the 5 ms health budget and the eval-ms metric as the tripwire.
2. **Paused Sim: heartbeat or flagged-frozen?** If `dia.automation.pause` stops Sim updating, a validate during the pause would report stale. Proposal: the runner keeps re-publishing (results frozen, `simFrame` still advancing) so a paused game is not stale; the alternative is serving the frozen results with a `paused` flag. Which does the orchestrator want for scenarios that pause and then validate?
3. **Throwing lambdas and latching.** Should the runner convert an exception into `passed = false, "checkpoint threw"` (needs try/catch; check whether the build allows exceptions) or leave as is? And may it stop re-evaluating a checkpoint after it first passes (stages already latch in their own fields) to save cost?

### Files affected (expected)

`Dia/DiaAutomation/CheckpointSnapshot.h`, `ISimCheckpointHost.h`, `SimCheckpointRunnerModule.{h,cpp}` (new); `AutomationService.{h,cpp}` (tagging, forwarding, snapshot serving); `dia.automation.architecture.module.md`; `DiaAutomation.vcxproj` and filters; CluicheTest and `cluiche_main` `.diaapp` (runner on SimPU last, `$automation.checkpoints` stream); `Cluiche/Tests/GoogleTests/Automation/`. No `RegisterCheckpoint` call-site changes.
