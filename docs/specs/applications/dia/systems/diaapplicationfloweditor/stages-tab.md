# Feature Spec: Stages Tab

## Parent System
@docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md

## Builds On
@docs/specs/applications/dia/systems/diaapplicationfloweditor/stage-configuration.md
@docs/specs/applications/dia/systems/diaapplicationfloweditor/live-state-overlay.md
@docs/specs/applications/dia/systems/diaapplicationfloweditor/live-transition-trigger.md

## Mockup
@docs/specs/applications/dia/systems/diaapplicationfloweditor/mockups/stages-tab.html

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia/dia.md | AD-003 |
| System | @docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md | ED-002, ED-003, ED-007, ED-008, ED-009 |
| System (upstream) | @docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md | SD-002 |

## Purpose

Today the editor surfaces application stages in two places: the small `StageConfiguration` sidebar list (add/remove/rename/reorder/set-initial/toggle-auto/add-transition) and the header `LiveTransitionPanel` (drop-down list of targets while connected). Neither view shows the **shape of the application's lifecycle** — the topology of stages and their declared transitions.

This feature adds a **Stages** tab — the **first** top-level tab in the editor (followed by Process Units, Modules, Streams) — that renders the stage list as a left-to-right transition graph. Stages come first because the application's lifecycle is the outermost mental model: stages contain modules, modules use streams.

- **Nodes** are stages, in manifest array order.
- **Edges** are declared transitions: a solid arrow from stage X to stage Y is drawn for each entry Y in `stage.transitions[]`.
- **Initial stage** is highlighted with a green ring.
- **Terminal stages** (zero outgoing transitions) appear as leaf nodes; the runtime may still reach them via manual `TransitionTo` from modules.

The graph is **read-only**. Structural editing (add/remove/rename/reorder/set-initial/toggle-auto/add-transition) stays in the existing `StageConfiguration` sidebar — no duplicated edit surface, no two-UI drift.

In **live mode**, the active runtime stage pulses green (per ED-008 traffic-light primitive), and if the active stage has `autoAdvance=true`, its outgoing edge(s) animate with a flowing dashed pattern. Clicking any non-active stage delegates to the existing `live-transition-trigger` flow (`bridgeRequest('app.transitionTo', { stage })`).

## Acceptance Criteria

1. **First tab** — A `Stages` tab is added as the **first** entry in AppV2's tab bar, ahead of `Process Units`, `Modules`, `Streams`. The tab bar order becomes `Stages | Process Units | Modules | Streams`. Stages is the **default active tab** on first load. Selecting it renders the stage transition graph in the main content area; the existing sidebar (`StageConfiguration` when no PU is selected) is unchanged.
2. **Linear layout** — Stage nodes are laid out left-to-right in `manifest.stages` array order, evenly spaced. No manual drag-positioning. No persisted layout metadata.
3. **Explicit transition edges** — A solid arrow from stage X to stage Y is drawn for each entry Y in `stage.transitions[]`. Stages with empty `transitions[]` have no outgoing edges. Multiple transitions from a single stage produce multiple arrows.
4. **Initial stage highlight** — The node whose name equals `manifest.initialStage` renders with a 3px green ring (`#3cb370`) and a sub-label `initial · auto` or `initial · manual`. Stages with `autoAdvance=true` render with a 2px amber ring (`#f0a030`); other stages render with a 2px dashed gray ring (`#555`).
5. **Read-only** — No node or edge supports drag, click-to-edit, double-click-to-rename, or context menu in static mode. All structural edits go through the existing `StageConfiguration` sidebar component (which remains visible when no PU is selected).
6. **Empty / single stage** — Manifest with zero stages renders the placeholder `No stages defined — add stages via the sidebar to begin.` Manifest with one stage renders just that node, no edges, with the appropriate initial/auto/manual styling.
7. **Live overlay — active stage** — When `useLiveStoreV2.connectionState === 'connected'` and `useLiveStoreV2.activeStage` is set, the matching stage node renders with a pulsing green ring (animation period 1.6s, per ED-008 `pulse` modifier) and the sub-label `● active`.
8. **Live overlay — animated edge** — When the active stage has `autoAdvance=true`, its outgoing transition edge(s) render green with a flowing dashed stroke pattern (visual hint that an auto-advance is in progress). All other edges render in static gray.
9. **Live click-to-transition** — In live mode, clicking any stage node that is **not** currently active dispatches `bridgeRequest('app.transitionTo', { stage: <name> })` (delegates to the existing `live-transition-trigger` feature). Clicking the active node is a no-op. Clicks in static mode (disconnected) are ignored.
10. **Re-render on manifest change** — Adding/removing/renaming/reordering stages, toggling auto-advance, adding/removing transitions, or changing the initial stage in the sidebar re-renders the graph immediately on the next React frame (state-driven; no manual refresh needed). Existing `useManifestStoreV2` reactivity carries this for free.
11. **No new C++ work** — All required data (`stages` with `transitions[]` and `autoAdvance`, `initialStage`, plus live `activeStage`) is already exposed through `useManifestStoreV2` and `useLiveStoreV2`. This feature is React-only.

