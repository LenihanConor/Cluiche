# Feature Spec: Domain-Level Log Instrumentation

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **domain-log-instrumentation** |

**Status:** `Approved` — 2026-05-19

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #1 (skeleton + logger fold) — `DIA_LOG_*` macros must exist in `<DiaObservation/Log/DiaLog.h>`. Feature #2 (foundation) — `ObservationFileSink` must be wired. Feature #1 must be Done before implementation begins.

---

## Problem Statement

After Feature #1, the logging infrastructure exists, but engine subsystems with zero observability coverage cannot be monitored in `log.jsonl`. A session may fail silently — the asset catalog loads nothing, an animation clip state transitions unexpectedly, a stream drops events — yet `log.jsonl` contains no trace of any of these events. This feature fills the log coverage gaps systematically: asset system lifecycle, animation clip playback, stream connect/disconnect, module lifecycle events, and enrichment (duration on lifecycle events, `module_id` tag).

---

## Solution Overview

This feature is purely additive — no new types, no new files in DiaObservation. Each instrumented subsystem gets `DIA_LOG_INFO` / `DIA_LOG_DEBUG` call sites added at key lifecycle boundaries. Channels follow the existing channel naming convention (lowercase kebab: `"asset"`, `"animation"`, `"stream"`, `"module"`). Enrichment means: lifecycle-start logs capture start time (`std::chrono::steady_clock`) so the completion log can compute and emit `duration_ms`.

All channels are `StringCRC` values. No new channel registration step is needed — `DIA_LOG_*` routes through the existing channel filter infrastructure.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `log.jsonl` contains an `asset.catalog.load.start` record when a stage load begins, with `count` field | Integration test: load a stage, parse `log.jsonl` |
| AC2 | `log.jsonl` contains an `asset.catalog.load.complete` record with `asset_count` and `duration_ms` | Integration test |
| AC3 | `log.jsonl` contains an `asset.catalog.load.fail` record with `reason` when loading fails | Unit test: mock a failing catalog load |
| AC4 | `log.jsonl` contains `asset.type.registered` records for each asset type registered at startup | Integration test |
| AC5 | `log.jsonl` contains `asset.record.created` and `asset.record.deleted` records for asset lifecycle | Integration test |
| AC6 | `log.jsonl` contains `anim.clip.loaded` and `anim.clip.unloaded` records | Integration test: load/unload an animation clip |
| AC7 | `log.jsonl` contains `anim.playback.start`, `anim.playback.stop`, `anim.playback.loop` records | Integration test: play a clip to loop |
| AC8 | `log.jsonl` contains `stream.writer.connected` and `stream.writer.disconnected` records when stream writers attach/detach | Integration test |
| AC9 | `log.jsonl` contains `stream.reader.connected` and `stream.reader.disconnected` records | Integration test |
| AC10 | `log.jsonl` contains `stream.overflow` record with `policy` field when an EventStreamStore drops/rejects an event | Unit test: fill a stream to capacity |
| AC11 | `log.jsonl` contains `stream.tap.attached` and `stream.tap.detached` records | Integration test |
| AC12 | `log.jsonl` contains `module.configure` record per module during `OnConfigure` | Integration test |
| AC13 | `log.jsonl` contains `module.connect_streams` record per module during `OnConnectStreams` | Integration test |
| AC14 | `log.jsonl` contains `module.state.transition` record on each explicit lifecycle state transition (kInactive→kStarting→kActive→kStopping→kInactive) | Integration test |
| AC15 | `module.state.transition` records carry `from_state`, `to_state`, and `module_id` fields | Parse record, assert fields |
| AC16 | Module lifecycle completion records (`module.start.complete`, `module.stop.complete`) carry `duration_ms` | Integration test: start a module, parse log, assert `duration_ms` present |
| AC17 | `log.jsonl` contains `thread.join` record when a dedicated module thread joins on shutdown | Integration test |
| AC18 | Frame tick boundary logs are emitted at `kTrace` level (compile-out in Release) | Unit test: Debug build only |
| AC19 | All new log records carry `module_id` tag (where applicable) | Spot-check records for presence of `module_id` field in JSON |
| AC20 | `dia pipeline --target cluichetest` green in Debug + Release | Build + run |

---

## Instrumentation Sites

