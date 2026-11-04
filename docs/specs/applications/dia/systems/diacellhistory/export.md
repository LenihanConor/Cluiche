# Feature Spec: DiaCellHistory Export

**Parent:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md
**Builds on:** @docs/specs/applications/dia/systems/diacellhistory/recorder-and-tiers.md (Feature 1), @docs/specs/applications/dia/systems/diavalueschema/export-query-and-reference.md (`ExportSubset`), @docs/specs/applications/dia/systems/diacelllayers/editor-mirror.md (base64 encodings)

**Gameplay Domains:** debug, tools

## Summary

Gets a recorded run out of the game, live and offline, in one format (CH-004). It delivers:

- two DiaDebugServer **topics**, `cells.history.aggregates` and `cells.history.events`;
- catch-up **queries** `cells.history.meta`, `cells.history.aggregates`, `cells.history.events` and `cells.history.cells`;
- the **`dia.cell_history` file** and the command `cells.history.export <path>`;
- a single C++ function, `HistoryExporter::WriteFile`, that the command and the headless runners (S-2.5, S-3, S-3.5) both call.

The World Sim Inspector reads exactly this: live through the topics and queries, offline from the file, through one `Source` model.

## Problem

The recorder holds the run in the game's memory. The editors that analyse it (T-1) run elsewhere, and headless runners finish before anyone looks. They need the same data in the same shape whether it comes live over the debug server or from a report file, with enough embedded description (labels, units, ramps, event meanings) that a file explains itself.

## Goals

1. **One format, two transports.** Live messages and the file use the same field names and encodings.
2. **Self-describing files.** Each file embeds the DiaValueSchema entries and ramps it needs, plus the world id used for navigation gating.
3. **Catch-up and stream.** A client that connects mid-run fetches the history so far by query, then follows the topics.
4. **Bounded messages.** Every reply or message stays under the 1 MB DiaDebugServer cap; large series are chunked.
5. **Compact.** Numeric arrays are base64, so a 40-year weekly run with 20 layers × 27 groups is about 18 MB, not 30.
6. **One writer.** The command and every headless runner use the same function.

## Non-Goals

- Snapshots' per-cell data (Feature 3 adds `cells.history.snapshot` on this transport).
- Delta computation (Feature 4).
- Importing a file back into the game or resuming from it.
- Gzip or other file compression.
- Reading files in the game (editors and tests read them).
- The editor-side `Source` model (T-1 Feature 1).

## Architecture

Lives in `Dia/DiaCellHistory/` (namespace `Dia::CellHistory`).

```
HistoryExporter      WriteFile(recorder, options, path): the one writer (also the encoders)
HistoryEncoding      base64 float32 / uint16 / uint8 arrays; shared with the query replies
HistoryTopics        publishes cells.history.aggregates and cells.history.events
HistoryQueries       meta, aggregates, events, cells
```

### Encodings

- **Float arrays** (min, mean, max): little-endian float32, base64.
- **Counts and group sizes:** little-endian uint16, base64.
- **Per-cell values:** the recorder's quantised storage, little-endian uint16 (float layers) or uint8 (category layers), base64, with the layer's `min` and `max` so a client dequantises: `v = min + q / 65535 × (max − min)`.
- Everything else is plain JSON. Sample and layer indices are the recorder's absolute sample indices.

### The `dia.cell_history` file (v1)

```json
{
  "format": "dia.cell_history", "version": 1,
  "header": { "app": "cow", "git_commit": "8f88d1aa", "generated_at": "2026-10-20T14:03:00Z",
              "world": "coast_range", "sampleCount": 2080, "aggregateFirst": 0, "cellFirst": 1560 },
  "bands": ["Day", "Week", "Season", "Year"],
  "samples": [ { "tick": 7, "label": "Y1 W1", "band": 1 } ],
  "groups": [ { "id": "world", "label": "World" },
              { "id": "world.region=Coast", "layer": "world.region", "value": "Coast", "label": "Coast" } ],
  "groupSizes": { "world": "<b64 u16 per sample>", "world.region=Coast": "<b64 u16 per sample>" },
  "aggregates": {
    "weather.precipitation": { "kind": "float", "first": 0,
        "groups": { "world": { "min": "<b64 f32>", "mean": "<b64 f32>", "max": "<b64 f32>" } } },
    "terrain.biome": { "kind": "category", "first": 0, "values": ["Grassland", "Forest"],
        "groups": { "world": "<b64 u16, sample-major, one count per value>" } }
  },
  "events": [ { "sample": 12, "type": "cow.drought", "group": "world.region=Coast", "cell": null,
                "payload": { "weeks": 4 }, "text": "4 dry weeks" } ],
  "snapshots": [],
  "tracked": ["ecology.producers_total"],
  "gaps": { "aggregates": [], "cells": [[0, 1559]], "events": [] },
  "cells": { "ecology.producers_total": { "kind": "float", "min": 0, "max": 1, "width": 104, "height": 64,
                                           "first": 1560, "count": 520, "data": ["<b64 u16 per sample>"] } },
  "schema": { "entries": [ ], "ramps": [ ] }
}
```

