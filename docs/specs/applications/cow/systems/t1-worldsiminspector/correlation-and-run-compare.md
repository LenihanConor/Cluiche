# Feature Spec: T-1 Correlation and Run Compare

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/time-series-and-events.md (Feature 2), @docs/specs/applications/cow/systems/t1-worldsiminspector/breakdown-distribution-minimap.md (Feature 3)

**Gameplay Domains:** debug

## Summary

Two analysis features that finish T-1's v1. **Correlation** plots one metric against another (with an optional lag) as a scatter, with Pearson r, a least-squares line and the sample count. **Run compare** adds a Source B (live or file) aligned on time to Source A, so the time series overlays both runs, the breakdown shows A, B and Δ, and events from both are listed with source badges. Compare mode touches only those views; the Why panel, distribution, mini-map and navigation stay on Source A.

## Problem

Two questions come up constantly when tuning the world sim: "does X move with Y, and how far behind?" (does rainfall lead vegetation by 8 weeks?), and "how does this run differ from that one?" (a lush pack against an arid one, or before and after a rule change). Reading these off separate charts by eye is slow and error-prone.

## Goals

1. **Correlation.** Plot metric X against metric Y over group × sample within the brush; show Pearson r, the least-squares line and n; allow a lag of Y behind X in samples (0 by default).
2. **Compare two runs.** A second source, live or file, aligned to Source A on time.
3. **See the difference.** The time series overlays A and B; the breakdown shows A, B and Δ; events from both are listed with source badges.
4. **Honest alignment.** The toolbar states how the sources were aligned and how much overlapped.
5. **Honest mismatch.** Sources with different groupings are compared at world level only, with a warning.

## Non-Goals

- Cross-source correlation (X from A, Y from B); each correlation uses one source.
- Side-by-side mini-maps or distributions, Why panel for Source B, navigation for Source B.
- More than two sources.
- Significance tests, partial correlation, rank correlation, regression beyond a straight line.
- Fixing or resampling misaligned runs (no interpolation between samples).

## Architecture

Files under `Cluiche/CoW/Editor/WorldSimInspector/UI/`:

```
model/
  pearson.ts      pearson(xs, ys) → { r, n, slope, intercept } | { undefined: reason }
  correlation.ts  correlationPoints(source, metricX, metricY, brush, lag) → points[{ group, sample, x, y }]
  alignment.ts    alignRuns(a, b) → { pairs[{ a, b }], mode: 'label'|'index', overlap, onlyA, onlyB }
  compare.ts      overlaySeries(a, b, metric, alignment) ; breakdownDelta(aRows, bRows) ; mergeEvents(a, b)
store/             correlation { metricX, metricY, lag, source }, sourceB, alignment
views/
  Correlation.tsx   uPlot scatter + fit line + r / n readout
  SourcePicker.tsx  (Feature 1) gains the Source B slot
  AlignmentNote.tsx toolbar text and warnings
```

Views call `model/` and the store only. Feature 1's store already holds the A and B slots; this feature populates B.

### Correlation

- **Points:** one per group × sample in the brush, for the chosen source. X and Y are each a metric (layer + stat); the group set is the source's chosen grouping (default: regions). A "Whole world" option gives one point per sample.
- **Lag:** Y is taken `lag` samples after X (lag ≥ 0, up to 52 in v1). Points whose lagged sample falls outside the run are dropped.
- **Pearson r** over all plotted points, with **n**, plus a least-squares line. When n < 3, or either axis has zero variance, r is shown as "undefined" with the reason.
- **Colouring:** points are coloured by group so within-group patterns stay visible; the legend toggles groups, and r is recomputed on the visible set. The readout also shows r for the whole-world series when the pooled r differs in sign from it, as a warning that pooling across groups can mislead.
- **Labels and tooltips:** from the schema, as in the other views.

### Run compare

- **Source B:** chosen in the toolbar's second slot (Live or a file). Live can be B only if A is a file; two live sources are not offered.
- **Alignment (`alignRuns`):**
  1. Match samples by label (for example "Y3 W12"). Samples in only one source are listed as `onlyA` / `onlyB`.
  2. If fewer than half of the labels match, fall back to sample index and say so.
  3. The toolbar shows the result: "Aligned by label: 2,080 of 2,080" or "Aligned by index (labels differ): 1,000 of 1,040".
