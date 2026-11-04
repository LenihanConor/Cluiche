---
schema: dia.module.v1
module_id: dia.cellhistory
name: DiaCellHistory
path: Dia/DiaCellHistory
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: dev
version: "0.1"
dependencies:
  required:
    - dia.core
    - dia.valueschema
    - dia.celllayers
    - dia.debugserver
    - dia.visualdebugger
  forbidden: []
public_api:
  headers:
    - DiaCellHistory/DiaCellHistory.h
    - DiaCellHistory/HistoryEncoding.h
    - DiaCellHistory/ISnapshotStore.h
    - DiaCellHistory/SnapshotCellCache.h
    - DiaCellHistory/SnapshotCellFile.h
    - DiaCellHistory/SnapshotNaming.h
    - DiaCellHistory/SnapshotManager.h
    - DiaCellHistory/SnapshotCommands.h
    - DiaCellHistory/SnapshotObservation.h
    - DiaCellHistory/HistoryExportCommands.h
    - DiaCellHistory/ExportObservation.h
    - DiaCellHistory/HistoryQueries.h
    - DiaCellHistory/QueryObservation.h
    - DiaCellHistory/HistoryTopics.h
    - DiaCellHistory/TopicObservation.h
    - DiaCellHistory/AtomicFile.h
    - DiaCellHistory/Testing/FakeSnapshotStore.h
    - DiaCellHistory/Testing/StubGrid.h
    - DiaCellHistory/Testing/FakeClock.h
    - DiaCellHistory/Testing/BudgetSizing.h
    - DiaCellHistory/Testing/FixtureEventSchemas.h
    - DiaCellHistory/CellHistoryCommands.h
    - DiaCellHistory/TimelineCardState.h
    - DiaCellHistory/CellHistoryDomain.h
    - DiaCellHistory/ViewHistorySourceAdapter.h
    - DiaCellHistory/SideReader.h
    - DiaCellHistory/DeltaValueSource.h
    - DiaCellHistory/DeltaScale.h
    - DiaCellHistory/GroupDeltaTable.h
    - DiaCellHistory/DeltaCellInfo.h
    - DiaCellHistory/DeltaCardState.h
  namespaces:
    - Dia::CellHistory::
  entry_points: []
---
# DiaCellHistory

Generic recorder for cell-grid layers registered in DiaCellLayers: always-on aggregates, opt-in quantised per-cell rings in a shared sample window, and a typed event log. Feature 1 (recorder and tiers) of the DiaCellHistory system; scrub, snapshots, compare and export are Features 2 to 5.

Log channel: `HIS`. Namespace: `Dia::CellHistory`. Runs on SimPU with the DiaCellLayers viewer. No game types.

## Public API