## Design

### Component placement

New file: `Dia/DiaApplicationEditor/UI/src/v2/StagesTab.tsx`. Wired in `AppV2.tsx` as the first tab; the existing default `useState<Tab>('graph')` flips to `useState<Tab>('stages')`:

```ts
type Tab = 'stages' | 'graph' | 'presence' | 'streams';
const [activeTab, setActiveTab] = useState<Tab>('stages');

(['stages', 'graph', 'presence', 'streams'] as Tab[]).map(tab => …)

{activeTab === 'stages' && (
    <div id="stages-tab-content" style={{ height: '100%' }}>
        <StagesTab />
    </div>
)}
```

The tab label rendered to the user is `Stages` (other labels remap, e.g. `graph → Process Units`; `stages → Stages` is the natural label). The existing tab-label switch in `AppV2.tsx` (`tab === 'graph' ? 'Process Units' : tab === 'presence' ? 'Modules' : 'Streams'`) is extended with a `'stages' → 'Stages'` arm.

### Data sources

```ts
const stages       = useManifestStoreV2(s => s.manifest?.stages ?? []);
const initialStage = useManifestStoreV2(s => s.manifest?.initialStage ?? '');
const isLive       = useLiveStoreV2(s => s.connectionState === 'connected');
const activeStage  = useLiveStoreV2(s => s.activeStage);
```

### Layout math

Pure function, deterministic, no persisted state:

```ts
const NODE_R       = 26;
const NODE_SPACING = 160;   // center-to-center
const PADDING_X    = 80;
const PADDING_Y    = 80;

interface NodeLayout { name: string; cx: number; cy: number; }

function layout(stages: StageV2[]): NodeLayout[] {
    return stages.map((s, i) => ({
        name: s.name,
        cx: PADDING_X + i * NODE_SPACING,
        cy: PADDING_Y,
    }));
}
```

Width = `PADDING_X * 2 + (stages.length - 1) * NODE_SPACING`. Container scrolls horizontally if the SVG exceeds tab width — small concession for >6 stages, but realistic manifests have <10.

### Rendering

Inline SVG (matches the `GraphView` precedent, no extra dependency). Per node:

```tsx
const isInitial   = stage.name === initialStage;
const isAutoAdv   = stage.autoAdvance;
const isActive    = isLive && activeStage === stage.name;

<g
    onClick={isLive && !isActive ? () => transitionTo(stage.name) : undefined}
    style={{ cursor: isLive && !isActive ? 'pointer' : 'default' }}
>
    {isActive && /* outer pulse ring (animated r) */}
    <circle
        cx={cx} cy={cy} r={NODE_R}
        fill={isActive ? '#1e3a26' : '#2d2d2d'}
        stroke={isInitial ? '#3cb370' : isAutoAdv ? '#f0a030' : '#555'}
        strokeWidth={isInitial ? 3 : 2}
        strokeDasharray={!isInitial && !isAutoAdv ? '3 3' : undefined}
    />
    <text x={cx} y={cy + 4} textAnchor="middle" fontSize={12}>{stage.name}</text>
    <text x={cx} y={cy + 40} textAnchor="middle" fontSize={10} fill={subLabelColor}>
        {subLabel}
    </text>
</g>
```

Per edge (for each entry T in `stage.transitions[]`, draw an arrow from stage to T):