- **Time series:** each plotted metric draws for A (solid) and B (dashed), on the shared axes; the legend names the source. Samples with no counterpart are drawn for their own source only.
- **Breakdown:** shows A, B and Δ (B − A) for each group and layer, with Δ coloured by sign. Rows present in only one source are shown with a blank for the other.
- **Events:** the timeline lists events from both sources with an A or B badge; the badge colour matches the series style. Event ids are compared by id; if the same cause or event id has different labels in the two schemas, the label from Source A is shown and a warning is logged.
- **Different groupings (different worlds):** if the two sources have different groupings, only world-level aggregates are compared. The Δ column in the breakdown is disabled with a banner: "These runs are from different worlds. Only world-level values are compared."
- **Features that stay on Source A:** Why panel, distribution, mini-map and navigation. Each shows a small "Source A" badge in compare mode.

### Logging and observation

Channel `[EDITOR] T1`. Logged: Source B opened or closed, alignment mode and overlap, fall-back to index, grouping mismatch, event label differences, undefined r with the reason. `performance.mark` around correlation and alignment in dev builds.

## Acceptance Criteria

1. The correlation view plots one point per group × sample in the brush for the chosen source, with r, n and a least-squares line.
2. With a lag of N, each Y is taken N samples after its X, and points beyond the end of the run are dropped; lag 0 is the default.
3. r is shown as "undefined" with a reason when n < 3 or either axis has zero variance.
4. Toggling a group in the legend recomputes r on the visible points.
5. When the pooled r and the whole-world r have opposite signs, the readout shows both and a warning.
6. With Source B set, the time series overlays B (dashed) on A (solid) for each metric, with the source named in the legend.
7. Alignment matches samples by label; with fewer than half matching it falls back to index; the toolbar states the mode and the overlap.
8. Samples present in only one source are drawn for that source only and are never interpolated.
9. The breakdown shows A, B and Δ (B − A) for each group and layer; rows present in only one source show a blank for the other.
10. The event timeline lists events from both sources with A/B badges.
11. If the two sources have different groupings, only world-level aggregates are compared: Δ is disabled and the banner is shown.
12. Two live sources cannot be selected.
13. Why panel, distribution, mini-map and navigation stay on Source A in compare mode and show a Source A badge.
14. Visual match against the mockup's correlation and compare views.

## Test Strategy

- **Vitest (model/):**
  - Pearson on known data (perfect positive, negative, zero, constant) and with lag.
  - Pooled vs whole-world sign difference warning.
  - Alignment by label, partial overlap, fall-back to index, only-A and only-B samples.
  - Overlay series with gaps.
  - Breakdown Δ with a row missing from one source.
  - Different-grouping fallback to world level.
  - Event merge with label differences.
- **React Testing Library:** correlation readout and undefined states, legend toggle recompute, Source B slot (two live refused), alignment note, banner, Source A badges.
- **E2E:** two fixture files with the same labels overlay; a fixture from a different world disables Δ.
- **Perf:** a 2,080-sample × 30-group correlation (62,400 points) computes in under 100 ms and draws within a frame budget; alignment of two 2,080-sample runs under 20 ms.
- **Visual gate:** compare against `mockups/world-sim-inspector.html` before Done.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | T-1 code in `Cluiche/CoW/Editor/` |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` + uPlot |
| CH-004 | Live and file data share one format | Both sources go through the `Source` interface |
| System: Run compare rules | Alignment by label, fall back to index; different groupings compare at world level only | Implemented as written |
| UI/business separation | Typed model first | All maths in `model/`; views are display |

No conflicts.

## Open Design Questions

1. **Pooled correlation can mislead.** Pooling points across regions can produce an r with the opposite sign to every region's own trend (Simpson's paradox). The spec shows the whole-world r next to the pooled r when their signs differ; check with real data whether per-group r (a small table) is also needed.
2. **Live as Source B.** Two live games cannot be connected to one editor today, so live-as-B only makes sense when A is a file. Revisit if a second live connection ever exists.
3. **Label fall-back threshold.** The "fewer than half of labels match" rule is a guess. Check it with real runs of different lengths and start dates.

## Status

`Approved` (2026-10-20). Plan not yet created.