| # | Record / Channel | Level | Location | Fields | Note |
|---|----------------|-------|----------|--------|------|
| 1 | `asset` / `asset.catalog.load.start` | INFO | `AssetCatalog::Load` entry | `stage`, `asset_count` | — |
| 2 | `asset` / `asset.catalog.load.complete` | INFO | `AssetCatalog::Load` exit | `stage`, `asset_count`, `duration_ms` | Compute from start |
| 3 | `asset` / `asset.catalog.load.fail` | ERROR | `AssetCatalog::Load` error path | `stage`, `reason` | — |
| 4 | `asset` / `asset.type.registered` | DEBUG | Asset type registration | `type_name` | — |
| 5 | `asset` / `asset.record.created` | DEBUG | Asset record creation | `asset_id`, `type` | — |
| 6 | `asset` / `asset.record.deleted` | DEBUG | Asset record deletion | `asset_id` | — |
| 7 | `animation` / `anim.clip.loaded` | INFO | Animation clip load complete | `clip_id` | — |
| 8 | `animation` / `anim.clip.unloaded` | INFO | Animation clip unload | `clip_id` | — |
| 9 | `animation` / `anim.playback.start` | DEBUG | Playback start | `clip_id` | — |
| 10 | `animation` / `anim.playback.stop` | DEBUG | Playback stop | `clip_id` | — |
| 11 | `animation` / `anim.playback.loop` | DEBUG | Loop point crossed | `clip_id`, `loop_count` | — |
| 12 | `stream` / `stream.writer.connected` | INFO | EventStreamStore writer attach | `stream_id` | — |
| 13 | `stream` / `stream.writer.disconnected` | INFO | EventStreamStore writer detach | `stream_id` | — |
| 14 | `stream` / `stream.reader.connected` | INFO | EventStreamStore reader attach | `stream_id` | — |
| 15 | `stream` / `stream.reader.disconnected` | INFO | EventStreamStore reader detach | `stream_id` | — |
| 16 | `stream` / `stream.overflow` | WARNING | EventStreamStore drop/reject | `stream_id`, `policy` | — |
| 17 | `stream` / `stream.tap.attached` | DEBUG | Tap added to EventStreamStore | `stream_id` | — |
| 18 | `stream` / `stream.tap.detached` | DEBUG | Tap removed | `stream_id` | — |
| 19 | `module` / `module.configure` | DEBUG | `Module::OnConfigure` | `module_id` | — |
| 20 | `module` / `module.connect_streams` | DEBUG | `Module::OnConnectStreams` | `module_id` | — |
| 21 | `module` / `module.state.transition` | INFO | Each `ModuleState` enum transition | `module_id`, `from_state`, `to_state` | — |
| 22 | `module` / `module.start.complete` | INFO | Module reaches kActive | `module_id`, `duration_ms` | — |
| 23 | `module` / `module.stop.complete` | INFO | Module reaches kInactive | `module_id`, `duration_ms` | — |
| 24 | `module` / `thread.join` | INFO | Dedicated module thread joins | `module_id` | — |
| 25 | `module` / `module.frame.tick` | TRACE | Module tick boundary | `module_id` | Compile-out in Release |

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaAssetRuntime/AssetCatalog.h/.cpp` (or equivalent) | Add #1–#3 log calls |
| `Dia/DiaAssetRuntime/AssetTypeRegistry.cpp` (or equivalent) | Add #4 |
| `Dia/DiaAssetRuntime/AssetRecord.cpp` (or equivalent) | Add #5–#6 |
| `Dia/DiaAnimation2D/Animation2DClip.cpp` (or equivalent) | Add #7–#8 |
| `Dia/DiaAnimation2D/Animation2DPlayback.cpp` (or equivalent) | Add #9–#11 |
| `Dia/DiaStream/EventStreamStore.cpp` (or equivalent) | Add #12–#18 |
| `Dia/DiaApplicationFlow/Module.cpp` (or equivalent) | Add #19–#25 |
| All modified files | Add `#include <DiaObservation/Log/DiaLog.h>` |

