# Feature Spec: Domain-Level Trace Instrumentation

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **domain-trace-instrumentation** |

**Status:** `Approved` — 2026-05-19

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #4 (DiaTrace Spans) — `DIA_TRACE_ZONE` macro and `TraceRegistry` must exist. Feature #4 must be Done before implementation begins.

---

## Problem Statement

After Feature #4, the trace infrastructure exists — `DIA_TRACE_ZONE`, `TraceRegistry`, `trace.jsonl` — but no engine subsystem is instrumented yet. Without call sites, `trace.jsonl` is always empty. This feature adds `DIA_TRACE_ZONE` spans to subsystems where per-frame causality and lifecycle duration are meaningful: the application frame hierarchy, render path, animation evaluator, streams, and multi-frame lifecycle transitions (stage transitions, module startup/stop, asset catalog load). Profiling scopes (Feature #9) model per-frame cost; trace spans model causality and multi-frame lifetime.

---

## Solution Overview

This feature is purely additive — no new types, no new files in DiaObservation. Each instrumented subsystem gets `DIA_TRACE_ZONE(name, category)` call sites added at the boundaries where span causality and lifetime are meaningful. Categories are the `uint32_t` bitmask constants already declared in `Dia::Observation::Trace::Category` (Feature #4). All spans default OFF (SD-O27); users enable per-category via config.

**Span vs Scope distinction:** `DIA_TRACE_ZONE` is used where parent-child causality or multi-frame duration matters (a module startup spanning multiple frames, a stage transition covering a long sequence). `DIA_PROFILE_SCOPE` (Feature #9) is used for per-frame cost data. Both may exist at the same call site with different roles.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | With `observation.traces.categories.diaapplicationflow = true`, `trace.jsonl` contains `app.frame` spans covering the full frame duration | Integration test: run cluichetest, parse `trace.jsonl` |
| AC2 | `pu.update` span is child of `app.frame` (correct `parent_span_id` linkage) | Parse `trace.jsonl` |
| AC3 | `module.tick.<name>` spans are children of `pu.update` | Parse parent linkage |
| AC4 | With `observation.traces.categories.diagraphics = true`, `trace.jsonl` contains `canvas.renderframe` spans | Integration test |
| AC5 | `canvas.startframe`, `canvas.processframe`, `canvas.endframe` are children of `canvas.renderframe` | Parse parent linkage |
| AC6 | With `observation.traces.categories.diaanimation = true`, `trace.jsonl` contains `anim.evaluate` spans | Integration test: play an animation |
| AC7 | `anim.clip.sample` and `anim.blend` are children of `anim.evaluate` when applicable | Parse parent linkage |
| AC8 | With `observation.traces.categories.diastream = true`, `trace.jsonl` contains `stream.send` and `stream.consume` spans | Integration test |
| AC9 | With `observation.traces.categories.diaassetruntime = true`, `trace.jsonl` contains `stage.transition` spans for stage load sequences | Integration test: trigger a stage transition |
| AC10 | `module.startup.<name>` trace spans cover the multi-frame period from `BeginStart` → `kActive` or `kFailed` | Integration test: measure span duration |
| AC11 | `module.stop.<name>` trace spans cover `BeginStop` → `kInactive` | Integration test |
| AC12 | `asset.catalog.load` spans cover `loader.start` → rules engine complete | Integration test |
| AC13 | Category disabled: `kDiaGraphics` disabled + `kDiaApplicationFlow` enabled → no `canvas.*` spans, but `app.frame` present | Integration test |
| AC14 | `trace.jsonl` records from these spans carry correct `parent_span_id` linkage | Parse all parent IDs — no dangling references |
| AC15 | Excluded sites (FrameStreamStore atomic reads/writes, individual keyframe interpolation, asset registry lookups, individual draw command submission) produce NO trace spans — verified by absence | Integration test: search `trace.jsonl` for excluded names |
| AC16 | `dia pipeline --target cluichetest` green in Debug + Release | Build + run |

---

## Instrumentation Sites

| # | Span Name | Category | Location | Parent | Note |
|---|-----------|----------|----------|--------|------|
| 1 | `app.frame` | `kDiaApplicationFlow` | Main PU update outer boundary | root | Wraps the full frame |
| 2 | `pu.update` | `kDiaApplicationFlow` | `ProcessingUnit::Update` | `app.frame` | Per PU tick |
| 3 | `module.tick.<name>` | `kDiaApplicationFlow` | Module tick dispatch | `pu.update` | Dynamic name = module ID |
| 4 | `canvas.renderframe` | `kDiaGraphics` | Canvas render method outer | root (or `app.frame` on render thread) | — |
| 5 | `canvas.startframe` | `kDiaGraphics` | Canvas start-frame sub-step | `canvas.renderframe` | — |
| 6 | `canvas.processframe` | `kDiaGraphics` | Draw-submission sub-step | `canvas.renderframe` | — |
| 7 | `canvas.endframe` | `kDiaGraphics` | Present/swap sub-step | `canvas.renderframe` | — |
| 8 | `anim.evaluate` | `kDiaAnimation` | Animation evaluator outer | root | Per-frame evaluation pass |
| 9 | `anim.clip.sample` | `kDiaAnimation` | Per-clip sampling inside evaluator | `anim.evaluate` | Dynamic name = clip ID |
| 10 | `anim.blend` | `kDiaAnimation` | Blend tree evaluation | `anim.evaluate` | — |
| 11 | `stream.send` | `kDiaStream` | `EventStreamStore` fan-out entry | root (or parent) | Per send; causality not per-frame |
| 12 | `stream.consume` | `kDiaStream` | `EventStreamStore` drain entry | root | Per drain |
| 13 | `stage.transition` | `kDiaApplicationFlow` | `ApplyPendingTransition` → drain complete | root | Multi-frame span |
| 14 | `module.startup.<name>` | `kDiaApplicationFlow` | `BeginStart` → `kActive`/`kFailed` | root | Multi-frame span |
| 15 | `module.stop.<name>` | `kDiaApplicationFlow` | `BeginStop` → `kInactive` | root | Multi-frame span |
| 16 | `asset.catalog.load` | `kDiaAssetRuntime` | Loader start → rules engine complete | root | May span multiple frames |

**Excluded sites (too fast, no children — must NOT be instrumented):**
- `FrameStreamStore` atomic read/write
- Individual keyframe interpolation
- Asset registry lookups
- Individual draw command submission

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaApplicationFlow/ProcessingUnit.cpp` (or equiv) | Add `app.frame` + `pu.update` span |
| `Dia/DiaApplicationFlow/ApplicationProcessingUnit.cpp` (or equiv) | Add `module.tick.<name>` span in module dispatch |
| `Dia/DiaApplicationFlow/ApplicationPhase.cpp` (or equiv) | Add `stage.transition` span |
| `Dia/DiaApplicationFlow/Module.cpp` (or equiv) | Add `module.startup.<name>` + `module.stop.<name>` multi-frame spans |
| `Dia/DiaGraphics/Canvas.cpp` (or equiv) | Add `canvas.renderframe` + sub-step spans |
| `Dia/DiaAnimation2D/Animation2DEvaluator.cpp` (or equiv) | Add `anim.evaluate` + `anim.clip.sample` + `anim.blend` spans |
| `Dia/DiaStream/EventStreamStore.cpp` (or equiv) | Add `stream.send` + `stream.consume` spans |
| `Dia/DiaAssetRuntime/AssetCatalog.cpp` (or equiv) | Add `asset.catalog.load` span |
| All modified files | Add `#include <DiaObservation/Trace/DiaTrace.h>` |

> Exact file paths must be confirmed before task dispatch — some DiaApplicationFlow files are under active restructuring. Span names and categories are stable regardless.

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for identifiers | Span names are `StringCRC`. Dynamic names (module ID, clip ID) are `StringCRC::GetString()` values wrapped in `StringCRC(...)` at call site. |
| PD-002 | ProcessingUnit/Phase/Module architecture | Span call sites are placed inside existing ProcessingUnit, Phase, and Module methods. No new types or structural changes to the PU/Phase/Module hierarchy. |
| PD-003 | Component-based entities | Orthogonal. |
| PD-004 | No STL containers in public APIs | No new APIs. `DIA_TRACE_ZONE` already handles everything via RAII. |
| PD-005 | x64 only | No new platform-specific constructs; all call sites are thin macro wrappers. |
| PD-006 | VS project files source of truth | No new project files. Existing `.vcxproj` files are unmodified (call sites only, no new translation units). |
| PD-007 | C++20 required | No new C++20 constructs introduced; call sites use existing macros from Feature #4. |
| PD-008 | Directory.Build.props owns build settings | No overrides added. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | `trace.jsonl` already written to session directory under `Cluiche/out/<App>/sessions/<id>/` by Feature #4. No change. |
| PD-010 | `.diagame` is project root | No new config entries required. Category enable/disable is handled by existing Feature #4 config block. |
| AD-001 | Module system with YAML frontmatter | No new modules. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | No new types. Category constants already in `Dia::Observation::Trace::Category`. |
| SD-O06 | OTel wire-format, no SDK | Span names chosen to be meaningful to tooling that reads `trace.jsonl`. Field names produced by Feature #4 infrastructure — no per-call-site action needed. |
| SD-O25 | Both `DIA_TRACE_ZONE` and `DIA_PROFILE_SCOPE` coexist | Instrumented sites use `DIA_TRACE_ZONE` for causality/lifetime. Feature #9's `DIA_PROFILE_SCOPE` sites for cost-within-frame may coexist at the same call sites — both correct and complementary. |
| SD-O27 | Traces default OFF | Call sites are ~1ns no-ops until config enables the category (ActiveMask AND = 0). |
| SD-O28 | Trace zones use bitmask categories, not StringCRC channels | All call sites pass `Category::k*` constants. |
| SD-O29 | Category constants as uint32_t bitmasks | Uses `Dia::Observation::Trace::Category` constants from Feature #4. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | `module.startup.<name>` and `module.stop.<name>` are multi-frame spans. RAII `ScopedZone` works within a single scope — how is a multi-frame span managed? | Use `DIA_TRACE_ZONE_NAMED(var, name, category)` on the Module object itself (member field of type `ScopedZone`), opened in `BeginStart()` and closed (member destroyed) in `DoStop()`. This requires `ScopedZone` to be storable as a class member — confirm with Feature #4 API. If not, use an explicit `Tracer::OpenSpan`/`CloseSpan` pair (which may need to be added in Feature #4 as a minor amendment). |
| OQ2 | DiaApplicationFlow files are under active restructuring. Where do `ProcessingUnit::Update`, module dispatch, and `BeginStart`/`BeginStop` live now? | Confirm file layout before dispatching tasks. |
| OQ3 | DiaAnimation2D — does `Animation2DEvaluator` exist? | If not, those tasks are `Deferred`. |
| OQ4 | `app.frame` span — is there a single outer frame boundary method that wraps `ProcessingUnit::Update`? | Confirm whether `app.frame` wraps a single method or needs to be placed at the PU tick driver level. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Per-frame overhead | With `kDiaApplicationFlow` enabled and 4 modules, what is the trace overhead per frame? | ~4 `module.tick.*` spans + 1 `app.frame` + 1 `pu.update` = 6 spans × ~200ns = ~1.2µs/frame. At 60fps this is 72µs/sec = 0.004% of the frame budget. When disabled: 6 × ~1ns = ~6ns/frame. |
| 2 | Multi-frame spans | A `module.startup.<name>` span spans multiple frames. Does it appear in `trace.jsonl` before the module reaches `kActive`? | No — `ScopedZone` only writes a closed span record on destructor. The span is pending in the thread-local open-span stack until the zone's scope ends (or the member is destroyed). If the span stays open longer than the per-thread ring can hold, `MaxOpenSpans` protection kicks in (Feature #4 AC12/AC13). Module startup typically takes <5 frames (well within MaxOpenSpans = 64). |
| 3 | `anim.clip.sample` | This fires per-clip per evaluator pass. At 10 clips × 60fps, that's 600 spans/sec. Is this manageable? | 600 × 200ns = 120µs/sec ≈ 0.001% of 60fps budget. When disabled (default): 600 × 1ns = 600ns/sec. File output: 600 spans × ~250 bytes = ~150KB/sec in `trace.jsonl` — notable but acceptable for a debugging session. |
| 4 | Deferred subsystems | DiaAnimation2D may not exist yet. What happens? | Tasks 6–7 are marked Deferred. They land when DiaAnimation2D is implemented. Other tasks are independent. |
| 5 | Excluded sites | How is "excluded" enforced? | By not adding call sites. The verification ACs (AC15) check that the excluded span names do NOT appear in `trace.jsonl` during an integration test run. If any appear, it is a spec violation. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Confirm DiaApplicationFlow layout; add `app.frame` + `pu.update` spans | AC1, AC2 | Planned | haiku | Verify files per OQ2 |
| 2 | Add `module.tick.<name>` spans in module dispatch | AC3, AC13 | Planned | haiku | |
| 3 | Add `module.startup.<name>` + `module.stop.<name>` multi-frame spans | AC10, AC11 | Planned | sonnet | Multi-frame RAII per OQ1 — confirm ScopedZone can be a member |
| 4 | Add `stage.transition` span in `ApplyPendingTransition` | AC9 | Planned | haiku | |
| 5 | Add canvas render spans — `canvas.renderframe` + sub-steps | AC4, AC5 | Planned | haiku | |
| 6 | Add animation evaluator spans — `anim.evaluate`, `anim.clip.sample`, `anim.blend` | AC6, AC7 | Planned | haiku | Deferred if DiaAnimation2D absent per OQ3 |
| 7 | Add stream spans — `stream.send`, `stream.consume` | AC8 | Planned | haiku | |
| 8 | Add `asset.catalog.load` span | AC12 | Planned | haiku | |
| 9 | Verify excluded sites produce no spans (AC15) | AC15 | Planned | haiku | Search `trace.jsonl` for excluded names |
| 10 | Integration tests — AC1–AC15 | All ACs | Planned | sonnet | Requires CluicheTest run with all categories enabled |
| 11 | Build verification | AC16 | Planned | haiku | |

---

## Status

`Approved` — 2026-05-19. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
