# Feature Spec: DiaCellLayers Editor Mirror

**Parent:** @docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md
**Builds on:** @docs/specs/applications/dia/systems/diacelllayers/registry-and-viewer.md (Feature 1), @docs/specs/applications/dia/systems/diacelllayers/selection.md (Feature 2), @docs/specs/applications/dia/systems/diacelllayers/selection-inspector.md (Feature 4)

**Gameplay Domains:** debug, tools

## Summary

Lets an editor outside the game see what the viewer shows. Over DiaDebugServer it adds:

- the topic **`cells.view.state`**, change-detected and rate-limited, which carries the active grid, base layer, overlays, a selection fingerprint and the history mode;
- the queries **`cells.layer.list`**, **`cells.layer.values`**, **`cells.select.get`** and **`cells.select.stats`**.

It is read-only; editors drive the viewer with the existing `cells.*` DiaAPI commands. The first consumers are T-1 (view state for its sync pill, stats for the selection) and T-2 (layer values for its cell editor).

## Problem

The viewer lives in the game, but the editors that explain it (the World Sim Inspector, the World Data Editor) run in CluicheEditor. They need to know which layer and selection the game is showing, to fetch layer values and selection stats, and to tell when the game view no longer matches what they last sent, without every editor inventing its own protocol.

## Goals

1. **One view-state topic** that tells an editor everything about the viewer it needs to stay in step.
2. **Exact selection identity** without sending thousands of indices: a count, a version and a defined order-independent hash.
3. **Pull what you need.** Queries for the layer list, a layer's values, the selection's cells and the selection's stats.
4. **Cheap.** Updates are coalesced; values and stats are chunked or computed on request, never pushed.
5. **Same bytes everywhere.** The hash and value encodings are defined exactly and tested against shared fixtures in C++ and TypeScript.
6. **Read-only.** Nothing in the mirror changes the game.

## Non-Goals

- Controlling the viewer (the `cells.layer.*`, `cells.overlay.*`, `cells.select.*` DiaAPI commands already do that).
- History data (`cells.history.*` belongs to DiaCellHistory), though the topic reserves a field for the history mode.
- Pushing layer values or stats on a topic.
- Editor UI (T-1, T-2 and others consume this).
- Authentication or access control beyond what DiaDebugServer already has.

## Architecture

Lives in `Dia/DiaCellLayers/` (namespace `Dia::CellLayers`), registering the topic and queries with DiaDebugServer, following its existing patterns. Amended by Streams Migration: the four queries are sim-hosted (they run on SimPU via the Sim-Hosted Queries host, not on Main), with no viewer mutex; `debug.domain_state` reads a demand-driven snapshot up to one Sim frame old. The TypeScript helpers live in `@dia/editor-ui` (`Dia/DiaEditorUI`).

```
ViewStatePublisher        builds and publishes cells.view.state (change-detected, coalesced)
SelectionFingerprint      count + version + 64-bit hash of the cell indices
MirrorQueries             cells.layer.list, cells.layer.values, cells.select.get, cells.select.stats
@dia/editor-ui/cells      selectionHash(), decodeLayerValues(), viewState types (TypeScript)
```

### Cell index

A cell index is `r * width + q` on the active grid (row-major offset coordinates, as in Features 1 and 2). Clients convert to `(q, r)` with the grid's width.

### Topic `cells.view.state`

```json
{
  "viewVersion": 412,
  "layersVersion": 7,
  "grid": { "id": "world.hex", "topology": "hex.flat.oddq", "width": 104, "height": 64 },
  "base": "weather.precipitation",
  "overlays": ["weather.wind", "selection.outline"],
  "selection": { "count": 138, "version": 29, "hash": "9c3f0a41d27be615" },
  "history": null
}
```

- `viewVersion` increases on every published change. `layersVersion` changes when a layer or overlay is registered or unregistered, so clients re-query `cells.layer.list` only then.
- `history` is `null` until DiaCellHistory fills it with `{ "mode": "live" | "sample", "sample": <index> }`; the shape is agreed with that spec.
- `hash` is a 16-character lower-case hex string (a 64-bit value; JavaScript cannot hold it as a number).

