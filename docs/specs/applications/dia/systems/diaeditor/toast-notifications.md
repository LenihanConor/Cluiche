# Feature Spec: toast-notifications

**Parent:** @docs/specs/applications/dia/systems/diaeditor/diaeditor.md
**Status:** Done
**Plan:** @docs/specs/applications/dia/systems/diaeditor/toast-notifications.plan.md

## Summary

Plugin operations (errors, saves, connection failures) are either silent or use `window.alert`, giving no consistent user feedback. This feature adds a framework-level notification service so any plugin (C++ or JS) can push toasts that render in a unified, auto-dismissing overlay managed by the shell.

## Goals

- Centralized notification API available to all plugins without per-plugin UI work
- Consistent visual language for error, warning, info, and success feedback
- Errors persist until dismissed; non-errors auto-dismiss after a configurable timeout
- Toast queue with stacking — multiple toasts coexist, oldest dismissed first
- Both C++ and JavaScript code paths can raise toasts

## Acceptance Criteria

### C++ API (`Dia::Editor::NotificationService`)

- New class in `Dia/DiaEditor/Notification/NotificationService.h`
- Exposes:
  ```cpp
  namespace Dia::Editor {
      enum class NotificationLevel { kInfo, kSuccess, kWarning, kError };

      struct NotificationRequest {
          NotificationLevel level;
          const char* title;
          const char* message;       // optional body (nullptr = title-only)
          float durationSeconds;     // 0 = use default per level
      };

      class NotificationService {
      public:
          void Push(const NotificationRequest& request);
          void DismissAll();
      };
  }
  ```
- `Push` serializes the notification as JSON and calls `WebUIBridge::NotifyUIDataChanged("editor.notification", ...)`
- Default durations: info 4s, success 3s, warning 6s, error 0 (manual dismiss only)
- Plugins access `NotificationService` via `PluginServiceLocator::GetService<NotificationService>()`

### WebUIBridge integration

- JS-side plugins can raise toasts without round-tripping to C++:
  ```js
  window.CluicheEditor.notify({ level: "error", title: "Save failed", message: "Disk full" });
  ```
- The shell's `EditorBridge.ts` exposes `notify()` which feeds the same toast renderer directly
- C++ pushes arrive via the existing `DiaEditor_onDataChanged` topic mechanism (topic: `"editor.notification"`)

### Shell toast renderer (CluicheEditor React UI)

- React component mounted at shell level (not inside plugin iframes)
- Renders toasts in bottom-right corner, stacked vertically (newest at bottom)
- Maximum 5 visible toasts; overflow queued and shown as slots free
- Each toast shows: level icon/colour, title, optional body, dismiss button (x)
- Auto-dismiss countdown pauses on hover
- Colour coding: info (blue), success (green), warning (amber), error (red)
- Entry animation: slide-in from right; exit animation: fade-out
- Accessible: `role="alert"`, `aria-live="polite"` (info/success) or `"assertive"` (warning/error)

### Plugin iframe relay

- Shell re-broadcasts `editor.notification` topic to plugin iframes via `postMessage` (existing pattern)
- Plugin iframes calling `window.CluicheEditor.notify()` post a message UP to the shell; shell renders the toast
- No plugin renders its own toasts — all rendering is shell-owned

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaEditor/Notification/NotificationService.h` | New — API class |
| `Dia/DiaEditor/Notification/NotificationService.cpp` | New — serializes to JSON, calls WebUIBridge |
| `Dia/DiaEditor/DiaEditor.vcxproj` | Add new files |
| `Dia/DiaEditor/DiaEditor.vcxproj.filters` | Add new filter group |
| `Cluiche/CluicheEditor/UI/src/notifications/ToastRenderer.tsx` | New — React toast component |
| `Cluiche/CluicheEditor/UI/src/notifications/useNotifications.ts` | New — Zustand store for toast queue |
| `Cluiche/CluicheEditor/UI/src/bridge/EditorBridge.ts` | Add `notify()` + subscribe to `editor.notification` topic |
| `Cluiche/CluicheEditor/UI/src/main.tsx` | Mount ToastRenderer at shell level |

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| SED-015 | DiaEditor is a pure library — no ApplicationFlow | `NotificationService` is a plain class; no Module/Phase. Compliant. |
| SED-022 | PluginServiceLocator for inter-plugin services | Plugins obtain `NotificationService` via `GetService<NotificationService>()`. Compliant. |
| PD-004 / AD-002 | No STL in public APIs | `NotificationRequest` uses `const char*`, not `std::string`. Compliant. |
| SED-004 | JSON wire format | Topic payload is JSON (`{ level, title, message, id }`). Compliant. |

## Design Decisions

1. **Toast persistence across plugin switches** — Toasts render on top of fullscreen panels. They are shell-level UI with a high z-index; never queued or hidden by layout changes.

2. **Action buttons** — v1 is purely informational (level + title + optional body). Action callbacks (e.g. "Retry") deferred to a follow-up if needed.
