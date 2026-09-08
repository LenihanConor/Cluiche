# Feature Spec: template-editor-react-migration

**System:** DiaEntityTemplateEditor
**App:** Dia
**Status:** Done

## Parent
@docs/specs/applications/dia/systems/diablueprinteditor/diablueprinteditor.md

## Summary

Replace the single-file plain HTML/JS UI (`Dia/DiaEntityTemplateEditor/UI/index.html`, ~1140 lines) with a React+Vite project that consumes `@dia/editor-ui` — aligning DiaEntityTemplateEditor with the five existing React+Vite editor plugins in tech stack, theme, test coverage, and bridge pattern.

No C++ changes. No protocol changes. No behaviour changes. The bridge topics (`entity_template_editor.*`), request/response contract, and JSON data shapes are identical. Only the UI implementation changes.

**Location:** `Dia/DiaEntityTemplateEditor/UI/` — replaces `index.html` with a Vite project whose `dist/index.html` is the served file.

## Goals

- DiaEntityTemplateEditor UI is TypeScript, testable with Vitest, and visually consistent with the other editor panels
- All bridge request/response calls use `useBridgeRequest` from `@dia/editor-ui` — no raw `diaRequest()` wrapper
- All push notifications use `useBridgeSubscribe` from `@dia/editor-ui` — no raw `window.DiaEditor_onDataChanged`
- `@dia/editor-ui` components used where they fit: `EmptyState`, `useToast`/`ToastRenderer`, `inputStyle`, `buttonStyle`, `theme`, `useResizableDivider` (promoted), `navigateFailedUtils` (promoted)
- All key UI behaviours have Vitest unit tests

## Binding Decisions

- SD-BPED-001: Three-panel layout (list / editor / usage) — unchanged
- SD-BPED-002: Blueprint file types: `.diaentitytemplate`, `.diacamera`, `.dialight` — unchanged
- Tech standard: React 18 + Vite + Zustand + `@dia/editor-ui` (from entity-inspector-react-migration precedent)

## Acceptance Criteria

### Project scaffold
- `Dia/DiaEntityTemplateEditor/UI/package.json` — name `dia-entity-template-editor-ui`, `@dia/editor-ui: file:../../DiaEditorUI`
- `Dia/DiaEntityTemplateEditor/UI/vite.config.ts` — root=`src`, build outDir=`../dist`, test env=`jsdom`
- `Dia/DiaEntityTemplateEditor/UI/tsconfig.json` — strict, ES2020, JSX react-jsx
- `npm run build` produces `dist/index.html` + `dist/assets/*.js`
- `npm run test` runs Vitest; all tests passing

### Shared library promotions (pre-work)
- `useResizableDivider` promoted to `@dia/editor-ui`, EntityInspector updated to import from there
- `navigateFailedUtils` promoted to `@dia/editor-ui`, AppFlowEditor updated to import from there

### Bridge integration
- `injectThemeVars()` called at app root before `ReactDOM.createRoot`
- Push notifications wired via `useBridgeSubscribe`:
  - `entity_template_editor.project_changed` → project overlay state
  - `asset_catalogue.registry_changed` → refresh blueprint list
  - `entity_template_editor.navigated` → select+load navigated blueprint
  - `entity_template_editor.navigate_failed` → show navigate-failed modal
- All request handlers use `useBridgeRequest<T>()`:
  - `entity_template_editor.get_project_state`
  - `entity_template_editor.get_list`
  - `entity_template_editor.load`
  - `entity_template_editor.update_field` (replaces bulk save — field-level persistence)
  - `entity_template_editor.add_component`
  - `entity_template_editor.remove_component`
  - `entity_template_editor.get_available_components`
  - `entity_template_editor.get_usage`
  - `asset_catalogue.create_asset` (navigate-failed create)
  - `asset_catalogue.delete_record` (navigate-failed remove)

### Shared components used
- `EmptyState` — project overlay ("No project loaded"), center panel ("Select a blueprint"), usage panel ("No scene references")
- `useToast` + `ToastRenderer` — status messages (saved, error, added, removed) replacing old `setStatus()` with auto-clear
- `inputStyle` — search inputs, field inputs
- `buttonStyle` — toolbar buttons, picker add, remove component, modal actions
- `theme` — all colour literals replaced with `theme.*` tokens
- `useResizableDivider` — left panel resize (from shared lib after promotion)
- `navigateFailedUtils` — `buildNavigateFailedContext` + `deriveExpectedPath` (from shared lib after promotion)

### Feature parity
- **Left panel**: Blueprint list grouped by type (Entity/Camera/Light), search filter, selection highlight, refresh
- **Center panel**: Blueprint ID header, component accordion (open/collapse), field rows with type-aware inputs, clear button, green override border, placeholder defaults, add/remove component with cascade confirmation
- **Right panel**: Usage list showing scene IDs + instance counts
- **Component picker**: Search, keyboard navigation (arrow/Enter), count display, cascade warning modal
- **Navigate-failed modal**: Create file / Remove entry / Dismiss
- **Project overlay**: Shown when no project loaded, hidden on project load
- **Toolbar**: Deferred — no C++ `create_template` endpoint exists; will be added by a future feature spec when the backend supports it

### Cleanup
- Old `UI/index.html` deleted
- Old `UI/mockup-navigate-failed.html` deleted
- C++ `GetUIPath()` unchanged (already points to `index.html` — Vite dist outputs `index.html`)

## Open Design Questions

1. **ConfirmDialog as shared component?** — Two confirmation modals needed (add component cascade, remove component cascade). AppFlowEditor has `RiskyChangeDialog` locally. Worth promoting a generic `ConfirmDialog` to `@dia/editor-ui`? Deferring for now — keep local, promote later if a third plugin needs it.
