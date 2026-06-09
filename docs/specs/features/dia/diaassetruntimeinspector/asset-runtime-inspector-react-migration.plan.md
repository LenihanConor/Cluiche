**Spec:** @docs/specs/features/dia/diaassetruntimeinspector/asset-runtime-inspector-react-migration.md
**Status:** In Progress

## Implementation Patterns

### Phase 1 — C++ bridge payload tests
New tests go in `Cluiche/Tests/GoogleTests/DiaAssetRuntimeInspector/TestDiaAssetRuntimeInspectorExhaustive.cpp`.
Use a `MockWebUIBridge` (captures `NotifyUIDataChanged` calls into a `std::vector<std::pair<std::string, Json::Value>>`).
Pattern from existing tests: construct a panel, activate with mock bridge, trigger the action, assert the captured JSON.
The exact payload shapes are confirmed from reading `PushSnapshotToUI`, `PushTreeToUI`, `PushInspectorToUI`, `PushLogToUI`, `PushIncrementalToUI` in the `.cpp` files.

### Phase 2 — React+Vite scaffold
Copy structure from `Dia/DiaEntityInspector/UI/`:
- `package.json` → rename to `dia-asset-runtime-inspector-ui`
- `vite.config.ts` → identical pattern (root: 'src', outDir: '../dist', jsdom test env)
- `tsconfig.json` → identical
- `src/main.tsx` → identical (`injectThemeVars()` + `ReactDOM.createRoot`)
- `src/index.html` → `<div id="root">`
- Update C++ plugin URL: `"dia://plugins/assetruntimeinspector/dist/index.html"`

### Phase 3 — Zustand store + types
Follow `DiaEntityInspector/UI/src/store.ts` pattern.
State slices per panel: `assets`, `treeNodes`, `globalAssets`, `inspectorData`, `logEntries`.
Bridge topics map directly to store setters.

### Phase 4 — React components (4 tabs)
Each tab is a React component in `src/components/`.
App.tsx uses `<TabBar>` from `@dia/editor-ui` with 4 tabs.
Bridge wiring in `App.tsx` via `useEffect` (same EIRM-003 pattern as EntityInspector).
State dot colours are local constants (not `theme.*`).

### Phase 5 — Vitest tests
One `.test.tsx` per component + `store.test.ts`.
Use `@testing-library/react` + `vitest`.
Mock bridge calls by directly calling the store setters.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | C++ bridge payload tests — add MockWebUIBridge and 6 payload shape tests to TestDiaAssetRuntimeInspectorExhaustive.cpp | `dia run googletest --filter="AssetStateTablePanel*:StateTransitionLogPanel*:StageAssetTreePanel*:RefCountInspectorPanel*"` passes with new tests | Done | sonnet | 38/38 pass; MockUISystem subclasses IUISystem; 6 BridgePayload tests added |
| 2 | React+Vite scaffold — package.json, vite.config.ts, tsconfig.json, src/main.tsx, src/index.html; update C++ plugin URL | `npm run build` produces dist/index.html; C++ builds clean | Done | haiku | npm install + build produce dist/index.html (147KB); C++ URL updated |
| 3 | Zustand store + types — store.ts with all 5 panel slices, types.ts for all data shapes | `npm run test` passes store.test.ts | Done | sonnet | 13/13 tests pass; InspectorData is discriminated union; appendLogEntry prepends |
| 4 | App.tsx — bridge wiring for all 8 topics, TabBar with 4 tabs, disconnect overlay, ConnectionStatus | `npm run test` passes App-level tests | Done | sonnet | 32/32 tests pass; 19 App tests cover all 8 topics + tab switching |
| 5 | AssetStateTable component — virtual scroll, state filter, ID search, sort, row click, poll interval, status line | `npm run test` passes AssetStateTable.test.tsx (≥ 10 tests) | Done | sonnet | 12 tests; virtual scroll with ResizeObserver guard; 70/70 total |
| 6 | StageAssetTree component — expand/collapse, state dots, global node, ref badge, asset click | `npm run test` passes StageAssetTree.test.tsx (≥ 6 tests) | Done | sonnet | 10 tests; sendBridgeRequest uses direct postMessage |
| 7 | RefCountInspector component — empty state, stage-scoped message, global refs list, missing asset message | `npm run test` passes RefCountInspector.test.tsx (≥ 4 tests) | Done | sonnet | 6 tests; uses EmptyState from @dia/editor-ui |
| 8 | StateTransitionLog component — pause/resume, clear, asset filter, transition filter, marker rendering | `npm run test` passes StateTransitionLog.test.tsx (≥ 6 tests) | Done | sonnet | 10 tests; PAUSED badge; italic markers; 70/70 total |
| 9 | Delete old plain HTML/JS/CSS files; run full pipeline verification | `dia pipeline --target diaassetruntimeinspector` passes; `dia run googletest --filter="AssetStateRow*:AssetStateTablePanel*:SharedPluginState*:TransitionLogEntry*:StateTransitionLogPanel*:SessionContext*:StageAssetTreePanel*:RefCountInspectorPanel*:SharedConnection*"` 60 tests pass | Pending | haiku | |
