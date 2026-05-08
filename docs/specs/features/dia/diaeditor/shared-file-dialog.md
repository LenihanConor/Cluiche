# Feature Spec: Shared File Dialog

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | Shared File Dialog | (this document) |

## Summary

A framework-level file dialog service registered on the WebUIBridge that any editor plugin can invoke from its UI. Provides native Win32 Open/Save dialogs via `"editor.open_file_dialog"` and `"editor.save_file_dialog"` request handlers, replacing duplicated platform-specific code in individual plugins and eliminating JavaScript `prompt()` fallbacks.

## Problem

- DiaApplicationEditor implements inline Win32 `GetOpenFileNameA` calls (duplicated at two call sites).
- DiaAssetCatalogueEditor uses a JavaScript `prompt()` for file paths — no native browse dialog, poor UX.
- Future plugins will each need file dialogs and would either duplicate Win32 code or invent their own solutions.
- No shared abstraction exists in DiaEditor for this common operation.

## Goals

1. Single implementation of native file dialogs owned by DiaEditor framework
2. Consistent file-browsing UX across all plugins
3. Configurable filters and defaults per invocation
4. Both Open and Save variants
5. Migrate existing plugins to the shared handler

## Non-Goals

- Folder-picker dialog (can be added later)
- Multi-file selection (can be added later)
- Cross-platform abstraction (Windows-only per PD-005)
- Custom file preview or thumbnails in the dialog

## Acceptance Criteria

| # | Criterion | Test |
|---|-----------|------|
| AC1 | `editor.open_file_dialog` request handler registered on WebUIBridge during `EditorView::RegisterBuiltInRequestHandlers()` | GoogleTest: handler registered and callable |
| AC2 | `editor.save_file_dialog` request handler registered on WebUIBridge during `EditorView::RegisterBuiltInRequestHandlers()` | GoogleTest: handler registered and callable |
| AC3 | Accepts JSON filter spec: `{ "filters": [{ "name": "...", "ext": "*.ext" }], "default_ext": "ext", "title": "..." }` | GoogleTest: validates input shape |
| AC4 | Returns `{ "success": true, "path": "..." }` on file selection | Manual: open dialog, pick file, verify response |
| AC5 | Returns `{ "success": false }` when dialog is cancelled | Manual: open dialog, cancel, verify response |
| AC6 | DiaApplicationEditor migrated — both call sites (main open + import add) use `editor.open_file_dialog` | Code review: no inline `GetOpenFileNameA` in DiaApplicationEditor |
| AC7 | DiaAssetCatalogueEditor migrated — `onOpen()` uses `editor.open_file_dialog` instead of `prompt()` | Manual: open catalogue, File > Open shows native dialog |
| AC8 | Optional `initial_dir` parameter sets the dialog's starting directory | Manual: pass initial_dir, dialog opens in that folder |

## API Design

### Request: `editor.open_file_dialog`

**Input:**
```json
{
  "filters": [
    { "name": "Dia Game Project", "ext": "*.diagame" },
    { "name": "Asset Catalogue", "ext": "*.json" },
    { "name": "All Files", "ext": "*.*" }
  ],
  "default_ext": "diagame",
  "title": "Open Project",
  "initial_dir": "C:/path/to/start"
}
```

All fields optional. Defaults: no filter (all files), no default extension, OS-default title, current working directory.

**Output (success):**
```json
{ "success": true, "path": "C:\\GitHub\\Cluiche\\Cluiche\\Assets\\CluicheTest\\cluichetest.diagame" }
```

**Output (cancelled):**
```json
{ "success": false }
```

### Request: `editor.save_file_dialog`

Same input/output shape. Uses `GetSaveFileNameA` instead of `GetOpenFileNameA`. Adds `OFN_OVERWRITEPROMPT` flag.

### C++ Implementation

```cpp
namespace Dia::Editor
{
    class FileDialogHandler
    {
    public:
        static Json::Value HandleOpenFileDialog(const Json::Value& data);
        static Json::Value HandleSaveFileDialog(const Json::Value& data);
    };
}
```

