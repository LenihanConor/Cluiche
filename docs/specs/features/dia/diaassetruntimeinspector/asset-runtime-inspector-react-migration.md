# Feature Spec: asset-runtime-inspector-react-migration

**System:** DiaAssetRuntimeInspector
**App:** Dia
**Status:** Draft

## Parent
@docs/specs/systems/dia/diaassetruntimeinspector.md

## Summary

Replace the multi-iframe plain HTML/JS UI (`Dia/DiaAssetRuntimeInspector/UI/index.html` + 4 sub-panel files) with a single React+Vite project that consumes `@dia/editor-ui` — aligning DiaAssetRuntimeInspector with the same tech stack, theme, test coverage, and bridge pattern as DiaEntityInspector and other editor plugins.

No C++ changes to panel logic, bridge topics, or data shapes. The 4 panels (Asset State Table, Stage/Asset Tree, Ref Count Inspector, State Transition Log) become 4 React tab components inside a single component tree, eliminating the iframe architecture.

**Location:** `Dia/DiaAssetRuntimeInspector/UI/` — replaces all plain HTML/JS/CSS files with a Vite project whose `dist/index.html` is the served file. `GetUIPath()` path updates from `"dia://plugins/assetruntimeinspector/index.html"` to `"dia://plugins/assetruntimeinspector/dist/index.html"`.

**Implementation order:** C++ bridge payload tests → React migration → Vitest tests

## Goals

- DiaAssetRuntimeInspector UI is TypeScript, testable with Vitest, and visually consistent with other editor panels
- Every bridge topic handler is a typed subscription via `useBridge` — no raw postMessage wiring or iframe relay
- `@dia/editor-ui` components (`TrafficLightDot`, `TabBar`, `ConnectionStatus`, `EmptyState`, `theme`, `useBridge`, `useResizableDivider`) are used where they fit; no duplicated primitives
- All key UI behaviours have Vitest unit tests (≥ 40 tests)
- Bridge payload shapes are locked by C++ tests before the UI migration begins

## Acceptance Criteria

### Phase 1 — C++ bridge payload tests

- New tests added to `Cluiche/Tests/GoogleTests/DiaAssetRuntimeInspector/TestDiaAssetRuntimeInspectorExhaustive.cpp` covering the JSON shapes of `NotifyUIDataChanged` calls:
  - `asset_runtime_inspector.snapshot` — `{ assets: [...], total: N }` shape verified
  - `asset_runtime_inspector.connection_state` — `{ connected: bool }` shape verified
  - `asset_runtime_inspector.inspector_data` — `{ hasSelection, assetId, state, scope, refCount, stageScoped, message, stageRefs: [...] }` shape verified
  - `asset_runtime_inspector.log_data` — `{ entries: [...], total, paused, maxEntries }` shape verified
  - `asset_runtime_inspector.log_entry` — incremental `{ entry, total, paused }` shape verified
  - `asset_runtime_inspector.tree_data` — `{ stages: [...], globalAssets: [...], selectedAssetId }` shape verified
- `dia run googletest --filter="AssetRuntimeInspector*"` passes with new tests included

### Phase 2 — React+Vite scaffold

- `Dia/DiaAssetRuntimeInspector/UI/package.json` — name `dia-asset-runtime-inspector-ui`, `@dia/editor-ui: file:../../DiaEditorUI`
- `Dia/DiaAssetRuntimeInspector/UI/vite.config.ts` — Vite app mode, output `../dist/`, base `./`
- `Dia/DiaAssetRuntimeInspector/UI/tsconfig.json` — strict, ES2020, JSX react-jsx
- `npm run build` produces `dist/index.html` + `dist/assets/*.js`
- `npm run test` runs Vitest; ≥ 40 passing tests
- `dia pipeline --target diaassetruntimeinspector` passes (build + test)

### Phase 2 — C++ plugin update

- `DiaAssetRuntimeInspectorPlugin` `GetUIPath()` (or equivalent URL field) returns `"dia://plugins/assetruntimeinspector/dist/index.html"`
- Original plain HTML/JS/CSS files deleted: `index.html`, `asset-state-table.html/js/css`, `stage-asset-tree.html/js/css`, `ref-count-inspector.html/js/css`, `state-transition-log.html/js/css`, `dia-bridge.js`

### Phase 2 — Bridge integration

