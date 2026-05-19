# Feature Spec: DiaProfiling — Domain Instrumentation

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **profiling-domain-instrumentation** |

**Status:** `Approved` — 2026-05-19

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #8 (DiaProfiling — Frame Instrumentation) — the `DIA_PROFILE_SCOPE` macro and `Profiler` singleton must exist. Feature #8 must be Done before implementation begins.

---

## Problem Statement

Feature #8 delivered the profiling infrastructure, but no engine subsystem is instrumented yet. Without call sites, `profile.jsonl` is always empty. This feature adds `DIA_PROFILE_SCOPE` to the four primary instrumentation sites identified in the system spec: the ProcessingUnit/Phase/Module tick hierarchy (frame backbone), DiaGraphics canvas render path, DiaStream fan-out/drain, and DiaAssetRuntime catalog load path. These sites produce the actionable per-frame timing data needed to diagnose frame-time regressions.

## Solution Overview

This feature is purely additive — no new types, no new files in `DiaObservation/`. Each instrumented subsystem gets one or more `DIA_PROFILE_SCOPE` call sites added to its hot path. Category constants are already declared in `ProfileCategory.h` (Feature #8). The `DIA_PROFILE_SCOPE_METRIC` variant is used at module-tick sites where a per-module-type histogram may be registered via Feature #13, eliminating redundant timing calls.

Exact file locations must be confirmed before task dispatch — some DiaApplicationFlow files are under active restructuring.

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | With `observation.profile.categories.diaapplicationflow = true`, `profile.jsonl` contains `pu.update`, `phase.update.<name>`, and `module.tick.<name>` records during a CluicheTest run | Integration test: run cluichetest, parse `profile.jsonl`, assert expected scope names |
| AC2 | `pu.update` scope is the root scope for its frame tree on the main PU thread (`parent_scope_id == 0`) | Parse `profile.jsonl`, assert all `pu.update` entries have `parent_scope_id: 0` |
| AC3 | `module.tick.<name>` scopes have `parent_scope_id` matching the enclosing `pu.update` or `phase.update.*` scope on the same thread | Parse parent linkage from `profile.jsonl` |
| AC4 | With `observation.profile.categories.diagraphics = true`, `profile.jsonl` contains `canvas.renderframe` records | Integration test |
| AC5 | `canvas.startframe`, `canvas.processframe`, `canvas.endframe` scopes are children of `canvas.renderframe` (correct `parent_scope_id` linkage) | Parse parent linkage |
| AC6 | With `observation.profile.categories.diastream = true`, `profile.jsonl` contains `stream.send` and `stream.consume` records when stream events fire | Integration test: trigger stream events, assert records |
| AC7 | With `observation.profile.categories.diaassetruntime = true`, `profile.jsonl` contains `asset.catalog.load` records during a stage load | Integration test: load a stage, assert records |
| AC8 | `asset.load.<type>` records appear nested under `asset.catalog.load` with correct `parent_scope_id` | Parse parent linkage |
| AC9 | Category disabled: `kDiaGraphics` disabled + `kDiaApplicationFlow` enabled → no `canvas.*` records, but `pu.update` records present | Integration test |
| AC10 | `dia pipeline --target cluichetest` green in Debug + Release with all profiling categories enabled | Build + run |

## Instrumentation Sites

| # | Scope Name | Category | Location | Note |
|---|-----------|----------|----------|------|
| 1 | `pu.update` | `kDiaApplicationFlow` | `ProcessingUnit::Update` entry | Root scope for the frame tree on its thread |
| 2 | `phase.update.<name>` | `kDiaApplicationFlow` | `ApplicationPhase::Update` entry | Dynamic name = phase StringCRC |
| 3 | `module.tick.<name>` | `kDiaApplicationFlow` | Module tick dispatch in `ApplicationProcessingUnit` | Dynamic name = module `kUniqueId`; use `DIA_PROFILE_SCOPE_METRIC` where histogram registered |
| 4 | `canvas.renderframe` | `kDiaGraphics` | Canvas render method outer scope | Root for render sub-tree |
| 5 | `canvas.startframe` | `kDiaGraphics` | Start-frame sub-step | Child of `canvas.renderframe` |
| 6 | `canvas.processframe` | `kDiaGraphics` | Draw-submission sub-step | Child of `canvas.renderframe` |
| 7 | `canvas.endframe` | `kDiaGraphics` | Present/swap sub-step | Child of `canvas.renderframe` |
| 8 | `stream.send` | `kDiaStream` | `EventStreamStore` fan-out entry | Per send call |
| 9 | `stream.consume` | `kDiaStream` | `EventStreamStore` drain entry | Per consume call |
| 10 | `asset.catalog.load` | `kDiaAssetRuntime` | `AssetCatalog` load method entry | Root for asset load tree |
| 11 | `asset.load.<type>` | `kDiaAssetRuntime` | Per-type asset loader dispatch | Dynamic name = asset type string |

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaApplicationFlow/ProcessingUnit.cpp` (or equivalent) | Add `DIA_PROFILE_SCOPE("pu.update", Category::kDiaApplicationFlow)` |
| `Dia/DiaApplicationFlow/ApplicationPhase.cpp` (or equivalent) | Add `DIA_PROFILE_SCOPE` per phase update |
| Module tick dispatch file in `DiaApplicationFlow` | Add `DIA_PROFILE_SCOPE_METRIC` per module tick |
| `Dia/DiaGraphics/Canvas.cpp` (or equivalent) | Add `canvas.renderframe` + sub-step scopes |
| `Dia/DiaStream/EventStreamStore.cpp` (or equivalent) | Add `stream.send` + `stream.consume` scopes |
| `Dia/DiaAssetRuntime/AssetCatalog.cpp` (or equivalent) | Add `asset.catalog.load` + `asset.load.<type>` scopes |
| CluicheTest `.diagame` | Add per-category toggles under `observation.profile.categories` |

> All modified files must `#include <DiaObservation/Profile/DiaProfile.h>` and link against `DiaObservation`.

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Scope names are `StringCRC`. Dynamic names (module ID, asset type) are computed as `StringCRC` at call site. |
| PD-004 | No STL containers in public APIs | No new APIs added. Call sites only. |
| AD-003 | Namespace `Dia::<Module>::` | No new types. `Category::k*` constants already in `Dia::Observation::Profile::Category`. |
| SD-O24 | Profiling is 5th pillar, frame-structured | All call sites use `DIA_PROFILE_SCOPE`; none use `DIA_TRACE_ZONE` (which would be wrong for per-frame cost data). |
| SD-O25 | Both macros coexist | Instrumented sites use `DIA_PROFILE_SCOPE` for per-frame cost. `DIA_TRACE_ZONE` from Feature #11 may add separate trace spans at the same sites for causality tracking — both are correct and complementary. |
| SD-O27 | Profiling defaults OFF | Call sites are ~1ns no-ops until config enables the category. |
| SD-O28 | Category is a bitmask integer | All call sites pass one of the `ProfileCategory::Category::k*` constants. |
| SD-O30 | Metric bridge at dual-instrumentation hot sites | `DIA_PROFILE_SCOPE_METRIC` used at `module.tick.<name>` sites where a histogram will be registered by Feature #13. |

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | DiaApplicationFlow files (ApplicationPhase.cpp, ApplicationProcessingUnit.cpp) appear deleted in git status. Where does the equivalent tick dispatch live now? | Confirm current file layout in DiaApplicationFlow before dispatching tasks 1–3. The instrumentation targets whatever the current implementation files are — scope names are stable even if files move. |
| OQ2 | `canvas.startframe` / `canvas.processframe` / `canvas.endframe` — do discrete sub-step methods exist in Canvas.cpp? | Confirm during task 4. If sub-steps are not discrete method calls, the instrumentation wraps the logical sections by inspecting the existing code. |
| OQ3 | `module.tick.<name>` uses a dynamic scope name built from `module.kUniqueId`. Is `kUniqueId` a `StringCRC`? | Yes per codebase conventions (`kUniqueId` is always `StringCRC`). The macro call is `DIA_PROFILE_SCOPE_METRIC(module.GetUniqueId().GetString(), kDiaApplicationFlow, histogram)` or equivalent. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Overhead | With `kDiaApplicationFlow` enabled and 4 active modules, what is the per-frame profiling overhead? | ~4 module scopes + 1 `pu.update` + N phase scopes × ~37ns each = ~250ns/frame. Below measurement noise for a 16ms frame budget. When disabled: ~1ns per call site = ~5ns total. |
| 2 | DiaStream | `stream.send` fires once per event fan-out. At high event rates (1000/sec), what is the profiling overhead? | 1000 × 37ns = 37µs/sec ≈ 0.002% of a 60fps budget. When disabled: 1000 × 1ns = 1µs/sec. Acceptable. |
| 3 | Asset loading | `asset.load.<type>` uses a dynamic scope name. Is `StringCRC` construction at load time acceptable? | Load paths are not per-frame hot paths. `StringCRC` construction is ~10ns (hash compute). For a load operation taking >1ms, a 10ns name hash is immeasurable. |
| 4 | `module.tick.<name>` | The `DIA_PROFILE_SCOPE_METRIC` variant requires a histogram pointer. What if the histogram hasn't been registered yet (Feature #13 not done)? | Pass `nullptr` — `DIA_PROFILE_SCOPE_METRIC` with `histogram == nullptr` behaves identically to `DIA_PROFILE_SCOPE` (AC15 of Feature #8). When Feature #13 lands, the histogram pointer is wired up without changing the macro call site. |
| 5 | Deleted files | If DiaApplicationFlow files are restructured, do the scope names change? | Scope names (`pu.update`, `module.tick.<name>`) are defined by this spec, not by the file structure. The file changes; the scope name string remains the same. Consumers (E2E, tooling) depend on the stable scope names, not the file paths. |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Confirm DiaApplicationFlow current file layout; instrument `ProcessingUnit::Update` with `pu.update` scope | AC1, AC2 | Planned | haiku | Verify file path before edit |
| 2 | Instrument `ApplicationPhase::Update` (or equivalent) with `phase.update.<name>` | AC1, AC3 | Planned | haiku | |
| 3 | Instrument module tick dispatch with `module.tick.<name>` using `DIA_PROFILE_SCOPE_METRIC` (histogram = nullptr until Feature #13) | AC1, AC3, AC9 | Planned | haiku | |
| 4 | Instrument Canvas render path — `canvas.renderframe` + sub-steps | AC4, AC5 | Planned | haiku | Confirm sub-step method boundaries per OQ2 |
| 5 | Instrument `EventStreamStore` send + drain | AC6 | Planned | haiku | |
| 6 | Instrument `AssetCatalog` load path — `asset.catalog.load` + `asset.load.<type>` | AC7, AC8 | Planned | haiku | Dynamic type name per OQ3 |
| 7 | Update CluicheTest `.diagame` — add category toggles | AC9, AC10 | Planned | haiku | |
| 8 | Integration tests — AC1–AC9 | All ACs | Planned | sonnet | Requires CluicheTest run with profiling enabled |

---

## Status

`Approved` — 2026-05-19. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
