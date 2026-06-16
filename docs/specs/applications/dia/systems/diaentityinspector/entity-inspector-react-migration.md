# Feature Spec: entity-inspector-react-migration

**System:** DiaEntityInspector
**App:** Dia
**Status:** Draft

## Parent
@docs/specs/applications/dia/systems/diaentityinspector/diaentityinspector.md

## Summary

Replace the single-file plain HTML/JS UI (`Dia/DiaEntityInspector/UI/index.html`) with a React+Vite project that consumes `@dia/editor-ui` — aligning DiaEntityInspector with the four existing React+Vite editor plugins (DiaApplicationFlowEditor, DiaApplicationFlowInspector, DiaPipelineEditor, CluicheEditor) in tech stack, theme, test coverage, and bridge pattern.

No C++ changes. No protocol changes. No behaviour changes. The bridge topics (`entity_inspector.*`), postMessage contract, and data shapes are identical. Only the UI implementation changes.

**Location:** `Dia/DiaEntityInspector/UI/` — replaces `index.html` with a Vite project whose `dist/index.html` is the served file. `GetUIPath()` updates from `"dia://plugins/entityinspector/index.html"` to `"dia://plugins/entityinspector/dist/index.html"`.

## Goals

- DiaEntityInspector UI is TypeScript, testable with Vitest, and visually consistent with the other editor panels
- Every bridge topic handler is a typed `useBridgeSubscribe` subscription — no raw `window.addEventListener('message', ...)` or `window.DiaEditor_onDataChanged` wiring
- `@dia/editor-ui` components (`TrafficLightDot`, `TabBar`, `ConnectionStatus`, `theme`) are used where they fit; no duplicated primitives
- All key UI behaviours have Vitest unit tests (entity list filter, tab switching, field accordion, watch add/remove, mailbox self-only toggle)

## Acceptance Criteria

### Project scaffold
- `Dia/DiaEntityInspector/UI/package.json` — name `dia-entity-inspector-ui`, `@dia/editor-ui: file:../../DiaEditorUI`
- `Dia/DiaEntityInspector/UI/vite.config.ts` — Vite lib mode or app mode targeting `dist/`
- `Dia/DiaEntityInspector/UI/tsconfig.json` — strict, ES2020, JSX react-jsx
- `npm run build` produces `dist/index.html` + `dist/assets/*.js`
- `npm run test` runs Vitest; ≥ 25 passing tests
- `dia pipeline --target diaentityinspector` passes (build + test)

### C++ plugin update
- `DiaEntityInspectorPlugin::GetUIPath()` returns `"dia://plugins/entityinspector/dist/index.html"`
- Original `Dia/DiaEntityInspector/UI/index.html` is deleted (or archived)

### Bridge integration
- `injectThemeVars()` called at app root before `ReactDOM.createRoot`
- All five bridge topics wired via `useBridgeSubscribe`:
  - `entity_inspector.connection_state` → connection UI
  - `entity_inspector.inspect_data` → entity list + fields + context strip
  - `entity_inspector.query_data` → queries tab
  - `entity_inspector.mailbox_data` → mailbox tab
  - `entity_inspector.watch_data` → watch tab
- Bridge requests (watch_add, watch_remove, write_field, get_connection_state) use `useBridgeRequest<T>()`
- `window.DiaEditor_onDataChanged` direct-call entry point preserved (wrap in a thin adapter that calls the same state setters)

### Shared components used
- `<ConnectionStatus>` — renders connection dot + "Connected" / "Not connected to game" text in the title bar
- `<TabBar>` — four tabs (Fields, Queries, Mailbox, Watch) with count badges; replaces the `.stab` / `.sc` inline implementation
- `<TrafficLightDot>` — used inside `ConnectionStatus` (automatically, via the shared component)
- `theme` object — all colour literals replaced with `theme.*` tokens (semantic exceptions: domain-type tag colours T/P/R/A/S/H/C and message-type badge colours remain hardcoded per the original comments)

### Feature parity: entity list
- Entities rendered with index, debug name, component short-tag chips, hierarchy indentation
- Search input filters by name substring (case-insensitive)
- Filter chips (All / T / P / R / A / S / H / C) restrict list to matching entities
- Clicking a row selects the entity and triggers `entity_inspector.inspect_request` via bridge
- Column divider is draggable (30% / 70% default; 180px min / 400px max)