- `samples` lists every sample ever recorded. Aggregate arrays cover `[first, sampleCount)` for each layer, which is later than 0 for a layer registered mid-run or after eviction.
- `groups` include the implicit `world` and one group per value of each grouping layer. Group ids are `<layerId>=<enumId>` (Feature 1), with `world` for the whole grid.
- `snapshots` is `[]` until Feature 3 adds entries `{name, sample, label}`.
- `cells` is present only with `--cells` and only for tracked layers; `gaps` marks evicted ranges.
- `schema` is the `ValueSchemaService::ExportSubset` result for every recorded layer's `schemaKey`, every grouping layer, and every event type present. An event `type` is looked up by `key` among `kind: event` schema entries (see Dependencies); an unresolved type logs one Warning.
- `header.world` comes from a world-id provider the game installs on the recorder (`SetWorldIdProvider`); CoW installs the loaded world id (F-3). With no provider it is an empty string, and T-1 treats such a file as navigation-disabled.

### Live topics

| Topic | Rate | Payload |
|---|---|---|
| `cells.history.aggregates` | at most once per second, only when new samples exist | `{ structure, fromSample, count, samples: [...], groupSizes, rows: {...} }` with the same encodings as the file, covering only the new samples |
| `cells.history.events` | per frame, batched, at most 256 events per message | `{ events: [...] }` in the file's event shape |

`structure` is a hash of the set of recorded layers and groups. When it changes (a layer registered, a grouping changed), a client re-fetches `cells.history.meta` because the arrays no longer line up.

### Queries (catch-up and pulls)

| Query | Arguments | Result |
|---|---|---|
| `cells.history.meta` | none | `header`, `bands`, `groups`, `tracked`, `gaps`, `structure`, per-layer `first` and kind, and the embedded `schema` |
| `cells.history.aggregates` | `{ from?, to?, layers? }` | The aggregate rows for that sample range in the file's shape, chunked: each reply stays under 1 MB and carries `next` when more remain |
| `cells.history.events` | `{ from?, to?, types? }` | Events in the range, at most 2,000 per reply with `next` |
| `cells.history.cells` | `{ layer, cells?, from, to }` | Quantised per-cell series for a tracked layer: `{ layer, kind, min, max, width, height, from, to, cells, data }`, chunked by sample range so each reply is under 1 MB; `cells` is an optional list of indices (default all) |

- An untracked layer, a range outside the retained window, or a bad argument returns an error object naming the reason (T-1 shows "track this layer").
- Replies are built on the recorder's PU like other queries (Feature 5 of DiaCellLayers has the same rule); a reply takes at most one frame.
- Together, `meta` plus a full `aggregates` catch-up plus `events` plus the topics give a client the same data as the file.

### `cells.history.export`

```
cells.history.export <path> [--cells] [--overwrite]
```

- Writes the file through `HistoryExporter::WriteFile`, to a temporary file in the same folder and then renames it, so a partial file never appears under the final name.
- Refuses to replace an existing file unless `--overwrite` is given.
- Relative paths resolve under `Cluiche/out/<App>/history/` (PD-009); absolute paths are allowed.
- Streams from the recorder's stores without making a second copy. Returns `{ ok, path, bytes, samples }`.
- The write runs on the caller's thread (SimPU) and blocks the simulation for its duration; a Large 40-year run with `--cells` is the worst case (see Open Q1).

### Headless runners

S-2.5, S-3 and S-3.5 call `HistoryExporter::WriteFile` at the end of a run and place the file next to their report, so T-1 opens runner output exactly as it opens a live export. The runners decide the path; the format is this feature's.

### Logging and observation

