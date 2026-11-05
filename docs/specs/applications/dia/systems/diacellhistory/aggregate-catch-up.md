# Feature Spec: DiaCellHistory Aggregate Catch-Up

**Parent:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md
**Builds on:** @docs/specs/applications/dia/systems/diacellhistory/export.md (Feature 5: `cells.history.aggregates` query, `HistoryQueries`)

**Gameplay Domains:** debug, tools

## Summary

`cells.history.aggregates` replies are capped at 15,000 bytes (the sim-hosted limit is 16,384; the Feature 5 spec assumed 1 MB). The query returns whole samples only and continues with `next.from`, so a request whose single sample does not fit fails with "reply too large: request fewer layers". A 20-layer x 27-group run cannot be caught up without the client already knowing which layers to ask for, and a 2,000-sample catch-up of one layer takes about 100 queries.

This feature makes the query always make progress and gives clients a cheap first screen:

- a **layer cursor**: when one whole sample does not fit, the reply holds a slice of layers, and `next` carries `{ from, layerOffset }` (the same pattern `cells.history.cells` uses with `cellOffset`);
- a **`groups` filter**: the client names the groups it wants (for example only `world`);
- a **`stride`**: one output sample per window of N recorded samples, re-aggregated, so T-1 can draw a whole-run overview in a few round trips and fill detail afterwards.

The topic and the `dia.cell_history` file are unchanged.

## Goals

- Any valid `cells.history.aggregates` request succeeds; no "reply too large" error for a valid request.
- A client can request only the groups it needs.
- A client can fetch a whole-run overview in a small, bounded number of queries.
- Existing clients that send `{ from?, to?, layers? }` keep working.

## Non-Goals

- No change to the 15,000-byte reply budget, the topics or the file format.
- No change to `cells.history.events` or `cells.history.cells`.
- No server-side caching of strided results.

## Tasks

| # | Task | Size |
|---|---|---|
| 1 | Request parsing: `groups` (array of group ids), `stride` (integer >= 1), `layerOffset` cursor field; validation and errors | S |
| 2 | Groups filter: restrict `groupSizes` and each layer's `groups` to the named groups; unknown group is an error | S |
| 3 | Layer-sliced reply: when a whole sample does not fit, fill layers in order until full; `next = { from, layerOffset }`; continue until the sample's layers are complete, then `{ from + n, 0 }` | M |
| 4 | Stride re-aggregation: per window, float layers min of mins / max of maxes / group-size-weighted mean of means; category layers report the last sample's counts (Design Decision 2); one output sample per window, partial last window included | M |
| 5 | Interaction: stride with layer slicing and groups filter; cursors carry the stride so a follow-up reply continues the same windows | S |
| 6 | Help text, `ValueParam`s, module doc and conformance fixtures (`Dia/DiaCellHistory/Testing/Conformance/`) updated; Feature 5 spec amended in place (query row, Open Design Question 2 closed, 15,000-byte cap noted) | S |
| 7 | Exhaustive test pass: gap audit, edge tests (empty range, range outside the retained window, structure change mid-catch-up, one group, stride larger than the range), reference-computation check of stride | M |

## Query contract

Request: `{ from?, to?, layers?, groups?, stride?, layerOffset? }`.

| Field | Meaning | Default |
|---|---|---|
| `from`, `to` | Sample range, `to` exclusive | The retained window |
| `layers` | Layer ids to include | All recorded layers |
| `groups` | Group ids to include (for example `["world"]`) | All groups |
| `stride` | Window size N in recorded samples; 1 = no re-aggregation | 1 |
| `layerOffset` | Index (within the filtered layer list) to resume at; set from `next`, not by hand | 0 |

Reply: the Feature 5 shape (`{ structure, from, count, samples, groupSizes, rows }`), restricted by `groups` and `layers`, plus `stride` echoed. `next` is `{ from, layerOffset }`; when `layerOffset` is 0 the cursor continues at the next sample (or window). A request that carries `layerOffset > 0` returns the rest of the layers for that one sample (or window) only, as `cells` does for a row.

- With `stride > 1`, `from` and `to` are aligned down to a window boundary relative to `from`; `samples[i]` carries the tick, label and band of the **last** recorded sample in window `i`; `count` is the number of windows in the reply; `from` of a following cursor is the first recorded sample of the next window.
- `groupSizes` for a window is the size at the window's last sample.
- Errors (nothing is changed): `stride` < 1, unknown layer, unknown group, `layerOffset` past the layer list, range outside the retained window, cursor with a different `structure` than the recorder's current hash ("structure changed: restart the catch-up").

## Acceptance Criteria

