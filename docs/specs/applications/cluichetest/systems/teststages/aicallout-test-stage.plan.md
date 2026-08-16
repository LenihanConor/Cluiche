**Spec:** @docs/specs/applications/cluichetest/systems/teststages/aicallout-test-stage.md
**Status:** Not Started

## Pre-dispatch resolutions

**ODQ1 — VisualDebuggerModule injection:** `ModuleRef<Cluiche::AppFlow::VisualDebuggerModule>{this}` declared in the header. Registration deferred to first `OnUpdate` via `mDomainsRegistered` flag (`vd->RegisterDomain()`); unregistered in `OnStop` (`vd->UnregisterDomain()`). Matches `DebugGalleryTestStageModule` exactly. Note: the spec references `DiaDebugDomainRegistry` — the actual API routes through `VisualDebuggerModule` directly.

**ODQ2 — Wander direction tables:** `static constexpr float kEmitterWanderDirs[6][8]` and `kResponderWanderDirs[6][8]` in the .cpp. Each entity tracks a `uint8_t mWanderDirIdx` cycling 0→7. No separate header needed. Angles hand-computed (approx π/4 increments, different starting offset per entity index for coverage).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `dia scaffold stage AICalloutTestStage` — 4 touch points | Stage appears in Boot menu | Todo | haiku | |
| 2 | `EmitterAgent` struct + 6 start positions (blue left arc, red right arc); `kEmitterWanderDirs[6][8]` constexpr in .cpp; wander at 1 m/s, cycle dir on arrival, clamp to r=10; emit every 5s with callout faction = emitter.faction; `mTotalExpired` from handle validity transitions | `aicallout.first_emit` passes; `mTotalExpired` increments during run | Todo | sonnet | |
| 3 | `RelayResponder` struct + 3-state loop (Idle/Traveling/Relaying); 6 responders (R0–R2 blue, R3–R5 red); `kResponderWanderDirs[6][8]`; Query passes responder.faction (radius 12); claim nearest same-faction callout; `mTotalClaimed` + `mTotalReleased` | `aicallout.first_claim` + `aicallout.first_release` pass | Todo | sonnet | Depends on Task 2 |
| 4 | `OnUpdate` frame loop: `mRegistry.Update(dt)` → `TickEmitters` → `TickResponders` → live count peak → `++mFrameCount` | All 5 checkpoints pass; `aicallout.ten_claims` by ~900f | Todo | sonnet | Depends on Tasks 2 + 3 |
| 5 | `ModuleRef<VisualDebuggerModule>{this}` in header; register 5 checkpoints in `OnStart`; defer `vd->RegisterDomain(*mCalloutDebugger)` to first `OnUpdate` via `mDomainsRegistered`; `vd->UnregisterDomain` + `mCalloutDebugger.reset()` in `OnStop` | Panel visible on `~`; callout table shows faction/position/TTL/claimed | Todo | sonnet | Parallel with Tasks 6 + 7 once Task 4 passes |
| 6 | `DrawDebugOverlay()`: faction-coloured diamonds + responder circles with arrows; faction-coloured pulsing rings (unclaimed), solid bright rings (claimed), white rings (TTL<2s), white 1-frame flash (release); ImGui sidebar with per-faction totals, cross-faction violations, per-responder state | Visually correct on manual run; ring state transitions visible | Todo | sonnet | Parallel with Tasks 5 + 7 once Task 4 passes |
| 7 | Register 6 metrics in `OnStart` (add `cross_faction_violations`); emit in `OnUpdate`; `OnStop` clears registry + resets all counters + handles | `cross_faction_violations == 0`; second run matches `total_emitted` | Todo | haiku | Parallel with Tasks 5 + 6 once Task 4 passes |
| 8 | Write `test_aicallout_e2e.py` + add to `default.json` | All 5 checkpoints; `cross_faction_violations == 0`; determinism second-pass passes | Todo | sonnet | Depends on Tasks 4 + 7 |
| 9 | Verify: `dia run cluichetest` → AICalloutTestStage; observe wandering emitters, chasing responders, ring colour transitions; all checkpoints pass | E2E gate | Todo | sonnet | |