- **Channel:** `HIS`.
- **Info:** export written (path, bytes, samples), first topic subscriber, last subscriber leaves.
- **Warning:** an event type with no schema entry (once per type); a reply clamped to the size cap; an export refused because the file exists.
- **Error:** write failure (disk, permissions), the temp file removed.
- **Traces / Profiling:** `DIA_TRACE_ZONE` around export; `DIA_PROFILE_SCOPE` on topic build and on each query.
- **Metrics:** topic messages sent, bytes sent, queries by kind, export count, export ms and bytes.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. `cells.history.export` writes a `dia.cell_history` file with the structure above; a 40-year weekly run with 20 layers × 27 groups is under 20 MB without `--cells`.
2. A client that decodes the base64 arrays (float32, uint16) recovers the recorder's aggregates and group sizes exactly, and per-cell values dequantise within `range / 65,535` of the originals.
3. The embedded `schema` contains an entry for every recorded layer, every grouping layer and every event type present, plus the ramps they use; a file opened with only its embedded schema shows the same labels, units and ramps as the live app.
4. `header.world` carries the id from the world-id provider; with no provider it is an empty string.
5. `--cells` adds per-cell data for tracked layers with their window start and gaps; without it the `cells` section is absent.
6. The file is written to a temporary name and renamed; a failed write leaves no file under the final name; an existing file is refused unless `--overwrite`.
7. `cells.history.aggregates` (topic) publishes at most once per second, only when there are new samples, and carries only the new rows; the file's and the topic's encodings are identical.
8. `cells.history.events` (topic) delivers each event once, batched, at most 256 per message.
9. A `structure` change (new layer or grouping) is signalled in the next aggregates message and in `cells.history.meta`.
10. `cells.history.meta`, `aggregates`, `events` and `cells` queries return the data for their ranges; each reply is under 1 MB and chunked with `next` when needed; reassembled chunks equal the single-shot data.
11. Queries return an error object for an untracked layer, a range outside the retained window, or bad arguments, and change nothing.
12. A client following meta, a full aggregates catch-up, events and the topics reconstructs the same data as the exported file for the same run.
13. Event `type` resolves to its schema entry by `key`; an event type with no entry logs one Warning and is exported without a schema entry.
14. The headless runners write the file through `HistoryExporter::WriteFile`, and T-1's file source opens runner output and live exports through the same model.
15. A Large 40-year run exports (without cells) in under 2 seconds in Release.

## Test Strategy

- **GoogleTests (TDD):**
  - Encoders: float32, uint16 and uint8 round trips; endianness; base64 edge lengths.
  - Export content for a fixture recorder (every section, with and without `--cells`, with a layer registered mid-run, with evicted ranges and gaps).
  - Atomic write: temp file, rename, failure injection, overwrite refusal.
  - Topics with a fake clock: rate limit, new-rows-only, structure change, event batching and the 256 cap.
  - Queries: ranges, chunk boundaries and reassembly, error cases, size cap.
  - Embedded schema: entries for layers, groups and event types, unresolved event type warning.
  - World id provider present and absent.
- **Vitest (T-1's file and live source tests, shared fixtures):** decode the exported file and query replies; a client rebuilt from meta plus catch-up plus topics equals the file-based model. The same fixture files used by the C++ tests live in `Dia/DiaCellHistory/Testing/Conformance/`.
- **Test utilities** in `DiaCellHistory/Testing/`: fixture recorder builder, fake clock, topic recorder, a decoder used by the tests.
- **Perf:** Large 40-year export time and size; topic build for a 20-layer, 27-group sample.
- **CoW proof (S-3, from the system spec):** the inspector shows identical aggregates live and from the export.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CH-004 | Aggregates and events stream; per-cell data is queried and chunked; live and file use one format | Topics and queries use the file's shapes |
| CH-005 | Generic: time labels, groupings and the world come from the game | Labels, bands, groups and the world-id provider come from the game |
| PD-009 | Generated output under `Cluiche/out/<App>/` | Default export folder |
| PD-004 | No STL in public APIs | DiaCore types |
| VS-005 | No singleton | The exporter uses the schema service and the recorder service |
| DiaDebugServer 1 MB message cap | Replies must be chunked | Every reply is chunked under the cap |
| Game logic on SimPU | Exporter and queries on SimPU | Same-PU contract |
| AD-001 (Dia) | Any `ValueSchemaService` change needs sign-off | The `FindEvent` lookup is flagged below |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Recorder stores and read API | Approved |
| DiaValueSchema Feature 6 | `ExportSubset` | Approved, not built |
| DiaValueSchema Feature 1 | **Event lookup by `key`** (`FindEvent`), amended with this spec | Approved; amended 2026-10-20 |
| DiaCellLayers Feature 5 | Base64 encoding conventions and the same-PU query rule | Approved, not built |
| DiaDebugServer topics and queries | Transport | Done |
| T-1 Features 1 and 6 | The consumer; reads `world` for gating | Approved |

## Open Design Questions

1. **Export hitch.** Writing on SimPU blocks the simulation, up to a couple of seconds for a Large run with `--cells`. That suits an explicit command and an end-of-run runner. If an in-game "export" button causes visible stalls, move the write to a worker over a copied snapshot.
2. **Aggregate catch-up size.** A reply carries as many samples as fit under 1 MB, so catching up on 2,000 samples of a 64-layer run takes tens of queries. If T-1's connect time is noticeable, add a streaming mode or a coarser summary for the first screen.
3. **Event type resolution.** Event types are resolved by `key` across all `for` formats, so two games or two systems must not reuse an event key. Feature 1 of DiaValueSchema now rejects duplicate event keys; revisit if namespacing is wanted.

## Status

`Approved` (2026-10-20). Plan: [export.plan.md](export.plan.md). Build needs AD-001 sign-off for the `FindEvent` addition to `ValueSchemaService`.
