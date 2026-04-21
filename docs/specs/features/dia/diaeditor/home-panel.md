# Feature Spec: Home Panel

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Home Panel** | (this document) |

## Problem Statement

CluicheEditor can be launched without a project path, and plugin-registered panels only appear after a project manifest is loaded. Before this feature, a naked launch produced an empty docking area with the message "no editor panels registered", which was a confusing dead-end. The editor needs a built-in, always-registered landing panel that (a) fills the docking area so the user never sees an empty workspace, (b) tells the user what state the editor is in (no project loaded), and (c) explains how to open one.

## Acceptance Criteria

- [x] A `Home` panel is registered unconditionally by `EditorView::Initialize` — before any project or plugin manifest is loaded
- [x] The panel renders at the `dia://editor/home/index.html` URL
- [x] The panel's HTML is self-contained (no JS bridge calls, no network dependencies) so it renders even if the bridge has not been initialized yet
- [x] The panel visually matches the VS Code-dark palette used elsewhere in the editor shell (`#1e1e1e` background, `#569cd6` headings)
- [x] When the editor is launched without a project arg, the docking area contains Home + Output Console and feels populated, not broken
- [x] The panel explains the two ways to advance: command-line project path, Ctrl+Shift+P for command palette
- [x] The panel lists the currently-wired keyboard shortcuts (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z, Ctrl+Shift+P)
- [x] Home stays registered when a project loads — it is not a modal "welcome wizard", it is a docked panel like any other

## Design

### Registration

Built-in panel, registered in `EditorView::Initialize` alongside the Output Console:

```cpp
// Dia/DiaEditor/MVC/EditorView.cpp  (EditorView::Initialize)
RegisterComponent("Home",           "dia://editor/home/index.html");
RegisterComponent("Output Console", "dia://editor/outputconsole/index.html");
```

Because this happens during module startup, the Home panel is in the panel registry *before* any `AfterModulesStart` hook runs — which means it's present whether or not a project loads.

### Asset layout

```
Dia/DiaEditor/Plugin/Assets/home/
    index.html       (self-contained; no external CSS/JS)
```

The post-build step on `CluicheEditor.vcxproj` copies `Dia/DiaEditor/Plugin/Assets/*` into `<exe>/editor/*`, and the CEF scheme handler resolves `dia://editor/home/index.html` against that base. No additional wiring is needed when adding new built-in assets beyond dropping a folder into `Plugin/Assets/`.

### Content (what the user sees)

The panel is deliberately low-fidelity — a static HTML page, no framework:

- `<h1>` title: "Cluiche Editor"
- Primary message: *"No project is loaded. Editor panels registered by plugins will appear here once a project is opened."*
- Section **Open a project**: explains `CluicheEditor.exe Data/test.cluicheproj`
- Section **Shortcuts**: lists Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z / Ctrl+Shift+P
- Hint block at bottom noting that "this panel is always registered by `EditorView` so the docking layout is never empty"

Inline styles keep the palette consistent with the editor shell without introducing a CSS asset dependency.

### Why not a splash screen / modal

Considered and rejected. A splash screen would close once a project loads; a docked panel persists and gives the user somewhere consistent to find shortcut reminders and onboarding breadcrumbs regardless of editor state. This also keeps the docking layout's panel-count invariants stable (panels never go to zero).

### What the panel does NOT do (scope)

- No JS bridge calls — does not subscribe to `console_entries`, does not call `get_panels`, does not emit commands
- No `postMessage` listeners — the iframe is purely static content
- No live editor state display — the panel's content does not react to project state changes
- No "Recent projects" list — deferred until persistent user-config exists

Keeping it static means it can't break the boot path: even if the bridge is misconfigured, the Home panel still renders correctly.

## Implementation Files

- `Dia/DiaEditor/Plugin/Assets/home/index.html` — static panel content
- `Dia/DiaEditor/MVC/EditorView.cpp` — registers `"Home"` panel with URL `dia://editor/home/index.html` in `Initialize`
- `Cluiche/CluicheEditor/CluicheEditor.vcxproj` — post-build copies `Plugin/Assets/*` to `<exe>/editor/*` (covers all built-in panels, not just Home)

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — panel id `"Home"` is registered through the same `RegisterComponent` path as plugin-supplied panels; uses `StringCRC` under the hood |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — registration happens inside `EditorView::Initialize`, which runs as part of the `EditorView` module |
| Platform | PD-004 | No STL in public APIs | **Compliant** — the public surface (`RegisterComponent(const char*, const char*)`) uses C strings |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — asset files are referenced by the existing `CluicheEditor.vcxproj` post-build step; no new project |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| Dia | AD-003 | Namespace convention | **Compliant** — implementation lives in `Dia::Editor::` |
| DiaEditor | SED-005 | CEF replaces Awesomium | **Compliant** — Home renders as an iframe inside the CEF browser |
| DiaEditor | SED-006 | Docking managed by JavaScript library | **Compliant** — Home is a Mosaic tile like any other panel |
| DiaEditor | SED-008 | Observer pattern (not polling) | **N/A** — static panel does not observe model |

**All binding decisions: COMPLIANT**

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Form factor | Splash screen, modal, or docked panel? | Docked panel | Docked panel — survives project open/close and keeps docking layout non-empty |
| 2 | Interactivity | Any live data / buttons / clickable project-open? | None in v0 | None — static HTML only; clickable "Open project" deferred until a file-picker bridge exists |
| 3 | Persistence | Should Home be user-hideable and remember state? | No | No — always registered; if the user rearranges the layout, Mosaic's persistence handles it |
| 4 | When project loads | Does Home stay or disappear? | Stays | Stays registered; plugin panels simply appear alongside it. No special "close Home" logic |
| 5 | Bridge coupling | Can Home call the JS bridge for richer content later? | Allowed but not now | v0 forbids it to keep boot resilient; a later richer-Home feature can opt in |
| 6 | Styling | Match editor dark palette or use neutral? | Match dark palette | Match — `#1e1e1e` / `#569cd6` / `#9cdcfe` inline styles for consistency |
| 7 | Failure mode | What if `dia://editor/home/index.html` 404s? | Hard error | Not in scope — this would indicate the post-build step failed; detected by the smoke test (`CluicheEditor.exe` with no args should show Home) |

## Status

`Approved` - v0 implemented
