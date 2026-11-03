**Spec:** @docs/specs/applications/dia/systems/diacellhistory/scrub-and-timeline.md
**Status:** In Progress

Feature 2 of DiaCellHistory (system order 1 → 2 → 4 → 3 → 5). **Prerequisite:** Feature 1 Done (it is). TDD throughout (RED proven with quoted output). Commit after each task. Models are sonnet or haiku only; any opus use needs the user's confirmation first.

**Verification policy (agreed with the user 2026-11-03):** per task and per feature, scoped GoogleTests only (`--filter` including fixture-named suites, `--shards >= 2`). The full GoogleTest run, `dia check sanitizer`, the Release build and the full E2E are NOT run per feature; they live in the system plan's final verification task, except the CellLayers + CellHistory E2E, which runs once after this feature (task 2.10). Per feature: Observation Opportunity Scan (notes only) and a short gap audit with edge tests; mutation checks apply to this feature's logic (2.2, 2.4, 2.6).

**Facts carried from the system plan (checked 2026-10-29, re-verify in 2.1):**
- `IViewValueSource` is already built in `DiaCellLayers/IViewValueSource.h` with `GetPresentation`; the built signature is `GetValues(const CellLayerDesc&, float*, uint32)` and `ViewEpoch()` returns `uint64_t`. The spec text (`StringCRC`, `uint32`) is stale: amend the Feature 2 and 4 specs to the code in 2.1 (the system plan authorises this amendment).
- `LayerUploader` consults the source; whether `LayerReader` does is unverified (2.1). If it does not, the fix is a DiaCellLayers edit and needs AD-001 sign-off from the user.
- `cells.view.state` is a DiaCellLayers Feature 5 topic; Feature 5 is built (streams migration Done), so 2.9 can wire the `history` field into the topic.
- Feature 1 recorder read API (`CellValues()`, samples, bands, events, `firstRetained`) is the data source; live reads never go through a view source.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 2.1 | **Spec amendment and hook audit:** amend Feature 2 and 4 specs to the built `IViewValueSource` signature; audit that `LayerReader`, the uploader, hover, query, inspector and `cells.layer.values` all consult the source and key caches on `(version, ViewEpoch)`; list gaps | Written audit; failing tests for each gap found | Done | sonnet | A gap in `LayerReader` is a DiaCellLayers edit: stop and ask the user for AD-001 sign-off Done 2026-11-03 (`ddf32976`): specs amended; audit found 2 gaps, fixed in 2.1a/2.1b (AD-001 given 2026-11-03). |
| 2.1a | **DiaCellLayers fix (AD-001 given):** `InspectorState` dirty checks (`NeedsCompute`, `NeedsComputeA`, `seenVersion`, ~`InspectorState.cpp:296-308,363`) also compare (fromSource, ViewEpoch) so selection stats recompute on a view change | RED-GREEN: existing failing `SelectionInspectorStateTest.ScrubbedSourceEpochChangeRecomputesStats`; scoped `*Inspector*:*CellLayer*:*Selection*:*Viewer*` | Done | sonnet | Blocks 2.5. Done 2026-11-03: `SourceKey()` (0 = live/no source, else epoch+1) added to B and A dirty checks; scoped 443 tests pass; A-side test added after the fix (never seen RED) |
| 2.1b | **DiaCellLayers fix (AD-001 given):** `cells.select.stats` cache `StatsKey` (`MirrorQueries.cpp:300-320`) includes epoch + live flag | RED-GREEN: mirror test that a view change within a frame re-reads | Done | sonnet | Done 2026-11-03: `InspectorState::SourceKey` (now public) pushed into the stats key; new test via real `StatsRig`; scoped 493 tests pass |
| 2.2 | `HistoryViewState` machine: Live / Sample(n); stays on its sample as new ones arrive; latest-sample scrub is not live; evicted-window notice (AC 1, 2, 6) | RED-GREEN: transitions, stay-put, window passes the viewed sample | Pending | sonnet | Delta mode is added in Feature 4 |
| 2.3 | `HistoryValueSource : IViewValueSource`: dequantised ring values, false for untracked / evicted, `IsLive`, `ViewEpoch` bump on any view change, second install rejected (Error) (AC 5, 9) | RED-GREEN with fake registry | Pending | sonnet | Installed on the `CellLayerRegistry` |
| 2.4 | Commands `cells.history.view`, `live`, `step <n> [band]`, `event prev\|next [type]` with JSON results and error cases (AC 3, 4) | RED-GREEN: samples, bands, clamps, type filter, every error unchanged | Pending | sonnet | |
| 2.5 | Viewer integration: scrubbed values reach hover, query, inspector, `cells.layer.values`; registry `IsLive()` false while scrubbed; "no per-cell history" state with Track button; sample badge on hover (AC 5, 7, 12, 14) | Fixture integration tests with the real viewer domain | Pending | sonnet | Cost bound: one read and one upload per view change at Large |
| 2.6 | `TimelineModel`: range, buckets (decide fixed 256 vs pixel-based: Open Q2, raise with the user if unclear), band ticks, coloured event ticks (`ui.colour`), gaps, pre-window dim, sparkline <= 256 points from aggregates, rebuilt only on new sample or view change (AC 10, 11) | RED-GREEN: bucketing, downsampling, gaps, rebuild trigger | Pending | sonnet | |
| 2.7 | Observation: metrics (view changes, scrub seconds, timeline rebuild ms), `DIA_PROFILE_SCOPE` on view change and rebuild, Info / Warning logs | RED-GREEN: metric values | Pending | haiku | |
| 2.8 | HTML mockup check: confirm `cell-layer-viewer.html` timeline; **user approves it as the visual gate before 2.9** | User approval | Pending | haiku | Mockup must be approved before an editor feature is built |
| 2.9 | Panel timeline card + banner ("Viewing sample N ... not live", Back to live), step / event / live buttons, slider runs `cells.history.view`; contract golden updated; both `debug-panel.html` pages; `cells.view.state.history` field `null` or `{mode, sample, label}` (AC 8, 13, 15, 16) | Golden + page tests; visual gate | Pending | sonnet | Blocked on 2.8 approval |
| 2.10 | CluicheTest wiring + E2E: stage creates the module, samples on a timer, E2E scrubs, steps, jumps to events, asserts viewed values and banner; perf tests (view change, rebuild at 2,080 samples) | `dia test e2e --scenario="*celllayers*"` (run once, after a `compile-code` pipeline) | Pending | sonnet | Quiet machine only (known load-sensitive flake) |
| 2.11 | Finalize: `dia check deps`, STL grep, module doc, Observation Opportunity Scan (notes), gap audit + edge tests, scoped GoogleTests, `dia docs spec-done` | Verify gate | Pending | haiku | Not Done until 2.9's visual gate is approved. No full suite / sanitizer / Release here (system task S7) |

