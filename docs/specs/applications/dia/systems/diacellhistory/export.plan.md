**Spec:** @docs/specs/applications/dia/systems/diacellhistory/export.md
**Status:** In Progress

Feature 5 of DiaCellHistory (system order 1 → 2 → 4 → 3 → 5). **Prerequisites:** Features 1-3 and 4 built; task 5.1 (`HistoryEncoding`, caller-buffer base64 encoders/decoders) Done 2026-11-04. TDD throughout (RED proven with quoted output). Commit after each task (or each agreed pair of small, coupled tasks). Models are sonnet or haiku only; any opus use needs the user's confirmation first. Split out of `diacellhistory.plan.md` on 2026-11-04.

**Verification policy:** scoped GoogleTests only (`--filter` including fixture-named suites, `--shards >= 2`); full run, sanitizer, Release (needed for 5.9's perf gate) and full E2E live in system task S7. Mutation checks only on pure threshold/arithmetic logic (none planned). Observation scan (notes) and a short gap audit per feature.

**Facts carried from earlier features (2026-11-04):**
- `HistoryEncoding.h`: `EncodeFloat32/UInt16/UInt8(v, n, char* out, capacity, outLen)`, `DecodeFloat32/UInt16/UInt8(text, len, out, n)`, `EncodedLength`; PD-004: no STL in public headers.
- Recorder read API: `SampleCount`, `Header(sample)`, `Aggregate`, `CategoryCounts`, `CellValues`, `Events`, `EventsLog`, `Aggregates()`, `Ring()`, `RecordedLayerId(i)`, `RunId()`, `ActiveGrid()`, `BuildStatus` (gaps, tracked), `GroupCount`. Quantise/dequantise functions are public in `CellRing.h`.
- `SnapshotManager::ListSummaries(out, cap)` already returns `{name, sample, label}` for ready snapshots: 5.2/5.6 wire it into the `snapshots` field (the file field and `cells.history.meta` do not exist until this feature). Snapshot per-cell reads are `cells.history.snapshot` (Feature 3, built, chunked by cell).
- Sim-hosted reply limit is 16,384 bytes per event (`kSimPayloadBytes` in `SimHostedTypes.h`), and the sim-hosted name ceiling is 64 (about 36 `cells.*` names exist after Feature 3 plus about 8 stage names). Feature 5 adds `cells.history.meta/aggregates/events/cells/export` (5 names). **The spec says replies stay under 1 MB; the real limit for a sim-hosted query reply is 16 KB per event, so chunking must be sized to 16 KB (like `cells.history.snapshot`) unless the queries are registered on Main; decide in 5.6 and record it.**
- Topics: DiaDebugServer topics (`PublishTopic`, `PublishTopicOrdered`) are Main-side or thread-safe; the recorder lives on SimPU, so topic publication from SimPU must use the thread-safe variants (PU boundaries are streams; see feedback on cross-PU reads).
- `ValueSchemaService::ExportSubset` and `FindEvent` exist (built 2026-10-22).
- Test utilities in `DiaCellHistory/Testing/`; `HistoryTestRig.h` Rig fixture; `FakeSnapshotStore` + `TempFolder`.

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