### Feature parity: Fields tab
- Component accordion — one section per component, first two open by default, expand state persists while entity is selected
- Each row: field name, type badge, current value (monospace), edit pencil button
- Edit via `prompt()` (preserves parity with current implementation — no richer widget in v1)
- On edit, calls `entity.write_field` via bridge
- Tier (c) footer buttons (Add Component, Remove Component, Destroy Entity) rendered, `disabled`, tooltip "Not available in v1"
- Hierarchy nav section shown when entity has parent or children; links navigate to parent/child

### Feature parity: Queries tab
- Query cards with CRC signature, entity count badge, member-list accordion
- Selected entity's membership highlighted (green chip)

### Feature parity: Mailbox tab
- Scrollable table of last 64 messages; columns: Frame, Sender, Address, Type, Payload
- "Self only" toggle restricts to messages involving the selected entity
- Clear button empties the local buffer
- Message type colour badges (Damage, Spawn, Destroy, State, Anim, Collision, Other)

### Feature parity: Watch tab
- Table of watched (entity, component, field) triples with live value and delta arrow
- Remove (×) per row
- Add-watch row: three cascading selects (Entity → Component → Field) + "+ Watch" button

### Status bar
- Left: connection status text (● Live / ● Disconnected)
- Middle: Frame #N · M entities
- Right: Selected entity name, watch count

### Tests (≥ 25 tests in `Dia/DiaEntityInspector/UI/src/`)
- Entity list filtering by search string
- Entity list filtering by chip
- Selecting an entity updates context strip and tab badges
- Tab switching via `<TabBar>` (Fields / Queries / Mailbox / Watch)
- Component accordion expand/collapse
- Hierarchy nav renders parent link
- Mailbox self-only toggle
- Mailbox clear
- Watch add-row: disabled "+ Watch" until all three selects chosen
- Watch remove (×) fires watch_remove bridge request
- Connection state push shows/hides disconnect overlay
- `window.DiaEditor_onDataChanged` adapter dispatches to same state

## Binding Decisions

| Source | ID | Decision | Impact |
|--------|----|----------|--------|
| DEUI-001 | DEUI-001 | `@dia/editor-ui` via `file:` path | Add `"@dia/editor-ui": "file:../../DiaEditorUI"` to `package.json` |
| DEUI-002 | DEUI-002 | Inline CSSProperties only, no CSS files | Replace all `<style>` blocks with inline style objects and `theme.*` tokens |
| DEUI-007 | DEUI-007 | `file:` paths, not npm workspace | Already addressed in DEUI-001 |
| DEUI-008 | DEUI-008 | `ConnectionStatus` has no host/port knowledge | Use `<ConnectionStatus state=… label="DiaEntityInspector" />` — no config dropdown |

## Decisions

| ID | Decision | Rationale |
|----|----------|-----------|
| EIRM-001 | Field editing uses an inline input in the table row, not `window.prompt()` | `prompt()` is blocked in some CEF configurations and fails silently; inline input is more robust |
| EIRM-002 | Resizable divider implemented as a `useResizableDivider(minPx, maxPx)` hook | `react-resizable-panels` is used by AppFlowEditor but for a multi-pane layout; EntityInspector has a single divider — a `useRef`/`useEffect` hook avoids a dependency for one use case |
| EIRM-003 | `DiaEditor_onDataChanged` assigned via `useEffect` at mount, nulled at unmount | C++ `WebUIBridge::NotifyUIDataChanged` calls `CallJSFunction("DiaEditor_onDataChanged", json)` directly; confirmed at `WebUIBridge.cpp:122`; same pattern as `AppV2.tsx:98` and `AppInspector.tsx:105` |

## Existing test coverage (non-regression gate)

The migration touches no C++. These 48 GoogleTests must pass before and after:

- `Cluiche/Tests/GoogleTests/DiaEntityInspector/IntegrationTestEntityInspectorPlugin.cpp` — 29 tests (`EntityInspectorPluginTest`, `WatchListControllerTest`, `EntityInspectorPluginLifecycle`, `WatchListControllerLifecycle`)
- `Cluiche/Tests/GoogleTests/DiaEntityInspector/TestEntityInspectSerializer.cpp` — 19 tests (`EntityInspectSerializerTest`)

Gate command: `dia run googletest --filter="EntityInspect*:WatchList*"`

## Status

`Approved`