Static utility — no state, no lifetime management. Registered in `EditorView::RegisterBuiltInRequestHandlers()` alongside existing handlers.

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaEditor/UI/FileDialogHandler.h` | New — declares static handler functions |
| `Dia/DiaEditor/UI/FileDialogHandler.cpp` | New — Win32 dialog implementation |
| `Dia/DiaEditor/MVC/EditorView.cpp` | Modified — register handlers in `RegisterBuiltInRequestHandlers()` |
| `Dia/DiaEditor/DiaEditor.vcxproj` | Modified — add new files |
| `Dia/DiaEditor/DiaEditor.vcxproj.filters` | Modified — add new files |
| `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` | Modified — replace inline `GetOpenFileNameA` with bridge call |
| `Dia/DiaAssetCatalogueEditor/UI/index.html` | Modified — replace `prompt()` with `diaRequest('editor.open_file_dialog', ...)` |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | Compliant — handler keys are `StringCRC("editor.open_file_dialog")` and `StringCRC("editor.save_file_dialog")` |
| PD-004 | No STL in public APIs | Compliant — `FileDialogHandler` uses `Json::Value` (already established pattern in WebUIBridge handlers); no `std::string` in public signature |
| PD-005 | x64 only | Compliant — Win32 API works on x64; no 32-bit concerns |
| PD-006 | VS project files are source of truth | Compliant — new files added to `.vcxproj` and `.vcxproj.filters` |
| PD-007 | C++20 required | Compliant — no language-level concerns |
| PD-008 | Directory.Build.props owns build paths | Compliant — no output path changes |
| PD-009 | Generated output under `Cluiche/out/` | N/A — no generated output |
| PD-010 | `.diagame` is project root | N/A — this feature is format-agnostic |
| AD-001 | Module YAML frontmatter | N/A — not a new module, additions to existing DiaEditor |
| AD-002 | No STL in public APIs | Compliant — see PD-004 |
| AD-003 | `Dia::<Module>::` namespace | Compliant — `Dia::Editor::FileDialogHandler` |
| SED-001 | Minimal stable plugin interface | Compliant — no IEditorPlugin changes; plugins call through existing bridge |
| SED-003 | Plugins at `Dia/Dia<System>Editor/` | N/A — this is framework code in DiaEditor, not a plugin |
| SED-015 | DiaEditor is pure library, no DiaApplication dep | Compliant — `FileDialogHandler` is static utility, no Module/Phase/PU |
| SED-020 | Plugin output under `Cluiche/out/CluicheEditor/<Plugin>/` | N/A — no file output |

## Open Questions

| # | Question | Status |
|---|----------|--------|
| 1 | Should multi-select be supported? | Deferred — can be added as optional `"multi_select": true` flag later |
| 2 | Should initial_dir be required or optional? | Resolved — optional, defaults to CWD |
| 3 | Should both DiaApplicationEditor call sites migrate? | Resolved — yes, both (main open + import add) |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | API | Should the filter `ext` field support multiple extensions per filter (e.g., `"*.diagame;*.diaapp"`)? | Yes — Win32 supports semicolon-separated patterns natively | Yes — pass semicolon-separated patterns directly to Win32 |
| 2 | Threading | Is it safe to call `GetOpenFileNameA` from the WebUIBridge handler thread? | Yes — CEF message loop runs on the main/UI thread, and Win32 file dialogs require the UI thread. The bridge handler executes synchronously on that thread. | Yes — confirmed safe; bridge handlers run on UI thread |
| 3 | Migration | Should DiaApplicationEditor's migration be atomic or can it keep a local fallback during transition? | Atomic — one-shot replacement, no fallback needed since the shared handler is identical in behavior | Atomic — identical Win32 logic relocated; fallback adds dead code for a scenario that can't occur |
| 4 | UX | Should the dialog remember the last-used directory per filter type? | No — keep it simple; plugins pass `initial_dir` if they want persistence (via their session context) | No — handler is stateless; plugins own directory persistence via session context (SED-021) |

## Status

`Done`
