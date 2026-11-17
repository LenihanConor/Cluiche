# Feature Spec: T-1 Species View

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md (Feature 13, amendment 2026-11-16)
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/time-series-and-events.md (Feature 2, series, `derived.ts` and event strip), @docs/specs/applications/cow/systems/t1-worldsiminspector/breakdown-distribution-minimap.md (Feature 3, mini-map and `groupCells`), @docs/specs/applications/cow/systems/t1-worldsiminspector/why-panel.md (Feature 4, cause parsing), @docs/specs/applications/cow/systems/t1-worldsiminspector/correlation-and-run-compare.md (Feature 5, lagged Pearson), @docs/specs/applications/cow/systems/t1-worldsiminspector/seasons.md (Feature 8, typical year)
**Requests (follow-ups, not blockers):** A1 roster links in the history schema; A1 adult and juvenile layers

**Gameplay Domains:** debug

## Summary

A page about **one animal**. Pick a species and see how it lives in the world: its population over time, where it lives, how it fares in each region, why it changed, and what its numbers move with. It is built from data DiaCellHistory already records for A1's `ecology.species.<id>` layers, plus A1's `cow.ecology.cause` events. Two parts that need new A1 data (food web from the roster, and age structure) are shown as clearly marked gaps until A1 provides them.

## Problem

T-1 shows the world, regions and functional groups. It cannot answer "what is happening to wolves?". The species chart overlays all species, and a species can be chosen as a chart line, but nothing opens a view about that animal. Tuning a species (its birth rate, its diet, its habitat) needs exactly that view.

## Goals

1. **Species workspace** with an animal picker listing every roster species, grouped by functional group, with unrecorded species greyed ("not recorded").
2. **Population:** the species' world total over the brush, with a typical-year overlay (Feature 8) and events marked, and its functional group's mean faded behind it for context.
3. **Where it lives:** a mini-map of its density (per-cell when tracked, else region level), with Feature 3's hot-spot and cell-trail modes and Feature 10's playback.
4. **By region:** a table of mean, change and a near-extinction flag per region, sortable; clicking a row selects the region in-game (Feature 6).
5. **Why it changed:** the why panel (Feature 4) scoped to this species' layer, summing A1 cause deltas (starvation, predation, competition, drought, births and so on) for a region and period.
6. **Related to:** the species' strongest lagged correlations against recorded layers (food: producers or prey; predators; weather), as candidates, with lag and r (Feature 12's labelling).
7. **Health:** the species' Feature 11 measures (near-extinction state, boom-bust period, drift) as bands.
8. **Honest gaps:** the food web and age structure panels exist but show what is missing and what it needs, never invented data.

## Non-Goals

- Editing a species' parameters (T-2 and F-3).
- Reading the species roster file directly (it is not part of recorded history). See Open Design Questions 1.
- Individual animals, herds or lineages (a later system; T-1 shows densities).
- A1's per-cell detail rows (juvenile and adult counts per cell): those stay in-game.

## Architecture

```
model/species/
  roster.ts       listSpecies(source) -> [{ id, group, recorded: aggregate|cells|none, label }]   from schema + layer list
  species.ts      speciesSummary(source, id, brush) -> { total, change, band, nearExtinct, regions[] }
  related.ts      relatedLayers(source, id, brush) -> ranked candidates (reuses Feature 12 lag scan restricted to this layer)
  causes.ts       speciesCauses(source, id, region, brush) -> reuses Feature 4 sumCauses with layer = species layer
store/  species { id, region, lens }
views/  SpeciesPicker, SpeciesHeader, PopulationChart, DensityMap, RegionTable, SpeciesWhy, RelatedList, FoodWebPanel (gap), AgeStructurePanel (gap)
```

- Views call only `model/`, the store and Feature 6's navigation model.
- **Recorded status per species** comes from the layer list: `aggregate` (recorded as aggregates only), `cells` (also per-cell tracked), or `none` (greyed).
- Reuses, does not reimplement: Feature 2 series and `derived.ts`, Feature 3 mini-map and cell trail, Feature 4 cause summation, Feature 5 correlation maths, Feature 8 typical year, Feature 11 measures.

### Species states

| State | Shown |
|---|---|
| Recorded with per-cell | Everything |
| Aggregate only | Everything except the density map (region level, with "Track this layer") and cell trail |
| Not recorded | An empty state: "ecology.species.<id> is not recorded in this source. S-3 records every species aggregate." with the picker still usable |
| Near-extinct | A banner naming the regions below the floor and the sample it fell under |

