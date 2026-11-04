**Spec:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md
**Status:** Pending

Build order 1 → 2 → 4 → 3 → 5. TDD throughout (RED proven with quoted output). Commit after each task. Plans are one file for now; split a feature into its own `<feature>.plan.md` when it is scheduled with `/implement`. Models are sonnet or haiku only; any opus use needs the user's confirmation first.

**Done means (per feature):** the `CellLayersViewer` stage E2E scenario (`Cluiche/Tests/E2E/scenarios/cluichetest/celllayers/`) is extended, or for Feature 1 (no UI) a stage-driven recorder smoke is added; `dia check deps` clean; STL grep clean; module doc updated; Observation Opportunity Scan; `dia docs spec-done` on the feature spec.

## Findings that change the plan (checked against the code 2026-10-29)

1. **`IViewValueSource` is already built** (`DiaCellLayers/IViewValueSource.h`, with `GetPresentation`; `LayerUploader` consults it). Its signature differs from the Feature 2/4 specs: `GetValues(const CellLayerDesc&, float*, uint32)` (spec says `StringCRC`) and `ViewEpoch()` returns `uint64_t`. **Amend the specs to the code** (Task 2.1); no DiaCellLayers hook work and no new sign-off for the hook itself. Still to verify in 2.1: `LayerReader` (query, inspector, `cells.layer.values`) consults the source, not only the uploader.
2. **`history.aggregate` is not an accepted schema key.** `ValueSchemaLoader.cpp` `kHistoryKeys` is `{track, group}`. Feature 1 AC 4 needs `history.aggregate: false`. This is a DiaValueSchema edit; **AD-001 sign-off given 2026-10-29**, built in Feature 1 Task 2.
3. **`history.group` lives in the `history` block, not `ui`** (system spec text says `ui`). Feature 1 already says `history.group`; amend the system spec line.
4. `ValueSchemaService::FindEvent` and `ExportSubset` already exist, so Feature 5's two schema dependencies are met; the backlog "FindEvent sign-off" is a formality for the AD-001 record only.
5. `CircularBufferC` is compile-time sized; the rings need a runtime-sized store (Task 1.4). That is a new DiaCellHistory-private type, not a DiaCore change.
6. Feature 3 end-to-end proof needs the CoW `ISnapshotStore` adapter over E1 (separate backlog row). Dia work uses a fake store.

## Sign-offs and prerequisites (raise before the task that needs them)

| Needed | For | Status |
|---|---|---|
| AD-001: add `aggregate` to `kHistoryKeys` (DiaValueSchema) | Feature 1 Task 2 | **Given 2026-10-29** |
| AD-001: new module `Dia/DiaCellHistory/` | Task 1.1 | Covered by the approved system spec; confirm |
| AD-001: `FindEvent` use / `ExportSubset` use | Tasks 1.8, 5.x | Both exist; record the use |
| DiaCellLayers Feature 5 (`cells.view.state` topic) | Task 2.9 `history` field; base64 conventions for Feature 5 | Feature 5 not built: do 2.9 as a stub-friendly task or build DiaCellLayers 5 first |
| DiaCellLayers Feature 3 (query) | Compare AC 7 (`vegetation < -0.1` over a delta) | Not built: its plan is next in DiaCellLayers; build before Task 4.8 |

## System tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| S1 | Feature 1: Recorder and tiers | [recorder-and-tiers.plan.md](recorder-and-tiers.plan.md) | Pending | sonnet | Needs DiaCellLayers Feature 1 (Done); plan written 2026-10-29 |
| S2 | Feature 2: Scrub and timeline | [scrub-and-timeline.plan.md](scrub-and-timeline.plan.md) | Done | sonnet | Hook exists; amend spec; DiaCellLayers 5 for the `history` field |
| S3 | Feature 4: Compare | [compare.plan.md](compare.plan.md) | In Progress | sonnet | Needs Feature 3's cell file for snapshot sides: build 4 with snapshot sides last, or build 3 first (see order note) |
| S4 | Feature 3: Snapshots | [snapshots.plan.md](snapshots.plan.md) | In Progress | sonnet | Fake store in Dia; CoW adapter is a separate row |
| S5 | Feature 5: Export | [export.plan.md](export.plan.md) | In Progress | sonnet | `HistoryEncoding` is reused by Feature 3's cell file, so Task 5.1 can be pulled forward |
| S6 | System close: CoW proof hooks, docs, backlog | module doc, `dia docs registry`, backlog | Pending | haiku | System spec Done only when all child specs are Approved |
| S7 | **Final verification** (agreed 2026-11-03: heavy checks run once, not per feature): full `dia run googletest --shards 2` (or more); `dia check sanitizer` (plus one mid-run after Feature 3); Release build and the Feature 1 AC 15 perf gate that was dropped; CellLayers + CellHistory E2E (also run once after Feature 2); mutation checks for Features 3 and 5 if not done per feature; apply the observation-scan suggestions collected in each feature's notes | Full suite, sanitizer, Release, E2E | Pending | sonnet | Runs after Feature 5; sanitizer mid-run after Feature 3. Release GoogleTests link is a known backlog failure: use `--config Debug --all` workaround or fix first  **Decision 2026-11-04 (user): the Release build and the Release perf gates (Feature 1 AC 15, Feature 5 AC 15) are SKIPPED.** Debug measurements are recorded in the feature plans instead; S7 keeps the full Debug GoogleTest run, `dia check sanitizer`, and the CellLayers + CellHistory E2E. |

