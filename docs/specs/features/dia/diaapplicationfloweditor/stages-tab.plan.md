# Plan: Stages Tab

## Spec
[stages-tab.md](stages-tab.md) — Approved 2026-05-20

## Session Notes

### Spec decisions summary

DiaApplicationFlowEditor is a CluicheEditor plugin (CEF + React frontend, C++ backend) editing `.diaapp` v2 manifests. **Binding decisions in scope:** PD-001 (StringCRC for IDs — N/A here, JS receives string names), PD-007 (C++20 — no new C++ in this feature), AD-003 (`Dia::ApplicationFlow::Editor::` namespace — N/A), ED-002 (three-tab layout) **superseded by ED-015** (four-tab: Stages first), ED-003 (live mode is overlay), ED-007 (React + CEF), ED-008 (`.tl` traffic-light primitive — pulse green for active stage), ED-009 (live button is 3-state in header — unchanged), ED-013 (validation always-on — unchanged), SD-002 (Stages replace Phases).

**Architecture invariant for this feature:** purely additive React change. The manifest already exposes `stages: StageV2[]` (ordered), where each `StageV2` now has `transitions: string[]` and `autoAdvance: boolean` (v3 schema; `autoStages` array removed). The live store already exposes `connectionState` and `activeStage`. No C++, no new bridge command (live click-to-transition reuses the existing `app.transitionTo` from `live-transition-trigger`).

**Edge model uses explicit transitions:** an arrow from stage X to stage Y is drawn for each entry Y in `stage.transitions[]`. Stages with empty `transitions[]` have no outgoing edges. **Layout is linear** left-to-right in array order, no manual positioning, no persisted layout sidecar. **Graph is read-only** — all structural edits stay in the existing `StageConfiguration` sidebar (no duplicated edit surface).

**Live overlay:** active stage gets a pulsing green ring (1.6s period), and if the active stage has `autoAdvance=true`, its outgoing transition edges animate with a flowing dashed pattern. Clicking any non-active stage in live mode dispatches `bridgeRequest('app.transitionTo', { stage: <name> })`. Clicking in static mode is ignored.

**Tab bar reorder:** new order is `Stages | Process Units | Modules | Streams` with `stages` as the default active tab. The `Tab` type union is reordered, the default `useState<Tab>` flips, the rendered label switch gets a `'stages' → 'Stages'` arm. Existing `AppV2.test.tsx` initial-tab assertion needs updating.

### Key file map

- `Dia/DiaApplicationEditor/UI/src/v2/AppV2.tsx` — tab bar, default active tab, content slot
- `Dia/DiaApplicationEditor/UI/src/v2/StagesTab.tsx` — **new** — graph component
- `Dia/DiaApplicationEditor/UI/src/v2/StagesTab.test.tsx` — **new** — vitest suite
- `Dia/DiaApplicationEditor/UI/src/v2/AppV2.test.tsx` — update default-tab expectation
- `Dia/DiaApplicationEditor/UI/src/v2/useManifestStoreV2.ts` — read-only consumer (no change)
- `Dia/DiaApplicationEditor/UI/src/v2/useLiveStoreV2.ts` — read-only consumer (no change)
- `Dia/DiaApplicationEditor/UI/src/v2/bridge.ts` — `bridgeRequest('app.transitionTo', …)` reused from live-transition-trigger
- `docs/specs/features/dia/diaapplicationfloweditor/mockups/stages-tab.html` — visual acceptance gate

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `StagesTab.tsx` skeleton: imports, store reads, layout math, empty/single-stage placeholders | `StagesTab.test.tsx`: zero stages → placeholder div; one stage → single node, no edges | Todo | sonnet | Pure render. Mock `useManifestStoreV2`, `useLiveStoreV2`, `./bridge` in tests |
| 2 | Render stage nodes (static): initial green ring, auto solid gray ring, manual dashed gray ring, sub-labels | `StagesTab.test.tsx`: data attributes (`data-stage-name`, `data-is-initial`, `data-is-auto`) drive assertions on stroke + dasharray | Todo | sonnet | |
| 3 | Render auto-advance edges only when source stage is in `autoStages` AND a successor exists | `StagesTab.test.tsx`: 5 stages, autoStages=['boot','splash'] → exactly 2 edges with `data-edge-from`/`data-edge-to`; manual-only manifests → 0 edges | Todo | sonnet | |
| 4 | Live overlay — active-stage pulse + outgoing auto edge animation; both gated on `connectionState==='connected'` | `StagesTab.test.tsx`: with mocked live store, active node has `data-active="true"`; outgoing edge has `data-live-edge="true"`; static mode → neither | Todo | sonnet | Use `useLiveStoreV2.setState` in beforeEach to control connection state |
| 5 | Live click-to-transition: click non-active stage in live mode → `bridgeRequest('app.transitionTo', { stage })`; static / active-self clicks are no-ops | `StagesTab.test.tsx`: 3 cases — live + non-active fires bridge; live + active is no-op; static + any is no-op | Todo | sonnet | Mock `./bridge` as in ValidationBarV2.test |
| 6 | Wire `Stages` as **first** tab in `AppV2.tsx`: reorder Tab union, default `useState<Tab>('stages')`, button order, label switch case, content slot | `AppV2.test.tsx`: initial render shows StagesTab content; tab buttons appear in order Stages/PU/Modules/Streams; clicking each activates correct content | Todo | haiku | Update existing initial-tab assertion |
| 7 | CSS keyframes (pulse on active node, dash-flow on live edge) inline in component or module CSS | Manual: matches mockup `mockups/stages-tab.html` | Todo | sonnet | Use `@keyframes` in a `<style>` element inside the component or in a small companion CSS — match the GraphView precedent |
| 8 | Visual polish per mockup: padding, sub-label colors, marker arrows, scroll behavior for >6 stages | Manual: side-by-side compare with `mockups/stages-tab.html` | Todo | sonnet | |
| 9 | Manual smoke: `dia run cluicheeditor`, load .diaapp manifest, switch to Stages tab, observe layout; connect live, observe pulse + animated edge | Quoted output of dia commands; user verification | Todo | sonnet | Cross-cutting end-to-end |
| 10 | Commit with conventional message ("Add Stages tab as first top-level view; auto-edge transition graph + live overlay") | `git status` clean | Todo | haiku | After tasks 1-9 done |

## Risks

- **Tab reorder breaks existing tests** — `AppV2.test.tsx` (and any iframe smoke tests) likely assume `graph` is the default tab. Task 6 includes the assertion update; if any other test file asserts initial tab indirectly (e.g., by querying `getByText('Process Units')` and clicking it), they may need adjustments. Will surface during the test pass.
- **`activeStage` shape mismatch** — the live store currently types `activeStage` as a number (CRC) or string (name) depending on how the bridge layer converts it. Need to verify which form is stored and compare against `stage.name` accordingly. If the store holds a CRC, we'll need a small lookup or a normalisation at write time. Task 4 will surface this — if it does, fix at the store level (single normalisation point), not in StagesTab.
- **Pulse animation in jsdom** — vitest + jsdom doesn't run CSS animations; tests must assert via class names or data attributes, not computed styles. Task 4's tests are written that way (`data-active`, `data-live-edge`).
- **Horizontal scroll for large manifests** — at 10+ stages the SVG exceeds typical tab width. Mitigation: wrap the SVG in a horizontally scrollable container with sensible default. Realistic manifests are <10 stages so this is a low-priority polish.

## Decisions log

(to be filled as tasks execute)
