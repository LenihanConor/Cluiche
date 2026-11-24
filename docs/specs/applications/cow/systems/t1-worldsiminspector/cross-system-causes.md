# Feature Spec: T-1 Cross-System Causes

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md (Feature 12, amendment 2026-11-16)
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/why-panel.md (Feature 4, A1 cause events), @docs/specs/applications/cow/systems/t1-worldsiminspector/correlation-and-run-compare.md (Feature 5, lagged Pearson), @docs/specs/applications/cow/systems/t1-worldsiminspector/navigation.md (Feature 6), @docs/specs/applications/cow/systems/t1-worldsiminspector/seasons.md (Feature 8, anomaly series), @docs/specs/applications/cow/systems/t1-worldsiminspector/space-over-time.md (Feature 10, storm and drought tracks)
**Requests:** A3 amendment (transition event carries its rule and source)

**Gameplay Domains:** debug

## Summary

Answers "did that storm cause the crash, and did the crash cause the scrubland?". It builds **cause chains** across systems: weather, then ecology, then succession. Each link is one of two kinds:
- **recorded**: the simulation says so. An A1 `cow.ecology.cause` event names a weather input, or an A3 transition names its pressure source.
- **candidate**: the editor infers it. Two nodes in the same region are separated by a lag that matches the pair's typical lag. This is correlation, not proof, and is labelled that way.

Chains are shown as time-aligned **swimlanes** (weather / ecology / succession) with link lines, plus a ranked **chain list**. Clicking any node jumps the game there.

There is also an **automatic lead/lag scan**: every metric pair at world level, run on seasonal anomalies so that the yearly cycle doesn't make everything correlate, ranked by strongest lagged r. Any pair can then be drilled down per region.

## Problem

Each system can be inspected alone, but the world-sim story is the interaction between them. The why panel explains one ecology layer's change from A1's ledger. Nothing connects a drought to the crash it caused, or the crash to the succession that followed, or finds which metrics lead which. Without it, tuning one system's effect on another is guesswork.

## Goals

1. **Nodes** from four sources:
   - weather events: storms (with tracks) and drought episodes;
   - A1 events: state changes and crashes;
   - **detected anomalies**: a region's layer stays below its typical-year p10 (Feature 8) for 4 or more weeks;
   - A3 transitions.
2. **Recorded links** from A1 cause events whose cause comes from weather, and from A3 transitions whose `source` names a pressure.
3. **Candidate links** between nodes in the same region (or overlapping a storm footprint), separated by a lag inside the pair type's window, scored by how well that lag matches the pair's scanned lag and by magnitude.
4. **Chains** across lanes, up to 4 nodes, ranked by their weakest link. Recorded links count as full strength.
5. **Display:** swimlanes over the brush with link lines (solid for recorded, dashed for candidate), plus a ranked chain list. Clicking a node raises a Feature 6 intent.
6. **Lead/lag scan:** all metric pairs at world level, on anomalies by default, lags 0 to 104 weeks, ranked by strongest |r|, with lag, r and effective sample size. Drilling into a pair shows it per region.
7. **Honest labels:** every candidate link and scan result says "candidate (correlation)". Recorded links name the event they come from.

## Non-Goals

- Proving causation, counterfactual runs or interventions. T-1 never runs the simulation.
- Links outside the same region (except through storm footprints). See Open Design Questions.
- Dragons and later systems. The node and link model takes new sources as data, but none are added.
- New recorded causes from A4. The amendment considered asking A4 and A3 for cause events. A4 is not asked: weather has no upstream inside the world sim, and A1 already records weather as a cause. A3 only needs its existing `source` carried on its event (below).
- Changing A1's ledger or cause events.

## Architecture

```
UI/causes/cause_origins.json     envelope cow.t1_cause_origins v1
  { "<A1 cause id>": "weather" | "ecology" | "succession" | "other", ... }   e.g. rainfall -> weather, grazing -> ecology
model/causes/                    (pure TS)
  nodes.ts       buildNodes(source, brush, opts) -> Node[] { id, lane, kind, region, cells?, from, to, layer?, magnitude }
  anomalyNodes.ts  detect(seriesByRegion, typicalYear, minWeeks=4) -> Node[]       uses Feature 8 baselines
  recorded.ts    recordedLinks(nodes, causeEvents, transitions, origins) -> Link[] { from, to, kind: 'recorded', evidence }
  candidate.ts   candidateLinks(nodes, lagProfile, windows) -> Link[] { kind: 'candidate', lag, score }
  chains.ts      chains(nodes, links, maxLen=4) -> Chain[] ranked by min link score
  lagScan.ts     scanWorld(metrics, anomaly=true, lags=0..104) -> Pair[] { x, y, lag, r, nEff }   (worker)
                 scanRegions(pair) -> per region { lag, r, nEff }
store/  causes { brush, lanesOn, minScore, scan, drill }
views/  Swimlanes, ChainList, LinkTooltip, LagScanTable, PairDrill
```