### Gaps shown as panels

- **Food web.** Needs the roster's links (who eats whom, diet weights). Until A1 embeds them in the history schema, the panel lists *candidate* related species by correlation and says "roster links not available". A1 follow-up: embed `diet` and `predators` per species in the schema block of the export.
- **Age structure.** Needs adult and juvenile counts as recorded layers (`ecology.species.<id>.adult`, `.juvenile`). Until A1 records them the panel says so. A1 follow-up, at a cost of up to two layers per species against the 64-layer cap, so only for species chosen in the stage config.

### Logging and observation

Channel `[EDITOR] T1`. Logged: species chosen, recorded status, unavailable panels (and why), related-layer scan (layers, ms). Dev builds add `performance.mark` around `speciesSummary`.

## Acceptance Criteria

1. The picker lists every roster species grouped by functional group, with unrecorded species greyed and not selectable as data.
2. Choosing a recorded species shows its population (world total) over the brush with a typical-year overlay, its group mean faded, and events marked, matching a fixture.
3. The density map is per-cell when the species layer is tracked, otherwise region level with a "Track this layer" action (Feature 6 allow-list) and an explanation.
4. The region table shows mean, change over the brush and a near-extinction flag per region, sortable; a row click raises a Feature 6 `group` intent.
5. The why panel shows this species' cause deltas for a chosen region and period, from `cow.ecology.cause` events for its layer, and says so when the layer is not cause-tracked.
6. The related list ranks recorded layers by strongest lagged |r| with lag and effective sample size, labelled "candidate (correlation)"; the species' own group and itself are excluded.
7. The health bands for this species (near-extinction, boom-bust, drift) use Feature 11's definitions.
8. An unrecorded species shows the empty state; an aggregate-only species hides the density map and cell trail with the reason.
9. The food web panel lists candidates from correlation only and states that roster links are not available; the age structure panel states which layers it needs. Neither shows invented values.
10. Everything works for live and file sources, and with Source B set the population chart overlays B with a Δ column in the region table.
11. Perf: choosing a species updates the page within 500 ms for a recorded 40-year run (aggregates), with the related scan running in a worker.

## Test Strategy

- **Vitest (model/):** species list and recorded status from fixture schemas; summary values and near-extinction flag; related ranking (known lagged pair, group and self excluded); species cause summation; unrecorded and aggregate-only states.
- **React Testing Library:** picker (grouped, greyed), each species state, gap panels, region table sort and intent, why panel for a species.
- **E2E:** a stub game with fixture history for several species including an unrecorded one; choosing a species renders the page and a region click sends the `group` commands.
- **Perf:** AC 11 on a generated fixture.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All code in `Cluiche/CoW/Editor/WorldSimInspector/` |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| Tool tiers (Inspector) | Inspector reads, never tunes | Reads only; no parameter editing |
| UI/business separation | Typed model first | `model/species/`; views only render |
| Component access rule | No service locators; data comes through recorded history | Species data and causes come from DiaCellHistory only, not from the roster file or game state |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| T-1 Features 2, 3, 4, 5, 6, 8, 11 | Series, map, causes, correlation, navigation, typical year, health | Approved, not built |
| A1 species layers (`ecology.species.<id>`), cause events | Population and causes | Approved, not built |
| S-3 recording amendment (all species aggregates) | Every species available | Approved 2026-11-16 (S-3 spec) |
| A1 roster links in the history schema | Food web | **Requested**, not yet in A1 |
| A1 adult and juvenile layers | Age structure | **Requested**, not yet in A1 |

Built against fixtures first.

## Open Design Questions

1. **Roster links: embed or read?** Embedding diet and predator links in the history schema (A1 follow-up) keeps files self-explaining and the Inspector tier clean. Reading `species.json` over the bridge is quicker, but breaks "a file explains itself" for offline files. Proposed: embed.
2. **Age structure cost.** Two layers per species could pass the 64-layer cap quickly, so it should be opt-in per species in the stage config. Decide when A1 Feature 9 is built.
3. **Cause coverage.** A1 emits top-3 causes per region per tracked layer, so a species page only has causes for species whose layers are tracked. If that is too sparse for rare species, ask A1 to emit species-level causes regardless of tracking (more events).

## Status

`Approved` (2026-11-16, with the v2 mockup's Species workspace). No plan. The A1 roster-link and age-structure follow-ups are requests only; A1 specs are not yet changed.