```tsx
const sourceActive = isLive && activeStage === source.name && source.autoAdvance;
<line
    x1={source.cx + NODE_R} y1={source.cy}
    x2={target.cx - NODE_R} y2={target.cy}
    stroke={sourceActive ? '#3cb370' : '#888'}
    strokeWidth={2}
    strokeDasharray={sourceActive ? '8 4' : undefined}
    className={sourceActive ? 'auto-edge-live' : 'auto-edge'}
    markerEnd={`url(#arrow-${sourceActive ? 'live' : 'static'})`}
/>
```

A small CSS keyframe animates `stroke-dashoffset` for the live edge (matches mockup):

```css
@keyframes dash-flow { from { stroke-dashoffset: 0; } to { stroke-dashoffset: -16; } }
.auto-edge-live { animation: dash-flow 0.8s linear infinite; }
```

The pulse on the active node uses an `<animate>` element on a second concentric `<circle>` (or a CSS keyframe with `transform-box: fill-box`).

### Transition dispatch

Single helper, mirrors the `LiveTransitionPanel`:

```ts
const transitionTo = (stageName: string) =>
    bridgeRequest('app.transitionTo', { stage: stageName });
```

This piggy-backs on the existing live-transition-trigger feature; nothing new on the C++ side.

### Edge cases

- **Zero stages** — render a centered placeholder div, no SVG.
- **One stage** — render that one node, no edges.
- **`initialStage` missing from `stages`** — already a validation error (`INITIAL_STAGE_INVALID`); the tab still renders, no node receives the green ring. Validation bar surfaces the issue.
- **`stage.transitions[]` contains an unknown target** — already a validation error (`TRANSITION_TARGET_INVALID`); edge rendering skips unknown targets silently.
- **`activeStage` not in `stages`** — possible during a transition or if the runtime reports a stage we don't know. Gracefully ignored — no node pulses; no warning surfaced from this feature.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `StagesTab.tsx` skeleton: data wiring, empty / single-stage placeholders, layout math | `StagesTab.test.tsx`: zero / one / N stages rendered with correct count | Todo | sonnet | Pure render logic |
| 2 | Render stage nodes (static): initial green ring, auto solid ring, manual dashed ring, sub-labels | `StagesTab.test.tsx`: initial node has green stroke; manual has dashed; auto has solid | Todo | sonnet | |
| 3 | Render auto-advance edges only when `stages[i] ∈ autoStages` | `StagesTab.test.tsx`: edge count matches auto-stage prefix; manual stages produce no outgoing edge | Todo | sonnet | |
| 4 | Live overlay: active-stage pulse + animated dashed auto edge | `StagesTab.test.tsx`: with mocked live store, active node has data-active="true"; outgoing edge has live class | Todo | sonnet | |
| 5 | Live click-to-transition: dispatch `app.transitionTo` on non-active node click | `StagesTab.test.tsx`: click in live mode fires bridgeRequest with stage name; static mode click is no-op; active-node click is no-op | Todo | sonnet | |
| 6 | Wire `Stages` as **first** tab in `AppV2.tsx` (default `activeTab='stages'`, button + content slot, label switch case) | `AppV2.test.tsx`: Stages is default active, all 4 tabs render in order, swap activates StagesTab | Todo | haiku | Update existing initial-tab assertion |
| 7 | Visual polish per mockup (pulse keyframe, dash-flow keyframe, padding, sub-label colors) | Manual: matches `mockups/stages-tab.html` | Todo | sonnet | |
| 8 | Manual smoke: load manifest, switch to Stages tab, connect live, observe pulse + animated edge | `dia run cluicheeditor`, load .diaapp, verify | Todo | sonnet | End-to-end |
| 9 | Commit with conventional message | `git status` clean | Todo | haiku | After tasks 1-8 done |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Stage names are already string keys in the manifest payload sent to JS; no CRC handling on the React side. |
| PD-007 | C++20 required | No new C++ in this feature. |
| AD-003 | Namespace `Dia::<Module>::` | No new C++ in this feature. |
| ED-002 | Three-tab layout: Graph, Presence, Streams | **Superseded** — adding Stages as the leading tab makes the bar `Stages | Process Units | Modules | Streams`. ED-002's intent (clean separation of orthogonal views) is preserved: Stages is the topology of the application's lifecycle, orthogonal to PU topology, module presence, and stream dataflow. Logged as decision ED-015 in the parent system spec on the same commit as this feature lands. |
| ED-003 | Live mode is overlay on static view, not separate mode | Honored — the same StagesTab renders both states; live additions are overlay (pulse, edge animation), not a mode switch. |
| ED-007 | React + CEF frontend | Pure React/SVG; no new C++. |
| ED-008 | Single `.tl` traffic-light dot primitive (grey/amber/green/red + `pulse`) | Active stage pulses green using the same color (`#3cb370`) and pulse cadence as `.tl.green.pulse`. The pulse is on the stage node ring rather than a `.tl` dot, but uses the same visual vocabulary — not a new indicator. |
| ED-009 | Live button is 3-state in the header | Unchanged; this feature reads `connectionState` from the same store the button drives. |
| ED-013 | Validation always-on, debounced 500ms | Unaffected — Stages tab does not run validation; validation bar continues to surface stage-related issues (`INITIAL_STAGE_INVALID`, `STAGE_REF_INVALID`). |
| SD-002 | Stages replace Phases | Reinforced — this tab makes stages a first-class topology view in the editor. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Layout | Why linear left-to-right and not force-directed (dagre) like the dependency graph could be? | Stages are a strict ordered list with at most ~10 entries. The array order *is* the auto-advance order; preserving it visually is more informative than a layout engine's compromise. Force-directed layouts also make "drag to reorder" semantics fuzzy, whereas left-to-right + array order has an obvious mapping. dagre is already a dependency (used elsewhere in the editor), so we could add it later if we ever exceed ~15 stages — not a real concern today. |
| 2 | Edges | Why only auto-edges and not also dashed "possible manual transition" edges between every pair? | Two reasons. (a) Manual transitions can target *any* stage from any stage; rendering N² edges is spaghetti at N≥4. (b) The manifest doesn't *declare* manual transitions, so any edge we drew would be a fabrication, not a fact. Live mode does the right thing: it shows where the runtime *did* go via the active-stage highlight. If we ever introduce explicit transition declarations (e.g., per-stage allowed-targets), we revisit. |
| 3 | Editing | Should we duplicate stage edits onto the graph (right-click context menu, click-node-to-set-initial)? | Not in MVP. The sidebar already does this, and adding parallel UI introduces drift risk + cognitive load (two ways to do the same thing). The graph's job is *show the topology*; the sidebar's job is *manipulate the list*. We can promote graph-side edits in a follow-up after observing how users actually use the tab. |
| 4 | Live overlay | What if the runtime is mid-transition and `activeStage` flips back and forth? | The store updates atomically per WebSocket message; React re-renders on each. Visually that means the pulse jumps from node to node, which is the correct signal. We don't smooth or interpolate — runtime truth is more useful than a prettier animation here. |
| 5 | Live click-to-transition | What if the user clicks a stage that's not reachable from the current state? | The runtime decides. The editor dispatches `app.transitionTo`; if the runtime rejects (e.g., not allowed at the moment), the existing `live-transition-trigger` feature is responsible for surfacing that. This feature stays a click-through. |
| 6 | Tab count + order | Why first, not last? Is four tabs too many? | Stages is first because it's the outermost layer of the mental model: the application's lifecycle contains everything else. PUs live inside stages; modules live inside PUs; streams connect PUs. Reading left-to-right gives the reader a natural top-down decomposition. Four short labels (`Stages`, `Process Units`, `Modules`, `Streams`) fit comfortably above 1024px width. Logged as ED-015 superseding ED-002. If a fifth is ever proposed, that's the trigger for grouping or a more compact pattern. |
| 10 | Default tab | Loading a manifest currently lands on `graph` (Process Units). Doesn't moving to `stages` change muscle memory? | Yes, deliberately. The default lands the user on the highest-level view of the manifest. Existing tests that assert "graph is default" need updating — flagged in the plan. The `AppV2.test.tsx` initial-render test will switch its expectation. |
| 7 | Re-render cost | Does this re-render on every WebSocket frame (live module state, stream throughput)? | Selectors are scoped — `useLiveStoreV2(s => s.activeStage)` only triggers re-renders when activeStage changes, not on module/stream state updates. Likewise the manifest selectors. Re-renders happen only when stage list or activeStage actually changes. |
| 8 | Mockup divergence | The mockup shows 5 stages with a specific sub-label vocabulary (`initial · auto`, `auto`, `manual`, `completed`). Are those normative? | Initial / auto / manual labels are normative (AC #4). The `completed` label in the mockup is purely illustrative for past-stages in live mode — we won't render it because the runtime doesn't tell us "completed" as a state separate from "active=false". Past stages just render in their static styling. |
| 9 | Accessibility | Is the graph keyboard-accessible? | Not in MVP. The existing GraphView and StreamsTab also lack full keyboard nav. Tracked as a system-level a11y backlog item; not a Stages-tab–specific gap. |

## Status

`Done` — 2026-05-20

Plan: [stages-tab.plan.md](stages-tab.plan.md)
