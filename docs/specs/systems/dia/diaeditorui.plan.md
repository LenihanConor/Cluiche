**Spec:** @docs/specs/systems/dia/diaeditorui.md
**Status:** Done

## Implementation Patterns

### Package structure
`Dia/DiaEditorUI/` is a pure TypeScript library with no Vite app entry point. It uses `"type": "module"` and exposes `src/index.ts` as its main. Each consuming editor adds it via `"@dia/editor-ui": "file:../../../DiaEditorUI"` in its `package.json` dependencies.

No `.vcxproj` — this is TypeScript only.

### Style approach (DEUI-002)
All styles are inline `CSSProperties` objects or values from the `theme` object. No CSS files, no Tailwind, no CSS modules. Style factories (`inputStyle`, `buttonStyle`) return `CSSProperties`.

### Component conventions
- Components: named exports, `React.FC<Props>` typed
- Props interfaces: exported, named `<Component>Props`
- `data-testid` attributes on root element of every component
- `data-state` / `data-connection-state` on stateful indicators (for test selectors)

### useBridge hook pattern
`useBridgeSubscribe` takes a stable array of `{topic, handler}` objects and registers/unregisters via `EditorBridge.subscribe()` inside a `useEffect`. Unsubscribe functions returned by `subscribe()` are called on cleanup.

`useBridgeRequest<T>()` returns a stable callback (via `useCallback`) that calls `EditorBridge.request<T>()`.

Both hooks work inside an iframe context (via `window.parent.postMessage`) and in the main frame (via `window.dia.callCpp`) — the `EditorBridge` abstraction already handles that difference. The hooks simply wrap it.

### Toast store (DEUI-004)
Module-level zustand store. `ToastRenderer` must be mounted once at each app's root. The `useToast()` hook exposes `push(message, severity?, durationMs?)` and `dismiss(id)`. Severity maps to the existing `ToastLevel` enum (`info/success/warning/error`). Duration 0 = sticky.

### Migration pattern (DEUI-005, DEUI-006)
When migrating a consuming editor:
1. Add `@dia/editor-ui` to its `package.json`
2. Replace the local component import with the shared one
3. Delete the now-unused local file
4. Update tests to use the shared component's `data-testid`

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold `Dia/DiaEditorUI/` package — `package.json`, `tsconfig.json`, empty `src/index.ts` | `npm install` in each consuming editor resolves the package | Done | haiku | |
| 2 | Implement `theme.ts` — `theme` object, `injectThemeVars()`, `inputStyle()`, `buttonStyle()` | Visual inspection + TypeScript compile | Done | sonnet | 9 tests pass |
| 3 | Implement `TrafficLightDot.tsx` — extract from AppFlowEditor, delete duplicate in AppFlowInspector | Existing `TrafficLightDot.test.tsx` passes with import path updated | Done | haiku | Added `title` prop for tooltip compat; shims in both editors |
| 4 | Implement `TabBar.tsx` — new component replacing inline tab rows in 3 editors | New vitest unit test; replace inline tabs in AppFlowEditor, AppFlowInspector, PipelineEditor | Done | sonnet | 8 tests; PipelinePanel had no tab bar |
| 5 | Implement `EmptyState.tsx` — generalise from DiaPipelineEditor's `EmptyState.tsx` | Existing PipelineEditor tests pass with import updated | Done | haiku | 4 tests; PipelinePanel updated with message/hint props |
| 6 | Implement `ConnectionStatus.tsx` — unified component; retire `ConnectionStatusDot`, `LiveConnectionButton` | New vitest unit test; update AppFlowEditor + AppFlowInspector | Done | sonnet | 12 tests; shims in both editors |
| 7 | Implement `useBridge.ts` — `useBridgeSubscribe` + `useBridgeRequest<T>` hooks | New vitest unit tests with mocked bridge; update AppFlowInspector bridge wiring | Done | sonnet | 19 tests; supports main-frame and iframe contexts |
| 8 | Implement Toast system — move `useNotifications` + `ToastRenderer` from CluicheEditor into shared package | Existing CluicheEditor toast tests pass with import path updated | Done | sonnet | 16 tests; CluicheEditor migration deferred (bridge wiring) |
| 9 | Wire shared package into all consuming editors — update `package.json` + replace inline colour literals with `theme` imports | `dia pipeline --target cluicheeditor` passes | Done | haiku | injectThemeVars() called at root in all 4 editors; pipeline 3/0 |