- Views call only `model/`, the store and Feature 6's navigation model (UI/business separation rule).
- The scan runs in a Web Worker with progress and cancel.

### Nodes

| Lane | Node | From |
|---|---|---|
| Weather | storm (id, path, peak size) | `weather.storm` and `weather.storm_track` (Feature 10) |
| Weather | drought episode (region, start, end) | `weather.drought`, `weather.drought_end` |
| Ecology | A1 state change or crash | A1 DiaCellHistory events |
| Ecology | anomaly drop (region, layer, start, end, depth) | Below the typical-year p10 for 4 or more consecutive weeks (Feature 8) |
| Succession | transition (cell, region, from, to, rule, source) | `cow.succession.transition` (A3 amendment) |

A node's region comes from its event `group`, from its cell's region (`groupCells`, Feature 10), or for storms from the regions its footprint crosses.

### Links

**Recorded:**
- **Weather → ecology:** an A1 `cow.ecology.cause` event in region R and band b, whose `cause` maps to `weather` in `cause_origins.json`, links the ecology node in R on that `layer` overlapping b to the weather node active in R at b. If no weather node is active, it links to an "unattributed weather" node with the cause name.
- **Ecology or weather → succession:** an A3 transition whose `source` is a pressure (`DROUGHT`, `OVERGRAZING`, `FIRE`, ...) links from the matching node in the transition's region that precedes it. If none matches, it links from an "unattributed <source>" node.
- The evidence (event id, delta, top cells) is shown on the link.

**Candidate:**
- **Windows by pair type:** weather → ecology 0 to 26 weeks; ecology → ecology 0 to 52 weeks; ecology → succession 0 to 104 weeks. They are stored in `cause_origins.json` beside the mapping, so they can be tuned as data.
- **Score:** a value between 0 and 1. It combines (a) how close the observed lag is to the pair's scanned lag at world level (Gaussian over 8 weeks) and (b) the target node's magnitude relative to its layer's spread. Links below `minScore` (default 0.3, user-adjustable) are hidden.
- A candidate link that duplicates a recorded link (same two nodes) is dropped in favour of the recorded one.

### Chains

- A chain is a path that moves forward in time across lanes. Lanes can repeat (ecology → ecology) but time can't go backwards. A chain has at most 4 nodes.
- Its score is the minimum link score (recorded links score 1). Ties are broken by total magnitude.
- The chain list shows the top 50, with a summary such as "Drought R3 W12 → producers −0.34 (R3, +6 w) → Grassland→Scrub ×14 (R3, +41 w)". Each part is clickable.

### Lead/lag scan