> Exact file paths must be confirmed before task dispatch — some DiaApplicationFlow files are under active restructuring. The instrumentation targets whatever the current implementation files are.

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for identifiers | Log channels (`"asset"`, `"stream"`, `"module"`, `"animation"`) are `StringCRC`. `module_id`, `clip_id`, `asset_id`, `stream_id` fields in log records are `StringCRC`-derived values. |
| PD-002 | ProcessingUnit/Phase/Module architecture | Log call sites are added inside existing Module lifecycle methods (`OnConfigure`, `OnConnectStreams`, state transitions). No new types or modules introduced. |
| PD-003 | Component-based entities | Orthogonal. |
| PD-004 | No STL containers in public APIs | No new APIs. Call sites only; `DIA_LOG_*` already handles formatting internally. |
| PD-005 | x64 only | `std::chrono::steady_clock` for duration measurement. |
| PD-006 | VS project files source of truth | No new project files. Existing `.vcxproj` files are unmodified by this feature (call sites only, no new translation units). |
| PD-007 | C++20 required | No new C++20 constructs introduced; call sites use existing macros. |
| PD-008 | Directory.Build.props owns build settings | No overrides added. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | `log.jsonl` already written to session directory under `Cluiche/out/<App>/sessions/<id>/` by Feature #2. No change. |
| PD-010 | `.diagame` is project root | No new config entries required. |
| AD-001 | Module system with YAML frontmatter | No new modules. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | No new types added. |
| SD-O02 | DiaLogger folded — `DIA_LOG_*` from `<DiaObservation/Log/DiaLog.h>` | All call sites use the new include path. |
| SD-O05 | `schema_version: "1.0"` on every record | Handled automatically by `ObservationFileSink` — no per-call-site action needed. |
| SD-O16 | Session ID on every record | Handled automatically by `Logger` + `ObservationFileSink` — no per-call-site action needed. |
| SD-O27 | Logs default ON | Log calls in subsystems are active by default at their declared level (INFO/DEBUG/TRACE). No extra config needed for basic coverage. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | DiaApplicationFlow files are under active restructuring (some deleted in git status). Where do `OnConfigure`, `OnConnectStreams`, and `ModuleState` transitions live in the current layout? | Must confirm file paths before dispatching tasks #1 (module lifecycle). The scope names and field names are stable regardless of file structure. |
| OQ2 | Does DiaAnimation2D exist as a module? The system spec references it but it may not be implemented yet. | Confirm before dispatching tasks #7–#8. If the module doesn't exist, those tasks are blocked and noted as Deferred. |
| OQ3 | Duration on lifecycle events requires capturing a `steady_clock::now()` at start and computing delta at completion. Is there an existing mechanism on Module for this, or must it be added inline? | Add a `uint64_t mStartNs` field to Module if needed, set in the `kStarting` state entry, read at `kActive`. Minimal invasive change. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Volume | With DEBUG-level logs enabled, how many records per frame are expected? | ~25 lifecycle records per startup (one-time) + 0 per-frame for INFO. TRACE-level frame tick adds ~N records/frame where N = module count. TRACE compiles out in Release. At INFO level in production, log volume is near-zero after startup. |
| 2 | Channels | Do the channels (`"asset"`, `"stream"`, `"module"`, `"animation"`) need to be registered? | No — channels are `StringCRC` values; the logger routes them through level-filter infrastructure without a registration step. Unregistered channels use the global log level. |
| 3 | `module_id` tag | The system spec says "module_id tag on all domain logs". How is this formatted in the JSON record? | `log.jsonl` records carry `channel` and `msg`; there is no structured `module_id` field in the base schema. The `module_id` is embedded in the message string: `"module_id=RenderModule state=kActive"`. It is NOT a top-level JSON field. Future schema v2 can promote it. |
| 4 | Animation | `anim.playback.loop` fires per loop-crossing. At 60fps with a 0.5s clip, that's 120 loop events/sec. Is this too verbose? | Logged at DEBUG level, which is filtered out by default (INFO is the default level threshold). When enabled intentionally, 120 records/sec is ~12KB/sec in `log.jsonl` — acceptable for a debugging session. |
| 5 | Deferred subsystems | DiaAnimation2D and DiaAssetRuntime may not have full implementations yet. What happens to those tasks? | Mark the tasks for those subsystems `Deferred` if the implementation files don't exist. The instrumentation sites and field names are stable as spec — they land when the subsystem lands. Other tasks in this feature are independent and unblocked. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Confirm DiaApplicationFlow current layout; add module lifecycle logs (#19–#25) to Module.cpp and state machine | AC12–AC18 | Planned | haiku | Verify files exist per OQ1 before editing |
| 2 | Add asset catalog load logs (#1–#3) to AssetCatalog | AC1–AC3 | Planned | haiku | Confirm file path |
| 3 | Add asset type + record logs (#4–#6) | AC4–AC5 | Planned | haiku | |
| 4 | Add stream connect/disconnect/overflow/tap logs (#12–#18) to EventStreamStore | AC8–AC11 | Planned | haiku | |
| 5 | Add animation clip + playback logs (#7–#11) | AC6–AC7 | Planned | haiku | Deferred if DiaAnimation2D not present per OQ2 |
| 6 | Integration tests — AC1–AC19 | All ACs | Planned | sonnet | Requires CluicheTest run |
| 7 | Build verification — `dia pipeline --target cluichetest` | AC20 | Planned | haiku | |

---

## Status

`Approved` — 2026-05-19. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
