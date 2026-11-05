# DiaCellHistory Conformance Fixtures

Shared between the C++ GoogleTests (`Cluiche/Tests/GoogleTests/DiaCellHistory/TestHistoryConformance.cpp`) and T-1's (World Sim Inspector) Vitest suite. They pin the `dia.cell_history` v1 file, the four `cells.history.*` query replies and the two topic messages, all produced by one deterministic fixture run. Spec: `docs/specs/applications/dia/systems/diacellhistory/export.md`.

## Files

| File | What it is |
|---|---|
| `export_basic.cell_history.json` | Full export without `--cells` (compact, as written by `HistoryExporter::WriteFile`) |
| `export_cells.cell_history.json` | Same run with `--cells` (adds the `cells` section; otherwise identical) |
| `query_meta_core.json` | First reply of `cells.history.meta` (part `core`), has a `next` cursor |
| `query_meta_schema_page0.json` | First reply of `cells.history.meta {part:"schema"}`, has `next` |
| `query_aggregates_page0.json` | First reply of `cells.history.aggregates {from:0}`, has `next` |
| `query_events_page0.json` | First reply of `cells.history.events {from:0}`, has `next` |
| `query_cells_page0.json` | First reply of `cells.history.cells {layer:"temp", from:10}`, has `next` |
| `query_aggregates_groups_world.json` | `cells.history.aggregates {from:0, groups:["world"]}` at the default budget: all 40 samples, only the `world` group, no `next` |
| `query_aggregates_stride4_world.json` | `{from:0, groups:["world"], stride:4}`: 10 windows of 4 samples (float min/max extrema, group-size-weighted mean, category counts of the window's last sample), `stride: 4` echoed, no `next` |
| `query_aggregates_slice_page0.json` | `{from:0}` with a 1,000-byte budget (below one whole sample): a layer slice, `layerOffset 0`, `layerCount 2`, `layerTotal 4`, `count 1`, `next {from:0, layerOffset:2, structure}` |
| `query_aggregates_slice_page1.json` | The page reached by following page 0's `next`: layers 2..3 of sample 0, `next {from:1, layerOffset:0, structure}` |
| `query_aggregates_stride_slice_page0.json` | `{from:0, stride:4}` with the same budget: the first layer slice of window [0,4), `stride 4`, `next {from:0, layerOffset:2, structure}` |
| `topic_aggregates_0.json` | First `cells.history.aggregates` topic message (samples 30..39) |
| `topic_events_0.json` | First `cells.history.events` topic message |

The query pages are pretty-printed (2 spaces, sorted keys); the export files are the exporter's raw compact bytes. Pages use deliberately tiny reply budgets (600..2400 bytes; 1,000 for the layer-sliced aggregates pages) so that every page has a `next` cursor; real replies are up to 15,000 bytes (sim-hosted limit 16,384).

## The fixture run

4x3 grid (12 cells), 40 weekly samples (`tick = 7 * week`, labels `Y1 W1`...). Layers: `temp` (float 0..100, tracked from sample 10 so `cells.temp.first = 10`), `rain` (float 0..50), `biome` (category: forest/plain/desert), `region` (category and the grouping layer: north/centre/south). Groups: `world`, `region=north`, `region=centre`, `region=south`. Two event types (`cow.drought` with payload `weeks`, `cow.flood` with payload `river`), 32 events, some with a `group` and/or `cell`. Header: `app ConformanceFixture`, `git_commit fixture0`, `generated_at 2026-01-01T00:00:00Z`, `world fixture-world`, no machine paths. The topic subscriber appears at sample 30, so topic message 0 holds `fromSample 30, count 10`. No bands are configured (`bands: []`), no snapshots, no gaps.

## Decoding (for the Vitest side)

- Arrays are base64 (standard alphabet, `=` padded) of **little-endian** raw values: float32 for `min`/`mean`/`max`, uint16 for group sizes, category counts and float cell codes, uint8 for category cell codes. `HistoryEncoding.h` is the C++ reference.
- `groupSizes[group]`: uint16 per sample covering `[aggregateFirst, sampleCount)`.
- `aggregates[layer]`: `{kind, first, groups}`. Float layers: `groups[group] = {min, mean, max}`, each float32 per sample covering `[first, sampleCount)`; NaN means no cells. Category layers: `values` (enum ids in order) and `groups[group]` = uint16 counts, **sample-major**, `values.length` per sample.
- `samples`: `{tick, label, band}` for retained headers starting at `header.sampleFirst`.
- `events`: `{sample, type, group?, cell?, payload?, text?}` in order; `type` is the schema key (`crc:xxxxxxxx` when the type has no schema entry).
- `cells[layer]` (only with `--cells`): `{kind, first, count, width, height, min, max, data}`; `data[i]` is sample `first + i`: width*height codes. Float layers: uint16 code `q`, value = `min + q / 65534 * (max - min)`, and `q = 65535 (0xFFFF)` is NaN. Category layers: uint8 category id per cell.
- Query replies and topic messages use the same field names and encodings: an aggregates reply or topic message carries `{count, samples, groupSizes, rows}` where `rows[layer]` is shaped like `aggregates[layer]` but covers only `[from, from+count)` (topic: `[fromSample, fromSample+count)`); a reply before a layer's `first` is NaN / zero counts. Cursors: aggregates `next:{from}`, events `next:{from, skip}` (pass both back), cells `next:{from, cellOffset}`, meta `next:{part, offset}`. `structure` is a 16-hex hash; a topic message with `structureChanged: true` means re-fetch `cells.history.meta`.

## Catch-up cursor (for T-1)

- Request `cells.history.aggregates {from?, to?, layers?, groups?, stride?, layerOffset?, structure?}`. `groups` is an array of group ids; `stride` N >= 1 re-aggregates windows of N samples relative to `from` (partial last window kept; float min/max/group-size-weighted mean; category layers = the window's last sample's counts); `stride` is echoed in the reply only when > 1.
- Normal path (whole samples fit the 15,000-byte budget): `count` samples/windows, `next = {from}` while more remain.
- When one whole sample (or window) does not fit, the reply is ONE sample/window holding a layer slice `[layerOffset, layerOffset+layerCount)` of `layerTotal` layers (`count` 1; `samples` and `groupSizes` repeat in every slice), with `next = {from, layerOffset, structure}`; when the slice completes and more remain, `next = {from + 1 (or + stride), layerOffset: 0, structure}`.
- The server is stateless: re-send to/layers/groups/stride plus the `next` fields (`from`, `layerOffset`, `structure`) unchanged. Merge slices of the same `from` by layer name.
- Errors: stale `structure` -> `structure changed: restart the catch-up` (re-fetch meta and restart); one layer too big -> `reply too large: request fewer groups`.

## Regenerating

The GoogleTest `HistoryConformance.CommittedFixturesEqualTheRegeneratedBytes` regenerates all files in memory and fails when any byte differs from the committed file (CR characters are ignored, so autocrlf checkouts pass). After a deliberate format change:

```bash
DIA_UPDATE_GOLDEN=1 dia run googletest --filter="*CommittedFixturesEqualTheRegeneratedBytes*" --shards 1
```

(`DIA_CELLHISTORY_REGEN_FIXTURES=1` also works.) Then review the diff, run the whole `*HistoryConformance*` filter, and tell T-1 so the Vitest side follows. Never hand-edit the generated JSON.

## Consumers

- C++: `TestHistoryConformance.cpp` (regeneration check, decoding against the recorder, query/topic pages versus the export file, live-follow reconstruction).
- TypeScript: T-1's file-source and live-source tests (future).
