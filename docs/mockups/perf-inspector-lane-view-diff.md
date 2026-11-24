# Perf Inspector: lane view vs mockup (plan row 5a)

Compared from `perf-inspector-mockup.html` (HTML/CSS/JS) against `Dia/DiaPerfInspector/UI/src/components/*` and `src/model/drawLane.ts`. Nobody has looked at the rendered pixels: **visual sign-off by the user is still required.**

Scope: the mockup's centre "Timeline" lanes area only (not the left module/A-B panel, selected-frame detail, or right-hand trips/latency/self-cost panels).

| Mockup element | Status | Notes |
|---|---|---|
| Toolbar: window select (10/30/60 s) | Differs | Three toggle buttons (`aria-pressed`, `.on` colours `#264f78`/`#3a74a8`) instead of a `<select>`. Same values. Slicing is client-side. |
| Toolbar: Pause button (`.on` -> "Resume") | Built | Same label flip and colours. Pausing also stops polling. |
| Toolbar: connection text, trip-condition select, Replay trip, Export Perfetto | Not built | Out of the lanes slice. Connection is the existing banner; trip rules/replay/export are other plan rows. |
| Timeline title bar ("last 10 s . 300 frames per lane . budget 33.3 ms") | Differs | Small "last N s" label in the toolbar only. Frame count and budget are not in the title (budget is per lane in the header). |
| Lane header (unit name, Hz) | Differs | Grid column is 84 px (mockup 64 px) and carries three lines: unit name (`#9cdcfe`, bold), "N Hz", "budget X ms". Mockup shows only name + Hz. |
| Stacked module bars | Built, differs in detail | Canvas columns of `window width / px` buckets (tick = max, modules = mean) instead of one SVG rect per frame. No sub-section (kid) shading at 0.7 opacity: the wire has module totals only. |
| Module colours | Differs | The 12 mockup hues are used, indexed by the model's CRC-hash colour index, so a module's colour is not the same as in the mockup. The mockup's grey `#808080` (Input) is replaced by `#c8c8c8` so it is distinct from the neutral colour. |
| Unattributed remainder | Built (new) | Neutral `#5a5a5a` segment on top of the module stack. Not in the mockup. |
| Over-budget marking | Differs | Mockup does not mark over-budget frames (they just poke above the budget line). Built: 2 px red `#f48771` cap on over-budget columns. |
| Budget line | Built | Dashed `#d7ba7d`, 4/3 dash, at the lane's `budgetMs`. y scale `max(budget x 1.25, lane max)` per lane (mockup: fixed 0..100 ms for all lanes). |
| Budget line text label ("budget 33.3 ms") | Differs | Moved to the lane header; canvas draws no text. |
| Trip shading | Built, no data | Red 10% fill + solid left edge, as the mockup. Draws from `tripRanges`, which is empty until plan row 3b defines the trip wire format. |
| Gap columns (no data) | Differs | Mockup has none. Built: dim grey full-height block (`rgba(128,128,128,.18)`), not hatched. |
| Clamped marker (value >= 655.35 ms) | Built (new) | 3 px white square at the top of the column. Not in the mockup. |
| Lane background `#1a1a1a`, 74 px height | Built | |
| Selected-frame outline (white rect) and click-to-select | Not built | Selection and the detail panel are a later plan row. |
| Event flags row (stage entered, drag start, ...) and dashed event lines | Not built | The `markers` array has no consumer/data yet. |
| Hover tooltip on event flags | Not built | Depends on the markers row. |
| Axis labels | Not built | The mockup has none either. Time is shared across lanes (newest at right) but nothing labels it. |
| Legend (swatch + module name, wrapped, 11 px, `#a0a0a0`) | Built, differs | Same style and left margin (84 px to match the header column). Lists every module across visible lanes (mockup: first word of each name) plus "unattributed". No A/B on/off state. |
| Colours/fonts/spacing | Mostly built | Page `#1e1e1e`, text `#ccc`, 12 px Segoe UI (from index.html), buttons `#3a3a3a`/`#4a4a4a`, 6px/8px padding. Lane spacing is 2 px as in the mockup. Not pixel-checked. |
| Error / empty states | Built (new) | Red error line under the toolbar. No lane rows when there is no data. Not in the mockup. |
