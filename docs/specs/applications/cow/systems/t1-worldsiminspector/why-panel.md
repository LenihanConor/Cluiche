# Feature Spec: T-1 Why Panel

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/navigation.md (Feature 6)
**Needs:** A1 Ecology `cow.ecology.cause` events (amended by this spec), @docs/specs/applications/cow/systems/a1-ecology/a1-ecology.md

**Gameplay Domains:** debug

## Summary

The panel that answers "why did this change?". The user chooses a region (or the whole world), a tracked layer and a period (the shared brush). T-1 sums the A1 ledger's cause events over that period and shows the result as a signed bar list: which causes added to the layer, which took away, and by how much. "Show cells" selects, in the live game, the cells that contributed most to a chosen cause.

## Problem

A time series shows that herbivores crashed in year 12. It does not say whether that was drought, predation, starvation or migration. A1's ledger knows, but it keeps only the last band and is overwritten each band. A1 records the top causes per band as events so the answer survives; T-1 needs to add them up over a period and let the user see where the cause happened.

## Goals

1. **Explain a change.** For a region, a tracked layer and the brush period, show the summed delta per cause, signed, largest first.
2. **Reconcile.** The bars plus an "Unattributed" remainder equal the layer's actual net change over the period.
3. **Find the place.** "Show cells" selects the cells that contributed most to a chosen cause, in the live game.
4. **Honest gaps.** No cause data gives an empty state that says why, not a fallback that looks like an explanation.
5. **Every cause is described.** Cause names, units and descriptions come from the DiaValueSchema entry for `cow.ecology.cause`.

## Non-Goals

- Per-band stacked bars over time (a later view).
- Causes for groups other than regions and the whole world (A1 emits causes per region only).
- Recomputing or editing the ledger, or reading the live ledger (`cow.ecology.ledger`); T-1 uses recorded events only.
- Comparing two sources side by side (Feature 5).
- Causes for layers other than A1's tracked ecology layers.

## Architecture

### A1 contract (amendment)

`cow.ecology.cause` (A1, per band, per region, per tracked layer, top 3 causes) gains the cells behind each cause:

```
cow.ecology.cause {
  group,                    // region id
  layer,                    // e.g. ecology.group.herbivore
  cause,                    // id from the cause vocabulary (schema-described)
  delta,                    // summed over the region this band
  cells: [ { q, r, delta } ]   // top 8 contributing cells by |delta| (new)
}
```

- `cells` is limited to `cause_cells_per_event` (A1 parameter, default 8) per event, in order of absolute delta. The number of causes per region is `cause_events_per_region` (default 3).
- Cell deltas are those the equations wrote to the ledger for that cause, so they sum to at most the event's `delta`.
- The event type and its fields are described in DiaValueSchema (`kind: event`), including the cause vocabulary with labels and units.

### Files

Under `Cluiche/CoW/Editor/WorldSimInspector/UI/`:

```
model/
  causes.ts       sumCauses(source, group, layer, brush) → { bars[], other, unattributed, net }
                  topCells(source, group, layer, cause, brush, limit) → Cell[]
  (uses events.ts and series.ts from Feature 2 for net change)
store/             whyGroup, whyLayer, whyCause
views/
  WhyPanel.tsx     pickers, bar list, net line, Show cells button
  CauseBar.tsx     signed bar with label, unit, tooltip
```

Views call `model/`, the store and Feature 6's navigation model only.

### Summation

- Collect every `cow.ecology.cause` event inside the brush for the chosen layer and group (all regions when the group is Whole world).
- Sum `delta` per `cause`. Rank by absolute size and show the **top 8**; the rest are combined into **Other**.
- **Net** is the layer's actual change over the period, from the aggregate at the brush end minus the aggregate at the brush start, for the same group.
- **Unattributed** = net minus the sum of all cause deltas (including Other). It is shown as its own bar so the list always reconciles.
- Bars are signed: positive extends right, negative left, with the unit from the schema.
- Because A1 emits only the top causes per region per band (`cause_events_per_region`, default 3), small causes never appear in events. They land in Unattributed, and the panel's footer says so.
- The Unattributed bar shows its share of |net| ("Unattributed: 14% of net"). Above 25% of |net|, the panel adds a note: "Many small causes aren't recorded. Raise `cause_events_per_region` for a fuller breakdown."

### Show cells

- Choosing a bar and pressing **Show cells** merges the `cells` lists of that cause's events in the brush, sums each cell's delta, and keeps the top contributors (by absolute delta).
- The result is sent through Feature 6 as a `cells` intent (`cells.select.cells`), at the brush end sample. The panel shows "Selected N of M" using the accepted set the game returns.
- Because each event carries at most 8 cells per cause per region per band, the merged list is a *lower bound* on contributors; the footer says "top contributing cells recorded".
- Disabled, with the Feature 6 tooltip, when navigation is not allowed.

