# Feature Spec: Plugin Theme System

**Parent:** @docs/specs/applications/cluicheeditor.md
**Status:** Done
**Plan:** @docs/specs/features/cluicheeditor/plugin-theme/plugin-theme-system.plan.md
**Research:** @docs/research/plugin_ui_theme/summary.md

## Summary

Provide a shared, classless CSS theme for all CluicheEditor plugins so they are visually consistent by default, require zero CSS knowledge to author, and can be globally re-themed by editing one file.

## Goals

1. **Zero-effort consistency** — plugins written with semantic HTML look correct without any CSS classes or inline styles
2. **Single-file re-theming** — changing `dia-overrides.css` variables updates every plugin's appearance
3. **Foolproof delivery** — the editor shell injects the theme into plugin iframes automatically; plugins cannot forget it
4. **Low skill floor** — plugin authors write `<button>`, `<table>`, `<section>` etc. and get professional styling
5. **Scaffold integration** — `dia scaffold plugin` emits a template that demonstrates correct patterns

## Non-Goals

- Light theme support (dark-only for v1)
- Complex component library (no Web Components or JS framework)
- Build tooling (no Sass, Tailwind, or preprocessor dependency)
- Styling the React shell itself (that remains inline in TSX components)

## Acceptance Criteria

| # | Criterion | Testable |
|---|-----------|----------|
| AC-1 | `pico.min.css` + `dia-overrides.css` exist in `Cluiche/CluicheEditor/UI/theme/` | File exists |
| AC-2 | `dia-overrides.css` maps Pico CSS variables to the existing dark palette (#1e1e1e backgrounds, #d4d4d4 text, #0e639c accents) | Visual inspection |
| AC-3 | The editor shell auto-injects a `<link>` to the theme CSS into every plugin iframe at load time | New plugin renders with theme without any manual `<link>` |
| AC-4 | All 6 existing plugins (home, hello, debug-layers, outputconsole, gameconnection, pluginbrowser) migrated to semantic HTML — inline `<style>` blocks removed | Diff shows style blocks removed; visual parity with current look |
| AC-5 | `dia scaffold plugin` emits an `index.html` template using semantic HTML patterns that render correctly with the injected theme | Run scaffold, open plugin, confirm styled |
| AC-6 | Modifying a CSS variable in `dia-overrides.css` (e.g. `--pico-primary`) visibly changes all plugins without other edits | Change variable, reload, verify |
| AC-7 | No plugin manually includes a `<link>` to the theme — injection is the sole delivery mechanism | Grep for theme link in plugin HTML files returns zero hits |

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | Download Pico CSS, create `Cluiche/CluicheEditor/UI/theme/pico.min.css` | S |
| 2 | Author `dia-overrides.css` mapping Pico variables to existing Cluiche palette | S |
| 3 | Register `dia://theme/` custom URL scheme in CEF (C++ SchemeHandlerFactory) to serve theme CSS files | M |
| 4 | Implement C++ CEF-layer theme injection — prepend `<link href="dia://theme/pico.min.css">` and `<link href="dia://theme/dia-overrides.css">` to plugin HTML before rendering | M |
| 5 | Migrate `home` plugin to semantic HTML (remove inline styles) | S |
| 6 | Migrate `hello` plugin to semantic HTML | S |
| 7 | Migrate `debug-layers` plugin to semantic HTML | S |
| 8 | Migrate `outputconsole` plugin to semantic HTML | S |
| 9 | Migrate `gameconnection` plugin to semantic HTML + read chart colors via `getComputedStyle()` | M |
| 10 | Migrate `pluginbrowser` plugin to semantic HTML | M |
| 11 | Update `dia scaffold plugin` template to emit themed semantic HTML | S |
| 12 | Add theme files to CluicheEditor vcxproj for deployment | S |
| 13 | Visual verification — confirm all plugins match current aesthetic | S |

## Files Touched

- `Cluiche/CluicheEditor/UI/theme/pico.min.css` (new)
- `Cluiche/CluicheEditor/UI/theme/dia-overrides.css` (new)
- `Cluiche/CluicheEditor/UI/src/` — shell injection logic (DockingManager or iframe creation)
- `Cluiche/CluicheEditor/Plugins/home/index.html` (migration)
- `Cluiche/CluicheEditor/Plugins/hello/index.html` (migration)
- `Cluiche/CluicheEditor/Plugins/debug-layers/index.html` (migration)
- `Cluiche/CluicheEditor/Plugins/outputconsole/index.html` (migration)
- `Cluiche/CluicheEditor/Plugins/gameconnection/index.html` (migration)
- `Cluiche/CluicheEditor/Plugins/pluginbrowser/index.html` (migration)
- `Dia/DiaCLI/dia_cli/commands/scaffold/plugin_cmd.py` (template update)
- `Cluiche/CluicheEditor/CluicheEditor.vcxproj` (theme file references)

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AED-005 | UI built with React + DiaUICEF (CEF) | Theme is pure CSS injected into CEF iframes — no conflict with React shell. Pico CSS works in any Chromium-based renderer. |
| AED-003 | Each system owns its editor as `<System>/Editor/` subdirectory | Theme is shared infrastructure owned by CluicheEditor, not by any individual system plugin. Plugins don't need to carry their own styles. |

No binding decisions are in conflict.

## Open Design Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Where should the theme CSS files physically live relative to plugin iframes so the injected `<link>` path resolves correctly? Options: (a) relative path from plugin to `../../theme/`, (b) served via a custom `dia://theme/` scheme, (c) absolute filesystem path injected by the shell. | **(b)** Custom `dia://theme/` URL scheme registered in CEF. Clean, path-independent, and survives folder reorganization. |
| 2 | Should the injection happen in the React shell (JS-side, modifying iframe `srcdoc` or `onload`) or in the C++ CEF layer (modifying the HTML before it reaches the renderer)? JS-side is simpler; C++-side is lower-level but more foolproof. | **(b)** C++ CEF layer. Inject the `<link>` before HTML reaches the renderer — maximally foolproof, no JS cooperation required from plugins. |
| 3 | For the `gameconnection` plugin which uses Canvas for charts — those charts are JS-drawn and won't be affected by CSS. Should chart colors also read from CSS variables (via `getComputedStyle`) to stay on-theme, or is hardcoded chart coloring acceptable? | **(a)** Yes — charts read theme colors via `getComputedStyle()` so re-theming is truly global including Canvas-drawn elements. |
