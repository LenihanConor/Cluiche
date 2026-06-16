# Plan: toast-notifications

**Spec:** @docs/specs/applications/dia/systems/diaeditor/toast-notifications.md
**Status:** Done

## Implementation Patterns

### C++ Layer (`Dia/DiaEditor/Notification/`)

- `NotificationService` is a plain class (not a Module/Phase) per SED-015
- Registers with `PluginServiceLocator` via `T::kUniqueId` pattern (see `PluginServiceLocator.h`)
- Pushes toasts by calling `WebUIBridge::NotifyUIDataChanged("editor.notification", payload)` — same pattern as `entity_inspector.inspect_data` and `scene_editor.dirty_changed`
- JSON payload: `{ "id": "<uuid>", "level": "error|warning|info|success", "title": "...", "message": "..." }`
- No STL in public API — `const char*` for strings, enum for level

### JS Shell Layer (`Cluiche/CluicheEditor/UI/src/notifications/`)

- `useNotifications.ts` — Zustand store (first Zustand store in the shell; add `zustand` to package.json)
- `ToastRenderer.tsx` — React component mounted at shell level in `main.tsx`, positioned fixed bottom-right with high z-index (above fullscreen panels per design decision)
- EditorBridge already subscribes to topics via `DiaEditor_onDataChanged` and re-broadcasts to iframes — no new plumbing needed there
- Add `notify()` to `EditorBridge` so JS-side code (shell + iframes) can raise toasts without C++ round-trip
- Iframes call `window.parent.postMessage({ __diaFromFrame: true, payload: { type: "editor.notify", data: {...} } })` — shell handles via existing frame message listener

### Plugin iframe pattern

- Iframes do NOT render their own toasts
- Iframes post up to shell; shell renders everything
- Pattern matches existing `__diaFromFrame` message flow (see PluginBrowser `index.html`)

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `NotificationService.h` — enum, struct, class with `Push`/`DismissAll`, `kUniqueId` | Compiles | Done | sonnet | Header-only API definition |
| 2 | Create `NotificationService.cpp` — serializes `NotificationRequest` to JSON, calls `NotifyUIDataChanged` | Compiles | Done | sonnet | Depends on WebUIBridge pointer (set via `Initialize(WebUIBridge*)`) |
| 3 | Add files to `DiaEditor.vcxproj` + `.vcxproj.filters` (new Notification filter group) | `dia pipeline --target cluicheeditor` builds | Done | haiku | Mechanical edit |
| 4 | Register `NotificationService` in CluicheEditor startup — construct, initialize with bridge, register on `PluginServiceLocator` | Compiles | Done | sonnet | In PluginLoaderModule::SetBridge |
| 5 | Add `zustand` dependency to CluicheEditor UI `package.json` | `npm install` succeeds | Done | haiku | |
| 6 | Create `useNotifications.ts` — Zustand store: toast queue (max 5 visible), `addToast`, `removeToast`, `dismissAll`, auto-dismiss timers | Unit-testable logic | Done | sonnet | Default durations: info 4s, success 3s, warning 6s, error manual |
| 7 | Create `ToastRenderer.tsx` — renders toast stack, dismiss button, hover pauses timer, slide-in/fade-out CSS, aria attributes | Visual in browser | Done | sonnet | Fixed position bottom-right, z-index above mosaic |
| 8 | Add `notify()` to `EditorBridge.ts` — accepts `{level, title, message?}`, generates ID, feeds store directly | Call from console works | Done | sonnet | Also subscribe to `editor.notification` topic to handle C++ pushes |
| 9 | Handle `editor.notify` in shell's iframe message listener — iframes can raise toasts via postMessage | iframe plugin can trigger toast | Done | sonnet | Add case in existing `__diaFromFrame` handler |
| 10 | Mount `<ToastRenderer />` in `main.tsx` at shell level | Renders in app | Done | haiku | After DockingManager, before CommandPalette |
| 11 | Build + manual verify: trigger toast from C++ (e.g. connection fail) and from JS (`notify()` in console) | Visual confirmation | Done | sonnet | `dia pipeline --target cluicheeditor` passes; 5915/5916 tests pass (1 pre-existing flaky WebSocket test) |