| Type | Purpose |
|---|---|
| `CellHistoryRecorder` | `Sample`, `Emit`, `TrackLayer`/`Untrack`, read API (`Header`, `Aggregate`, `CategoryCounts`, `CellValues`, `Events`), status. Published via a `ServiceStream`. |
| `CellHistoryModule` | SimPU module: creates the recorder, applies the `.diastage` `history` block, registers commands. |
| `CellHistoryCommands` | `cells.history.track`, `untrack`, `status` (DIA_DEBUG). |
| `TimelineCardState` | Typed model of the timeline card + not-live banner JSON (`TimelineModel` + `HistoryViewStatus`); `HistoryField` is the future `cells.view.state.history` value. Rebuilds only on sample/view/layer-state change. |
| `ViewHistorySourceAdapter` | `IViewHistorySource` over (recorder, `HistoryViewState`): `HistoryField()` = `TimelineCardState::HistoryField`, `HistoryStamp()` = `Epoch()*2 + (Sample mode ? 1 : 0)` (new recorder samples do not change the field, so they are not in the stamp). Install with `ViewStatePublisher::SetHistorySource`. |
| `CellHistoryDomain` | Panel-only `IDebugDomain` "Cell History" (id `cellhistory`): state = the card; panel commands `view`/`live`/`step`/`event`/`delta`/`track` call the same executors as `cells.history.*` (DIA_DEBUG). |
| `SideReader` | Compare (F4): reads one delta side (`now` = layer live `read`; `sample:<i>` = ring `CellValues`; `snapshot:<name>` = dequantised cell file via an optional `SnapshotCellCache`: no cache = Unavailable, unloadable / other grid / layer absent / cell count mismatch = NoData) with `ParseSide`/`SidesEqual`. NoData never falls back to live. |
| `DeltaValueSource` | Compare (F4): delta-mode values, helper owned by `HistoryValueSource` (not installed itself). Float layer = B - A from two `SideReader` reads; NaN on either side = NaN cell; category layer, NoData or Unavailable side = `GetValues` false (no live fallback). Recorded-vs-recorded error <= 2 x range / 65,535; `now` exact. One HIS Warning per layer per view epoch on a data gap; profile scope `cellhistory.delta_read`. |
| `DeltaScale` | Compare (F4): pure auto-scale logic for delta mode, time injected. Scale = max abs delta rounded up to 1/2/5 x 10^n (`NiceCeil`); grows immediately; shrinks only after max < 40% of scale for 2 s; first read of an all-zero delta = 1.0, floor 1e-6; NaN/inf/negative maxima ignored. `DeltaValueSource::CurrentScale(view)` returns the fixed scale or the auto value; reset on view epoch change (new delta, fixed/auto switch); `SetClock` injects the wall clock. |
| `GroupDeltaTable` | Compare (F4): inspector group summary for delta mode. `AccumulateGroups`/`SortByAbsChange` (pure) plus `GroupDeltaBuilder::Build`: per group of each grouping category layer, mean on side A, mean on side B and change (B - A) over cells where both base sides are non-NaN, sorted by abs change (ties: group value, layer order), capped at 512 rows (`truncated`). Membership from the chosen side's grouping values (`DeltaSpec::groupSideB`), falling back to live values (`usedLive`, one HIS Warning per builder) when that side has none; base with no data on a side = empty table + `baseNoData`. Selection is a plain `uint8_t` mask (nullptr = none). Group values must be whole numbers in [0, 256). Profile scope `cellhistory.group_table`. |
| `DeltaCellInfo` | Compare (F4 4.8): `DeltaCellReader::Get` = A, B, delta and per-side `hasData` for ONE cell, from two whole-layer `SideReader` reads cached on (view epoch, layer id, layer version, window start); delta NaN unless both sides have data and the layer is float. `DeltaCellDetails` registers a "History delta" `CellDetailProvider` (A (side) / B (side) / Delta rows) on the delta layer's grid only while in delta mode (`Sync(registry)` each frame). |
| `DeltaCardState` | Compare (F4 4.8): the `delta` object of the `CellHistoryDomain` state (absent outside delta): layer, a, b, `groups` (group table JSON; restricted to the viewer selection via the registry, whole grid when none; grouping layers = category layers of the grid with `history.group`; labels from the schema enum) and `hover` (both side values + delta). One table build per change of epoch / selection / window / layers version / live layer versions (`BuildCount`); never per frame. Profile scope `cellhistory.delta_card_groups`. |
| `HistoryConfig` | Parsed `history` block (bands, track, grid, budgets, `maxAggregateLayers`). |
| `AggregateStore` | Tier 1: per-group min/mean/max and category counts per sample; oldest-row eviction with gaps. |
| `CellRing` | Tier 2: shared-window quantised per-cell rings (uint16 float, uint8 category). |
| `EventLog` | Tier 3: typed events assigned to the next sample, budgeted. |
| `CellHistoryObservation` / `CellHistoryHealth` | Metrics, trace/profile scopes, Degraded above 90% of any budget. |
| `HistoryEncoding` | Feature 5 (Export): base64 encoders/decoders for float32, uint16 and uint8 arrays with little-endian byte order. Encoder functions (`EncodeFloat32`, `EncodeUInt16`, `EncodeUInt8`) write to caller-provided buffers with capacity checking; decoder functions (`DecodeFloat32`, `DecodeUInt16`, `DecodeUInt8`) read from base64 text. `EncodedLength` computes required buffer size. Used by export file writer and live query replies. |
| `HistoryExporter` | Feature 5 (Export, tasks 5.2-5.3): `WriteFile(recorder, ExportOptions, path, ExportResult&)` writes the `dia.cell_history` v1 JSON file, streamed section by section (one layer's arrays at a time in scratch buffers; no whole-run JSON tree), always compiled. Sections: header (incl. `sampleFirst`), bands, samples (retained headers only), groups (labels from schema enum labels), groupSizes, aggregates (float32 min/mean/max; category u16 sample-major; per-layer `first`), events (type text recovered from the schema's `kind: event` entries, else `crc:xxxxxxxx`), snapshots (`ExportOptions::snapshots` via `ListSummaries`), tracked, gaps; `--cells` (`options.cells`) adds the ring's raw quantised payload per tracked layer (u16 LE / u8, `first`, `count`, one base64 string per sample). Tasks 5.4/5.5: the `schema` section (written last) is `ValueSchemaService::ExportSubset` over every recorded layer's schema key, every grouping layer and the event types present (resolved through `FindEvent` full keys); `ExportSubset` adds the ramps the entries use. An unresolved event type logs ONE HIS Warning per type per export (`ExportResult::unresolvedEventTypes`) and is still exported as `crc:xxxxxxxx`. `header.world` = `options.world`, else `CellHistoryRecorder::WorldId()` (`SetWorldIdProvider(fn, user)`; none -> empty string). The file is streamed to `<path>.tmp` then renamed (`AtomicFile`, shared with `SnapshotCellFile`); an existing file is refused first ("file exists; use --overwrite", Warning) unless `options.overwrite`; a failed write/rename removes the temp and logs an Error; success logs Info (path, bytes, samples). `ResolvePath(path, app, outDir, buf, cap, err, errCap)`: absolute paths pass through; relative paths reject `..` segments and resolve to `<outDir>/<app>/history/<path>`. Trace zone and profile scope `cellhistory.export`. Read accessors added for it: `AggregateStore::GroupAt/LayerInfo`, `CellRing::LayerInfo/ReadQuantised`, `CellHistoryRecorder::Registry/Schema/BandCount/BandName/HeaderFirst/GroupSource`. `Testing/ExportDecoder.h` parses a written file and decodes its arrays for tests. |
| `ISnapshotStore` | Feature 3 (Snapshots): game-implemented interface (RequestTake/RequestRestore with frame-boundary callbacks, Exists, Delete, Folder). DiaCellHistory never touches the game's state files. `Testing/FakeSnapshotStore.h` is the scripted fake (queued requests run by `RunFrameBoundary()`, `FailNext`, optional real `.state` files, per-test `TempFolder`). |
| `SnapshotCellFile` | Feature 3: `dia.cell_snapshot` v1 per-cell copies file `<store folder>/snapshot_<name>.cells.json`. `SnapshotCellData` model (header, grid, up to 64 layers), `BuildFromLive` (quantises every recorded layer from live values with CellRing's `QuantiseCellFloat/Category`), `WriteAtomic` (temp + rename), `Read`/`ReadHeaderOnly` (validating), `Dequantise`. |
| `SnapshotNaming` | Feature 3: `IsValidSnapshotName` (1-32 of `A-Za-z0-9_-`), `CountSnapshots` (cell files in the store folder; `kMaxSnapshots = 16`), `SnapshotGate` (one operation in flight), `CheckTake` pre-flight (name, in flight, exists unless overwrite, cap). |
| `SnapshotManager` | Feature 3: `Take(name, overwrite, TakeResult&)` (pending result; checks via `CheckTake` plus a no-samples refusal; capture of live quantised data at the sim frame boundary; store `RequestTake`; cell file written atomically in the store callback; `Delete` of the store half on a cell file failure, of a partial state on a store failure; gate released in the callback), fixed `SnapshotRecord` list (24 slots) for list, injectable time and world. `Restore(name, RestoreResult&)` (refusals before the store: name, in flight, state/cell file missing, unreadable header, grid id/width/height mismatch; callback: same run truncates via `TruncateAfter` with a Warning of the dropped count, foreign run or a sample not in the history calls `ResetHistory(seed)` with a Warning; store failure changes nothing; `LastRestore()`; `SetViewState` goes live via `HistoryViewState::NotifyHistoryReset`; `SetResetSeedProvider`, default time plus counter). Feature 1 additions for it: `CellHistoryRecorder::ResetHistory/ActiveGrid`, `Clear()` on AggregateStore/CellRing/EventLog, `CellRing::TruncateAfter` clamps gaps and re-pends layers. |
| `SnapshotCellCache` | Compare (F4 4.9): 2-entry LRU of snapshot cell files for `snapshot:<name>` delta sides. `Get(name, err, cap)` loads on a miss (synchronous `SnapshotCellFile::Read` of `Path(store.Folder(), name)`), bumps recency on a hit; a failed load returns nullptr + reason and logs ONE HIS Error per failing name; `Exists`, `Peek`, `Clear`, counters `Hits/Misses/Loads/LoadFailures`, `GridMatches(data, CellGridDesc)` (id, width, height; the CALLER checks it against the layer grid). `Invalidate(name)` (called by `SnapshotManager` when an overwriting take writes the cell file and by `SnapshotCommands` delete) drops the entry and, via `SetViewState`, calls `HistoryViewState::NotifySnapshotChanged(name)`, which bumps the epoch only when the delta in force uses that snapshot. One shared cache per history setup: `HistoryViewCommands`, `HistoryValueSource`, `CellHistoryDomain` (timeline markers = the snapshot header sample, group table, hover, selected-cell rows), `SnapshotManager`, `SnapshotCommands` each get `SetSnapshotCache`. `cells.history.delta` validates snapshot sides (invalid name, unknown snapshot, load error, different grid) and keeps the old "not available yet" message when no cache is set. Profile scope `cellhistory.snapshot_cache_load`. |
| `SnapshotCommands` | Feature 3 (DIA_DEBUG): sim-hosted `cells.snapshot.take/restore/list/delete` and the editor query `cells.history.snapshot { name, layer, offset?, count? }` (5 names, `IMirrorQueryRegistrar`, `{error}` convention, help text, rollback on a rejected name). list merges this session's records with the `snapshot_*.cells.json` files of the store folder (status pending/ready/failed, sameRun, orphans failed; cap 32 + `truncated`; a store state with no cell file and no record is undiscoverable, ISnapshotStore cannot enumerate). delete removes state + cell file + record, refused while in flight. The layer query returns the stored quantised payload (uint16 LE / uint8) base64 in chunks of at most 4096 cells because a sim-hosted reply is limited to 16 KB. Also `SnapshotManager::ListSummaries` ({name, sample, label} of ready snapshots, for the `snapshots` field of cells.history.meta and the export in Feature 5) and `RemoveRecord`. |
| `SnapshotObservation` | Feature 3 metrics (`SetObservation` on `SnapshotManager` and `SnapshotCommands`; null = off): gauges `dia.cellhistory.snapshots` (ready records, refreshed on take, delete, list), `snapshot_take_ms` (capture in Take + cell file write in the callback), `snapshot_restore_ms` (request to callback end), `snapshot_bytes` (last cell file); counters `snapshot_failures` (refused/failed take, restore, delete, store and file failures), `snapshot_queries_served` (cells.history.snapshot calls). Injectable ms clock; no MetricInfo (as Features 1 and 2); the registry has no unregister. Profile scopes `cellhistory.snapshot_take/_write/_restore/_list/_query`; the list Warning for an orphan is logged once per name per `SnapshotCommands` lifetime (32-name memory). |
| `HistoryExportCommands` | Feature 5 (DIA_DEBUG): sim-hosted `cells.history.export { path, cells?, overwrite? }` -> `{ ok, path, bytes, samples }` or `{error}` (1 name; help <= 256 chars; same registrar/rollback pattern as `SnapshotCommands`). Runs on the caller's thread (SimPU) and blocks the simulation while it writes (export spec Open Q1). Host setters: `SetAppName`, `SetBuildInfo(gitCommit)`, `SetOutDir` (the `Cluiche/out` folder; the history folder is created on demand for relative paths), `SetSnapshots(const SnapshotManager*)`, `SetTimeProvider` (unix seconds, null = system clock), `SetObservation`. `FormatUtc` renders `generated_at` as ISO 8601 UTC. |
| `ExportObservation` | Feature 5 metrics: counter `dia.cellhistory.exports`, gauges `dia.cellhistory.export_ms` and `export_bytes` (last successful export, updated by `HistoryExportCommands`). Injectable ms clock; same conventions as `SnapshotObservation`. |
| `HistoryQueries` | Feature 5 (DIA_DEBUG): sim-hosted read-only catch-up queries `cells.history.meta {part?, offset?}`, `cells.history.aggregates {from?, to?, layers?}`, `cells.history.events {from?, to?, types?, skip?}`, `cells.history.cells {layer, from?, to?, cells?, cellOffset?}` (4 names; help <= 256 chars; rollback on a rejected name). `to` is exclusive. **Deviation from the export spec:** each reply is capped at `MaxReplyBytes()` = 15,000 serialised bytes (the sim-hosted host limit `kSimPayloadBytes` is 16,384), not 1 MB, and carries a `next` cursor (`meta`: `{part, offset}`; `aggregates`: `{from}`; `events`: `{from, skip}` resuming mid-sample, at most 2,000 events; `cells`: `{from, cellOffset}` row-major, splitting a sample into column slices when one row exceeds the cap). A request whose single sample of the requested layers does not fit is an error (`request fewer layers`). `meta` carries the recorder's `StructureHash()` as `structure`; `meta part=schema` pages the embedded schema (`HistoryExporter::BuildSchema`). Test seams `SetMaxReplyBytes`, `SetMaxEvents`. Errors: untracked layer, range outside the retained window, bad arguments, invalid cursor. |
| `QueryObservation` | Feature 5 metrics: counters `dia.cellhistory.query_meta`, `query_aggregates`, `query_events`, `query_cells` (successful replies by kind) and `query_bytes` (serialised bytes served); one Warning per `HistoryQueries` instance the first time a reply is clamped to the size cap. |
| `HistoryTopics` / `ITopicPublisher` | Feature 5 (DIA_DEBUG): live topics `cells.history.aggregates` and `cells.history.events`, ticked once per sim frame by the owner on the recorder's PU and published through `ITopicPublisher` (the application adapts it to `DebugServer::PublishTopicOrdered` + `GetTopicSubscriberCount`; ordered because latest-wins would drop sample rows). Aggregates: at most one burst per `MinAggregateInterval` (1 s, injectable seconds clock), only with new samples AND a subscriber; payload `{structure, fromSample, count, samples, groupSizes, rows, structureChanged?}` for the new samples only, built by the same row builders as `cells.history.aggregates` (`HistoryRows`); a backlog is split into messages of consecutive ranges each <= `MaxMessageBytes()` (512 KB; transport cap 1 MB), max 8 per Tick. First publication starts at the SampleCount when the topics were built / the last Tick without subscribers (no replay). Events: every Tick, each committed event once in the file's shape, batches of <= 256, max 8 messages per Tick, cursor `(sample, index)`. No subscriber: no work, cursors follow the head. Truncate/reset (`SampleCount` shrinks, `RunId` changes): cursors move to the new head. A rejected publish is counted, the cursor still advances, one Warning per second; clients detect gaps via `fromSample`/`structure`. Info logs on first/last subscriber. Adds no sim-hosted names. |
| `TopicObservation` | Feature 5 metrics: counters `dia.cellhistory.topic_messages`, `topic_bytes`, `topic_drops`; profile scope `cellhistory.topic_build`. |
| `AtomicFile` | `TempPath` (`<path>.tmp`), `Commit` (rename over the final name; on failure removes the temp), `Discard`. Used by `SnapshotCellFile::WriteAtomic` and `HistoryExporter`. |

All tier stores provide `TruncateAfter(sample)` for Feature 3 (snapshot restore). `Testing/` ships stubs (`StubGrid`, `FakeClock`, `BudgetSizing`, `FixtureEventSchemas`).