### Selection hash

For a selection S, the hash is the XOR of `mix(i)` over every cell index i in S, where `mix` is the SplitMix64 finaliser:

```
z = i + 0x9E3779B97F4A7C15
z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
z = (z ^ (z >> 27)) * 0x94D049BB133111EB
mix(i) = z ^ (z >> 31)           // all arithmetic modulo 2^64
```

- It is order-independent, and the empty selection has hash `0000000000000000`.
- A client that knows the accepted set (T-1 after `cells.select.cells`) computes the same hash and compares it to the topic's. If they differ it can fetch the set with `cells.select.get`.
- Shared test vectors (index lists and their hashes) live in `Dia/DiaCellLayers/Testing/Conformance/selection_hash.json` and are checked by both the GoogleTests and the Vitest tests.

### Publishing

- The publisher compares a fingerprint of the published fields each frame and publishes only on change.
- Updates are **coalesced to at most 10 per second**; if changes arrive faster, the latest state is held and sent when the interval ends, so the last state is always delivered once changes stop.
- A new subscriber receives the current state immediately.

### Queries

| Query | Arguments | Result |
|---|---|---|
| `cells.layer.list` | none | Layers and overlays grouped by grid: id, `schemaKey`, group, kind (float or category), grid id, hotkey, plus `layersVersion` |
| `cells.layer.values` | `{ layer, offset?, count? }` | `{ layer, version, gridId, width, height, offset, count, total, encoding: "f32le-base64", data }` |
| `cells.select.get` | `{ offset?, count? }` | `{ version, hash, total, offset, indices: [..] }` (sorted ascending; up to 1,000 indices per call) |
| `cells.select.stats` | `{ layers: [ids] \| "all" }` | The Feature 4 stats JSON for the current selection: per-layer stats, selection `count`, `version`, `hash` |

- **Layer values** are little-endian float32 per cell, base64-encoded; category ids travel as exact floats. A reply stays under the 16 KB sim-hosted cap (at most 1,000 cells per call, about 5.3 KB of base64); a layer bigger than that is fetched in chunks by `offset` and `count`, and the `version` lets a client detect a change between chunks. The values are the ones the viewer shows (the same `LayerReader` the query uses), so they are scrub-consistent once DiaCellHistory is built.
- **Stats** are computed on the viewer's PU at most once per frame, and the response is sent when computed (one frame of latency at most). `layers: "all"` is allowed because it is an explicit request; the Feature 4 cost rules do not apply, but the one-pass-per-frame limit does.
- An unknown layer, an out-of-range `offset` or a non-numeric argument returns an error object and changes nothing.

### Logging and observation

- **Channel:** `CEL`.
- **Debug:** queries served (name, bytes); Info: first subscriber, last subscriber leaves.
- **Warning:** a query asked for more than the limit (clamped, once per client).
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on topic build and on each query.
- **Metrics:** topic publishes, coalesced updates, queries by kind, bytes served.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. `cells.view.state` carries `viewVersion`, `layersVersion`, the grid, the base layer, the overlays, the selection `{count, version, hash}` and `history`.
2. It publishes only when one of those changes; a subscriber receives the current state on subscribe.
3. Changes are coalesced to at most 10 publishes per second, and the final state is always delivered once changes stop.
4. `layersVersion` changes when a layer or overlay is registered or unregistered, and not otherwise.
5. The selection hash equals the XOR of the SplitMix64 mix of every selected index; the empty selection hashes to `0000000000000000`; the C++ and TypeScript implementations both match the shared vectors.
6. `cells.layer.list` lists layers and overlays by grid with the fields above and `layersVersion`.
7. `cells.layer.values` returns base64 float32 values with the layer version; a layer larger than one chunk is returned correctly in chunks that concatenate to the full array; each response is under 1 MB; the TypeScript decoder matches the C++ encoding.
8. `cells.layer.values` returns the values the viewer shows for that layer; an unknown layer returns an error.
9. `cells.select.get` returns the selection's indices in ascending order in chunks of at most 1,000, with the version and hash.
10. `cells.select.stats` returns the Feature 4 stats for the listed layers (or all) for the current selection, with selection count, version and hash, computed at most once per frame.
11. Bad arguments (non-numeric offset, out-of-range offset, unknown layer) return errors and change nothing.
12. The mirror changes no game state: no query or topic mutates the viewer, selection or layers.
13. Serving a Large-grid layer (6,656 cells), a full-selection `cells.select.get` and a 20-layer `cells.select.stats` each stay within a frame budget on the viewer's PU.
14. The TypeScript helpers (`selectionHash`, `decodeLayerValues`, the view-state types) are exported from `@dia/editor-ui` and covered by Vitest against the conformance fixtures.