**Order note.** The spec order is 1 → 2 → 4 → 3 → 5, but Feature 4's snapshot sides read Feature 3's cell file, and Features 3 and 5 share the base64 encoders. Plan: build 4 with `now` and `sample` sides first (Tasks 4.1–4.8), add the `snapshot:` side (Task 4.9) after Feature 3, and build `HistoryEncoding` (Task 5.1) before Feature 3.

## Feature 1: Recorder and tiers

Detailed plan: [recorder-and-tiers.plan.md](recorder-and-tiers.plan.md) (14 tasks). Features 2 to 5 below stay as outlines until each is scheduled; split each into its own `<feature>.plan.md` then.

## Feature 2: Scrub and timeline

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 2.1 | **Spec amendment and hook audit:** amend Feature 2 and 4 specs to the built `IViewValueSource` signature; audit that `LayerReader`, the uploader, hover, query, inspector and `cells.layer.values` all consult the source and key caches on `(version, ViewEpoch)`; list gaps | Written audit; failing tests for each gap found | Pending | sonnet | If `LayerReader` does not consult it, fixing that is a DiaCellLayers edit (sign-off) |
| 2.2 | `HistoryViewState` machine: Live / Sample(n); stays on its sample as new ones arrive; latest-sample scrub is not live; evicted-window notice (AC 1, 2, 6) | RED-GREEN: transitions, stay-put, window passes the viewed sample | Pending | sonnet | Delta mode is added in Feature 4 |
| 2.3 | `HistoryValueSource : IViewValueSource`: dequantised ring values, false for untracked / evicted, `IsLive`, `ViewEpoch` bump on any view change, second install rejected (Error) (AC 5, 9) | RED-GREEN with fake registry | Pending | sonnet | Installed on the `CellLayerRegistry` |
| 2.4 | Commands `cells.history.view`, `live`, `step <n> [band]`, `event prev\|next [type]` with JSON results and error cases (AC 3, 4) | RED-GREEN: samples, bands, clamps, type filter, every error unchanged | Pending | sonnet | |
| 2.5 | Viewer integration: scrubbed values reach hover, query, inspector, `cells.layer.values`; registry `IsLive()` false while scrubbed; "no per-cell history" state with Track button; sample badge on hover (AC 5, 7, 12, 14) | Fixture integration tests with the real viewer domain | Pending | sonnet | Cost bound: one read and one upload per view change at Large |
| 2.6 | `TimelineModel`: range, buckets (decide fixed 256 vs pixel-based: Open Q2), band ticks, coloured event ticks (`ui.colour`), gaps, pre-window dim, sparkline ≤ 256 points from aggregates, rebuilt only on new sample or view change (AC 10, 11) | RED-GREEN: bucketing, downsampling, gaps, rebuild trigger | Pending | sonnet | |
| 2.7 | Observation: metrics (view changes, scrub seconds, timeline rebuild ms), `DIA_PROFILE_SCOPE` on view change and rebuild, Info / Warning logs | RED-GREEN: metric values | Pending | haiku | |
| 2.8 | HTML mockup check: extend or confirm `cell-layer-viewer.html` timeline (it exists); user approves it as the visual gate before 2.9 | User approval | Pending | haiku | Rule: mockup approved before an editor feature is built |
| 2.9 | Panel timeline card + banner ("Viewing sample N … not live", Back to live), step / event / live buttons, slider runs `cells.history.view`; contract golden updated; both `debug-panel.html` pages; `cells.view.state.history` field `null` or `{mode, sample, label}` (AC 8, 13, 15, 16) | Golden + page tests; visual gate | Pending | sonnet | `history` field needs DiaCellLayers Feature 5; until then expose it via `debug.domain_state` and wire into the topic when Feature 5 lands |
| 2.10 | CluicheTest wiring + E2E: stage creates the module, samples on a timer, E2E scrubs, steps, jumps to events, asserts viewed values and banner; perf tests (view change, rebuild at 2,080 samples) | `dia test e2e --scenario="*celllayers*"` | Pending | sonnet | |
| 2.11 | Finalize (as Feature 1 task 14); visual gate approved | Verify gate | Pending | haiku | Not Done until 2.9's gate is approved |