## Hook audit (2026-11-03)

Task 2.1. Spec text amended to the built signature (`scrub-and-timeline.md` hook block, `compare.md` hook addition). Audit by code reading; paths under `Dia/DiaCellLayers/`.

| Consumer | Consults source | Keys on (version, ViewEpoch) | GetValues false = no data, no live fallback | Verdict |
|---|---|---|---|---|
| LayerUploader | Yes, `LayerUploader.cpp:108,149` (from `registry.ViewValueSource()`) | Yes, gate at `:139` also keys fromSource; epoch stored `:194` | Yes, `:151-154` releases slot, returns `NoData` | OK |
| LayerReader | Yes, `LayerReader.cpp:127-128,156` | Yes, `:139-140` (version, epoch, fromSource) | Yes, `:156,170` hasData=false, values=nullptr | OK |
| Hover | Indirect: `CellLayerViewerDomain.cpp:296-310` reads `LayerUploader::BaseValues` (copy of the uploaded, i.e. viewed, values) | Inherits the uploader gate | `UpdateHover` returns invalid unless result is `Ok` (`:299`), so NoData shows no hover | OK |
| Query | Yes, `QueryEvaluator.cpp:127` via `LayerReader` | Via reader | Reader hasData flag | OK (test `TestQueryCommands.cpp:524`) |
| Selection tools / category | Yes, `SelectionTools.cpp:258`, `SelectionStats.cpp:159,190,194` via reader | Via reader | Reader hasData flag | OK |
| Inspector single-cell / card | Yes, `InspectorModel.cpp:66` via reader (re-read each build) | Via reader | Test `TestInspectorModel.cpp:162` | OK |
| **Inspector stats cache** | Reads go via reader, but recompute is not triggered | **No.** `InspectorState.cpp:296-308` (`NeedsCompute`, `NeedsComputeA`) and `:363` compare layer `version()` only | n/a | **GAP 1** |
| `cells.layer.values` | Yes, `MirrorQueries.cpp:151` via reader | Via reader | `:152` returns error "layer has no data in this view" (test `TestMirrorLayerValues.cpp:707`) | OK |
| `cells.select.stats` mirror cache | Reads via reader | **No.** Cache key `MirrorQueries.cpp:300-320` pushes `version()` only; no `ViewEpoch()` or `IsLive()` | n/a | **GAP 2** (low: entries are also per-frame, `:331`, and depend on GAP 1's inspector for Layers mode) |
| Registry reject second source | `CellLayerRegistry.cpp:46-50` logs `DIA_LOG_ERROR("CEL", ...)`, returns false | n/a | n/a | OK (test `TestViewValueSource.cpp:128`) |
| Registry `IsLive()` | `CellLayerRegistry.h:108` = no source or `source->IsLive()` | n/a | n/a | OK (tests `TestViewValueSource.cpp:137`, `TestCellLayerRegistryEdge.cpp:247`) |

### Gap list

- **GAP 1: InspectorState dirty check ignores the view source.** While scrubbed (installed, not live, or `ViewEpoch()` changed) with an unchanged layer `version()`, the shown selection stats (mean/min/max/histogram, A/B stats) stay at the previously computed values. Failing test added: `SelectionInspectorStateTest.ScrubbedSourceEpochChangeRecomputesStats` in `Cluiche/Tests/GoogleTests/DiaCellLayers/TestInspectorState.cpp` (RED: mean stays 2.5, expected 25 then 150). Fix: `NeedsCompute`/`NeedsComputeA` and the stored `seenVersion` compare an additional (fromSource, epoch) pair. **Needs DiaCellLayers production edit (AD-001 sign-off).**
- **GAP 2: `cells.select.stats` cache key omits `ViewEpoch()`/IsLive.** `MirrorQueries.cpp:300-320`. Bounded to one frame by the `mFrame` check, so only a same-frame view change returns a stale answer. Not tested (needs the full mirror rig; the same-frame window makes it hard to observe). Fix: push `registry.ViewValueSource()` epoch and live flag into `StatsKey`. **Needs DiaCellLayers production edit (AD-001 sign-off).**
- **Not a gap, noted:** hover depends on the uploader result being `Ok`; during NoData there is deliberately no hover value.
- **Not a gap, noted:** `LayerReader` cache is per-reader slot (LRU); source epoch wrap is covered by `TestViewSourceModuleEdge.cpp:72`.

Task 2.5 must not be marked Done with GAP 1 open (inspector would show live stats under a "viewing sample N" banner).