## Test Strategy

- **GoogleTests (TDD):**
  - View-state content for fixture viewers; change detection; coalescing with a fake clock (burst of changes gives at most 10 per second and a trailing publish); late subscriber.
  - Hash: the shared vectors, order independence, empty selection, a full-grid selection.
  - Each query: list, values (single chunk, multiple chunks, boundaries, versions, unknown layer), selection get (chunks, ordering), stats (default vs all, once per frame).
  - Read-only check: state before and after every query is identical.
- **Vitest (`@dia/editor-ui`):** `selectionHash` against the shared vectors (BigInt implementation), `decodeLayerValues` against C++-produced fixtures, view-state type guards.
- **Integration:** a stub game publishing the topic to a stub editor client; T-1's sync pill logic (Navigation spec) run against it.
- **Test utilities** in `DiaCellLayers/Testing/`: fake clock, topic recorder, fixture viewer, the conformance fixtures.
- **Perf:** the three Large-grid responses within budget.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CL-005 | Generic, no game types | Cell indices and layer ids only |
| CL-006 | No singletons | Reached through the registry service and DiaDebugServer registration |
| Tool tiers | Inspector reads, Editor edits files | The mirror is read-only; control is by DiaAPI commands |
| Editor actions via DiaAPI | Every action is a command | No new actions; queries are reads |
| PD-004 | No STL in public APIs | DiaCore types in public headers |
| Game logic on SimPU | Viewer and queries on SimPU | Same-PU contract from Feature 1 |
| CH-004 (CoW) | Live and file data share one format | Stats and values reuse Feature 4 and `LayerReader` shapes |
| AD-001 (Dia) | `@dia/editor-ui` addition needs sign-off | TypeScript helpers flagged for sign-off before build |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1, 2 and 4 | Viewer, selection, stats | Approved |
| DiaDebugServer topics and queries | Transport | Done |
| `@dia/editor-ui` (DiaEditorUI) | TypeScript helpers | Done (extended; sign-off needed) |
| DiaCellHistory | Fills the `history` field | Draft (TBD) |
| T-1 Features 1 and 6 | Consume the topic, stats and hash | Approved |
| T-2 Feature 6 | Consumes `cells.layer.values` | Approved (system spec), features TBD |

## Open Design Questions

1. **Query latency across PUs.** Queries are answered on the viewer's PU, so a reply can take up to a frame. Follow the DiaDebugServer pattern other domains use; check the exact mechanism when planning, and confirm that a frame of latency is acceptable for T-1's sync pill.
2. **`history` field shape (resolved 2026-10-20, DiaCellHistory Feature 2).** `null` when no value source is installed, otherwise `{ "mode": "live" | "sample", "sample": <index or null>, "label": "<text or null>" }`, or in delta mode (DiaCellHistory Compare) `{ "mode": "delta", "a": "now" | "sample:<i>" | "snapshot:<name>", "b": "<same forms>", "scale": <number or null> }`.
3. **Very large grids.** 1,000 cells per `cells.layer.values` chunk (amended from 196,608 by Streams Migration, to fit the 16 KB sim-hosted reply cap) means a Large grid (6,656 cells) takes 7 calls; revisit if a client needs fewer round trips.

## Status

`Approved` (2026-10-20). Plan: [editor-mirror.plan.md](editor-mirror.plan.md) (not started). Build needs AD-001 sign-off for the `@dia/editor-ui` helpers.