## Feature 4: Compare

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 4.1 | `SideReader` for `now` and `sample:<i>`: window edges, untracked layer, NaN; snapshot side stubbed to "unavailable" until 4.9 (AC 1, 2, 8) | RED-GREEN | Pending | sonnet | |
| 4.2 | `HistoryViewState` gains `Delta(A, B, scale)` and `off` returning to the held view; `view` / `live` leave delta; `step` / `event` refused ("leave delta mode first"); `IsLive()` false (AC 10, 11) | RED-GREEN: all transitions | Pending | sonnet | |
| 4.3 | `DeltaValueSource`: B − A, NaN on either side, category layers no data, quantisation bound `2 × range / 65,535` for recorded sides (AC 1, 2, 8, 12) | RED-GREEN: fixtures, NaN, category | Pending | sonnet | Reuses the Feature 2 source plumbing |
| 4.4 | Auto scale: nice number (1/2/5 × 10ⁿ), grow immediately, shrink after 2 s under 40% (injectable clock), `scale <n>` fixed, `auto` (AC 5) | RED-GREEN with fake clock | Pending | sonnet | |
| 4.5 | `GetPresentation` for delta: `ResolveDeltaRamp`, range ±scale, unit prefix "Δ"; scrub unchanged (AC 4, 6) | RED-GREEN with a fake viewer | Pending | sonnet | Hook already supports it; verify the viewer honours ramp, range and prefix |
| 4.6 | Commands `cells.history.delta <layer> <A> <B>`, `scale`, `groupside`, `off` with every error case (AC 3, 10) | RED-GREEN: all errors change nothing | Pending | sonnet | |
| 4.7 | `GroupDeltaTable`: per grouping layer, mean A / mean B / change, sorted by absolute change, side switch (default B), selection restriction, omit empty groups, fallback to live grouping (AC 9) | RED-GREEN | Pending | sonnet | One pass per frame on the viewer PU |
| 4.8 | Viewer integration: hover signed delta with both side values, query over deltas, inspector stats over deltas ("no data" layers listed), `cells.view.state.history` delta shape, timeline A / B markers (AC 7, 15) | Fixture integration | Pending | sonnet | Query part needs DiaCellLayers Feature 3 built |
| 4.9 | `SnapshotCellCache` (2-entry LRU, synchronous load, failure = Error + no change) and the `snapshot:<name>` side (AC 13) | RED-GREEN with fake cell files | Pending | sonnet | **After Feature 3's cell file exists** |
| 4.10 | Observation, panel banner / legend / group table, golden, both pages, visual gate against the mockup delta view (AC 14, 16, 17) | Golden + page + visual gate | Pending | sonnet | Mockup exists |
| 4.11 | E2E extension (take two samples, delta, assert sign and group table), perf at Large, finalize | E2E + verify | Pending | sonnet | |

## Feature 3: Snapshots

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 3.1 | `ISnapshotStore` interface + `FakeSnapshotStore` in `Testing/` (scripted success / failure, recorded callbacks, temp-folder helper) | Used by 3.2 on | Pending | sonnet | Interface per spec; callbacks via function pointer + user data |
| 3.2 | `SnapshotCellFile` write / read (`dia.cell_snapshot` v1, header, grid, layers via `HistoryEncoding`), atomic temp + rename (AC 2) | RED-GREEN: round trip within range/65,535 | Pending | sonnet | Needs 5.1 encoders |
| 3.3 | Name validation (1–32 of `A-Za-z0-9_-`), 16-snapshot cap counting files from the store folder, overwrite rule, one-operation-in-flight (AC 3, 8a) | RED-GREEN | Pending | sonnet | |
| 3.4 | `SnapshotManager::Take`: pending result, frame-boundary capture of every recorded layer, store request, cell file write, both-or-neither with cleanup (`Delete`) on any failure (AC 1, 4) | RED-GREEN: success, store fails, file write fails, no partial files | Pending | sonnet | |
| 3.5 | `Restore` same run: grid check before the store call, truncate samples / aggregates / ring slots / events (including unattached), Warning with the dropped count, older-than-window empties the ring with a gap (AC 5, 6, 8a) | RED-GREEN | Pending | sonnet | Needs truncation methods on every tier: add to Feature 1 stores here |
| 3.6 | `Restore` foreign run: reset history, new `runId`, sample numbering from 0; store failure leaves history untouched; viewer to live, selection kept (AC 7, 8, 9) | RED-GREEN | Pending | sonnet | `runId` generated at recorder start (small Feature 1 addition) |
| 3.7 | Commands `cells.snapshot.take [overwrite]`, `restore`, `list` (with orphans as `failed`), `delete` (both halves) (AC 10) | RED-GREEN | Pending | sonnet | |
| 3.8 | Query `cells.history.snapshot {name, layer}` and the `snapshots` field of meta / export (AC 11, 12) | RED-GREEN | Pending | sonnet | Meta field wired in Feature 5 |
| 3.9 | Observation (metrics, profile scopes, logs); perf: capture 64 layers at Large, cell file < ~1 MB (AC 13) | RED-GREEN | Pending | haiku | |
| 3.10 | Panel snapshot list / take / restore controls (mockup has snapshots); golden, pages, visual gate; E2E with the fake store in the CluicheTest stage | Golden + E2E | Pending | sonnet | |
| 3.11 | Finalize. CoW adapter and S-3 proof (state hash match) tracked on the CoW `ISnapshotStore` row | Verify gate | Pending | haiku | |

