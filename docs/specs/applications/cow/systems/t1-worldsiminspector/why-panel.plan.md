# Plan: T-1 Why Panel (Feature 4)

**Spec:** @docs/specs/applications/cow/systems/t1-worldsiminspector/why-panel.md
**Status:** Not Started
**Mockup (approved 2026-11-16):** `mockups/world-sim-inspector-v2.html`, workspace "Causes", state "Why panel" (line ~458, `Why panel F4`), plus `mockups/world-sim-inspector.html` (v1 Why panel). The visual gate for the view tasks (AC 12).

## Prerequisites (gate, checked in Task 0)

As of 2026-11-16 `Cluiche/CoW/Editor/` holds only the `CoWEditor` stub (`CoWEditor.h/.cpp/.vcxproj/.filters`); there is no `WorldSimInspector/` folder, no UI project, and no `cow.ecology.cause` code anywhere (only specs).

| Needs | From | Used by tasks |
|---|---|---|
| UI project scaffold (Vite, React 18, zustand, `@dia/editor-ui`, Vitest, RTL), `Source` model, shared brush, schema tooltips, "Track this layer" pattern | T-1 Feature 1 (and Feature 3 for the Track pattern) | 1-9 |
| `model/events.ts`, `model/series.ts` (net change from aggregates) | T-1 Feature 2 (spec lists them under `UI/model/`) | 2, 3 |
| `navigation.ts` (`NavIntent` kind `cells`), `canNavigate`, toast, bridge `navigate(plan)` returning the accepted set | T-1 Feature 6 | 4, 6, 7 |
| `cow.ecology.cause {group, layer, cause, delta, cells}` events | A1 Ecology (Approved + amended 2026-10-20, **not built**) | fixtures until built; E2E uses a stub game |
| DiaValueSchema entry for `cow.ecology.cause` (labels, units, cause vocabulary) | DiaValueSchema (Approved, not built) | fixtures embed a schema block (Feature 1 file format) |
| `cells.select.cells <q,r...>` returning `{requested, accepted}` | DiaCellLayers Feature 2 (`selection.md`: spec'd, cap 2,048 per call; **not built**) | Show cells live proof only; model/UI use a stub |

**Do not start Task 1 until Features 1 and 6 are Done** (Feature 2 for Task 3's net-change reuse). Tasks 1-3 are pure TS and can start once Feature 1's scaffold and Feature 2's `events.ts`/`series.ts` exist. If A1 or `cells.select.cells` is unbuilt, the plan proceeds against fixtures and stub game; the live proof is a backlog row, not a blocker.

## Sign-offs (AD-001)

None new for T-1 code: all of it is CoW editor code under `Cluiche/CoW/Editor/WorldSimInspector/`. Two external items are not owned by this plan: the A1 `cells` amendment (A1's build, spec already amended) and the DiaCellLayers `cells.select.cells` command (Dia change, needs AD-001 sign-off at DiaCellLayers build time; spec'd in `selection.md`). This plan adds no Dia header or implementation.

## Implementation Patterns

### Cause model (Tasks 1-3), all pure TS, no React, no bridge
- Files in `UI/model/`: `causeEvents.ts` (parser, exported for reuse), `causes.ts` (summation and top cells). Views import `causes.ts`, the store and Feature 6's `navigation.ts` only (UI/business separation).
- `causeEvents.ts`: `parseCauseEvents(events, schema) -> CauseEvent[]` with `{sample, group, layer, cause, delta, cells: {q,r,delta}[]}`; drops malformed entries with a logged warning; `causeLabel(cause, schema)` falls back to the raw id with a logged warning (Resolved Decision 3). Also `hasCauseEventType(source)`. **Feature 12 (`cross-system-causes.md`) reuses this parser and the `cause_origins` mapping idea (`cause -> origin`, file `UI/causes/cause_origins.json`).** Keep the parser free of Why-panel concepts, export it from `model/`, and do not add the origin mapping here.
- `causes.ts`: `sumCauses(source, group, layer, brush) -> { bars[], other: {delta, count}, unattributed: {delta, shareOfNet}, net }`. Sum per cause over events in the brush (all regions for Whole world), rank by `abs`, top 8, the rest into Other with a count. `net` = aggregate at brush end minus brush start for the same group and layer (reuse Feature 2 `series.ts`). `unattributed = net - sum(all cause deltas incl. Other)`. `shareOfNet = abs(unattributed)/abs(net)`; note flag above 0.25; log line above 0.10; net of 0 gives share 0 without dividing.
- `topCells(source, group, layer, cause, brush, limit) -> Cell[]`: merge `cells` of the cause's events in the brush, sum delta per `(q,r)`, order by `abs`, keep `limit`; returns the total merged count M for "Selected N of M".
- `whyState(source, group, layer, brush)` returns the empty-state kind: `hidden` (no event type), `notTracked` (live, layer untracked, offers Track), `noData` (file or period, with first sample that has cause data when the brush is before it).
- Ordering ties broken by cause id for deterministic bars. Dev builds wrap summation in `performance.mark`.

### Store and views (Tasks 4-6)
- Store slices `whyGroup`, `whyLayer`, `whyCause`, plus `showCellsResult {accepted, requested}`; the Show cells action builds a `{kind:'cells'}` intent at the brush end sample and calls the store's `navigate(intent)` (the only bridge path). No command strings in views.
- Views: `WhyPanel.tsx` (pickers, bar list, net line, footer notes, Show cells), `CauseBar.tsx` (signed bar, label and unit from schema, tooltip). Positive extends right, negative left. Disabled Show cells stays visible with the Feature 6 tooltip from `canNavigate`.
- Layer picker lists only layers with cause events in the source. Panel is removed from the Causes layout when `whyState` is `hidden`.

### Fixtures
- One shared fixture set under the UI test folder: 3 regions x 4 bands x 2 layers, 10+ causes (to exercise top 8 + Other), overlapping cells across bands, a layer with no causes, a file without the event type, and a brush that starts before the first cause event. Cause ids and units come from an embedded schema block.

### Verification cadence
- Per task: scoped Vitest (`npx vitest run <file>` from the UI folder) or scoped `dia run googletest --filter="*WorldSimInspector*" --shards 2` if any C++ is touched (none expected).
- Full Vitest, GoogleTests and E2E run once, in Task 9.
- No mutation checks (decided 2026-11-12). Task 10 is a gap audit plus edge tests only.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Gate: confirm T-1 Features 1 and 6 are Done (scaffold, `Source`, `events.ts`/`series.ts`, `navigation.ts`, bridge `navigate`); if not, stop and report. Check A1 `cow.ecology.cause` exists in code (expected: no, specs only, so the plan runs on fixtures) and `cells.select.cells` status (spec'd in DiaCellLayers `selection.md`, not built, needs AD-001 at its build). Record baseline Vitest and `*WorldSimInspector*` GoogleTests pass counts | - | Not Started | haiku | No code. Repo today: `Cluiche/CoW/Editor/` is the `CoWEditor` stub only. Spec assumes `UI/model/events.ts` and `series.ts` from Feature 2 and a `Source` that exposes events and aggregates |
| 1 | RED -> GREEN: `causeEvents.ts` parser, schema label/unit lookup with raw-id fallback, `hasCauseEventType`; export from `model/` | AC 1, 10, 11 (consumer side): fixture parses; malformed event dropped with warning; renamed cause still labelled from embedded schema; missing type returns false | Not Started | sonnet | Reused by Feature 12 (`cross-system-causes.md`); keep generic, no Why-panel logic |
| 2 | RED -> GREEN: `sumCauses` per cause with signed deltas, rank by abs, top 8 + Other with count, whole world vs one region, brush window recompute | AC 1, 2, 5: hand-computed fixture; exactly 8, 9 and 20 causes; ties; empty brush; region vs world | Not Started | sonnet | Tests first, quote RED |
| 3 | RED -> GREEN: net from aggregates, Unattributed, share of net, 25% note flag, 10% log threshold, zero-net case; `whyState` empty-state kinds | AC 3, 4, 9, 10: bars + Other + Unattributed == net exactly; share at 24/25/26%; net 0; layer untracked (live) vs no data (file); brush before first cause sample names that sample | Not Started | sonnet | Net uses Feature 2 `series.ts` |
| 4 | RED -> GREEN: `topCells` merge, per-cell sum, top-limit, M count; store slices and Show cells action building the `cells` intent at brush end sample; "Selected N of M" from accepted set; Show cells gated by `canNavigate` | AC 6, 7, 8: overlapping cells sum across bands; limit boundary; plan has `cells.select.cells` then `cells.history.view` at brush end; accepted fewer than requested shown; disabled reason passed through | Not Started | sonnet | Reuses Feature 6 model; bridge stubbed. Cap handling (2,048) is the game's, T-1 sends its top-N |
| 5 | `CauseBar.tsx` and `WhyPanel.tsx`: pickers (Whole world/regions, layers with causes, brush labels), signed bars, net line, Unattributed bar with share and note, footers ("small causes land in Unattributed", "top contributing cells recorded"), Other with count | RTL AC 1, 2, 4, 5: every mockup state; schema tooltips; note appears above 25% only; changing brush or group recomputes | Not Started | sonnet | Visual check against v2 mockup "Causes > Why panel" |
| 6 | Empty states and Show cells UI: four empty-state rows, Track button only for live untracked layer (Feature 3 pattern), panel hidden when no event type, Show cells enabled/disabled with tooltip, "Selected N of M" | RTL AC 7-10: each empty state; no Track button on a file; hidden layout; disabled tooltip text from `canNavigate` | Not Started | sonnet | |
| 7 | Wire into the Causes workspace of the plugin shell (hides when `hidden`); `[EDITOR] T1` logs for Other truncation, unattributed above 10%, merged cell lists; dev `performance.mark` | RTL + Vitest: workspace renders panel alongside Feature 12 slot; log lines asserted; panel absent for non-A1 source | Not Started | sonnet | Feature 12 shares the Causes workspace; leave its slot untouched |
| 8 | E2E scenario with a stub game publishing cause events (with `cells`) | AC 6, 7 E2E: Show cells sends `cells.select.cells` with expected cells then `cells.history.view`; stub accepting fewer shows "Selected N of M"; different-world file disables button | Not Started | sonnet | `dia test e2e` does not build: run `dia pipeline --target cluichetest --stage compile-code` first. Real A1 events not needed |
| 9 | Visual gate against both mockups (one screenshot per state), then ONE full run: Vitest, `dia run googletest --shards 2`, E2E | AC 12; all suites pass. Report pass/fail only | Not Started | sonnet | Perf: summation over a 40-year fixture (about 770 entries/band worst case) well inside a frame; record the number |
| 10 | Gap audit and edge tests (no mutation checks): re-read ACs against tests; add edges (no cause events but net nonzero, positive and negative causes cancelling, cell deltas summing above event delta, duplicate cells in one event, cause missing `cells`, region removed between bands, unicode labels, schema label change) | Added tests fail first where they find a bug | Not Started | sonnet | Explicit task per the exhaustive-test rule |
| 11 | Close-out: Observation Opportunity Scan (suggestions only), spec Status to Done via `dia docs spec-done`, update the plan file and parent features table, backlog row to history; add backlog rows for A1 `cells` field live proof and the `cells.select.cells` live proof | - | Not Started | haiku | Spec flips Done on fixtures and stub E2E; live proof against real A1 and DiaCellLayers is the recorded backlog item (A1 Feature test "cause event carries up to 8 cells" belongs to A1) |