- `injectThemeVars()` called at app root before `ReactDOM.createRoot`
- All bridge topics wired via `useBridge`:
  - `asset_runtime_inspector.connection_state` → connected flag, disconnect overlay
  - `asset_runtime_inspector.snapshot` → asset list, filtered count
  - `asset_runtime_inspector.table_filters` → restored filter values on load
  - `asset_runtime_inspector.tree_data` → stage + global asset nodes
  - `asset_runtime_inspector.stage_children` → children merged into stage node
  - `asset_runtime_inspector.inspector_data` → ref count inspector content
  - `asset_runtime_inspector.log_data` → full log push (on connect/resume)
  - `asset_runtime_inspector.log_entry` → incremental log push
- Bridge requests use `useBridge` send:
  - `asset_runtime_inspector.update_filters` — state filter + ID search
  - `asset_runtime_inspector.force_refresh` — refresh button
  - `asset_runtime_inspector.set_poll_interval` — poll interval input
  - `asset_runtime_inspector.select_asset` — table row click
  - `asset_runtime_inspector.expand_stage` / `collapse_stage` — tree toggle
  - `asset_runtime_inspector.tree_select_asset` — tree asset click
  - `asset_runtime_inspector.log_pause` / `log_resume` — pause button
  - `asset_runtime_inspector.log_clear` — clear button
  - `asset_runtime_inspector.log_set_max` — max entries slider

### Phase 2 — Shared components used

- `<ConnectionStatus>` — disconnect overlay when `connected === false`
- `<TabBar>` — 4 tabs: Asset State Table / Stage Tree / Ref Count / Transition Log
- `<TrafficLightDot>` — connection indicator dot (via `ConnectionStatus`)
- `<EmptyState>` — "no asset selected" in Ref Count Inspector, empty tree/log states
- `theme` — all hardcoded hex literals replaced with `theme.*` tokens (state dot colours — Loaded=green, Loading=yellow, Failed=red, Staged=grey, Unloaded=orange, Null=muted — remain as domain constants)
- `useBridge` — replaces hand-rolled `dia-bridge.js` + iframe relay postMessage wiring
- `useResizableDivider` — optional: left panel / detail split if layout warrants it

### Phase 2 — Feature parity: Asset State Table tab

- Filterable table: state dropdown (All/Null/Staged/Loading/Loaded/Failed/Unloaded), ID search (substring, case-insensitive)
- Sortable columns: Asset ID, State, Scope, Ref Count, Deploy Path
- Virtual scrolling: ROW_HEIGHT=24px, ±5 row buffer, `requestAnimationFrame` scroll debounce
- Row click fires `select_asset` bridge request; selected row highlighted
- Poll interval input: clamped ≥ 0.1s; sends `set_poll_interval`
- Refresh button: fires `force_refresh`
- Status line: "X / Y assets" (filtered / total), "Last refresh: HH:MM:SS"

### Phase 2 — Feature parity: Stage/Asset Tree tab

- Collapsible stage nodes with asset count: `Stage Name (N)`
- `[Global]` node shown only when global assets exist
- State dot per asset node (colour by state enum)
- Ref count badge on global asset nodes only: `ref:N`
- Toggle click fires `expand_stage` / `collapse_stage`
- Asset click fires `tree_select_asset`
- Expand state preserved when snapshot version increments

### Phase 2 — Feature parity: Ref Count Inspector tab

- No selection → `<EmptyState>` shown
- Stage-scoped asset → scope/state/refCount + "Stage-scoped asset — single reference from owning stage." message
- Global asset → scope/state/refCount + stage refs list
- Asset absent from snapshot → "Asset no longer present in runtime." message

### Phase 2 — Feature parity: State Transition Log tab

- Entry format: `[HH:MM:SS.mmm] AssetID OldState -> NewState`
- Disconnect/reconnect markers rendered distinctly (italics or muted)
- Asset ID filter: case-insensitive substring
- Transition filter dropdown: All / specific transition pairs (Null→Staged, Staged→Loading, Loading→Loaded, Loading→Failed, Loaded→Unloaded, Failed→Loading, Any→Failed)
- Max entries slider (10–4096)
- Pause/Resume button: text toggles, fires `log_pause` / `log_resume`; "PAUSED" badge shown when paused
- Clear button: fires `log_clear`

### Phase 3 — Vitest tests (≥ 40 tests in `Dia/DiaAssetRuntimeInspector/UI/src/`)