## Feature 5: Export

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 5.1 | `HistoryEncoding`: base64 float32 / uint16 / uint8, little-endian, edge lengths | RED-GREEN: round trips, endianness, padding | Done | haiku | Done 2026-11-04: `HistoryEncoding.h/.cpp`, caller-buffer encoders `EncodeFloat32/UInt16/UInt8(v, n, char* out, capacity, outLen)` (PD-004: first version used `std::string` in the public header; caught in review and fixed), decoders `DecodeFloat32/UInt16/UInt8(text, len, out, n)`, `EncodedLength`; 40 tests (20 per shard) pass; deps clean. Bit-exact float round trips, little-endian by shifts. |
| 5.2 | `HistoryExporter::WriteFile` core sections: header, bands, samples, groups, groupSizes, aggregates, events, tracked, gaps (AC 1, 2) | RED-GREEN: fixture recorder, decode and compare exactly | Pending | sonnet | Streams from stores, no second copy |
| 5.3 | `--cells` section, mid-run registered layers (`first`), evicted gaps (AC 5) | RED-GREEN | Pending | sonnet | |
| 5.4 | Embedded `schema` via `ValueSchemaService::ExportSubset` for recorded layers, grouping layers and event types (`FindEvent`); unresolved event type Warning once; world-id provider (`SetWorldIdProvider`) (AC 3, 4, 13) | RED-GREEN | Pending | sonnet | Both services already exist |
| 5.5 | Atomic write (temp + rename), failure injection, `--overwrite` refusal, relative path under `Cluiche/out/<App>/history/` (PD-009), command `cells.history.export` (AC 6) | RED-GREEN | Pending | sonnet | |
| 5.6 | Queries `meta`, `aggregates`, `events`, `cells` with `from/to/layers/types/cells`, chunking under 1 MB with `next`, error objects (AC 10, 11) | RED-GREEN: chunk boundaries, reassembly equals single shot | Pending | sonnet | |
| 5.7 | Topics `cells.history.aggregates` (≤ 1/s, only new rows, `structure` hash) and `cells.history.events` (batched ≤ 256) (AC 7, 8, 9) | RED-GREEN with fake clock and topic recorder | Pending | sonnet | Uses DiaDebugServer topics (Done) |
| 5.8 | Conformance fixtures in `Testing/Conformance/` shared with T-1's Vitest; live-follow reconstruction test (AC 12, 14) | C++ + shared fixture decode | Pending | sonnet | T-1 side is T-1's job |
| 5.9 | Observation, perf: 40-year Large export < 2 s Release, < 20 MB without cells (AC 1, 15) | Perf tests | Pending | haiku | |
| 5.10 | E2E: export from the stage, decode, compare with `cells.history.meta` + queries; finalize; `dia docs spec-done`; update DiaValueSchema Feature 6 AC 9 note | E2E + verify | Pending | sonnet | Closes the Feature 6 AC 9 follow-up from the backlog row |

## Risks and open items

1. **Release link failure** (backlog Loose End, 46 unresolved externals in the Release GoogleTests): the Feature 1 perf gate (Task 12) and Feature 5 perf (5.9) need a Release run; use the workaround or fix first.
2. **Truncation touches every tier** (3.5): Feature 1 Tasks 4 to 6 now include `TruncateAfter(sample)` on each store, so this feature does not reopen Feature 1.
3. **Open spec questions to settle when reached:** timeline bucket rule (2.6), export hitch (5.5, only if it stalls), aggregate overflow decimation (only when a real run reaches the budget).
4. **Edits while scrubbed** are each system's rule (A1, S-1), not Dia's: add to those specs when they are planned.
5. DiaCellLayers Features 3 and 5 should land before Task 4.8 and 2.9's topic field respectively; otherwise those tasks ship partial and are re-opened.

## Rough size

~60 tasks: Feature 1 14, Feature 2 11, Feature 4 11, Feature 3 11, Feature 5 10, system 3. Features 1 and 5 core are mostly pure logic (fast TDD); Features 2, 3, 4 each carry a panel card, so each has a user visual gate.