- **Metrics:** every float layer's world mean (up to the recorder's 64-layer cap), on anomaly series by default. Raw series are a toggle, with a warning that the seasonal cycle inflates r.
- **Output:** for each ordered pair, the lag (0 to 104 weeks) with the largest |r|, plus r and the effective sample size (adjusted for autocorrelation). The ranked table can be filtered by layer prefix (`weather.`, `ecology.`).
- **Drill:** for one pair, the same scan per region, drawn as a small table and a map of lag or r per region (on Feature 10's region fill).
- **Cost:** 64 × 63 pairs × 105 lags × 2,080 samples is about 9 × 10⁸ multiply-adds. That runs in a worker in seconds, with progress. Results are cached per source and brush.

### Logging and observation

Channel `[EDITOR] T1`. Logged: nodes and links built (counts by kind), unmapped cause ids (once each), scan start, progress milestones, finish (pairs, ms), cancel, and drill-downs. Dev builds add `performance.mark` around chain building.

## Acceptance Criteria

1. Nodes are built from every source in the table, each in its lane, with region, time range and magnitude. A source that is missing (A3 not built, Feature 10 tracks absent) leaves its lane empty with the reason.
2. Anomaly nodes are created when a region's layer is below its typical-year p10 for at least 4 consecutive weeks, and they match a fixture.
3. A recorded weather → ecology link is made for each A1 cause event whose cause maps to `weather`. With no active weather node, it links to an "unattributed weather" node. An unmapped cause id is logged once and treated as `other`.
4. A recorded → succession link is made for each A3 transition whose `source` is a pressure, from the latest matching preceding node in its region, otherwise from an "unattributed <source>" node.
5. Candidate links connect nodes in the same region or overlapping a storm footprint, within the pair type's lag window, scored as stated. Links below `minScore` are hidden, and a duplicate of a recorded link is dropped.
6. Chains move forward in time, have at most 4 nodes, are ranked by weakest link, and the list shows the top 50 with clickable parts.
7. Swimlanes draw nodes over the brush with recorded links solid and candidate links dashed. Hovering a link shows its evidence or its lag and score.
8. Clicking a node raises a Feature 6 intent: an event intent for event nodes, a group intent with the history view at the node's start for anomaly nodes.
9. The lead/lag scan covers all world-level float-layer pairs on anomaly series by default, reports lag, r and effective sample size, ranks by |r|, runs in a worker with progress and cancel, and caches by source and brush.
10. Drilling into a pair shows the per-region lag and r as a table and a region map.
11. Every candidate link and scan result is labelled "candidate (correlation)". Raw-series scans show the seasonal warning.
12. `cause_origins.json` loads and validates (envelope, known origins, numeric windows). A bad entry is rejected with the reason.
13. Perf: chain building for a 40-year run with 30 regions and about 5,000 nodes finishes in under 1 s in a worker. The full world scan at 64 layers finishes in under 30 s with progress at least every second.

## Test Strategy

- **Vitest (model/):**
  - node building per source
  - anomaly detection edges (3 vs 4 weeks)
  - recorded links, including unattributed and unmapped causes
  - candidate scoring and window edges
  - recorded-over-candidate de-duplication
  - chain ordering and the forward-in-time rule
  - lag scan on a synthetic pair with a known lag (anomaly vs raw)
  - effective sample size on an autocorrelated series
- **React Testing Library:** swimlanes with both link styles, chain list click intents, scan table progress and cancel, drill map.
- **E2E:** a stub game with fixture history containing a drought, an A1 cause event naming rainfall, an anomaly drop and an A3 transition with `source: DROUGHT`. The top chain reads drought → producers → transition with two recorded links, and clicking the transition sends the navigation commands.
- **Perf:** AC 13 on generated fixtures.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All T-1 code in `Cluiche/CoW/Editor/WorldSimInspector/`. The A3 change is in A3's own CoW module |
| CoW AD-003 | GDD owns design intent | No design targets set here. Lag windows and score thresholds are analysis settings, not game design |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| Tool tiers (Inspector) | Inspector reads, never tunes or runs the sim | Reads only. No counterfactual runs (Non-Goals) |
| UI/business separation | Typed model first | `model/causes/`. Views render nodes, links and chains only |
| A1 cause event contract (amended 2026-10-20) | `cow.ecology.cause {group, layer, cause, delta, cells}` | Used as is. Origins are mapped editor-side |

No conflicts.

## Requested amendment (applied on approval)

**A3: transition events carry rule and source.** The `cow.succession.transition` DiaCellHistory event payload is `{ cell, from, to, rule, source }`, where `rule` is the `succession_rules.json` id and `source` is the rule's `source` (for example `DROUGHT`). Its `kind: event` schema entry lists these fields. The `BiomeTransitionComplete` message is unchanged. CoW-side, no sign-off.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| T-1 Features 4, 5, 6, 8, 10 | Cause events, lagged Pearson, navigation, anomalies and typical year, tracks and region fill | Approved, not built |
| A1 (cause events, state and crash events) | Ecology nodes, recorded links | Approved, not built |
| A3 + amendment | Succession nodes, recorded links | Approved, not built |
| A4 Feature 5 + T-1 consumer amendment | Weather nodes | Approved, not built |

Meaningful only once A4, A1 and A3 run together (S-3 Living mode, S-3.5). Built against fixtures first.

## Open Design Questions

1. **Same-region only.** Drought fronts and storms cross region borders, and ecology effects can show up next door. v1 links within a region, plus storm footprints. Adjacent-region links need a region adjacency graph. S-1 has geography nodes and regions, but no adjacency query is exposed yet. Add it if real chains look truncated at borders.
2. **Too many candidates.** With 64 layers and 30 regions, many pairs reach a high |r| by chance. The effective sample size and the anomaly default reduce this but don't remove it. If the scan table fills with noise, add a multiple-comparison correction (for example, false discovery rate) as a filter.
3. **Where cause origins live.** `cause_origins.json` duplicates knowledge A1 owns: which ledger causes come from weather. The cleaner home is an `origin` on each cause value in A1's DiaValueSchema entry, read from the embedded schema. v1 uses the T-1 file, with a test that flags unmapped causes. Move it to A1's schema when A1 is built, if A1 agrees.

## Status

`Approved` (2026-11-16). A3 T-1 consumer amendment applied to `a3-biomesuccession.md` the same day. No plan. The views are drawn in the new world-sim mockup (owed, see the parent amendment) before UI work.