### Pickers

- **Group:** Whole world or one region (from the source's region grouping).
- **Layer:** layers that have cause events in the source; others are not listed.
- **Period:** the shared brush; the panel shows its start and end labels.

### Empty states

| Situation | Behaviour |
|---|---|
| Layer not tracked (live) | "No cause data for this layer." with **Track this layer** (Feature 3 rules) |
| File has no cause events for the layer or period | "No cause data in this period." No button |
| Period before A1 emitted causes | Same message, naming the first sample with cause data |
| Source has no `cow.ecology.cause` event type (not an A1 run) | The Why panel is hidden from the layout |

### Logging and observation

Channel `[EDITOR] T1`. Logged: cause sums truncated into Other, unattributed share above a threshold (10% of |net|), cells lists merged for Show cells. `performance.mark` around summation in dev builds.

## Acceptance Criteria

1. For a chosen region, layer and brush, the panel lists summed deltas per cause, signed and sorted by absolute size, with label and unit from the schema.
2. At most 8 causes are listed; further causes are combined into Other with a count.
3. The net change equals the aggregate at brush end minus brush start for the same group and layer.
4. An Unattributed bar equals net minus all listed and combined causes, so the bars always reconcile to the net. It shows its share of |net|, and above 25% the panel adds the "raise `cause_events_per_region`" note.
5. Whole world sums the events of every region; changing the brush recomputes the bars.
6. Show cells merges and sums the chosen cause's cell lists across the brush and sends a `cells` intent at the brush end sample.
7. After Show cells, the panel shows "Selected N of M" from the accepted set.
8. Show cells is disabled, with a reason tooltip, whenever navigation is disabled.
9. For a layer with no cause events, the panel shows the empty state, with a Track button only for a live source where tracking applies.
10. If the source has no `cow.ecology.cause` event type, the Why panel does not appear.
11. A1 emits `cells` (up to 8, by absolute delta) on each cause event, and the schema describes them (A1 amendment, tested in A1).
12. Visual match against the mockup's Why panel.

## Test Strategy

- **Vitest (model/):** sums over a fixture event set (multiple bands and regions); top 8 and Other; signed bars; net from aggregates; unattributed reconciliation; whole-world vs region; cell merging and top limit; empty states.
- **React Testing Library:** pickers, bars, tooltips from the schema, Show cells enabled/disabled, "Selected N of M".
- **A1 (when built):** the cause event carries up to 8 cells, ordered by absolute delta, and their deltas do not exceed the event's delta.
- **E2E:** stub game publishing cause events; Show cells sends `cells.select.cells` with the expected cells.
- **Visual gate:** compare against `mockups/world-sim-inspector.html` before Done.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | T-1 code in `Cluiche/CoW/Editor/`; cause event is A1's, in CoW |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| CH-004 | Live and file data share one format | Reads events through the `Source` interface only; no live ledger query |
| T-1 Resolved Decision 3 | Cause events only for tracked layers, top 3 per region per band | Used as given; the panel reconciles with Unattributed |
| UI/business separation | Typed model first | Summation in `causes.ts`; view is display |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1, 2, 6 | Source, series, navigation | Approved |
| A1 cause events with `cells` | All of this feature | **Amended 2026-10-20 (a1-ecology.md); A1 not built** |
| DiaCellLayers `cells.select.cells` | Show cells | Requested (Feature 6 dependency) |
| DiaValueSchema | Cause vocabulary, labels, units | Approved, not built |

## Resolved Decisions

1. **Event size** (2026-10-20). Keep 8 cells per event, but as an A1 parameter `cause_cells_per_event`. The worst case (3 × 8 × 16 regions × 10 layers = 3,840 entries a band) assumes everything is full; the S-3 default tracked set has about 2 ecology layers (roughly 770 entries a band at worst). After A1 Feature 3 exists, record events per run in the S-3 long-run report; if a 40-year run's event data exceeds a few MB, lower the default.
2. **Unattributed share** (2026-10-20). Keep the top 3 causes, as the A1 parameter `cause_events_per_region` (raising it to 5 becomes a data change). The panel shows the Unattributed percentage and a note above 25% of |net|; the 10% threshold stays as a log line only. The fix lives in A1, not T-1.
3. **Cause vocabulary stability** (2026-10-20). The raw-id fallback with a logged warning is enough: every export embeds its own schema (Feature 1), so a renamed cause in an old file still has a label. The fallback triggers only for a malformed file. Comparing two files with different labels for the same cause id is handled in Feature 5 (compare by id, label from Source A, warn on a difference).

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs A1's `cow.ecology.cause` events with `cells` (A1 amended 2026-10-20) and DiaCellLayers `cells.select.cells`.