1. A request that previously failed with "reply too large" (one sample of 20 layers x 27 groups) now succeeds and returns a slice with `next = { from, layerOffset }`.
2. Following the cursor to the end and concatenating the slices equals the data an unlimited-size reply would carry, sample for sample and layer for layer.
3. Every reply serialises to at most the query's reply budget (15,000 bytes), including `next`.
4. `groups: ["world"]` returns only the `world` group in `groupSizes` and in every layer's `groups`; an unknown group returns an error and changes nothing.
5. With `stride = N`, each output sample's float min, mean and max equal a reference computation over its window (min of mins, max of maxes, group-size-weighted mean); the last window may be partial and is included.
6. With `stride = N`, `samples[i]` carries the last recorded sample's tick, label and band of window `i`, and a following cursor starts at the next window.
7. A `stride` larger than the range returns one output sample covering the range; `stride = 1` returns exactly the unstrided data.
8. `stride`, `groups` and `layers` combine: slices and cursors keep the same filters and stride across replies.
9. A cursor created before a `structure` change returns the "structure changed" error; a fresh request after it succeeds.
10. Requests with only `{ from?, to?, layers? }` return the same data as before this feature when their samples fit one reply.
11. `stride` < 1, an unknown layer, an unknown group, a bad `layerOffset` or a range outside the retained window returns an error object naming the reason and changes nothing.
12. With a 20-layer x 27-group run of 2,000 samples, a `{ groups: ["world"], stride: 50 }` overview needs at most the number of queries recorded in the plan (target: 3 queries or fewer, confirmed by measurement; see Design Decision 3).
13. The `cells.history.aggregates` topic and the exported `dia.cell_history` file are byte-identical to before this feature for the same run.
14. The query help (`--help`) lists `groups`, `stride` and `layerOffset` with descriptions.

## Test Strategy

- **GoogleTests (TDD):** request validation; groups filter; layer slicing boundaries (a slice ends exactly at a layer boundary, a sample that fits whole, a single layer that alone is too big); reassembly equals single-shot; stride against a reference implementation, including partial last window and category layers; structure change mid-catch-up; backward compatibility for old request shapes; reply budget at the limit (`SetMaxReplyBytes` seam).
- **Conformance fixtures:** add strided and sliced replies to `Dia/DiaCellHistory/Testing/Conformance/` so T-1's Vitest decoders use the same cases.
- **Perf:** query count and build time for the 2,000-sample 20 x 27 catch-up and the strided overview.
- **Mutation check:** only on the stride arithmetic (min, max, weighted mean, window alignment).

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CH-004 | Live and file use one format; queries are chunked | Reply shape is the file's; chunking via the cursor |
| DiaDebugServer sim-hosted reply limit | Replies stay under the 16,384-byte host limit (15,000-byte budget) | Every reply is built to the budget; the cursor guarantees progress |
| Game logic on SimPU | Queries answered on the recorder's PU | Same-PU contract, unchanged |
| PD-004 | No STL in public APIs | DiaCore types in headers |
| AD-001 (Dia) | Changes to shared Dia contracts need sign-off | The query request/reply shape is a Dia contract: flagged below |

No conflicts.

**AD-001 sign-off needed:** extending the `cells.history.aggregates` request and `next` shapes (additive, backward compatible). Raise it when task 1 starts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 5 (Export) | The query, encodings, `HistoryQueries`, conformance fixtures | Done |
| Feature 1 (Recorder) | Aggregate store read API | Done |
| T-1 Feature 1 and the live source | The consumer | Approved |

## Design Decisions (resolved at spec time)

1. **Structure changes restart the catch-up.** The cursor is an index (`layerOffset`), and a cursor whose `structure` hash differs from the recorder's current one returns "structure changed: restart the catch-up". A layer-id cursor was rejected: the client has to refetch `meta` after a structure change anyway.
2. **Category layers under stride report the last sample's counts** (end-of-window state). Summed counts were rejected (different units from a single sample). Transitions inside a window are hidden in the overview; a client zooms in with `stride = 1`. Document this in the schema reference so T-1 labels the overview as end-of-window state.
3. **Round-trip target: a world-only overview in 3 queries or fewer** (to be confirmed by measurement in the plan, which sets AC 12's number). T-1's live source always sends `groups: ["world"]` first (with a `stride` giving about 100 to 200 windows) and loads other groups and `stride = 1` detail lazily per visible layer. No server-side default for `groups`: the request's meaning does not change. The full-detail catch-up is not covered by the target.

## Open Design Questions

None.

## Status

`Approved` (2026-11-05). Plan: [aggregate-catch-up.plan.md](aggregate-catch-up.plan.md).