**Store:**
- `connection_state` push → sets connected flag
- `snapshot` push → asset list stored, filtered count updated
- `table_filters` push → filter values restored
- `tree_data` push → stages + global assets stored
- `stage_children` push → children merged into correct stage node
- `inspector_data` push → selection state populated
- `log_data` push → full log written
- `log_entry` push → entry appended

**AssetStateTable:**
- State filter: each value filters correctly
- ID search: case-insensitive substring match
- Sort by each column
- Row click fires `select_asset`
- Poll interval clamped to ≥ 0.1s
- Status line shows "X / Y assets"

**StageAssetTree:**
- Expand toggle fires `expand_stage` / `collapse_stage`
- Asset click fires `tree_select_asset`
- State dot correct colour per state enum
- `[Global]` node absent when no global assets
- Ref count badge on global nodes only

**RefCountInspector:**
- No selection → empty state shown
- Stage-scoped asset → single-reference message, no stage query
- Global asset → stage refs list rendered
- Missing asset → "no longer present" message

**StateTransitionLog:**
- Pause/Resume button text toggles, fires correct request
- Clear button fires `log_clear`
- Asset ID filter: case-insensitive substring
- Transition filter: drops non-matching entries
- Disconnect/reconnect markers rendered distinctly

## Binding Decisions

| Source | ID | Decision | Impact |
|--------|----|----------|--------|
| DEUI-001 | DEUI-001 | `@dia/editor-ui` via `file:` path | Add `"@dia/editor-ui": "file:../../DiaEditorUI"` to `package.json` |
| DEUI-002 | DEUI-002 | Inline CSSProperties only, no CSS files | Replace all CSS files with inline style objects and `theme.*` tokens |
| DEUI-007 | DEUI-007 | `file:` paths, not npm workspace | Addressed by DEUI-001 |
| DEUI-008 | DEUI-008 | `ConnectionStatus` has no host/port knowledge | Use `<ConnectionStatus state=… label="DiaAssetRuntimeInspector" />` |
| SD-ARED-002 | SD-ARED-002 | Read-only — no mutation of runtime state | Migration adds no write operations; all bridge requests remain observer-only |
| SD-ARED-003 | SD-ARED-003 | Communication via WebSocket/JSON, not direct C++ API | Bridge topics and JSON shapes preserved exactly; no protocol changes |
| SD-ARED-005 | SD-ARED-005 | All panels require live game connection | `<ConnectionStatus>` disconnect overlay shown when `connected === false` |

## Decisions

| ID | Decision | Rationale |
|----|----------|-----------|
| ARIRM-001 | Virtual scrolling is a local component, not extracted to `@dia/editor-ui` | Only one plugin needs it currently; extract when a second consumer appears |
| ARIRM-002 | Single React app replaces the iframe relay architecture | Iframes exist only to work around plain HTML's lack of component isolation; React components provide that natively with less overhead and simpler bridge wiring |
| ARIRM-003 | WebSocket flag (`enableWebSocket = true`) is C++-only — React UI uses standard `useBridge` postMessage | The WebSocket is managed by `LiveConnectionPluginBase`; the UI side sees only the same `postMessage` bridge as every other plugin. No UI changes needed. |
| ARIRM-004 | C++ bridge payload tests added before UI migration | Locks the JSON contract that the React store will consume; prevents silent regressions if C++ serialisation changes later |

## Open Design Questions

| # | Question | Status |
|---|----------|--------|
| 1 | Should the Asset State Table and Stage Tree share a resizable left/right divider (like EntityInspector) or keep a simple full-width tab layout? | Open — default to full-width tabs; revisit if the tree needs persistent side-by-side with the table |
| 2 | The log renders newest-first in the plain JS via reverse push. Should the React component preserve newest-first or switch to newest-last (more common in log UIs)? | Open — preserve newest-first to match existing behaviour; flag for user if they want to change |

## Non-regression gate

These 60 C++ tests must pass before and after the migration (plus any new bridge payload tests added in Phase 1):

- `Cluiche/Tests/GoogleTests/DiaAssetRuntimeInspector/TestDiaAssetRuntimeInspectorExhaustive.cpp` — 60 tests

Gate command: `dia run googletest --filter="AssetStateRow*:AssetStateTablePanel*:SharedPluginState*:TransitionLogEntry*:StateTransitionLogPanel*:SessionContext*:StageAssetTreePanel*:RefCountInspectorPanel*:SharedConnection*"`

## Status

`Done` — plan: @docs/specs/features/dia/diaassetruntimeinspector/asset-runtime-inspector-react-migration.plan.md
